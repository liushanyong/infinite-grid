#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

#include "acdb/AcDbHandle.h"
#include "acdb/AcDbMaterial.h"
#include "acdb/AcDbTessellate.h"
#include "rendering/RenderTypes.h"
#include "acgs/model/DrawContext.h"

namespace acgs
{

class AcGsModel;

enum class AcGiShaderAlgorithm
{
    Realistic,
    Cad,
    Conceptual,
    Depth,
    Grayscale,
    Shaded,
    Sketch,
    Wireframe,
    XRay
};

struct AcGiMaterial
{
    AcGiShaderAlgorithm algorithm = AcGiShaderAlgorithm::Shaded;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 accentColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissiveColor{0.0f, 0.0f, 0.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.35f;
    float transparency = 0.5f;
    float lineWidth = 1.0f;
    float alphaMaskCutoff = 0.0f;

    bool operator==(const AcGiMaterial &) const = default;
};

// A renderer-side resolution entry for AcGi face-material IDs.  The material
// carries the shading style; |imageIndex| is a 1-based index into the owning
// AcGsModel geometry images (0 = untextured material color).
struct AcGiMaterialBinding
{
    acdb::AcDbObjectId materialId;
    AcGiMaterial material;
    std::uint32_t imageIndex = 0;
};

// Converts the canonical AcDbMaterial authoring fields into the submission
// value. Shared by AcGsModel and AcGsMetafile so replay and direct model
// assembly cannot drift.
[[nodiscard]] inline AcGiMaterial toAcGiMaterial(
    const acdb::AcDbMaterial &material)
{
    AcGiMaterial converted;
    converted.algorithm =
        material.renderClass == acdb::RenderClass::Realistic
            ? AcGiShaderAlgorithm::Realistic
            : AcGiShaderAlgorithm::Shaded;
    converted.baseColor = material.baseColorFactor;
    converted.accentColor = material.specularFactor;
    converted.emissiveColor = material.emissiveFactor;
    converted.metallic = material.metallicFactor;
    converted.roughness = material.roughnessFactor;
    converted.transparency = 1.0f - material.baseColorFactor.a;
    converted.alphaMaskCutoff =
        material.alphaMask > 0.5f ? material.alphaMaskCutoff : 0.0f;
    return converted;
}

// A submission-ready mesh command preserves instancing.  Large stress fields
// stay compact on the CPU while the backend retains control of its pipelines.
struct MeshBatchCommand
{
    rendering::MeshType prototype = rendering::MeshType::Cube;
    // Non-zero: draw uploaded static geometry (imported glTF) instead of the
    // prototype; ids are device-allocated (createStaticMesh).
    std::uint32_t staticMeshId = 0;
    bool opaque = false;
    bool realistic = false;
    bool cadAlgorithm = false;
    glm::vec4 material{0.0f, 0.35f, 0.0f, 0.5f};
    // 0 = scene default albedo texture; static-model batches set their own.
    std::uint32_t diffuseTextureIndex = 0;
    AcGiMaterial acgiMaterial;
    std::vector<rendering::MeshInstance> instances;
};

// GPU curve commands keep double-precision authoring data on the CPU. The
// submitter rebases them; the curve shader performs the parameter evaluation.
struct CurveBatchCommand
{
    rendering::CurveAlgorithm algorithm = rendering::CurveAlgorithm::Bezier;
    int degree = 3;
    uint32_t sampleCount = 64;
    AcGiMaterial acgiMaterial;
    std::vector<glm::dvec3> controlPoints;
    std::string name = "Curve";
    std::vector<double> weights;
    std::vector<double> knots;
    glm::dvec3 center{0.0};
    glm::dvec3 axisU{1.0, 0.0, 0.0};
    glm::dvec3 axisV{0.0, 1.0, 0.0};
    double radius = 1.0;
    double startAngle = 0.0;
    double sweep = glm::two_pi<double>();
};

// The infinite grid is a protocol command, not a finite tessellated CAD line
// soup.  It keeps the shader-side infinite plane while making the renderer
// call explicit and owned by the scene submitter.
struct GridCommand
{
    rendering::GridRenderData data;
};

// Environment state is explicit in the draw list so demo code does not need a
// separate direct-to-renderer setup path.
struct LightCommand
{
    rendering::RealisticLightsRenderData data;
};

// Frame-local and static draw lists share one command vocabulary.  Geometry
// carries strokes/fills/points through the existing WorldDraw sink; mesh and
// grid commands cover renderer-accelerated paths that must not be flattened.
//
// (The regen-protocol AcGiDrawable lives in acgi/AcGiDrawable.h: entity
// drawables feed this draw list through the Gs replay, AcGsView::submit
// consumes it.)
class AcGsModel final
{
public:
    [[nodiscard]] acdb::TessellatedEntity &geometry() { return geometry_; }
    [[nodiscard]] const acdb::TessellatedEntity &geometry() const
    {
        return geometry_;
    }

    [[nodiscard]] std::vector<MeshBatchCommand> &meshBatches()
    {
        return meshBatches_;
    }

    [[nodiscard]] std::vector<CurveBatchCommand> &curveBatches()
    {
        return curveBatches_;
    }
    [[nodiscard]] const std::vector<CurveBatchCommand> &curveBatches() const
    {
        return curveBatches_;
    }

