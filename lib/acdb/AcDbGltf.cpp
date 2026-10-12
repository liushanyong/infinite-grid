#include "acdb/AcDbGltf.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Must match lib/tinygltf/source/tiny_gltf.cpp: the library compiles
// without stb_image, and the header's inline member initializers reference
// the default load/write callbacks unless these macros are defined here too.
#define TINYGLTF_NO_STB_IMAGE 1
#define TINYGLTF_NO_STB_IMAGE_WRITE 1
#include <tiny_gltf.h>

namespace acdb
{

namespace
{

constexpr int kGltfModeTriangles = 4;

// glTF is right-handed Y-up; this project is right-handed Z-up (the XY grid
// plane with +Z up).  A +90-degree rotation about X maps Y-up onto Z-up with
// determinant +1, so triangle winding survives untouched.
glm::dmat4 gltfToWorldRotation()
{
    return glm::rotate(glm::dmat4(1.0), glm::radians(90.0),
                       glm::dvec3(1.0, 0.0, 0.0));
}

// The project builds tinygltf with TINYGLTF_NO_STB_IMAGE, so image bytes
// only arrive through this callback.  Keep them encoded ("as-is"): the host
// decodes through bimg when uploading, and the acdb layer stays free of any
// image decoder dependency.
bool storeRawImage(tinygltf::Image *image, const int imageIndex,
                   std::string *, std::string *, int, int,
                   const unsigned char *bytes, int size, void *)
{
    (void)imageIndex;
    image->as_is = true;
    image->width = 0;
    image->height = 0;
    image->component = 0;
    image->bits = 8;
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    if (bytes && size > 0)
        image->image.assign(bytes, bytes + size);
    return true;
}

// Sequential accessor component reader.  Sparse accessors are rejected
// (rare, and the demo meshes never use them); strides and every integer
// component type the spec allows are handled here.
class AccessorReader
{
public:
    AccessorReader() = default;

    AccessorReader(const tinygltf::Model &gltf, int accessorIndex)
    {
        if (accessorIndex < 0 ||
            size_t(accessorIndex) >= gltf.accessors.size())
            return;
        const tinygltf::Accessor &accessor =
            gltf.accessors[size_t(accessorIndex)];
        // The vendored generation of tinygltf leaves sparse.count
        // uninitialized unless isSparse is set; gate on the flag first.
        if (accessor.bufferView < 0 ||
            size_t(accessor.bufferView) >= gltf.bufferViews.size() ||
            (accessor.sparse.isSparse && accessor.sparse.count > 0))
            return;
        const tinygltf::BufferView &view =
            gltf.bufferViews[size_t(accessor.bufferView)];
        if (view.buffer < 0 || size_t(view.buffer) >= gltf.buffers.size())
            return;
        const tinygltf::Buffer &buffer = gltf.buffers[size_t(view.buffer)];

        // GetTypeSizeInBytes yields the component count for the accessor's
        // element type; both helpers return -1 for unknown kinds.
        componentCount_ = size_t(
            std::max(0, tinygltf::GetTypeSizeInBytes(
                            static_cast<std::uint32_t>(accessor.type))));
        componentSize_ = size_t(std::max(
            0, tinygltf::GetComponentSizeInBytes(static_cast<std::uint32_t>(
                   accessor.componentType))));
        if (componentCount_ == 0 || componentSize_ == 0)
            return;
        stride_ = view.byteStride != 0
                      ? size_t(view.byteStride)
                      : componentCount_ * componentSize_;
        if (stride_ < componentCount_ * componentSize_)
            return;
        if (view.byteOffset + accessor.byteOffset +
                stride_ * (accessor.count ? accessor.count - 1 : 0) +
                componentCount_ * componentSize_ >
            buffer.data.size())
            return;

        accessor_ = &accessor;
        base_ = buffer.data.data() + view.byteOffset + accessor.byteOffset;
        valid_ = true;
    }

    bool valid() const { return valid_; }
    size_t count() const { return valid_ ? accessor_->count : 0; }

    double read(size_t element, size_t component) const
    {
        const unsigned char *p =
            base_ + stride_ * element + component * componentSize_;
        double value = 0.0;
        switch (accessor_->componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            value = double(*reinterpret_cast<const int8_t *>(p));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            value = double(*p);
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            value = double(*reinterpret_cast<const int16_t *>(p));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            value = double(*reinterpret_cast<const uint16_t *>(p));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            value = double(*reinterpret_cast<const uint32_t *>(p));
            break;
        default:
            value = double(*reinterpret_cast<const float *>(p));
            break;
        }
        if (accessor_->normalized)
        {
            switch (accessor_->componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                value = std::max(value, -128.0) / 127.0;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                value /= 255.0;
                break;
            case TINYGLTF_COMPONENT_TYPE_SHORT:
                value = std::max(value, -32768.0) / 32767.0;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                value /= 65535.0;
                break;
            default:
                break;
            }
        }
        return value;
    }

private:
    const tinygltf::Accessor *accessor_ = nullptr;
    const unsigned char *base_ = nullptr;
    size_t stride_ = 0;
    size_t componentCount_ = 0;
    size_t componentSize_ = 0;
    bool valid_ = false;
};

// glTF nodes carry either an explicit column-major matrix or TRS; both
// compose into one local matrix here and the node walk bakes the product
// into the vertices.
glm::dmat4 localNodeTransform(const tinygltf::Node &node)
{
    if (node.matrix.size() == 16)
    {
        glm::dmat4 matrix(1.0);
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                matrix[column][row] = node.matrix[size_t(column * 4 + row)];
        return matrix;
    }
    glm::dvec3 translation(0.0);
    glm::dquat rotation(1.0, 0.0, 0.0, 0.0);
    glm::dvec3 scale(1.0);
    if (node.translation.size() == 3)
        translation = {node.translation[0], node.translation[1],
                       node.translation[2]};
    if (node.rotation.size() == 4)
        rotation = glm::dquat(node.rotation[3], node.rotation[0],
                              node.rotation[1], node.rotation[2]);
    if (node.scale.size() == 3)
        scale = {node.scale[0], node.scale[1], node.scale[2]};
    return glm::translate(glm::dmat4(1.0), translation) *
           glm::mat4_cast(rotation) *
           glm::scale(glm::dmat4(1.0), scale);
}

std::vector<std::uint8_t> readWholeFile(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return {};
    const std::streamoff size = stream.tellg();
    if (size <= 0)
        return {};
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size),
                                   std::uint8_t{0});
    stream.seekg(0);
    stream.read(reinterpret_cast<char *>(data.data()), size);
    if (!stream)
        return {};
    return data;
}

struct ImportContext
{
    const tinygltf::Model *gltf = nullptr;
    std::filesystem::path baseDir;
    std::string textureKeyBase; // file name, disambiguates embedded images
    AcDbGltfImport *out = nullptr;
    std::map<std::string, std::size_t> textureByKey;
    std::map<std::string, int> usedNames;
    bool hasGeometry = false;
    glm::dvec3 minimum{0.0};
    glm::dvec3 maximum{0.0};
};

// Returns the texture key for the material texture at |textureIndex|,
// registering the image bytes (embedded bufferView / data URI / external
// file) on first use.
std::string resolveTexture(ImportContext &ctx, int textureIndex)
{
    const tinygltf::Model &gltf = *ctx.gltf;
    if (textureIndex < 0 || size_t(textureIndex) >= gltf.textures.size())
        return {};
    const tinygltf::Texture &texture = gltf.textures[size_t(textureIndex)];
    if (texture.source < 0 || size_t(texture.source) >= gltf.images.size())
        return {};
    const tinygltf::Image &image = gltf.images[size_t(texture.source)];

    std::string key = image.uri;
    if (key.empty())
        key = ctx.textureKeyBase + "#image" +
              std::to_string(texture.source);

    const auto found = ctx.textureByKey.find(key);
    if (found != ctx.textureByKey.end())
        return key;

    std::vector<std::uint8_t> bytes = image.image;
    if (bytes.empty() && !image.uri.empty())
    {
        // tinygltf keeps only the uri when the external image load failed;
        // try once against the model's own directory before giving up.
        bytes = readWholeFile(ctx.baseDir / image.uri);
    }
    if (bytes.empty())
        return {};

    AcDbGltfTexture entry;
    entry.key = key;
    entry.name = image.name.empty() ? key : image.name;
    entry.bytes = std::move(bytes);
    ctx.textureByKey[key] = ctx.out->textures.size();
    ctx.out->textures.push_back(std::move(entry));
    return key;
}