    CurveBatchCommand &addCurveBatch()
    {
        curveBatches_.emplace_back();
        return curveBatches_.back();
    }
    [[nodiscard]] const std::vector<MeshBatchCommand> &meshBatches() const
    {
        return meshBatches_;
    }

    void setGrid(const rendering::GridRenderData &data)
    {
        grid_ = GridCommand{data};
    }
    [[nodiscard]] const std::optional<GridCommand> &grid() const
    {
        return grid_;
    }
    void setLights(const rendering::RealisticLightsRenderData &data)
    {
        lights_ = LightCommand{data};
    }
    [[nodiscard]] const std::optional<LightCommand> &lights() const
    {
        return lights_;
    }

    AcGiMaterialBinding &setMaterial(const acdb::AcDbObjectId &materialId,
                                     AcGiMaterial material,
                                     std::uint32_t imageIndex = 0)
    {
        AcGiMaterialBinding &binding = materials_[materialId];
        binding.materialId = materialId;
        binding.material = material;
        binding.imageIndex = imageIndex;
        return binding;
    }

    // Convenience bridge for AcDbMaterial authors.  diffuseTexture is a
    // session key elsewhere in the import path; texture bindings that already
    // have decoded pixels pass their geometry image index explicitly.
    AcGiMaterialBinding &setMaterial(const acdb::AcDbObjectId &materialId,
                                     const acdb::AcDbMaterial &material,
                                     std::uint32_t imageIndex = 0)
    {
        return setMaterial(materialId, toAcGiMaterial(material), imageIndex);
    }

    [[nodiscard]] const AcGiMaterialBinding *findMaterial(
        const acdb::AcDbObjectId &materialId) const
    {
        if (!materialId.isValid())
            return nullptr;
        const auto found = materials_.find(materialId);
        return found != materials_.end() ? &found->second : nullptr;
    }

    MeshBatchCommand &addMeshBatch(const rendering::MeshType prototype,
                                   const bool opaque, const bool realistic,
                                   const glm::vec4 &material,
                                   const bool cadAlgorithm = false)
    {
        MeshBatchCommand batch;
        batch.prototype = prototype;
        batch.opaque = opaque;
        batch.realistic = realistic;
        batch.cadAlgorithm = cadAlgorithm;
        batch.material = material;
        batch.acgiMaterial.algorithm =
            realistic ? AcGiShaderAlgorithm::Realistic
                      : AcGiShaderAlgorithm::Shaded;
        meshBatches_.push_back(std::move(batch));
        return meshBatches_.back();
    }

    void merge(const AcGsModel &other)
    {
        // Raster image indices are 1-based offsets into the owning model.
        // Rebase the incoming fill/image references before appending images.
        const std::uint32_t imageIndexOffset =
            static_cast<std::uint32_t>(geometry_.images.size());
        geometry_.strokes.insert(geometry_.strokes.end(),
                                 other.geometry_.strokes.begin(),
                                 other.geometry_.strokes.end());
        const size_t fillBegin = geometry_.fills.size();
        geometry_.fills.insert(geometry_.fills.end(),
                               other.geometry_.fills.begin(),
                               other.geometry_.fills.end());
        geometry_.points.insert(geometry_.points.end(),
                                other.geometry_.points.begin(),
                                other.geometry_.points.end());
        geometry_.images.insert(geometry_.images.end(),
                                other.geometry_.images.begin(),
                                other.geometry_.images.end());
        for (size_t i = fillBegin; i < geometry_.fills.size(); ++i)
        {
            if (geometry_.fills[i].imageIndex != 0)
                geometry_.fills[i].imageIndex += imageIndexOffset;
        }
        for (const auto &[materialId, binding] : other.materials_)
        {
            AcGiMaterialBinding merged = binding;
            merged.materialId = materialId;
            if (merged.imageIndex != 0)
                merged.imageIndex += imageIndexOffset;
            materials_.insert_or_assign(materialId, std::move(merged));
        }
        meshBatches_.insert(meshBatches_.end(),
                             other.meshBatches_.begin(),
                             other.meshBatches_.end());
        curveBatches_.insert(curveBatches_.end(),
                             other.curveBatches_.begin(),
                             other.curveBatches_.end());
        if (other.grid_)
            grid_ = *other.grid_;
        if (other.lights_)
            lights_ = *other.lights_;
    }

    void clear()
    {
        geometry_ = {};
        materials_.clear();
        meshBatches_.clear();
        curveBatches_.clear();
        grid_.reset();
        lights_.reset();
    }

    // Frame lists are rebuilt every frame. Keeping vector capacity avoids a
    // full free/alloc cycle while still releasing the contained commands.
    void clearKeepCapacity()
    {
        geometry_.strokes.clear();
        geometry_.fills.clear();
        geometry_.points.clear();
        geometry_.images.clear();
        materials_.clear();
        meshBatches_.clear();
        curveBatches_.clear();
        grid_.reset();
        lights_.reset();
    }

private:
    acdb::TessellatedEntity geometry_;
    std::unordered_map<acdb::AcDbObjectId, AcGiMaterialBinding> materials_;
    std::vector<MeshBatchCommand> meshBatches_;
    std::vector<CurveBatchCommand> curveBatches_;
    std::optional<GridCommand> grid_;
    std::optional<LightCommand> lights_;
};

} // namespace acgs