// The vendored tinygltf is the parameter-map generation of the glTF 2.0
// API: PBR fields live in Material::values keyed by their JSON names, and
// everything else (emissive/alphaMode/alphaCutoff) in Material::additionalValues.
AcDbMaterial resolveMaterial(ImportContext &ctx, int materialIndex,
                             float *entityAlpha)
{
    AcDbMaterial material;
    const tinygltf::Model &gltf = *ctx.gltf;
    if (materialIndex < 0 || size_t(materialIndex) >= gltf.materials.size())
        return material;

    const tinygltf::Material &gltfMaterial =
        gltf.materials[size_t(materialIndex)];
    const auto baseColor = gltfMaterial.values.find("baseColorFactor");
    if (baseColor != gltfMaterial.values.end() &&
        baseColor->second.number_array.size() == 4)
    {
        material.baseColorFactor = glm::vec4(
            float(baseColor->second.number_array[0]),
            float(baseColor->second.number_array[1]),
            float(baseColor->second.number_array[2]),
            float(baseColor->second.number_array[3]));
    }
    const auto metallic = gltfMaterial.values.find("metallicFactor");
    if (metallic != gltfMaterial.values.end() &&
        metallic->second.has_number_value)
        material.metallicFactor = float(metallic->second.number_value);
    const auto roughness = gltfMaterial.values.find("roughnessFactor");
    if (roughness != gltfMaterial.values.end() &&
        roughness->second.has_number_value)
        material.roughnessFactor = float(roughness->second.number_value);

    const auto emissive = gltfMaterial.additionalValues.find("emissiveFactor");
    if (emissive != gltfMaterial.additionalValues.end() &&
        emissive->second.number_array.size() >= 3)
    {
        material.emissiveFactor = glm::vec4(
            float(emissive->second.number_array[0]),
            float(emissive->second.number_array[1]),
            float(emissive->second.number_array[2]), 1.0f);
    }

    std::string alphaMode = "OPAQUE";
    const auto alphaModeValue =
        gltfMaterial.additionalValues.find("alphaMode");
    if (alphaModeValue != gltfMaterial.additionalValues.end())
        alphaMode = alphaModeValue->second.string_value;
    float alphaCutoff = 0.5f;
    const auto alphaCutoffValue =
        gltfMaterial.additionalValues.find("alphaCutoff");
    if (alphaCutoffValue != gltfMaterial.additionalValues.end() &&
        alphaCutoffValue->second.has_number_value)
        alphaCutoff = float(alphaCutoffValue->second.number_value);
    if (alphaMode == "MASK")
    {
        material.alphaMask = 1.0f;
        material.alphaMaskCutoff = glm::clamp(alphaCutoff, 0.0f, 1.0f);
    }
    else if (alphaMode == "BLEND")
    {
        material.alphaMask = 0.0f;
    }

    const auto baseTexture = gltfMaterial.values.find("baseColorTexture");
    if (baseTexture != gltfMaterial.values.end())
        material.diffuseTexture =
            resolveTexture(ctx, baseTexture->second.TextureIndex());

    // Entity alpha drives the blend path in the instance pipeline.  MASK
    // keeps full opacity (the cutoff gates fragments in the shader);
    // authored base-color alpha below 1 lands in the blended batch.
    *entityAlpha = material.alphaMask > 0.5f
                       ? 1.0f
                       : glm::clamp(material.baseColorFactor.a, 0.0f, 1.0f);
    return material;
}

bool importGltfPrimitive(ImportContext &ctx,
                         const tinygltf::Mesh &gltfMesh, int primitiveIndex,
                         const glm::dmat4 &nodeWorld, const std::string &name,
                         AcDbPolyFaceMesh &out, std::string &skipReason)
{
    const tinygltf::Model &gltf = *ctx.gltf;
    const tinygltf::Primitive &primitive =
        gltfMesh.primitives[size_t(primitiveIndex)];

    if (primitive.mode != kGltfModeTriangles)
    {
        skipReason = name + ": non-triangle primitive mode " +
                     std::to_string(primitive.mode);
        return false;
    }

    const auto positionAttribute = primitive.attributes.find("POSITION");
    if (positionAttribute == primitive.attributes.end())
    {
        skipReason = name + ": missing POSITION";
        return false;
    }
    const AccessorReader positions(gltf, positionAttribute->second);
    if (!positions.valid() || positions.count() < 3)
    {
        skipReason = name + ": POSITION accessor unusable";
        return false;
    }

    const AccessorReader normals(gltf, [&]() -> int {
        const auto found = primitive.attributes.find("NORMAL");
        return found != primitive.attributes.end() ? found->second : -1;
    }());
    if (normals.valid() && normals.count() != positions.count())
    {
        skipReason = name + ": NORMAL count mismatch";
        return false;
    }

    const AccessorReader uvs(gltf, [&]() -> int {
        const auto found = primitive.attributes.find("TEXCOORD_0");
        return found != primitive.attributes.end() ? found->second : -1;
    }());
    if (uvs.valid() && uvs.count() != positions.count())
    {
        skipReason = name + ": TEXCOORD_0 count mismatch";
        return false;
    }

    // Indices: every component type the spec allows; without an index
    // accessor the primitive is a sequential triangle soup.
    const size_t vertexCount = positions.count();
    std::vector<std::uint32_t> indices;
    if (primitive.indices >= 0)
    {
        const AccessorReader indexReader(gltf, primitive.indices);
        if (!indexReader.valid() || indexReader.count() < 3)
        {
            skipReason = name + ": index accessor unusable";
            return false;
        }
        indices.reserve(indexReader.count());
        for (size_t i = 0; i < indexReader.count(); ++i)
        {
            const double value = indexReader.read(i, 0);
            if (value < 0.0 || value >= double(vertexCount))
            {
                skipReason = name + ": index out of range";
                return false;
            }
            indices.push_back(std::uint32_t(value));
        }
    }
    else
    {
        indices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i)
            indices[i] = std::uint32_t(i);
    }

    // Bake node transform (+ Y-up to Z-up) into the vertices.  Normals go
    // through the inverse-transpose so non-uniform node scales stay right.
    const glm::dmat4 total = gltfToWorldRotation() * nodeWorld;
    const glm::dmat3 normalMatrix =
        glm::mat3(glm::inverseTranspose(total));

    out.geometry.positions.resize(vertexCount);
    out.geometry.normals.resize(normals.valid() ? vertexCount : 0);
    out.geometry.uvs.resize(uvs.valid() ? vertexCount : 0);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        const glm::dvec4 transformed =
            total * glm::dvec4(positions.read(i, 0), positions.read(i, 1),
                               positions.read(i, 2), 1.0);
        out.geometry.positions[i] =
            AcGePoint3d(transformed.x, transformed.y, transformed.z);
        ctx.hasGeometry = true;
        ctx.minimum = glm::min(ctx.minimum, glm::dvec3(transformed));
        ctx.maximum = glm::max(ctx.maximum, glm::dvec3(transformed));
        if (normals.valid())
        {
            const glm::dvec3 normal = normalMatrix * glm::dvec3(
                normals.read(i, 0), normals.read(i, 1), normals.read(i, 2));
            out.geometry.normals[i] = glm::vec3(glm::normalize(normal));
        }
        if (uvs.valid())
            out.geometry.uvs[i] = glm::vec2(float(uvs.read(i, 0)),
                                            float(uvs.read(i, 1)));
    }

    // Area-weighted smooth normals when the file carries none (draw-over
    // exporters commonly omit them).  Accumulated per indexed vertex, so
    // the shared-vertex connectivity in the index buffer is what smooths.
    if (!normals.valid())
    {
        out.geometry.normals.assign(vertexCount, glm::vec3(0.0f));
        for (size_t t = 0; t + 2 < indices.size(); t += 3)
        {
            const glm::dvec3 a(out.geometry.positions[indices[t]].x,
                               out.geometry.positions[indices[t]].y,
                               out.geometry.positions[indices[t]].z);
            const glm::dvec3 b(out.geometry.positions[indices[t + 1]].x,
                               out.geometry.positions[indices[t + 1]].y,
                               out.geometry.positions[indices[t + 1]].z);
            const glm::dvec3 c(out.geometry.positions[indices[t + 2]].x,
                               out.geometry.positions[indices[t + 2]].y,
                               out.geometry.positions[indices[t + 2]].z);
            const glm::dvec3 faceNormal = glm::cross(b - a, c - a);
            for (const std::uint32_t index : {indices[t], indices[t + 1],
                                              indices[t + 2]})
                out.geometry.normals[index] += glm::vec3(faceNormal);
        }
        for (glm::vec3 &normal : out.geometry.normals)
        {
            const float length2 = glm::dot(normal, normal);
            normal = length2 > 0.0f ? normal / std::sqrt(length2)
                                    : glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }

    out.geometry.indices = std::move(indices);
    return true;
}

void walkGltfNodes(ImportContext &ctx, const std::vector<int> &nodeIndices,
                   const glm::dmat4 &parent)
{
    const tinygltf::Model &gltf = *ctx.gltf;
    for (int nodeIndex : nodeIndices)
    {
        if (nodeIndex < 0 || size_t(nodeIndex) >= gltf.nodes.size())
            continue;
        const tinygltf::Node &node = gltf.nodes[size_t(nodeIndex)];
        const glm::dmat4 world = parent * localNodeTransform(node);
        if (node.mesh >= 0 && size_t(node.mesh) < gltf.meshes.size())
        {
            const tinygltf::Mesh &gltfMesh = gltf.meshes[size_t(node.mesh)];
            for (int primitiveIndex = 0;
                 primitiveIndex < int(gltfMesh.primitives.size());
                 ++primitiveIndex)
            {
                std::string baseName = gltfMesh.name.empty()
                                           ? "mesh" + std::to_string(node.mesh)
                                           : gltfMesh.name;
                if (gltfMesh.primitives.size() > 1)
                    baseName += "#" + std::to_string(primitiveIndex);
                int &used = ctx.usedNames[baseName];
                std::string entityName = baseName;
                if (used > 0)
                    entityName += "@" + std::to_string(used);
                ++used;

                AcDbPolyFaceMesh entity;
                entity.common.name = "GLTF/" + entityName;
                entity.common.layer = "GLTF";
                std::string skipReason;
                if (importGltfPrimitive(ctx, gltfMesh, primitiveIndex, world,
                                        entityName, entity, skipReason))
                {
                    float entityAlpha = 1.0f;
                    entity.material = resolveMaterial(
                        ctx,
                        gltfMesh.primitives[size_t(primitiveIndex)].material,
                        &entityAlpha);
                    entity.common.color = glm::vec4(
                        entity.material.baseColorFactor.r,
                        entity.material.baseColorFactor.g,
                        entity.material.baseColorFactor.b, entityAlpha);
                    entity.style = MeshStyle::Realistic;
                    ctx.out->meshes.push_back(std::move(entity));
                }
                else if (!skipReason.empty())
                {
                    ctx.out->skipped.push_back(skipReason);
                }
            }
        }
        walkGltfNodes(ctx, node.children, world);
    }
}

// ---- export helpers -------------------------------------------------------

std::size_t appendBufferView(tinygltf::Model &model, tinygltf::Buffer &buffer,
                             const void *data, std::size_t byteLength,
                             int target)
{
    // Every accessor type written here is 4-byte scalars, so pad to keep
    // each view 4-byte aligned (spec requirement for accessors).
    while (buffer.data.size() % 4 != 0)
        buffer.data.push_back(0);
    const std::size_t offset = buffer.data.size();
    buffer.data.resize(offset + byteLength);
    if (byteLength)
        std::memcpy(buffer.data.data() + offset, data, byteLength);

    tinygltf::BufferView view;
    view.buffer = 0;
    view.byteOffset = offset;
    view.byteLength = byteLength;
    view.target = target;
    model.bufferViews.push_back(std::move(view));
    return model.bufferViews.size() - 1;
}

std::size_t appendFloatAccessor(tinygltf::Model &model,
                                std::size_t bufferView, std::size_t count,
                                int elementType,
                                const std::vector<double> *minMax)
{
    tinygltf::Accessor accessor;
    // The vendored tinygltf leaves byteOffset uninitialized in its
    // Accessor constructor; zero it or garbage gets serialized.
    accessor.byteOffset = 0;
    accessor.bufferView = int(bufferView);
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = count;
    accessor.type = elementType;
    if (minMax)
    {
        accessor.minValues = minMax[0];
        accessor.maxValues = minMax[1];
    }
    model.accessors.push_back(std::move(accessor));
    return model.accessors.size() - 1;
}

// Converts one geometry channel (positions/normals/uvs) into tightly packed
// float32 and tracks min/max while converting.
template <std::size_t N>
std::vector<float> packFloatChannel(
    const std::vector<glm::vec<N, float, glm::defaultp>> &values,
    std::vector<double> &minOut, std::vector<double> &maxOut)
{
    std::vector<float> packed(values.size() * N);
    minOut.assign(N, 0.0);
    maxOut.assign(N, 0.0);
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        for (std::size_t c = 0; c < N; ++c)
        {
            const float component = values[i][c];
            packed[i * N + c] = component;
            const double asDouble = double(component);
            if (i == 0 || asDouble < minOut[c])
                minOut[c] = asDouble;
            if (i == 0 || asDouble > maxOut[c])
                maxOut[c] = asDouble;
        }
    }
    return packed;
}

} // namespace

bool importGltf(const std::filesystem::path &path, AcDbGltfImport &out,
                std::string *error)
{
    out = AcDbGltfImport{};

    std::vector<std::uint8_t> fileData = readWholeFile(path);
    if (fileData.empty())
    {
        if (error)
            *error = "cannot read " + path.string();
        return false;
    }

    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(storeRawImage, nullptr);

    std::string parseError;
    std::string parseWarning;
    const std::filesystem::path baseDir =
        path.parent_path().empty() ? std::filesystem::path(".")
                                   : path.parent_path();
    const std::string baseDirString = baseDir.string();
    bool parsed = false;
    const std::string extension = [&] {
        std::string value = path.extension().string();
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        return value;
    }();
    if (extension == ".glb")
    {
        parsed = loader.LoadBinaryFromMemory(
            &gltf, &parseError, &parseWarning, fileData.data(),
            static_cast<unsigned int>(fileData.size()), baseDirString);
    }
    else
    {
        parsed = loader.LoadASCIIFromString(
            &gltf, &parseError, &parseWarning,
            reinterpret_cast<const char *>(fileData.data()),
            static_cast<unsigned int>(fileData.size()), baseDirString);
    }
    if (!parsed)
    {
        if (error)
            *error = "glTF parse failed: " + parseError;
        return false;
    }

    ImportContext ctx;
    ctx.gltf = &gltf;
    ctx.baseDir = baseDir;
    ctx.textureKeyBase = path.filename().string();
    ctx.out = &out;

    const bool defaultSceneValid = gltf.defaultScene >= 0 &&
                                   size_t(gltf.defaultScene) <
                                       gltf.scenes.size();
    if (defaultSceneValid)
    {
        walkGltfNodes(ctx, gltf.scenes[size_t(gltf.defaultScene)].nodes,
                      glm::dmat4(1.0));
    }
    else if (!gltf.scenes.empty())
    {
        walkGltfNodes(ctx, gltf.scenes.front().nodes, glm::dmat4(1.0));
    }
    else
    {
        // Scene-less documents: walk every node as a root.
        std::vector<int> allNodes(gltf.nodes.size());
        for (std::size_t i = 0; i < gltf.nodes.size(); ++i)
            allNodes[i] = int(i);
        walkGltfNodes(ctx, allNodes, glm::dmat4(1.0));
    }

    out.minimum = ctx.minimum;
    out.maximum = ctx.maximum;
    if (out.meshes.empty())
    {
        std::ostringstream message;
        message << "no triangle geometry imported from " << path.string();
        for (const std::string &reason : out.skipped)
            message << "; " << reason;
        if (error)
            *error = message.str();
        return false;
    }
    return true;
}

bool exportGltf(const std::vector<const AcDbPolyFaceMesh *> &meshes,
                const std::filesystem::path &path,
                const AcDbGltfExportOptions &options, std::string *error)
{
    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "infinite-grid acdb::exportGltf";

    tinygltf::Buffer &buffer = model.buffers.emplace_back();
    tinygltf::Scene &scene = model.scenes.emplace_back();
    scene.name = "infinite-grid";

    std::map<std::string, std::size_t> materialByName;
    int exported = 0;
    for (const AcDbPolyFaceMesh *entity : meshes)
    {
        if (!entity || entity->geometry.positions.size() < 3 ||
            entity->geometry.indices.size() < 3)
            continue;

        const std::vector<AcGePoint3d> &positions = entity->geometry.positions;
        const std::size_t vertexCount = positions.size();

        // Positions (with the min/max the POSITION accessor requires).
        std::vector<float> packedPositions(vertexCount * 3);
        std::vector<double> positionMin(3, 0.0);
        std::vector<double> positionMax(3, 0.0);
        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            const float xyz[3] = {float(positions[i].x), float(positions[i].y),
                                  float(positions[i].z)};
            for (int c = 0; c < 3; ++c)
            {
                packedPositions[i * 3 + c] = xyz[c];
                const double asDouble = double(xyz[c]);
                if (i == 0 || asDouble < positionMin[c])
                    positionMin[c] = asDouble;
                if (i == 0 || asDouble > positionMax[c])
                    positionMax[c] = asDouble;
            }
        }
        const std::size_t positionView =
            appendBufferView(model, buffer, packedPositions.data(),
                             packedPositions.size() * sizeof(float),
                             TINYGLTF_TARGET_ARRAY_BUFFER);
        const std::size_t positionAccessor =
            appendFloatAccessor(model, positionView, vertexCount, TINYGLTF_TYPE_VEC3,
                                nullptr);
        model.accessors[positionAccessor].minValues = positionMin;
        model.accessors[positionAccessor].maxValues = positionMax;

        // Normals (zero-filled when the entity carries none).
        std::vector<glm::vec3> normals = entity->geometry.normals;
        if (normals.size() != vertexCount)
            normals.assign(vertexCount, glm::vec3(0.0f));
        std::vector<double> normalMin;
        std::vector<double> normalMax;
        const std::vector<float> packedNormals =
            packFloatChannel<3>(normals, normalMin, normalMax);
        const std::size_t normalView =
            appendBufferView(model, buffer, packedNormals.data(),
                             packedNormals.size() * sizeof(float),
                             TINYGLTF_TARGET_ARRAY_BUFFER);
        const std::size_t normalAccessor =
            appendFloatAccessor(model, normalView, vertexCount, TINYGLTF_TYPE_VEC3,
                                nullptr);

        // UVs (zero-filled when the entity carries none).
        std::vector<glm::vec2> uvs = entity->geometry.uvs;
        if (uvs.size() != vertexCount)
            uvs.assign(vertexCount, glm::vec2(0.0f));
        std::vector<double> uvMin;
        std::vector<double> uvMax;
        const std::vector<float> packedUvs =
            packFloatChannel<2>(uvs, uvMin, uvMax);
        const std::size_t uvView =
            appendBufferView(model, buffer, packedUvs.data(),
                             packedUvs.size() * sizeof(float),
                             TINYGLTF_TARGET_ARRAY_BUFFER);
        const std::size_t uvAccessor =
            appendFloatAccessor(model, uvView, vertexCount, TINYGLTF_TYPE_VEC2, nullptr);

        // Indices (uint32).
        std::vector<std::uint32_t> indices = entity->geometry.indices;
        const std::size_t indexView =
            appendBufferView(model, buffer, indices.data(),
                             indices.size() * sizeof(std::uint32_t),
                             TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);
        tinygltf::Accessor indexAccessor;
        indexAccessor.byteOffset = 0; // see appendFloatAccessor
        indexAccessor.bufferView = int(indexView);
        indexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        indexAccessor.count = indices.size();
        indexAccessor.type = TINYGLTF_TYPE_SCALAR;
        model.accessors.push_back(std::move(indexAccessor));
        const std::size_t indexAccessorIndex = model.accessors.size() - 1;

        // One material per entity, deduped by the exported field set.
        std::size_t materialIndex = model.materials.size();
        const std::string materialKey =
            entity->common.name + "/" +
            std::to_string(entity->material.metallicFactor) + "/" +
            std::to_string(entity->material.roughnessFactor);
        const auto materialFound = materialByName.find(materialKey);
        if (materialFound != materialByName.end())
        {
            materialIndex = materialFound->second;
        }
        else
        {
            tinygltf::Material material;
            material.name = entity->common.name + "_material";
            const glm::vec4 baseColor = entity->material.baseColorFactor;
            tinygltf::Parameter baseColorParameter;
            baseColorParameter.number_array = {
                double(baseColor.r), double(baseColor.g), double(baseColor.b),
                double(baseColor.a)};
            material.values["baseColorFactor"] = baseColorParameter;
            tinygltf::Parameter metallicParameter;
            metallicParameter.number_value =
                double(entity->material.metallicFactor);
            metallicParameter.has_number_value = true;
            material.values["metallicFactor"] = metallicParameter;
            tinygltf::Parameter roughnessParameter;
            roughnessParameter.number_value =
                double(entity->material.roughnessFactor);
            roughnessParameter.has_number_value = true;
            material.values["roughnessFactor"] = roughnessParameter;
            tinygltf::Parameter emissiveParameter;
            emissiveParameter.number_array = {
                double(entity->material.emissiveFactor.r),
                double(entity->material.emissiveFactor.g),
                double(entity->material.emissiveFactor.b)};
            material.additionalValues["emissiveFactor"] = emissiveParameter;
            if (entity->material.alphaMask > 0.5f)
            {
                tinygltf::Parameter alphaModeParameter;
                alphaModeParameter.string_value = "MASK";
                material.additionalValues["alphaMode"] = alphaModeParameter;
                tinygltf::Parameter alphaCutoffParameter;
                alphaCutoffParameter.number_value =
                    double(entity->material.alphaMaskCutoff);
                alphaCutoffParameter.has_number_value = true;
                material.additionalValues["alphaCutoff"] =
                    alphaCutoffParameter;
            }
            else if (baseColor.a < 1.0f)
            {
                tinygltf::Parameter alphaModeParameter;
                alphaModeParameter.string_value = "BLEND";
                material.additionalValues["alphaMode"] = alphaModeParameter;
            }
            materialByName[materialKey] = model.materials.size();
            model.materials.push_back(std::move(material));
        }

        tinygltf::Primitive primitive;
        primitive.attributes["POSITION"] = int(positionAccessor);
        primitive.attributes["NORMAL"] = int(normalAccessor);
        primitive.attributes["TEXCOORD_0"] = int(uvAccessor);
        primitive.indices = int(indexAccessorIndex);
        primitive.material = int(materialIndex);
        primitive.mode = kGltfModeTriangles;

        tinygltf::Mesh mesh;
        mesh.name = entity->common.name.empty()
                        ? "mesh" + std::to_string(exported)
                        : entity->common.name;
        mesh.primitives.push_back(std::move(primitive));

        tinygltf::Node node;
        node.name = mesh.name;
        node.mesh = int(model.meshes.size());
        scene.nodes.push_back(int(model.nodes.size()));

        model.nodes.push_back(std::move(node));
        model.meshes.push_back(std::move(mesh));
        ++exported;
    }

    if (exported == 0)
    {
        if (error)
            *error = "no mesh geometry to export";
        return false;
    }

    model.defaultScene = 0;
    if (!tinygltf::TinyGLTF().WriteGltfSceneToFile(
            &model, path.string(), /*embedImages*/ false,
            options.embedBuffers, /*prettyPrint*/ !options.binary,
            options.binary))
    {
        if (error)
            *error = "failed to write " + path.string();
        return false;
    }
    return true;
}

} // namespace acdb
