#include "rendering/BgfxRenderer.h"
#include "rendering/ProceduralMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <fstream>
#include <utility>
#include <vector>
#include <cstdarg>

#include <glm/gtc/type_ptr.hpp>

#include <bx/allocator.h>
#include <bx/error.h>
#include <bimg/decode.h>
namespace GridShaders
{
// Rebuild this translation unit whenever the embedded infinite-grid shader
// headers are regenerated.
#include "shaders/infiniteGrid/vertex.h"
#include "shaders/infiniteGrid/frag.h"
} // namespace GridShaders
namespace CubeShaders
{
#include "shaders/centerAnchor/vertex.h"
#include "shaders/centerAnchor/frag.h"
} // namespace CubeShaders
namespace PointShaders
{
#include "shaders/targetPoint/vertex.h"
#include "shaders/targetPoint/frag.h"
} // namespace PointShaders
namespace MeshInstanceShaders
{
#include "shaders/centerAnchorInstance/vertex.h"
#include "shaders/centerAnchorInstance/frag.h"
} // namespace MeshInstanceShaders

namespace RealisticMeshShaders
{
#include "shaders/realisticMesh/vs_realistic_mesh.h"
#include "shaders/realisticMesh/fs_pbr_mesh.h"
} // namespace RealisticMeshShaders
namespace PointInstanceShaders
{
#include "shaders/targetPointInstance/vertex.h"
#include "shaders/targetPointInstance/frag.h"
} // namespace PointInstanceShaders

namespace CadAlgorithmShaders
{
#include "shaders/cadAlgorithm/vs_cad_instance.h"
#include "shaders/cadAlgorithm/fs_cad_styles.h"
} // namespace CadAlgorithmShaders

namespace PrimPolylineShaders
{
#include "shaders/cadPrimitives/polyline/vs_polyline.h"
#include "shaders/cadPrimitives/polyline/fs_polyline.h"
} // namespace PrimPolylineShaders

namespace LineInstanceShaders
{
#include "shaders/cadPrimitives/lineInstance/vs_line_instance.h"
} // namespace LineInstanceShaders

namespace MeshEdgeRibbonShaders
{
#include "shaders/meshEdgeRibbon/vs_mesh_edge_ribbon.h"
} // namespace MeshEdgeRibbonShaders

namespace PrimFilledShaders
{
#include "shaders/cadPrimitives/filled/vs_filled.h"
#include "shaders/cadPrimitives/filled/fs_filled.h"
} // namespace PrimFilledShaders

namespace PrimCurveShaders
{
#include "shaders/cadPrimitives/curve/vs_curve.h"
#include "shaders/cadPrimitives/curve/fs_curve.h"
} // namespace PrimCurveShaders

namespace PresentShaders
{
#include "shaders/present/vs_present.h"
#include "shaders/present/fs_present.h"
} // namespace PresentShaders
namespace GpuPickShaders
{
#include "shaders/gpuPick/vs_gpu_pick.h"
#include "shaders/gpuPick/fs_gpu_pick.h"
} // namespace GpuPickShaders

namespace TextSdfShaders
{
#include "text/shaders/vs_text_sdf.h"
#include "text/shaders/fs_text_sdf.h"
} // namespace TextSdfShaders
namespace SelectionOutlineShaders
{
#include "shaders/selectionOutline/fs_outline_overlay.h"
} // namespace SelectionOutlineShaders

namespace rendering
{
namespace
{
class BgfxScreenshotCallback final : public bgfx::CallbackI
{
public:
    void fatal(const char* filePath, uint16_t line,
               bgfx::Fatal::Enum code, const char* str) override
    {
        std::cerr << "bgfx fatal: " << filePath << ":" << line
                  << " code=" << int(code) << " " << str << std::endl;
        std::abort();
    }

    void traceVargs(const char* filePath, uint16_t line,
                    const char* format, va_list argList) override
    {
        char text[2048];
        vsnprintf(text, sizeof(text), format, argList);
        std::cerr << "bgfx: " << filePath << ":" << line << " " << text
                  << std::endl;
    }

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}

    void screenShot(const char* filePath, uint32_t width, uint32_t height,
                    uint32_t pitch, const void* data, uint32_t size,
                    bool yflip) override
    {
        if (!filePath || !data || width == 0 || height == 0)
            return;

        const size_t rowBytes = width * 4u;
        std::vector<uint8_t> rows(size);
        for (uint32_t y = 0; y < height; ++y)
        {
            const uint32_t sourceY = yflip ? (height - 1u - y) : y;
            const uint8_t* source = static_cast<const uint8_t*>(data) +
                sourceY * pitch;
            std::memcpy(rows.data() + y * rowBytes, source, rowBytes);
        }

        const uint32_t pixelBytes = static_cast<uint32_t>(rows.size());
        const uint32_t fileBytes = 14u + 40u + pixelBytes;
        std::ofstream out(filePath, std::ios::binary);
        if (!out)
        {
            std::cerr << "Failed to create screenshot: " << filePath
                      << std::endl;
            return;
        }

        const uint8_t header[54] = {
            'B', 'M', uint8_t(fileBytes), uint8_t(fileBytes >> 8),
            uint8_t(fileBytes >> 16), uint8_t(fileBytes >> 24),
            0, 0, 0, 0, 54, 0, 0, 0,
            40, 0, 0, 0,
            uint8_t(width), uint8_t(width >> 8), uint8_t(width >> 16),
            uint8_t(width >> 24),
            uint8_t(height), uint8_t(height >> 8), uint8_t(height >> 16),
            uint8_t(height >> 24),
            1, 0, 32, 0, 0, 0, 0, 0,
            uint8_t(pixelBytes), uint8_t(pixelBytes >> 8),
            uint8_t(pixelBytes >> 16), uint8_t(pixelBytes >> 24),
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        };
        out.write(reinterpret_cast<const char*>(header), sizeof(header));
        out.write(reinterpret_cast<const char*>(rows.data()),
                  std::streamsize(rows.size()));
        std::cout << "Saved screenshot: " << filePath << std::endl;
    }

    void captureBegin(uint32_t, uint32_t, uint32_t,
                      bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

BgfxScreenshotCallback gBgfxScreenshotCallback;


// CAD visual styles use ordered passes into one explicit MSAA scene target.
// Only the background view clears; later passes inherit its depth buffer.
constexpr bgfx::ViewId kViewBackground = 0;
constexpr bgfx::ViewId kViewDepthPrepass = 1;
constexpr bgfx::ViewId kViewSolidFill = 2;
constexpr bgfx::ViewId kViewEdges = 3;
constexpr bgfx::ViewId kViewWire = 4;
constexpr bgfx::ViewId kViewOverlay = 5;
constexpr bgfx::ViewId kViewGpuPick = 6;
constexpr bgfx::ViewId kViewGpuPickBlit = 7;
constexpr bgfx::ViewId kViewPresent = 8;
constexpr bgfx::ViewId kViewGpuPickDebug = 9;
constexpr bgfx::ViewId kViewGpuPickDebugBlit = 10;
constexpr bgfx::ViewId kViewGpuPickDebugPresent = 11;
constexpr bgfx::ViewId kViewText = 12;
constexpr bgfx::ViewId kViewSelectionOutline = 12;
constexpr uint32_t kGpuPickDebugSize = 512;

bool gpuPickDebugEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("GRID_GPU_PICK_DEBUG");
        return value != nullptr && *value != '\0' &&
               std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

// Layer compositing: layer N shifts its geometry closer in view space so it
// always composites above layer N-1 regardless of draw order.  The shared
// log-depth mapping spans object and overlay slabs, so the bias is derived
// from each draw's own view depth as a fixed fraction of log-mapped depth.
constexpr float kLayerNormalizedBias = 1.0f / 65536.0f;
constexpr float kEdgeNormalizedDepthBias = 1.0f / 65536.0f;
constexpr float kLn2 = 0.6931471805599453f;
// Full-scene ID debug can expose far more entities than a 1x1 pick.
// The CAD demo scene queues ~4.3k triangle batches (strokes + fills + tiny
// impostors) in a dense view; the old 4096 cap silently dropped the tail,
// including the picked entity, so its outline could never render. Size the
// caps for the shipped demo with headroom instead.
constexpr size_t kMaxGpuPickInstances = 8192;
constexpr size_t kMaxGpuPickTriangleBatches = 16384;

float logDepthDenominator(const glm::vec4 &logDepth)
{
    const float near = std::max(logDepth.y, 1e-6f);
    const float far = std::max(logDepth.z, near * 1.000001f);
    return std::log2(far / near);
}

float normalizedLogDepth(float viewDepth, const glm::vec4 &logDepth, float denom)
{
    const float near = std::max(logDepth.y, 1e-6f);
    float t = std::log2(std::max(viewDepth / near, 1.0f)) / denom;
    return std::clamp(t, 0.0f, 1.0f);
}

float layerOffsetUnits(float layer, float viewDepth, const glm::vec4 &logDepth)
{
    if (layer <= 0.0f)
        return 0.0f;
    const float d = std::max(viewDepth, 1.0f);
    // dt/dd of the log mapping times the per-layer normalized bias.
    return layer * kLayerNormalizedBias * d * kLn2 * logDepthDenominator(logDepth);
}

float edgeDepthBiasUnits(float viewDepth)
{
    return std::max(viewDepth, 1.0f) * kEdgeNormalizedDepthBias;
}

// Normalized sort depth for bgfx depth-sorted views (0 = near, 1 = far).
uint32_t normalizedSortDepth(float viewDepth, const glm::vec4 &logDepth, float denom)
{
    return static_cast<uint32_t>(normalizedLogDepth(viewDepth, logDepth, denom)
                                 * 16777215.0f);
}

struct CubeVertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

std::array<CubeVertex, 36> makeCubeVertices()
{
    const glm::vec3 px(1.0f, 0.0f, 0.0f);
    const glm::vec3 nx(-1.0f, 0.0f, 0.0f);
    const glm::vec3 py(0.0f, 1.0f, 0.0f);
    const glm::vec3 ny(0.0f, -1.0f, 0.0f);
    const glm::vec3 pz(0.0f, 0.0f, 1.0f);
    const glm::vec3 nz(0.0f, 0.0f, -1.0f);

    return {
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), pz},

        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), nz},

        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), px},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), px},

        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nx},

        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), py},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), py},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), py},

        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), ny},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), ny},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), ny},
};
}

std::array<CubeVertex, 24> makeCubeEdgeVertices()
{
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 corners[8] = {
        glm::vec3(-0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f,  0.5f),
        glm::vec3( 0.5f,  0.5f,  0.5f),
        glm::vec3( 0.5f,  0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f, -0.5f),
    };

    return {
        CubeVertex{corners[0], up}, CubeVertex{corners[1], up},
        CubeVertex{corners[1], up}, CubeVertex{corners[2], up},
        CubeVertex{corners[2], up}, CubeVertex{corners[3], up},
        CubeVertex{corners[3], up}, CubeVertex{corners[0], up},
        CubeVertex{corners[4], up}, CubeVertex{corners[5], up},
        CubeVertex{corners[5], up}, CubeVertex{corners[6], up},
        CubeVertex{corners[6], up}, CubeVertex{corners[7], up},
        CubeVertex{corners[7], up}, CubeVertex{corners[4], up},
        CubeVertex{corners[0], up}, CubeVertex{corners[4], up},
        CubeVertex{corners[1], up}, CubeVertex{corners[5], up},
        CubeVertex{corners[2], up}, CubeVertex{corners[6], up},
        CubeVertex{corners[3], up}, CubeVertex{corners[7], up},
    };
}

bgfx::UniformHandle createUniformHandle(
    const char *name, bgfx::UniformType::Enum type, uint16_t count = 1)
{
    const bgfx::UniformHandle handle = bgfx::createUniform(name, type, count);
    if (!bgfx::isValid(handle))
        std::cerr << "Failed to create bgfx uniform: " << name << std::endl;
    return handle;
}

void destroyUniform(bgfx::UniformHandle &handle)
{
    if (bgfx::isValid(handle))
        bgfx::destroy(handle);
    handle = BGFX_INVALID_HANDLE;
}

glm::mat4 projectionForDirect3D(const glm::mat4 &projection)
{
    // bgfx reports whether the backend expects NDC depth in [-1, 1].
    // OpenGL-style backends keep homogeneousDepth; D3D, Vulkan and Metal use
    // [0, 1], so remap z/w with z' = 0.5 * z + 0.5 * w.
    // In glm (column-major) mat[col][row], the '0.5 * w' term that lands on
    // the z-row lives at column 3, row 2 (i.e. depthRange[3][2]).
    const bgfx::Caps *caps = bgfx::getCaps();
    if (!caps || caps->homogeneousDepth)
        return projection;

    glm::mat4 depthRange(1.0f);
    depthRange[2][2] = 0.5f;
    depthRange[3][2] = 0.5f;
    return depthRange * projection;
}

float normalizedLogDepth(float viewDepth, const glm::vec4 &logDepth)
{
    if (logDepth.x < 0.5f)
        return viewDepth;

    const float depth = std::max(viewDepth, logDepth.y);
    const float numerator = std::log2(std::max(depth / logDepth.y, 1.0f));
    const float denominator =
        std::log2(std::max(logDepth.z / logDepth.y, 1.000001f));
    return std::clamp(numerator / denominator, 0.0f, 1.0f);
}

std::array<float, 4> packVec4(const glm::vec3 &value, float extra)
{
    return {value.x, value.y, value.z, extra};
}

float depthStyleForRenderMode(RenderMode mode)
{
    return mode == RenderMode::DepthBuffer ? 7.0f : 0.0f;
}

static void appendEdgeVertex(std::vector<float> &out, const glm::vec3 &position,
                             const glm::vec3 &normal, const glm::vec2 &uv)
{
    out.push_back(position.x);
    out.push_back(position.y);
    out.push_back(position.z);
    out.push_back(normal.x);
    out.push_back(normal.y);
    out.push_back(normal.z);
    out.push_back(uv.x);
    out.push_back(uv.y);
}

glm::vec2 sphericalUv(float v, float u)
{
    return glm::vec2(u, 1.0f - v);
}

std::vector<float> makeCubeFeatureEdges()
{
    std::vector<float> out;
    out.reserve(24 * 8);
    for (const CubeVertex &vertex : makeCubeEdgeVertices())
    {
        const glm::vec3 &position = vertex.position;
        const glm::vec3 &normal = vertex.normal;
        glm::vec2 uv;
        if (std::abs(normal.x) > 0.5f)
            uv = glm::vec2(position.z + 0.5f, position.y + 0.5f);
        else if (std::abs(normal.y) > 0.5f)
            uv = glm::vec2(position.x + 0.5f, position.z + 0.5f);
        else
            uv = glm::vec2(position.x + 0.5f, position.y + 0.5f);
        appendEdgeVertex(out, position, normal, uv);
    }
    return out;
}

std::vector<float> makeSphereFeatureEdges()
{
    constexpr int kStacks = 12;
    constexpr int kSlices = 16;
    constexpr float kRadius = 0.5f;
    auto point = [](int stack, int slice) {
        const float v = glm::pi<float>() * static_cast<float>(stack) / kStacks;
        const float u = 2.0f * glm::pi<float>() * static_cast<float>(slice) / kSlices;
        return glm::vec3(kRadius * std::sin(v) * std::cos(u),
                         kRadius * std::cos(v),
                         kRadius * std::sin(v) * std::sin(u));
    };
    std::vector<float> out;
    for (int slice = 0; slice < kSlices; slice += 2)
        for (int stack = 0; stack < kStacks; ++stack)
        {
            const glm::vec3 a = point(stack, slice);
            const glm::vec3 b = point(stack + 1, slice);
            appendEdgeVertex(out, a, glm::normalize(a),
                             sphericalUv(float(stack) / kStacks,
                                         float(slice) / kSlices));
            appendEdgeVertex(out, b, glm::normalize(b),
                             sphericalUv(float(stack + 1) / kStacks,
                                         float(slice) / kSlices));
        }
    for (int stack = 3; stack < kStacks; stack += 3)
        for (int slice = 0; slice < kSlices; ++slice)
        {
            const glm::vec3 a = point(stack, slice);
            const glm::vec3 b = point(stack, slice + 1);
            appendEdgeVertex(out, a, glm::normalize(a),
                             sphericalUv(float(stack) / kStacks,
                                         float(slice) / kSlices));
            appendEdgeVertex(out, b, glm::normalize(b),
                             sphericalUv(float(stack) / kStacks,
                                         float(slice + 1) / kSlices));
        }
    return out;
}

std::vector<float> makeConeFeatureEdges()
{
    constexpr int kSegments = 16;
    constexpr float kRadius = 0.5f;
    constexpr float kHalfHeight = 0.5f;
    const glm::vec3 apex(0.0f, kHalfHeight, 0.0f);
    auto rim = [kRadius, kHalfHeight](int segment) {
        const float phi = 2.0f * glm::pi<float>() * segment / kSegments;
        return glm::vec3(kRadius * std::cos(phi), -kHalfHeight,
                         kRadius * std::sin(phi));
    };
    std::vector<float> out;
    for (int segment = 0; segment < kSegments; ++segment)
    {
        const glm::vec3 a = rim(segment);
        const glm::vec3 b = rim(segment + 1);
        appendEdgeVertex(out, a, glm::vec3(a.x, 0.0f, a.z),
                         glm::vec2(float(segment) / kSegments, 0.0f));
        appendEdgeVertex(out, b, glm::vec3(b.x, 0.0f, b.z),
                         glm::vec2(float(segment + 1) / kSegments, 0.0f));
    }
    for (int segment = 0; segment < kSegments; segment += 4)
    {
        const glm::vec3 p = rim(segment);
        appendEdgeVertex(out, apex, glm::normalize(apex - p),
                         glm::vec2(0.5f, 1.0f));
        appendEdgeVertex(out, p, glm::vec3(p.x, 0.0f, p.z),
                         glm::vec2(float(segment) / kSegments, 0.0f));
    }
    return out;
}

std::vector<float> makeTorusFeatureEdges()
{
    constexpr int kMajor = 16;
    constexpr int kMinor = 8;
    constexpr float kMajorRadius = 0.325f;
    constexpr float kMinorRadius = 0.175f;
    auto point = [](int major, int minor) {
        const float u = 2.0f * glm::pi<float>() * major / kMajor;
        const float v = 2.0f * glm::pi<float>() * minor / kMinor;
        const float cu = std::cos(u);
        const float su = std::sin(u);
        const float cv = std::cos(v);
        const float sv = std::sin(v);
        return glm::vec3((kMajorRadius + kMinorRadius * cv) * cu,
                         kMinorRadius * sv,
                         (kMajorRadius + kMinorRadius * cv) * su);
    };
    auto normal = [](int major, int minor) {
        const float u = 2.0f * glm::pi<float>() * major / kMajor;
        const float v = 2.0f * glm::pi<float>() * minor / kMinor;
        const float cu = std::cos(u);
        const float su = std::sin(u);
        const float cv = std::cos(v);
        const float sv = std::sin(v);
        return glm::vec3(cv * cu, sv, cv * su);
    };
    std::vector<float> out;
    for (const int minor : {0, kMinor / 2})
        for (int major = 0; major < kMajor; ++major)
        {
            const glm::vec3 a = point(major, minor);
            const glm::vec3 b = point(major + 1, minor);
            appendEdgeVertex(out, a, normal(major, minor),
                             glm::vec2(float(major) / kMajor,
                                       float(minor) / kMinor));
            appendEdgeVertex(out, b, normal(major + 1, minor),
                             glm::vec2(float(major + 1) / kMajor,
                                       float(minor) / kMinor));
        }
    for (const int major : {0, kMajor / 4, kMajor / 2, kMajor * 3 / 4})
        for (int minor = 0; minor < kMinor; ++minor)
        {
            const glm::vec3 a = point(major, minor);
            const glm::vec3 b = point(major, minor + 1);
            appendEdgeVertex(out, a, normal(major, minor),
                             glm::vec2(float(major) / kMajor,
                                       float(minor) / kMinor));
            appendEdgeVertex(out, b, normal(major, minor + 1),
                             glm::vec2(float(major) / kMajor,
                                       float(minor + 1) / kMinor));
        }
    return out;
}

bgfx::VertexBufferHandle createMeshBuffer(const std::vector<float> &vertices)
{
    if (vertices.empty())
        return BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(),
                   static_cast<uint32_t>(vertices.size() * sizeof(float))),
        layout);
}


bgfx::VertexBufferHandle createCadMeshBuffer(const std::vector<float> &vertices)
{
    if (vertices.empty() || vertices.size() % 8 != 0)
        return BGFX_INVALID_HANDLE;

    // The CAD algorithm shader uses a barycentric coordinate for wireframe and
    // unified shading.  Mesh generators emit triangle soup, so every three
    // vertices form one triangle and receive (1,0,0), (0,1,0), and (0,0,1).
    const size_t vertexCount = vertices.size() / 8;
    std::vector<float> cadVertices;
    cadVertices.reserve(vertexCount * 11);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        const size_t source = i * 8;
        cadVertices.insert(cadVertices.end(), vertices.begin() + source,
                           vertices.begin() + source + 8);
        switch (i % 3)
        {
        case 0:
            cadVertices.insert(cadVertices.end(), {1.0f, 0.0f, 0.0f});
            break;
        case 1:
            cadVertices.insert(cadVertices.end(), {0.0f, 1.0f, 0.0f});
            break;
        default:
            cadVertices.insert(cadVertices.end(), {0.0f, 0.0f, 1.0f});
            break;
        }
    }

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 3, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(cadVertices.data(),
                   static_cast<uint32_t>(cadVertices.size() * sizeof(float))),
        layout);
}

bgfx::VertexBufferHandle createFeatureEdgeLineBuffer(
    const std::vector<float> &edgeLineList)
{
    if (edgeLineList.empty() || edgeLineList.size() % 16 != 0)
        return BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(edgeLineList.data(),
                   static_cast<uint32_t>(edgeLineList.size() * sizeof(float))),
        layout);
}

bgfx::VertexBufferHandle createCadCubeBuffer(const std::array<CubeVertex, 36> &vertices)
{
    (void)vertices;
    return createCadMeshBuffer(proceduralMeshVertices(MeshType::Cube));
}

// Shader headers contain one binary per supported API.  bgfx validates the
// binary type at shader creation, so select the array using the renderer that
// bgfx actually initialized (which may differ from the requested API only if
// a future fallback path is added).
struct ShaderBinary
{
    const uint8_t *data;
    uint32_t size;
};

template <typename T>
ShaderBinary selectShaderBinary(
    bgfx::RendererType::Enum renderer,
    const T *d3d11, size_t d3d11Size,
    const T *d3d12, size_t d3d12Size,
    const T *glsl, size_t glslSize,
    const T *spirv, size_t spirvSize)
{
    switch (renderer)
    {
    case bgfx::RendererType::Direct3D12:
        return {d3d12, static_cast<uint32_t>(d3d12Size)};
    case bgfx::RendererType::OpenGL:
    case bgfx::RendererType::OpenGLES:
        return {glsl, static_cast<uint32_t>(glslSize)};
    case bgfx::RendererType::Vulkan:
        return {spirv, static_cast<uint32_t>(spirvSize)};
    case bgfx::RendererType::Direct3D11:
    default:
        return {d3d11, static_cast<uint32_t>(d3d11Size)};
    }
}

#define SELECT_SHADER_BINARY(NAMESPACE, NAME)                                  \
    selectShaderBinary(                                                        \
        bgfx::getRendererType(),                                               \
        NAMESPACE::NAME##_dx11, sizeof(NAMESPACE::NAME##_dx11),                \
        NAMESPACE::NAME##_dx12, sizeof(NAMESPACE::NAME##_dx12),                \
        NAMESPACE::NAME##_glsl, sizeof(NAMESPACE::NAME##_glsl),                \
        NAMESPACE::NAME##_spv, sizeof(NAMESPACE::NAME##_spv))

std::vector<float> makeGpuPickMeshVertices(MeshType mesh)
{
    const std::vector<float> &source = proceduralMeshVertices(mesh);
    std::vector<float> result;
    result.reserve(source.size() + source.size() / 8);
    const size_t faceCount = source.size() / (3 * kProceduralMeshFloatStride);
    for (size_t face = 0; face < faceCount; ++face)
    {
        const size_t first = face * 3 * kProceduralMeshFloatStride;
        // The face index must interleave per vertex: the pick layout reads
        // 9-float records ([pos3][normal3][uv2][face1]), so appending the
        // indices after the whole face block shifted every vertex after the
        // first and exploded the instanced surface path of the ID pass.
        for (size_t vertex = 0; vertex < 3; ++vertex)
        {
            const size_t vertexFirst =
                first + vertex * kProceduralMeshFloatStride;
            result.insert(result.end(), source.begin() + vertexFirst,
                          source.begin() + vertexFirst +
                              kProceduralMeshFloatStride);
            result.push_back(static_cast<float>(face));
        }
    }
    return result;
}

std::vector<float> makeGpuPickEdgeVertices(const std::vector<float> &edges)
{
    // Feature edges use pos4+normal4. The pick layout uses pos3+normal3+uv2+
    // faceIndex1, so expand each vertex while preserving the same positions.
    std::vector<float> result;
    result.reserve(edges.size() * 9 / 8);
    for (size_t i = 0; i + 8 <= edges.size(); i += 8)
    {
        result.insert(result.end(), edges.begin() + i,
                      edges.begin() + i + 3);
        result.insert(result.end(), edges.begin() + i + 4,
                      edges.begin() + i + 7);
        result.push_back(0.0f); // uv.x
        result.push_back(0.0f); // uv.y
        result.push_back(0.0f); // faceIndex
    }
    return result;
}

} // namespace


BgfxRenderer::BgfxRenderer(GraphicsApi api)
    : m_api(api)
{
    // The vendored bgfx build has no WebGPU device backend yet. Keep the
    // requested WebGPU selection addressable through a D3D12 compatibility
    // adapter while the native renderer is introduced incrementally.
    m_webgpuMigration = api == GraphicsApi::WebGPU;
    if (m_webgpuMigration)
        m_api = GraphicsApi::Direct3D12;
}

const char *BgfxRenderer::name() const
{
    return m_webgpuMigration ? "bgfx-webgpu-migration" : "bgfx";
}

const char *BgfxRenderer::graphicsApiName() const
{
    if (m_initialized)
    {
        switch (bgfx::getRendererType())
        {
        case bgfx::RendererType::Direct3D11:
            return "Direct3D11";
        case bgfx::RendererType::Direct3D12:
            return "Direct3D12";
        case bgfx::RendererType::OpenGL:
            return "OpenGL";
        case bgfx::RendererType::OpenGLES:
            return "OpenGL ES";
        case bgfx::RendererType::Vulkan:
            return "Vulkan";
        case bgfx::RendererType::Metal:
            return "Metal";
        default:
            break;
        }
    }

    switch (m_api)
    {
    case GraphicsApi::Direct3D11:
        return "Direct3D11";
    case GraphicsApi::Direct3D12:
        return "Direct3D12";
    case GraphicsApi::WebGPU:
        return "WebGPU";
    case GraphicsApi::OpenGL:
        return "OpenGL";
    case GraphicsApi::Vulkan:
        return "Vulkan";
    case GraphicsApi::Auto:
        return "Auto";
    }
    return "Unknown";
}

void BgfxRenderer::setRealisticLights(const RealisticLightsRenderData &lights)
{
    m_realisticLights = lights;
    m_realisticLights.direction = glm::normalize(m_realisticLights.direction);
    m_realisticLights.pointLightCount =
        std::min<uint32_t>(m_realisticLights.pointLightCount,
                           static_cast<uint32_t>(m_realisticLights.pointLights.size()));
}

uint32_t BgfxRenderer::loadMeshTexture(const std::string &path)
{
    if (!m_initialized || path.empty())
        return 0;

    size_t fileSize = 0;
    void *fileData = SDL_LoadFile(path.c_str(), &fileSize);
    if (!fileData || fileSize == 0 || fileSize > UINT32_MAX)
    {
        if (fileData)
            SDL_free(fileData);
        std::cerr << "Failed to load mesh texture: " << path << std::endl;
        return 0;
    }

    bx::DefaultAllocator allocator;
    bx::Error error;
    bimg::ImageContainer *image = bimg::imageParse(
        &allocator, fileData, static_cast<uint32_t>(fileSize),
        bimg::TextureFormat::BGRA8, &error);
    SDL_free(fileData);
    if (!image || !image->m_data || image->m_size == 0)
    {
        if (image)
            bimg::imageFree(image);
        std::cerr << "Failed to decode mesh texture: " << path << std::endl;
        return 0;
    }

    const bgfx::TextureHandle texture = bgfx::createTexture2D(
        static_cast<uint16_t>(image->m_width),
        static_cast<uint16_t>(image->m_height),
        image->m_numMips > 1, 1,
        static_cast<bgfx::TextureFormat::Enum>(image->m_format),
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(image->m_data, image->m_size));
    bimg::imageFree(image);
    if (!bgfx::isValid(texture))
    {
        std::cerr << "Failed to create mesh texture: " << path << std::endl;
        return 0;
    }

    m_meshTextures.push_back(texture);
    return static_cast<uint32_t>(m_meshTextures.size());
}
Uint32 BgfxRenderer::windowFlags() const
{
    return 0;
}

bool BgfxRenderer::configureSDL()
{
    return true;
}

bool BgfxRenderer::initialize(SDL_Window *window)
{
    m_window = window;

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    m_width = static_cast<uint16_t>(std::max(1, width));
    m_height = static_cast<uint16_t>(std::max(1, height));

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void *hwnd = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!hwnd)
    {
        std::cerr << "Failed to get Win32 HWND from SDL window." << std::endl;
        return false;
    }

    bgfx::Init init;
    switch (m_api)
    {
    case GraphicsApi::Direct3D11:
        init.type = bgfx::RendererType::Direct3D11;
        break;
    case GraphicsApi::Direct3D12:
        init.type = bgfx::RendererType::Direct3D12;
        break;
    case GraphicsApi::OpenGL:
        init.type = bgfx::RendererType::OpenGL;
        break;
    case GraphicsApi::Vulkan:
        init.type = bgfx::RendererType::Vulkan;
        break;
    case GraphicsApi::Auto:
    default:
        init.type = bgfx::RendererType::Direct3D11;
        break;
    }
    init.platformData.nwh = hwnd;
    init.resolution.width = m_width;
    init.resolution.height = m_height;
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4;
    const char *gpuDebug = std::getenv("GRID_GPU_DEBUG");
    init.debug = gpuDebug != nullptr;
    init.callback = &gBgfxScreenshotCallback;

    if (!bgfx::init(init))
    {
        std::cerr << "Failed to initialize bgfx " << graphicsApiName()
                  << " renderer." << std::endl;
        return false;
    }

    // dbgTextPrintf output is only drawn when this debug flag is set
    // (see the "else if (m_debug & BGFX_DEBUG_TEXT)" branch of the
    // backend submit); enable it so the FPS overlay is visible.
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    m_initialized = true;
    if (!createRenderResources())
    {
        shutdown();
        return false;
    }

    if (gpuPickDebugEnabled())
        m_gpuPickDebugVisible = true;

    std::cout << "bgfx " << bgfx::getRendererName(bgfx::getRendererType())
              << " renderer initialized." << std::endl;
    return true;
}

void BgfxRenderer::shutdown()
{
    if (!m_initialized)
    {
        m_window = nullptr;
        return;
    }

    destroySceneFrameBuffer();
    if (bgfx::isValid(m_gridProgram))
        bgfx::destroy(m_gridProgram);
    m_gridProgram = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_cubeProgram))
        bgfx::destroy(m_cubeProgram);
    if (bgfx::isValid(m_meshInstanceProgram))
        bgfx::destroy(m_meshInstanceProgram);
    if (bgfx::isValid(m_pbrMeshProgram))
        bgfx::destroy(m_pbrMeshProgram);
    m_pbrMeshProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_pointInstanceProgram))
        bgfx::destroy(m_pointInstanceProgram);
    if (bgfx::isValid(m_cadAlgorithmProgram))
        bgfx::destroy(m_cadAlgorithmProgram);
    if (bgfx::isValid(m_polylineProgram))
        bgfx::destroy(m_polylineProgram);
    if (bgfx::isValid(m_lineInstanceProgram))
        bgfx::destroy(m_lineInstanceProgram);
    if (bgfx::isValid(m_lineInstanceQuadBuffer))
        bgfx::destroy(m_lineInstanceQuadBuffer);
    m_lineInstanceProgram = BGFX_INVALID_HANDLE;
    m_lineInstanceQuadBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_meshEdgeRibbonProgram))
        bgfx::destroy(m_meshEdgeRibbonProgram);
    m_meshEdgeRibbonProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_fillProgram))
        bgfx::destroy(m_fillProgram);
    if (bgfx::isValid(m_curveProgram))
        bgfx::destroy(m_curveProgram);
    m_cubeProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cubeBuffer))
        bgfx::destroy(m_cubeBuffer);
    if (bgfx::isValid(m_instanceCubeBuffer))
        bgfx::destroy(m_instanceCubeBuffer);
    m_instanceCubeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cadCubeBuffer))
        bgfx::destroy(m_cadCubeBuffer);
    if (bgfx::isValid(m_cadSphereBuffer))
        bgfx::destroy(m_cadSphereBuffer);
    m_cadSphereBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cadConeBuffer))
        bgfx::destroy(m_cadConeBuffer);
    m_cadConeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cadTorusBuffer))
        bgfx::destroy(m_cadTorusBuffer);
    m_cadTorusBuffer = BGFX_INVALID_HANDLE;
    m_cubeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_sphereBuffer))
        bgfx::destroy(m_sphereBuffer);
    m_sphereBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_coneBuffer))
        bgfx::destroy(m_coneBuffer);
    m_coneBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_torusBuffer))
        bgfx::destroy(m_torusBuffer);
    m_torusBuffer = BGFX_INVALID_HANDLE;
    for (bgfx::TextureHandle &texture : m_meshTextures)
    {
        if (bgfx::isValid(texture))
            bgfx::destroy(texture);
    }
    m_meshTextures.clear();
    if (bgfx::isValid(m_whiteTexture))
        bgfx::destroy(m_whiteTexture);
    m_whiteTexture = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cubeEdgeBuffer))
        bgfx::destroy(m_cubeEdgeBuffer);
    m_cubeEdgeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_sphereEdgeBuffer))
        bgfx::destroy(m_sphereEdgeBuffer);
    m_sphereEdgeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_coneEdgeBuffer))
        bgfx::destroy(m_coneEdgeBuffer);
    m_coneEdgeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_torusEdgeBuffer))
        bgfx::destroy(m_torusEdgeBuffer);
    m_torusEdgeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cubeEdgeRibbonBuffer))
        bgfx::destroy(m_cubeEdgeRibbonBuffer);
    m_cubeEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_sphereEdgeRibbonBuffer))
        bgfx::destroy(m_sphereEdgeRibbonBuffer);
    m_sphereEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_coneEdgeRibbonBuffer))
        bgfx::destroy(m_coneEdgeRibbonBuffer);
    m_coneEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_torusEdgeRibbonBuffer))
        bgfx::destroy(m_torusEdgeRibbonBuffer);
    m_torusEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_aabbBuffer))
        bgfx::destroy(m_aabbBuffer);
    m_aabbBuffer = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_pointProgram))
        bgfx::destroy(m_pointProgram);
    m_pointProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_pointBuffer))
        bgfx::destroy(m_pointBuffer);
    m_pointBuffer = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_selectionOutlineProgram))
        bgfx::destroy(m_selectionOutlineProgram);
    m_selectionOutlineProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_presentProgram))
        bgfx::destroy(m_presentProgram);
    m_presentProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_presentQuadBuffer))
        bgfx::destroy(m_presentQuadBuffer);
    m_presentQuadBuffer = BGFX_INVALID_HANDLE;
    destroySceneFrameBuffer();

    destroyUniform(m_cubeRelativePositionLow);
    destroyUniform(m_eyeHigh);
    destroyUniform(m_eyeLow);
    destroyUniform(m_cadView);
    destroyUniform(m_cadProjection);
    destroyUniform(m_cadCameraPos);
    destroyUniform(m_cadBaseColor);
    destroyUniform(m_cadLightDir);
    destroyUniform(m_cadStyleParams);
    destroyUniform(m_cadWireframeColor);
    destroyUniform(m_cadStrokeParams);
    destroyUniform(m_cadFlatShade);
    destroyUniform(m_primParams);
    destroyUniform(m_depthDisplayParams);
    destroyUniform(m_curveCP);
    destroyUniform(m_curveKnot);
    destroyUniform(m_curveParams);
    destroyUniform(m_curveColor);
    destroyUniform(m_curveArc);
    destroyUniform(m_meshSurface);
    destroyUniform(m_albedoSampler);
    destroyUniform(m_realisticMaterial);
    destroyUniform(m_rAmbient);
    destroyUniform(m_rDirection);
    destroyUniform(m_rDirectionColor);
    destroyUniform(m_rPointPositions);
    destroyUniform(m_rPointColors);
    destroyUniform(m_rParams);
    destroyUniform(m_gridInvViewProj);
    destroyUniform(m_gridViewProj);
    destroyUniform(m_gridCamFront);
    destroyUniform(m_gridOrthoPlaneCenter);
    destroyUniform(m_gridOrthoRight);
    destroyUniform(m_gridOrthoUp);
    destroyUniform(m_gridOriginRelative);
    destroyUniform(m_gridPlaneNormal);
    destroyUniform(m_gridPlaneTangentU);
    destroyUniform(m_gridPlaneTangentV);
    destroyUniform(m_gridAxisColorU);
    destroyUniform(m_gridAxisColorV);
    destroyUniform(m_gridStartAxisOrigin);
    destroyUniform(m_gridStartAxisDirection);
    destroyUniform(m_gridStartAxisVisible);
    destroyUniform(m_gridStartAxisLine);
    destroyUniform(m_gridAxisOriginGridRelative);
    destroyUniform(m_gridAxisLineX);
    destroyUniform(m_gridAxisLineZ);
    destroyUniform(m_gridIsOrtho);
    destroyUniform(m_gridGroundRelativeY);
    destroyUniform(m_gridStep);
    destroyUniform(m_gridAxisVisible);
    destroyUniform(m_gridScreenHeight);
    destroyUniform(m_gridScreenWidth);
    destroyUniform(m_gridColorMajor);
    destroyUniform(m_gridColorMinor);
    destroyUniform(m_gridOpacity);
    destroyUniform(m_gridOrthoPlaneValid);
    destroyUniform(m_logDepth);
    destroyUniform(m_layerOffset);
    destroyUniform(m_view);
    destroyUniform(m_projection);
    destroyUniform(m_cubeRelativePosition);
    destroyUniform(m_cubeOpacity);
    destroyUniform(m_cubeColor);
    destroyUniform(m_meshEdgeOverride);
    destroyUniform(m_meshEdgeRibbonParams);
    destroyUniform(m_presentSampler);
    destroyUniform(m_selectionOutlineParams);
    destroyUniform(m_selectionOutlineColor);
    destroyUniform(m_presentParams);
    destroyUniform(m_pointPosition);
    destroyUniform(m_pointSize);
    destroyUniform(m_pointColor);

    destroyGpuPickResources();
    destroySceneFrameBuffer();

    bgfx::shutdown();
    m_initialized = false;
    m_window = nullptr;
}

bool BgfxRenderer::createSceneFrameBuffer()
{
    if (!m_width || !m_height)
        return false;

    const uint64_t colorFlags = BGFX_TEXTURE_RT |
                                BGFX_TEXTURE_RT_MSAA_X4 |
                                BGFX_SAMPLER_U_CLAMP |
                                BGFX_SAMPLER_V_CLAMP;
    const uint64_t depthFlags = BGFX_TEXTURE_RT_WRITE_ONLY |
                                BGFX_TEXTURE_RT_MSAA_X4;

    bgfx::TextureHandle textures[2];
    textures[0] = bgfx::createTexture2D(m_width, m_height, false, 1,
                                        bgfx::TextureFormat::BGRA8, colorFlags);
    textures[1] = bgfx::createTexture2D(m_width, m_height, false, 1,
                                        bgfx::TextureFormat::D24S8, depthFlags);
    if (!bgfx::isValid(textures[0]) || !bgfx::isValid(textures[1]))
    {
        if (bgfx::isValid(textures[0]))
            bgfx::destroy(textures[0]);
        if (bgfx::isValid(textures[1]))
            bgfx::destroy(textures[1]);
        std::cerr << "Failed to create CAD MSAA scene target." << std::endl;
        return false;
    }

    m_sceneFrameBuffer = bgfx::createFrameBuffer(2, textures, true);
    if (!bgfx::isValid(m_sceneFrameBuffer))
    {
        std::cerr << "Failed to create CAD scene framebuffer." << std::endl;
        return false;
    }
    bgfx::setName(m_sceneFrameBuffer, "CADSceneMSAA");
    return true;
}

void BgfxRenderer::destroySceneFrameBuffer()
{
    if (bgfx::isValid(m_sceneFrameBuffer))
        bgfx::destroy(m_sceneFrameBuffer);
    m_sceneFrameBuffer = BGFX_INVALID_HANDLE;
}

bool BgfxRenderer::createGpuPickResources()
{
    const auto pickVertexBinary =
        SELECT_SHADER_BINARY(GpuPickShaders, vs_gpu_pick);
    const auto pickFragmentBinary =
        SELECT_SHADER_BINARY(GpuPickShaders, fs_gpu_pick);
    const bgfx::ShaderHandle vertexShader = bgfx::createShader(
        bgfx::copy(pickVertexBinary.data, pickVertexBinary.size));
    const bgfx::ShaderHandle fragmentShader = bgfx::createShader(
        bgfx::copy(pickFragmentBinary.data, pickFragmentBinary.size));
    bgfx::setName(vertexShader, "gpu_pick_vs");
    bgfx::setName(fragmentShader, "gpu_pick_fs");
    m_gpuPickProgram = bgfx::createProgram(vertexShader, fragmentShader, true);
    m_gpuPickLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .end();

    const auto createBuffer = [this](MeshType mesh) {
        const std::vector<float> vertices = makeGpuPickMeshVertices(mesh);
        return bgfx::createVertexBuffer(
            bgfx::copy(vertices.data(),
                       static_cast<uint32_t>(vertices.size() * sizeof(float))),
            m_gpuPickLayout);
    };
    const auto createEdgeBuffer = [this](const std::vector<float> &edges) {
        const std::vector<float> vertices = makeGpuPickEdgeVertices(edges);
        return bgfx::createVertexBuffer(
            bgfx::copy(vertices.data(),
                       static_cast<uint32_t>(vertices.size() * sizeof(float))),
            m_gpuPickLayout);
    };
    m_gpuPickCubeBuffer = createBuffer(MeshType::Cube);
    m_gpuPickSphereBuffer = createBuffer(MeshType::Sphere);
    m_gpuPickConeBuffer = createBuffer(MeshType::Cone);
    m_gpuPickTorusBuffer = createBuffer(MeshType::Torus);
    m_gpuPickCubeEdgeBuffer = createEdgeBuffer(makeCubeFeatureEdges());
    m_gpuPickSphereEdgeBuffer = createEdgeBuffer(makeSphereFeatureEdges());
    m_gpuPickConeEdgeBuffer = createEdgeBuffer(makeConeFeatureEdges());
    m_gpuPickTorusEdgeBuffer = createEdgeBuffer(makeTorusFeatureEdges());

    const uint64_t rtFlags = BGFX_TEXTURE_RT |
                             BGFX_SAMPLER_MIN_POINT |
                             BGFX_SAMPLER_MAG_POINT |
                             BGFX_SAMPLER_U_CLAMP |
                             BGFX_SAMPLER_V_CLAMP;
    const uint64_t readbackFlags = BGFX_TEXTURE_READ_BACK |
                                   BGFX_TEXTURE_BLIT_DST |
                                   BGFX_SAMPLER_MIN_POINT |
                                   BGFX_SAMPLER_MAG_POINT |
                                   BGFX_SAMPLER_U_CLAMP |
                                   BGFX_SAMPLER_V_CLAMP;
    bgfx::TextureHandle color = bgfx::createTexture2D(
        1, 1, false, 1, bgfx::TextureFormat::BGRA8, rtFlags);
    bgfx::TextureHandle depth = bgfx::createTexture2D(
        1, 1, false, 1, bgfx::TextureFormat::D24S8, rtFlags);
    m_gpuPickReadback = bgfx::createTexture2D(
        1, 1, false, 1, bgfx::TextureFormat::BGRA8, readbackFlags);
    if (bgfx::isValid(color) && bgfx::isValid(depth) &&
        bgfx::isValid(m_gpuPickReadback))
    {
        const bgfx::TextureHandle attachments[2] = {color, depth};
        m_gpuPickFrameBuffer = bgfx::createFrameBuffer(2, attachments, true);
    }
    else
    {
        if (bgfx::isValid(color))
            bgfx::destroy(color);
        if (bgfx::isValid(depth))
            bgfx::destroy(depth);
    }
    bgfx::setName(m_gpuPickFrameBuffer, "GpuPick1x1");
    bgfx::setName(m_gpuPickReadback, "GpuPickReadback");

    if (gpuPickDebugEnabled() && !createGpuPickDebugResources())
        return false;

    return bgfx::isValid(m_gpuPickFrameBuffer) &&
           bgfx::isValid(m_gpuPickReadback) &&
           bgfx::isValid(m_gpuPickProgram) &&
           bgfx::isValid(m_gpuPickCubeBuffer) &&
           bgfx::isValid(m_gpuPickSphereBuffer) &&
           bgfx::isValid(m_gpuPickConeBuffer) &&
           bgfx::isValid(m_gpuPickTorusBuffer) &&
           bgfx::isValid(m_gpuPickCubeEdgeBuffer) &&
           bgfx::isValid(m_gpuPickSphereEdgeBuffer) &&
           bgfx::isValid(m_gpuPickConeEdgeBuffer) &&
           bgfx::isValid(m_gpuPickTorusEdgeBuffer);
}

void BgfxRenderer::destroyGpuPickResources()
{
    if (bgfx::isValid(m_gpuPickFrameBuffer))
        bgfx::destroy(m_gpuPickFrameBuffer);
    m_gpuPickFrameBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_gpuPickReadback))
        bgfx::destroy(m_gpuPickReadback);
    m_gpuPickReadback = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_gpuPickProgram))
        bgfx::destroy(m_gpuPickProgram);
    m_gpuPickProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle *buffers[] = {
        &m_gpuPickCubeBuffer, &m_gpuPickSphereBuffer,
        &m_gpuPickConeBuffer, &m_gpuPickTorusBuffer,
        &m_gpuPickCubeEdgeBuffer, &m_gpuPickSphereEdgeBuffer,
        &m_gpuPickConeEdgeBuffer, &m_gpuPickTorusEdgeBuffer};
    for (bgfx::VertexBufferHandle *buffer : buffers)
    {
        if (bgfx::isValid(*buffer))
            bgfx::destroy(*buffer);
        *buffer = BGFX_INVALID_HANDLE;
    }
    for (auto &entry : m_gpuPickTriangleGeometry)
    {
        if (bgfx::isValid(entry.second.buffer))
            bgfx::destroy(entry.second.buffer);
    }
    m_gpuPickTriangleGeometry.clear();
    m_gpuPickPrimitives.clear();
    m_gpuPickActive = false;
    m_gpuPickReadPending = false;
    destroyGpuPickDebugResources();
}

bool BgfxRenderer::createGpuPickDebugResources()
{
    if (bgfx::isValid(m_gpuPickDebugFrameBuffer) &&
        m_gpuPickDebugWidth == m_width && m_gpuPickDebugHeight == m_height)
    {
        return true;
    }

    destroyGpuPickDebugResources();

    const uint16_t width = std::max<uint16_t>(1, m_width);
    const uint16_t height = std::max<uint16_t>(1, m_height);
    const uint64_t rtFlags = BGFX_TEXTURE_RT |
                             BGFX_SAMPLER_MIN_POINT |
                             BGFX_SAMPLER_MAG_POINT |
                             BGFX_SAMPLER_U_CLAMP |
                             BGFX_SAMPLER_V_CLAMP;
    const uint64_t readbackFlags = BGFX_TEXTURE_READ_BACK |
                                   BGFX_TEXTURE_BLIT_DST |
                                   BGFX_SAMPLER_MIN_POINT |
                                   BGFX_SAMPLER_MAG_POINT |
                                   BGFX_SAMPLER_U_CLAMP |
                                   BGFX_SAMPLER_V_CLAMP;
    bgfx::TextureHandle color = bgfx::createTexture2D(
        width, height, false, 1, bgfx::TextureFormat::BGRA8, rtFlags);
    bgfx::TextureHandle depth = bgfx::createTexture2D(
        width, height, false, 1, bgfx::TextureFormat::D24S8, rtFlags);
    m_gpuPickDebugReadback = bgfx::createTexture2D(
        width, height, false, 1, bgfx::TextureFormat::BGRA8, readbackFlags);
    m_gpuPickDebugVisualTexture = bgfx::createTexture2D(
        width, height, false, 1, bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    m_gpuPickDebugReadbackData.assign(size_t(width) * height * 4u, 0u);

    if (bgfx::isValid(color) && bgfx::isValid(depth) &&
        bgfx::isValid(m_gpuPickDebugReadback) &&
        bgfx::isValid(m_gpuPickDebugVisualTexture))
    {
        const bgfx::TextureHandle attachments[2] = {color, depth};
        m_gpuPickDebugFrameBuffer = bgfx::createFrameBuffer(
            2, attachments, true);
        m_gpuPickDebugWidth = width;
        m_gpuPickDebugHeight = height;
    }
    else
    {
        if (bgfx::isValid(color))
            bgfx::destroy(color);
        if (bgfx::isValid(depth))
            bgfx::destroy(depth);
    }

    bgfx::setName(m_gpuPickDebugFrameBuffer, "GpuPickDebug");
    bgfx::setName(m_gpuPickDebugReadback, "GpuPickDebugReadback");
    bgfx::setName(m_gpuPickDebugVisualTexture, "GpuPickDebugVisual");
    return bgfx::isValid(m_gpuPickDebugFrameBuffer) &&
           bgfx::isValid(m_gpuPickDebugReadback) &&
           bgfx::isValid(m_gpuPickDebugVisualTexture);
}

void BgfxRenderer::destroyGpuPickDebugResources()
{
    if (bgfx::isValid(m_gpuPickDebugFrameBuffer))
        bgfx::destroy(m_gpuPickDebugFrameBuffer);
    m_gpuPickDebugFrameBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_gpuPickDebugReadback))
        bgfx::destroy(m_gpuPickDebugReadback);
    m_gpuPickDebugReadback = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_gpuPickDebugVisualTexture))
        bgfx::destroy(m_gpuPickDebugVisualTexture);
    m_gpuPickDebugVisualTexture = BGFX_INVALID_HANDLE;
    m_gpuPickDebugReadbackData.clear();
    m_gpuPickDebugReadPending = false;
}

bool BgfxRenderer::beginFrame(const glm::vec4 &clearColor)
{
    if (!m_initialized)
        return false;

    ++m_frame;

    // DepthBuffer style normalization switch: GRID_DEPTH_RAW=1 displays the
    // raw hardware depth instead of the log-normalized slab mapping.
    // Uniform values persist across submits, so setting it once per frame
    // reaches every program that declares uDepthDisplay.
    static const bool depthRawDisplay = [] {
        const char *value = std::getenv("GRID_DEPTH_RAW");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    const float depthDisplayParams[4] = {
        depthRawDisplay ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(m_depthDisplayParams, depthDisplayParams);

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    width = std::max(1, width);
    height = std::max(1, height);
    if (width != m_width || height != m_height)
    {
        bgfx::reset(static_cast<uint16_t>(width),
                    static_cast<uint16_t>(height),
                    BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
        m_width = static_cast<uint16_t>(width);
        m_height = static_cast<uint16_t>(height);
        destroySceneFrameBuffer();
        if (!createSceneFrameBuffer())
            return false;
        if ((m_gpuPickDebugVisible || m_gpuPickSceneDebug || m_gpuPickScenePassEnabled ||
             m_selectionOutlineId != 0) &&
            !createGpuPickDebugResources())
        {
            return false;
        }
    }

    const auto channel = [](float value) {
        return uint32_t(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    };
    const uint32_t rgba = channel(clearColor.r) << 24 |
                          channel(clearColor.g) << 16 |
                          channel(clearColor.b) << 8 |
                          channel(clearColor.a);

    bgfx::setViewName(kViewBackground, "CAD Background");
    bgfx::setViewName(kViewDepthPrepass, "CAD Hidden-Line Depth");
    bgfx::setViewName(kViewSolidFill, "CAD Solid Fill");
    bgfx::setViewName(kViewEdges, "CAD Edges");
    bgfx::setViewName(kViewWire, "CAD Wires");
    bgfx::setViewName(kViewOverlay, "CAD Overlay");
    bgfx::setViewName(kViewPresent, "CAD Present");

    for (const bgfx::ViewId view : { kViewBackground, kViewDepthPrepass,
                                     kViewSolidFill, kViewEdges,
                                     kViewWire, kViewOverlay })
    {
        bgfx::setViewFrameBuffer(view, m_sceneFrameBuffer);
        bgfx::setViewRect(view, 0, 0, m_width, m_height);
    }

    bgfx::setViewClear(kViewBackground,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       rgba, 1.0f, 0);
    bgfx::setViewClear(kViewDepthPrepass, BGFX_CLEAR_NONE);
    bgfx::setViewClear(kViewSolidFill, BGFX_CLEAR_NONE);
    bgfx::setViewClear(kViewEdges, BGFX_CLEAR_NONE);
    bgfx::setViewClear(kViewWire, BGFX_CLEAR_NONE);
    bgfx::setViewClear(kViewOverlay, BGFX_CLEAR_NONE);
    bgfx::setViewFrameBuffer(kViewPresent, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kViewPresent, BGFX_CLEAR_NONE);

    for (const bgfx::ViewId view : { kViewBackground, kViewDepthPrepass,
                                     kViewSolidFill, kViewEdges,
                                     kViewWire, kViewOverlay, kViewPresent })
    {
        bgfx::setViewRect(view, 0, 0, m_width, m_height);
        bgfx::touch(view);
    }
    // The solid channel composites mesh fills and solid fills together by
    // depth instead of grouping them per program.
    bgfx::setViewMode(kViewSolidFill,
                      bgfx::ViewMode::DepthDescending);
    bgfx::setViewMode(kViewWire,
                      m_renderMode.flags().wireframe3d
                          ? bgfx::ViewMode::Default
                          : bgfx::ViewMode::Sequential);
    // Selection outlines are submitted before the CAD overlay that owns the
    // source stroke/curve.  Sequential compositing guarantees the original
    // line draws on top instead of being covered by the wider outline.
    bgfx::setViewMode(kViewOverlay, bgfx::ViewMode::Sequential);
    return true;
}

void BgfxRenderer::endFrame()
{
    if (!m_initialized)
        return;


    // FPS overlay: bgfx debug text renders as the last step of the
    // frame.
    ++m_frameCount;
    const uint32_t now = SDL_GetTicks();
    if (m_fpsLastTick == 0)
        m_fpsLastTick = now;
    const uint32_t elapsed = now - m_fpsLastTick;
    if (elapsed >= 500)
    {
        m_fps = static_cast<float>(m_frameCount) * 1000.0f /
                static_cast<float>(elapsed);
        m_frameCount = 0;
        m_fpsLastTick = now;
    }
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(1, 1, 0x0f, "FPS: %.1f (%.1f ms)",
                        m_fps, m_fps > 0.0f ? 1000.0f / m_fps : 0.0f);
    if (m_gpuPickSceneDebug)
    {
        bgfx::dbgTextPrintf(
            1, 2, 0x0f,
            "GPU ID queue: mesh=%llu/%llu tri=%llu/%llu droppedMesh=%llu droppedTri=%llu failed=%llu",
            static_cast<unsigned long long>(m_gpuPickQueueStats.queuedMeshes),
            static_cast<unsigned long long>(m_gpuPickQueueStats.meshCapacity),
            static_cast<unsigned long long>(m_gpuPickQueueStats.queuedTriangles),
            static_cast<unsigned long long>(m_gpuPickQueueStats.triangleCapacity),
            static_cast<unsigned long long>(m_gpuPickQueueStats.droppedMeshes),
            static_cast<unsigned long long>(m_gpuPickQueueStats.droppedTriangles),
            static_cast<unsigned long long>(m_gpuPickQueueStats.resourceFailures));
    }

    renderGpuPickPass();

    // Resolve the explicit MSAA scene target to the backbuffer.  This happens
    // after debug text, so the text belongs to the resolved image too.
    if (bgfx::isValid(m_sceneFrameBuffer) && bgfx::isValid(m_presentProgram))
    {
        const bgfx::TextureHandle sceneColor = bgfx::getTexture(m_sceneFrameBuffer);
        if (bgfx::isValid(sceneColor))
        {
            bgfx::setViewFrameBuffer(kViewPresent, BGFX_INVALID_HANDLE);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_MSAA);
            float fxaaEnabled = 0.0f;
            if (std::getenv("GRID_FXAA") != nullptr)
            {
                fxaaEnabled = 1.0f;
            }
            const std::array<float, 4> presentParams = {
                fxaaEnabled,
                1.0f / static_cast<float>(m_width),
                1.0f / static_cast<float>(m_height),
                0.0f,
            };
            bgfx::setUniform(m_presentParams, presentParams.data());
            bgfx::setTexture(0, m_presentSampler, sceneColor);
            bgfx::setVertexBuffer(0, m_presentQuadBuffer);
            bgfx::submit(kViewPresent, m_presentProgram);
        }
    }

    // Direct GPU-pick debug view: show the last captured ID texture in a
    // centered square so it can be inspected without exporting a file.
    const bool debugPresentReady = m_gpuPickSceneDebug
        ? true
        : m_gpuPickDebugReadFrame != 0u &&
          bgfx::isValid(m_gpuPickDebugVisualTexture);
    if (m_gpuPickDebugVisible && debugPresentReady &&
        bgfx::isValid(m_gpuPickDebugFrameBuffer) &&
        bgfx::isValid(m_presentProgram) &&
        bgfx::isValid(m_presentQuadBuffer))
    {
        const uint16_t debugSize = m_gpuPickSceneDebug
            ? m_width
            : uint16_t(std::min<uint32_t>(
                  kGpuPickDebugSize, std::min<uint32_t>(m_width, m_height)));
        const uint16_t debugX = m_gpuPickSceneDebug
            ? 0u
            : uint16_t((m_width - debugSize) / 2u);
        const uint16_t debugY = m_gpuPickSceneDebug
            ? 0u
            : uint16_t((m_height - debugSize) / 2u);
        const uint16_t debugW = m_gpuPickSceneDebug ? m_width : debugSize;
        const uint16_t debugH = m_gpuPickSceneDebug ? m_height : debugSize;

        bgfx::setViewName(kViewGpuPickDebugPresent, "CAD Gpu Pick Debug Present");
        bgfx::setViewFrameBuffer(kViewGpuPickDebugPresent, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(kViewGpuPickDebugPresent,
                          debugX, debugY, debugW, debugH);
        bgfx::setViewClear(kViewGpuPickDebugPresent, BGFX_CLEAR_NONE,
                           0x00000000u, 1.0f, 0);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                       BGFX_STATE_MSAA);
        std::array<float, 4> debugPresentParams = {
            0.0f,
            1.0f / static_cast<float>(m_width),
            1.0f / static_cast<float>(m_height),
            0.0f,
        };
        bgfx::setUniform(m_presentParams, debugPresentParams.data());
        if (m_gpuPickSceneDebug)
        {
            // Present the ID target directly; readback stays for CPU inspection.
            debugPresentParams = {
                0.0f,
                static_cast<float>(m_gpuPickDebugMinId),
                static_cast<float>(m_gpuPickDebugMaxId),
                1.0f,
            };
            bgfx::setUniform(m_presentParams, debugPresentParams.data());
            bgfx::setTexture(0, m_presentSampler,
                             bgfx::getTexture(m_gpuPickDebugFrameBuffer, 0));
        }
        else
        {
            bgfx::setTexture(0, m_presentSampler, m_gpuPickDebugVisualTexture);
        }
        bgfx::setVertexBuffer(0, m_presentQuadBuffer);
        bgfx::submit(kViewGpuPickDebugPresent, m_presentProgram);
    }

    renderSelectionOutlinePass();

    bgfx::frame();
}

void BgfxRenderer::present()
{
    // bgfx::frame() presents in endFrame(); SDL3 remains the window owner.
}

void BgfxRenderer::requestDebugScreenShot(const std::string &filePath)
{
    if (!m_initialized || filePath.empty())
        return;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, filePath.c_str());
}


void BgfxRenderer::setGpuPickDebugVisible(bool visible)
{
    if (!m_initialized)
        return;

    m_gpuPickDebugVisible = visible;
    if (visible && !bgfx::isValid(m_gpuPickDebugFrameBuffer))
    {
        if (!createGpuPickDebugResources())
        {
            std::cerr << "Failed to create GPU pick debug resources." << std::endl;
            m_gpuPickDebugVisible = false;
        }
    }
}

void BgfxRenderer::setGpuPickSceneDebug(bool visible)
{
    if (!m_initialized)
        return;

    m_gpuPickSceneDebug = visible;
    m_gpuPickDebugVisible = visible;
    if (visible)
    {
        if (!bgfx::isValid(m_gpuPickDebugFrameBuffer) &&
            !createGpuPickDebugResources())
        {
            std::cerr << "Failed to create GPU pick scene debug resources." << std::endl;
            m_gpuPickSceneDebug = false;
            m_gpuPickDebugVisible = false;
        }
    }
    else
    {
        m_gpuPickActive = false;
        m_gpuPickPrimitives.clear();
        m_gpuPickQueueStats = {};
    }
}
void BgfxRenderer::setGpuPickScenePassEnabled(bool enabled)
{
    m_gpuPickScenePassEnabled = enabled;
    if (enabled)
    {
        if (!bgfx::isValid(m_gpuPickDebugFrameBuffer))
            createGpuPickDebugResources();
    }
    else
    {
        m_gpuPickActive = false;
        m_gpuPickPrimitives.clear();
        m_gpuPickQueueStats = {};
    }
}


void BgfxRenderer::setGpuPickIdRange(uint32_t minId, uint32_t maxId)
{
    m_gpuPickDebugMinId = minId;
    m_gpuPickDebugMaxId = maxId;
}

void BgfxRenderer::setSelectionOutlineId(uint32_t objectId)
{
    m_selectionOutlineId = objectId;
    if (objectId != 0 && !bgfx::isValid(m_gpuPickDebugFrameBuffer))
        createGpuPickDebugResources();
}
void BgfxRenderer::setSelectionOutlineAll(bool enabled)
{
    m_selectionOutlineAll = enabled;
    if (enabled)
    {
        if (!bgfx::isValid(m_gpuPickDebugFrameBuffer))
            createGpuPickDebugResources();
    }
}

void BgfxRenderer::renderSelectionOutlinePass()
{
    if (!bgfx::isValid(m_gpuPickDebugFrameBuffer))
    {
        return;
    }
    if (!bgfx::isValid(m_selectionOutlineProgram))
    {
        return;
    }
    if (!bgfx::isValid(m_presentQuadBuffer))
    {
        return;
    }
    if (!m_selectionOutlineAll)
    {
        if (m_selectionOutlineId == 0)
        {
            return;
        }
    }

    bgfx::setViewName(kViewSelectionOutline, "CAD Selection Outline");
    bgfx::setViewFrameBuffer(kViewSelectionOutline, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(kViewSelectionOutline, 0, 0, m_width, m_height);
    bgfx::setViewClear(kViewSelectionOutline, BGFX_CLEAR_NONE,
                       0x00000000u, 1.0f, 0);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_MSAA |
                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                         BGFX_STATE_BLEND_INV_SRC_ALPHA));
    const float params[4] = {float(m_selectionOutlineId), m_selectionOutlineAll ? 1.0f : 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(m_selectionOutlineParams, params);
    const glm::vec4 outlineColor(1.0f, 0.55f, 0.05f, 1.0f);
    bgfx::setUniform(m_selectionOutlineColor,
                     glm::value_ptr(outlineColor));
    bgfx::setTexture(0, m_presentSampler,
                     bgfx::getTexture(m_gpuPickDebugFrameBuffer, 0));
    bgfx::setVertexBuffer(0, m_presentQuadBuffer);
    bgfx::submit(kViewSelectionOutline, m_selectionOutlineProgram);
}

void BgfxRenderer::drawGrid(const GridRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_gridProgram))
        return;
    // The grid is translucent, but its line/axis pixels must occlude
    // geometry behind the grid plane. Fragments with zero alpha are already
    // discarded, so only visible line pixels write depth; the gaps between
    // lines stay transparent for later passes.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LEQUAL |
                   BGFX_STATE_BLEND_ALPHA |
                   BGFX_STATE_WRITE_Z);
    bgfx::setVertexBuffer(0, m_pointBuffer); // Grid and target-point share the same fullscreen-triangle layout/data.
    bgfx::setUniform(m_gridInvViewProj, glm::value_ptr(data.invViewProj));
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    const glm::mat4 depthViewProj =
        projectionForDirect3D(data.projection) * data.view;
    bgfx::setUniform(m_gridViewProj, glm::value_ptr(depthViewProj));
    bgfx::setUniform(m_gridCamFront, packVec4(data.camFront, 0.0f).data());
    bgfx::setUniform(m_gridOrthoPlaneCenter,
                     packVec4(data.orthoPlaneCenter, 0.0f).data());
    bgfx::setUniform(m_gridOrthoRight,
                     packVec4(data.orthoRight, 0.0f).data());
    bgfx::setUniform(m_gridOrthoUp,
                     packVec4(data.orthoUp, 0.0f).data());
    bgfx::setUniform(m_gridOriginRelative,
                     packVec4(data.planeOriginRelative, data.plane).data());
    bgfx::setUniform(m_gridPlaneNormal,
                     packVec4(data.planeNormal, 0.0f).data());
    bgfx::setUniform(m_gridPlaneTangentU,
                     packVec4(data.planeTangentU, 0.0f).data());
    bgfx::setUniform(m_gridPlaneTangentV,
                     packVec4(data.planeTangentV, 0.0f).data());
    bgfx::setUniform(m_gridAxisColorU,
                     packVec4(data.axisColorU, 0.0f).data());
    bgfx::setUniform(m_gridAxisColorV,
                     packVec4(data.axisColorV, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisOrigin,
                     packVec4(data.startAxisOrigin, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisDirection,
                     packVec4(data.startAxisDirection, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisVisible,
                     glm::value_ptr(glm::vec4(data.startAxisVisible, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridStartAxisLine,
                     packVec4(data.startAxisLine, 0.0f).data());
    bgfx::setUniform(
        m_gridAxisOriginGridRelative,
        packVec4(glm::vec3(data.axisOriginGridRelative, 0.0f), 0.0f).data());
    bgfx::setUniform(m_gridAxisLineX, packVec4(data.axisLineX, 0.0f).data());
    bgfx::setUniform(m_gridAxisLineZ, packVec4(data.axisLineZ, 0.0f).data());
    bgfx::setUniform(m_gridIsOrtho,
                     glm::value_ptr(glm::vec4(data.isOrtho, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridGroundRelativeY,
                     glm::value_ptr(glm::vec4(data.groundRelativeY, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridStep,
                     glm::value_ptr(glm::vec4(data.step, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(
        m_gridAxisVisible,
        packVec4(glm::vec3(data.axisVisible, 0.0f), 0.0f).data());
    bgfx::setUniform(
        m_gridScreenHeight,
        glm::value_ptr(glm::vec4(data.screenHeight, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(
        m_gridScreenWidth,
        glm::value_ptr(glm::vec4(data.screenWidth, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridColorMajor,
                     packVec4(data.gridColorMajor, 0.0f).data());
    bgfx::setUniform(m_gridColorMinor,
                     packVec4(data.gridColorMinor, 0.0f).data());
    bgfx::setUniform(m_gridOpacity,
                     glm::value_ptr(glm::vec4(data.gridOpacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridOrthoPlaneValid,
                     glm::value_ptr(glm::vec4(data.orthoPlaneValid, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    bgfx::submit(kViewBackground, m_gridProgram);
}


static std::vector<std::pair<glm::vec3, glm::vec3>> makeEdgeSegments(
    rendering::MeshType mesh);
struct EdgeMeshData
{
    std::vector<std::pair<glm::vec3, glm::vec3>> segments;
    float boundingRadius = 0.0f;
};

static const auto makeEdgeMeshData()
{
    std::array<EdgeMeshData,
               size_t(rendering::MeshType::Torus) + 1> result{};

    auto build = [&result](rendering::MeshType mesh) {
        EdgeMeshData &data = result[size_t(mesh)];
        data.segments = makeEdgeSegments(mesh);
        for (const auto &segment : data.segments)
        {
            data.boundingRadius = std::max(
                data.boundingRadius,
                std::max(glm::length(segment.first),
                         glm::length(segment.second)));
        }
    };

    build(rendering::MeshType::Cube);
    build(rendering::MeshType::Sphere);
    build(rendering::MeshType::Cone);
    build(rendering::MeshType::Torus);
    return result;
}

static bool edgeInstanceVisible(const glm::mat4 &view,
                                const glm::mat4 &projection,
                                const glm::vec3 &translation,
                                float radius)
{
    // A conservative screen-space sphere test.  The radius deliberately uses
    // the Frobenius bound of the instance basis: false positives are allowed,
    // but never a false negative that would cull visible edges.
    const glm::vec4 centerView = view * glm::vec4(translation, 1.0f);
    const float centerDepth = -centerView.z;
    if (!(centerDepth + radius > 0.0f))
        return false;

    const glm::vec4 clip = projection * centerView;
    if (clip.w <= 0.0f)
        return true; // The sphere straddles the camera; let GPU clipping decide.

    const float projectionScale = std::max(
        std::abs(projection[0][0]), std::abs(projection[1][1]));
    const float ndcRadius = projectionScale * radius /
        std::max(std::abs(clip.w), 0.001f);
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return std::abs(ndc.x) <= 1.0f + ndcRadius &&
           std::abs(ndc.y) <= 1.0f + ndcRadius;
}

static std::vector<std::pair<glm::vec3, glm::vec3>> makeEdgeSegments(
    rendering::MeshType mesh)
{
    std::vector<float> vertices;
    switch (mesh)
    {
    case rendering::MeshType::Sphere:
        vertices = makeSphereFeatureEdges();
        break;
    case rendering::MeshType::Cone:
        vertices = makeConeFeatureEdges();
        break;
    case rendering::MeshType::Torus:
        vertices = makeTorusFeatureEdges();
        break;
    case rendering::MeshType::Cube:
    default:
        vertices = makeCubeFeatureEdges();
        break;
    }

    // Feature-edge line buffers store two vertices per segment.  Each source
    // vertex has position + normal (8 floats); normals are not needed here.
    std::vector<std::pair<glm::vec3, glm::vec3>> segments;
    segments.reserve(vertices.size() / 16);
    for (size_t offset = 0; offset + 16 <= vertices.size(); offset += 16)
    {
        segments.emplace_back(
            glm::make_vec3(vertices.data() + offset),
            glm::make_vec3(vertices.data() + offset + 8));
    }
    return segments;
}

// The ribbon vertex buffer is expanded once per mesh type.  Each source edge
// segment becomes two triangles; every ribbon vertex carries both endpoints
// plus (along, side), so the GPU can expand it for each mesh instance.
bgfx::VertexBufferHandle createEdgeRibbonBuffer(
    const std::vector<std::pair<glm::vec3, glm::vec3>> &segments)
{
    if (segments.empty())
        return BGFX_INVALID_HANDLE;

    constexpr float kQuadIndices[][2] = {
        {0.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},
        {0.0f, -1.0f}, {1.0f,  1.0f}, {0.0f, 1.0f},
    };

    std::vector<float> vertices;
    vertices.reserve(segments.size() * 6 * 8);
    for (const auto &segment : segments)
    {
        for (const auto &[along, side] : kQuadIndices)
        {
            vertices.insert(vertices.end(), {
                segment.first.x, segment.first.y, segment.first.z, along,
                segment.second.x, segment.second.y, segment.second.z, side,
            });
        }
    }

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(),
                   static_cast<uint32_t>(vertices.size() * sizeof(float))),
        layout);
}

void BgfxRenderer::drawEdgeRibbonsForInstances(
    const glm::mat4 &view,
    const glm::mat4 &projection,
    const DoubleSingleVec3 &eye,
    const MeshInstance *instances,
    uint32_t instanceCount,
    MeshType mesh,
    float layer,
    const glm::vec4 &logDepth,
    float edgeHalfWidth,
    float edgeSoftness)
{
    if (!m_initialized || !bgfx::isValid(m_meshEdgeRibbonProgram) ||
        !instances || instanceCount == 0)
    {
        return;
    }

    bgfx::VertexBufferHandle edgeRibbonBuffer = m_cubeEdgeRibbonBuffer;
    switch (mesh)
    {
    case MeshType::Sphere:
        edgeRibbonBuffer = m_sphereEdgeRibbonBuffer;
        break;
    case MeshType::Cone:
        edgeRibbonBuffer = m_coneEdgeRibbonBuffer;
        break;
    case MeshType::Torus:
        edgeRibbonBuffer = m_torusEdgeRibbonBuffer;
        break;
    case MeshType::Cube:
        break;
    }
    if (!bgfx::isValid(edgeRibbonBuffer))
        return;

    const glm::mat4 proj = projectionForDirect3D(projection);
    const float prim[4] = {
        0.0f, edgeSoftness, depthStyleForRenderMode(m_renderMode.mode()), 0.0f};
    const float edgeParams[4] = {edgeHalfWidth, 0.0f, 0.0f, 0.0f};
    // Opaque edges: the ribbon shader darkens the fill color by 10% and
    // writes alpha 1, so no blending is applied.
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA;
    const float denom = logDepthDenominator(logDepth);

    // One instance covers every feature-edge segment of one mesh instance.
    // The 80-byte MeshInstance payload is exactly what the fill pass uses.
    const glm::vec3 instanceTranslation =
        glm::vec3(instances[0].positionHigh) +
        glm::vec3(instances[0].positionLow);
    const glm::vec3 eyeTranslation = eye.high + eye.low;
    const glm::vec4 instanceView =
        view * glm::vec4(instanceTranslation - eyeTranslation, 1.0f);
    const float instanceDepth = -instanceView.z;
    const float layerOffsetValue =
        layerOffsetUnits(layer, instanceDepth, logDepth);
    const uint32_t sortDepth =
        normalizedSortDepth(instanceDepth - layerOffsetValue, logDepth, denom);
    const float layerOffset[4] = {layerOffsetValue, 0.0f, 0.0f, 0.0f};

    bgfx::setUniform(m_view, glm::value_ptr(view));
    bgfx::setUniform(m_projection, glm::value_ptr(proj));
    bgfx::setUniform(m_eyeHigh, glm::value_ptr(glm::vec4(eye.high, 0.0f)));
    bgfx::setUniform(m_eyeLow, glm::value_ptr(glm::vec4(eye.low, 0.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(logDepth));
    bgfx::setUniform(m_primParams, prim);
    bgfx::setUniform(m_meshEdgeRibbonParams, edgeParams);
    bgfx::setUniform(m_layerOffset, layerOffset);

    constexpr uint16_t kStride = sizeof(MeshInstance);
    uint32_t first = 0;
    while (first < instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            instanceCount - first, kStride);
        if (available == 0)
            break;
        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, available, kStride);
        std::memcpy(idb.data, instances + first,
                    size_t(idb.num) * kStride);

        bgfx::setState(state);
        bgfx::setVertexBuffer(0, edgeRibbonBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::submit(kViewOverlay, m_meshEdgeRibbonProgram, sortDepth);
        first += idb.num;
    }
}

void BgfxRenderer::drawCube(const CubeRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_cubeProgram))
        return;

    bgfx::VertexBufferHandle meshBuffer = m_instanceCubeBuffer;
    bgfx::VertexBufferHandle edgeBuffer = m_cubeEdgeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere:
        meshBuffer = m_sphereBuffer;
        edgeBuffer = m_sphereEdgeBuffer;
        break;
    case rendering::MeshType::Cone:
        meshBuffer = m_coneBuffer;
        edgeBuffer = m_coneEdgeBuffer;
        break;
    case rendering::MeshType::Torus:
        meshBuffer = m_torusBuffer;
        edgeBuffer = m_torusEdgeBuffer;
        break;
    case rendering::MeshType::Cube:
        break;
    }

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const RenderModeFlags modeFlags = m_renderMode.flags();
    const bool depthStyleMode = m_renderMode.mode() == RenderMode::DepthBuffer;
    const uint64_t transparentFillState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_CULL_CW |
        BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
    const uint64_t opaqueFillState = transparentFillState | BGFX_STATE_WRITE_Z;
    const uint64_t prepassState = BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_WRITE_Z | BGFX_STATE_MSAA;
    const uint64_t edgeState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES |
        BGFX_STATE_LINEAA | BGFX_STATE_MSAA;
    const float cubeStyle[4] = { depthStyleForRenderMode(m_renderMode.mode()), 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_primParams, cubeStyle);
    // Layer compositing and depth-sorted submission inside the solid
    // channel so mesh fills and solid fills interleave by true depth.
    const glm::vec3 objectTranslation =
        (data.object.high - data.eye.high) + (data.object.low - data.eye.low);
    const glm::vec4 objectView = data.view * glm::vec4(objectTranslation, 1.0f);
    const float denom = logDepthDenominator(data.logDepth);
    const float objectDepth = -objectView.z;
    const float layerOffsetValue =
        layerOffsetUnits(data.layer, objectDepth, data.logDepth);
    const uint32_t sortDepth =
        normalizedSortDepth(objectDepth - layerOffsetValue, data.logDepth, denom);
    const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_layerOffset, layerOffset);
    const auto submitFill = [this, &data, &projection, meshBuffer, sortDepth](bgfx::ViewId view,
                               uint64_t state) {
        bgfx::setState(state);
        bgfx::setTransform(glm::value_ptr(data.model));
        bgfx::setVertexBuffer(0, meshBuffer);
        bgfx::setUniform(m_view, glm::value_ptr(data.view));
        bgfx::setUniform(m_projection, glm::value_ptr(projection));
        bgfx::setUniform(m_cubeRelativePosition,
                         glm::value_ptr(glm::vec4(data.object.high, 1.0f)));
        bgfx::setUniform(m_cubeRelativePositionLow,
                         glm::value_ptr(glm::vec4(data.object.low, 0.0f)));
        bgfx::setUniform(m_eyeHigh,
                         glm::value_ptr(glm::vec4(data.eye.high, 0.0f)));
        bgfx::setUniform(m_eyeLow,
                         glm::value_ptr(glm::vec4(data.eye.low, 0.0f)));
        bgfx::setUniform(m_cubeOpacity,
                         glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
        bgfx::setUniform(m_cubeColor,
                         glm::value_ptr(glm::vec4(data.objectColor, 1.0f)));
        bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
        bgfx::submit(view, m_cubeProgram, sortDepth);
    };

    const auto submitEdges = [this, &data, &projection, edgeBuffer, edgeState, sortDepth,
                             objectDepth](bgfx::ViewId view,
                                                    uint64_t state) {
        if (!bgfx::isValid(edgeBuffer))
            return;
        const float edgeLayerOffset[4] = {
            layerOffsetUnits(data.layer, objectDepth, data.logDepth) +
            edgeDepthBiasUnits(objectDepth), 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_layerOffset, edgeLayerOffset);
        bgfx::setState(state);
        bgfx::setTransform(glm::value_ptr(data.model));
        bgfx::setVertexBuffer(0, edgeBuffer);
        bgfx::setUniform(m_view, glm::value_ptr(data.view));
        bgfx::setUniform(m_projection, glm::value_ptr(projection));
        bgfx::setUniform(m_cubeRelativePosition,
                         glm::value_ptr(glm::vec4(data.object.high, 1.0f)));
        bgfx::setUniform(m_cubeRelativePositionLow,
                         glm::value_ptr(glm::vec4(data.object.low, 0.0f)));
        bgfx::setUniform(m_eyeHigh,
                         glm::value_ptr(glm::vec4(data.eye.high, 0.0f)));
        bgfx::setUniform(m_eyeLow,
                         glm::value_ptr(glm::vec4(data.eye.low, 0.0f)));
        bgfx::setUniform(m_cubeOpacity,
                         glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
        bgfx::setUniform(m_cubeColor,
                         glm::value_ptr(glm::vec4(data.objectColor, 1.0f)));
        bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
        bgfx::submit(view, m_cubeProgram, sortDepth);
    };

    if (modeFlags.hiddenLine && modeFlags.meshFill)
        submitFill(kViewDepthPrepass, prepassState);
    if (modeFlags.meshFill && !modeFlags.hiddenLine)
        submitFill(kViewSolidFill, depthStyleMode || data.opacity >= 0.999f
                                      ? opaqueFillState
                                      : transparentFillState);
    if (modeFlags.show3dEdges)
        submitEdges(modeFlags.wireframe3d ? kViewEdges : kViewWire, edgeState);
}

void BgfxRenderer::drawMeshInstances(const MeshInstancesRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_meshInstanceProgram) ||
        !data.instances || data.instanceCount == 0)
        return;

    bgfx::VertexBufferHandle meshBuffer = m_cubeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere:
        meshBuffer = m_sphereBuffer;
        break;
    case rendering::MeshType::Cone:
        meshBuffer = m_coneBuffer;
        break;
    case rendering::MeshType::Torus:
        meshBuffer = m_torusBuffer;
        break;
    case rendering::MeshType::Cube:
        break;
    }

    constexpr uint16_t kStride = sizeof(MeshInstance);
    static_assert(kStride == 80,
                  "MeshInstance must carry 5 vec4s to leave room for UV vertex attributes");
    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const RenderModeFlags modeFlags = m_renderMode.flags();
    const bool depthStyleMode = m_renderMode.mode() == RenderMode::DepthBuffer;
    const uint64_t fillState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA |
        (depthStyleMode || data.opaque ? BGFX_STATE_WRITE_Z
                                       : BGFX_STATE_BLEND_ALPHA);
    const uint64_t prepassState = BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_WRITE_Z | BGFX_STATE_MSAA;
    const uint64_t edgeState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES |
        BGFX_STATE_LINEAA | BGFX_STATE_MSAA;
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_eyeHigh, glm::value_ptr(glm::vec4(data.eye.high, 0.0f)));
    bgfx::setUniform(m_eyeLow, glm::value_ptr(glm::vec4(data.eye.low, 0.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    const float meshStyle[4] = { depthStyleForRenderMode(m_renderMode.mode()), 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_primParams, meshStyle);
    const float meshSurface[4] = {
        std::clamp(data.headlight, 0.0f, 1.0f), data.triplanarUv, 0.0f, 0.0f};
    bgfx::setUniform(m_meshSurface, meshSurface);
    bgfx::TextureHandle albedoTexture = m_whiteTexture;
    if (data.diffuseTextureIndex > 0 &&
        data.diffuseTextureIndex <= m_meshTextures.size() &&
        bgfx::isValid(m_meshTextures[data.diffuseTextureIndex - 1]))
    {
        albedoTexture = m_meshTextures[data.diffuseTextureIndex - 1];
    }
    bgfx::setTexture(0, m_albedoSampler, albedoTexture);
    bgfx::ProgramHandle fillProgram = m_meshInstanceProgram;
    if (data.realistic && bgfx::isValid(m_pbrMeshProgram))
    {
        fillProgram = m_pbrMeshProgram;
        std::array<glm::vec4, 4> pointPositions{};
        std::array<glm::vec4, 4> pointColors{};
        const uint32_t pointCount = std::min<uint32_t>(
            m_realisticLights.pointLightCount, pointPositions.size());
        for (uint32_t i = 0; i < pointCount; ++i)
        {
            pointPositions[i] = glm::vec4(m_realisticLights.pointLights[i].position,
                                          m_realisticLights.pointLights[i].radius);
            pointColors[i] = glm::vec4(m_realisticLights.pointLights[i].color, 1.0f);
        }
        bgfx::setUniform(m_realisticMaterial, glm::value_ptr(data.material));
        bgfx::setUniform(m_rAmbient,
                         glm::value_ptr(glm::vec4(m_realisticLights.ambient, 1.0f)));
        bgfx::setUniform(m_rDirection, glm::value_ptr(glm::vec4(
            m_realisticLights.direction, m_realisticLights.directionIntensity)));
        bgfx::setUniform(m_rDirectionColor, glm::value_ptr(glm::vec4(
            m_realisticLights.directionColor, 1.0f)));
        bgfx::setUniform(m_rPointPositions,
                         glm::value_ptr(pointPositions.front()), 4);
        bgfx::setUniform(m_rPointColors,
                         glm::value_ptr(pointColors.front()), 4);
        bgfx::setUniform(m_rParams,
                         glm::value_ptr(glm::vec4(float(pointCount), 0.0f, 0.0f, 0.0f)));
    }
    // Depth-sort instance batches inside the solid channel and apply the
    // requested compositing layer.
    const glm::vec3 instanceTranslation = glm::vec3(data.instances[0].positionHigh)
                                        + glm::vec3(data.instances[0].positionLow);
    const glm::vec3 eyeTranslation = data.eye.high + data.eye.low;
    const glm::vec4 instanceView =
        data.view * glm::vec4(instanceTranslation - eyeTranslation, 1.0f);
    const float denom = logDepthDenominator(data.logDepth);
    const float instanceDepth = -instanceView.z;
    const float layerOffsetValue =
        layerOffsetUnits(data.layer, instanceDepth, data.logDepth);
    const uint32_t sortDepth =
        normalizedSortDepth(instanceDepth - layerOffsetValue, data.logDepth, denom);
    const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_layerOffset, layerOffset);

    const auto submitChunks = [&](bgfx::ViewId view, bgfx::ProgramHandle program,
                                  bgfx::VertexBufferHandle buffer, uint64_t state) {
        uint32_t first = 0;
        while (first < data.instanceCount)
        {
            const uint32_t available = bgfx::getAvailInstanceDataBuffer(
                data.instanceCount - first, kStride);
            if (available == 0)
                break;
            bgfx::InstanceDataBuffer idb;
            bgfx::allocInstanceDataBuffer(&idb, available, kStride);
            auto *gpu = reinterpret_cast<MeshInstance *>(idb.data);
            std::memcpy(gpu, data.instances + first,
                        sizeof(MeshInstance) * idb.num);

            bgfx::setState(state);
            bgfx::setVertexBuffer(0, buffer);
            bgfx::setInstanceDataBuffer(&idb);
            bgfx::submit(view, program, sortDepth);
            first += idb.num;
        }
    };

    const float edgeOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float edgeOn[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    if (modeFlags.hiddenLine && modeFlags.meshFill)
    {
        bgfx::setUniform(m_meshEdgeOverride, edgeOff);
        submitChunks(kViewDepthPrepass, m_meshInstanceProgram, meshBuffer, prepassState);
    }
    if (modeFlags.meshFill && !modeFlags.hiddenLine)
    {
        bgfx::setUniform(m_meshEdgeOverride, edgeOff);
        submitChunks(kViewSolidFill, fillProgram, meshBuffer, fillState);
    }
    if (modeFlags.show3dEdges)
    {
        // Hardware PT_LINES had unreliable AA on D3D11.  Expand feature edges
        // into screen-space ribbons and reuse the same line AA path.
        drawEdgeRibbonsForInstances(
            data.view, data.projection, data.eye,
            data.instances, data.instanceCount, data.mesh,
            data.layer, data.logDepth,
            data.edgeHalfWidth,
            data.edgeSoftness);
    }
}

void BgfxRenderer::drawTargetPointInstances(
    const TargetPointInstancesRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_pointInstanceProgram) ||
        !data.instances || data.instanceCount == 0)
        return;
    constexpr uint16_t kStride = 48;
    struct PointInstanceGpu
    {
        glm::vec4 positionDepth;
        glm::vec4 screenSize;
        glm::vec4 colorOpacity;
    };
    static_assert(sizeof(PointInstanceGpu) == kStride,
                  "Point instance must match the GPU instance stride");

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_LEQUAL |
                           BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
    const float pointStyle[4] = {
        m_renderMode.mode() == RenderMode::DepthBuffer ? 1.0f : 0.0f,
        0.0f, 0.0f, 0.0f
    };
    bgfx::setUniform(m_primParams, pointStyle);

    uint32_t first = 0;
    static std::vector<PointInstanceGpu> visibleInputs;
    while (first < data.instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            data.instanceCount - first, kStride);
        if (available == 0)
            break;
        visibleInputs.clear();
        visibleInputs.reserve(available);

        for (uint32_t i = 0; i < available; ++i)
        {
            const TargetPointInstance &input = data.instances[first + i];
            const glm::vec4 viewPosition = data.view *
                glm::vec4(input.relativePosition, 1.0f);
            const glm::vec4 clip = projection * viewPosition;
            if (!(clip.w > 0.0f))
                continue;
            const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
            float depth = clip.z / clip.w;
            if (data.isOrtho <= 0.5f)
            {
                const float viewDepth = -viewPosition.z;
                const float biasWorld =
                    std::max(0.5f * data.pixelSizeWorld, 0.01f);
                depth = normalizedLogDepth(
                    std::max(viewDepth - biasWorld, data.logDepth.y),
                    data.logDepth);
            }

            const float pointSize = input.pointSize > 0.0f
                                        ? input.pointSize
                                        : data.pointSize;
            const glm::vec2 pixelSizeNdc(
                pointSize * 2.0f / float(m_width),
                pointSize * 2.0f / float(m_height));

            visibleInputs.push_back({
                glm::vec4(ndc, depth, 1.0f),
                glm::vec4(pixelSizeNdc, 0.0f, 0.0f),
                glm::vec4(input.color, 1.0f)
            });
        }

        if (visibleInputs.empty())
        {
            first += available;
            continue;
        }

        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb,
                                      static_cast<uint32_t>(visibleInputs.size()),
                                      kStride);
        std::memcpy(idb.data, visibleInputs.data(),
                    visibleInputs.size() * kStride);

        bgfx::setState(state);
        bgfx::setVertexBuffer(0, m_pointBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::submit(kViewOverlay, m_pointInstanceProgram);
        first += available;
    }
}

static float shaderStyleForRenderMode(RenderMode mode)
{
    switch (mode)
    {
    case RenderMode::Wireframe2D:
    case RenderMode::Wireframe3D:
    case RenderMode::HiddenLine:
        return 6.0f; // wireframe branch
    case RenderMode::Shaded:
    case RenderMode::ShadedWithEdges:
        return 0.0f; // realistic PBR branch
    case RenderMode::DepthBuffer:
        return depthStyleForRenderMode(RenderMode::DepthBuffer);
    }
    return 6.0f;
}

static float shaderStyleForSurfaceAlgorithm(SurfaceAlgorithm algorithm)
{
    switch (algorithm)
    {
    case SurfaceAlgorithm::Realistic:
    case SurfaceAlgorithm::Shaded:
        return 0.0f;
    case SurfaceAlgorithm::Conceptual:
        return 1.0f;
    case SurfaceAlgorithm::Cad:
        return 4.0f;
    case SurfaceAlgorithm::Grayscale:
        return 3.0f;
    case SurfaceAlgorithm::Sketch:
        return 5.0f;
    case SurfaceAlgorithm::Wireframe:
        return 6.0f;
    case SurfaceAlgorithm::Depth:
        return 7.0f;
    case SurfaceAlgorithm::XRay:
        return 8.0f;
    }
    return 0.0f;
}

void BgfxRenderer::drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_cadAlgorithmProgram) ||
        !bgfx::isValid(m_meshInstanceProgram) || !data.instances ||
        data.instanceCount == 0)
        return;

    bgfx::VertexBufferHandle meshBuffer = m_cadCubeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere:
        meshBuffer = m_cadSphereBuffer;
        break;
    case rendering::MeshType::Cone:
        meshBuffer = m_cadConeBuffer;
        break;
    case rendering::MeshType::Torus:
        meshBuffer = m_cadTorusBuffer;
        break;
    case rendering::MeshType::Cube:
        break;
    }

    constexpr uint16_t kStride = sizeof(MeshInstance);
    static_assert(kStride == 80,
                  "CAD MeshInstance must carry 5 vec4s to leave room for UV vertex attributes");
    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const RenderModeFlags modeFlags = m_renderMode.flags();
    const bool depthStyleMode = data.renderMode == RenderMode::DepthBuffer;
    const uint64_t fillState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA |
        (depthStyleMode ? BGFX_STATE_WRITE_Z : 0);
    const uint64_t prepassState = BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_WRITE_Z | BGFX_STATE_MSAA;
    const uint64_t wireState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES |
        BGFX_STATE_LINEAA | BGFX_STATE_MSAA;
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    bgfx::setUniform(m_cadView, glm::value_ptr(data.view));
    bgfx::setUniform(m_cadProjection, glm::value_ptr(projection));
    bgfx::setUniform(m_eyeHigh, glm::value_ptr(glm::vec4(data.eye.high, 0.0f)));
    bgfx::setUniform(m_eyeLow, glm::value_ptr(glm::vec4(data.eye.low, 0.0f)));
    bgfx::setUniform(m_cadCameraPos, glm::value_ptr(glm::vec4(data.cameraPos, 1.0f)));
    bgfx::setUniform(m_cadBaseColor, glm::value_ptr(data.material.baseColor));
    bgfx::setUniform(m_cadLightDir, glm::value_ptr(glm::vec4(glm::normalize(data.lightDir), 0.0f)));
    bgfx::setUniform(m_cadWireframeColor, glm::value_ptr(data.material.accentColor));
    bgfx::setUniform(m_cadStrokeParams, glm::value_ptr(glm::vec4(data.strokeWidth, data.strokeDensity, 0.0f, 0.0f)));
    // Layer compositing and depth-sorted submission inside the solid channel.
    const glm::vec3 instanceTranslation = glm::vec3(data.instances[0].positionHigh)
                                        + glm::vec3(data.instances[0].positionLow);
    const glm::vec3 eyeTranslation = data.eye.high + data.eye.low;
    const glm::vec4 instanceView =
        data.view * glm::vec4(instanceTranslation - eyeTranslation, 1.0f);
    const float denom = logDepthDenominator(data.logDepth);
    const float instanceDepth = -instanceView.z;
    const float layerOffsetValue =
        layerOffsetUnits(data.layer, instanceDepth, data.logDepth);
    const uint32_t sortDepth =
        normalizedSortDepth(instanceDepth - layerOffsetValue, data.logDepth, denom);
    const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_layerOffset, layerOffset);


    const float fillStyle = data.material.algorithm == SurfaceAlgorithm::Shaded
        ? shaderStyleForRenderMode(data.renderMode)
        : shaderStyleForSurfaceAlgorithm(data.material.algorithm);
    const float fillStyleParams[4] = { fillStyle, data.material.metallic,
                                       data.material.roughness,
                                       data.material.transparency };
    const float flatShadeParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    const float edgeOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    const auto submitChunks = [&](bgfx::ViewId view, bgfx::ProgramHandle program,
                                  bgfx::VertexBufferHandle buffer, uint64_t state) {
        uint32_t first = 0;
        while (first < data.instanceCount)
        {
            const uint32_t available = bgfx::getAvailInstanceDataBuffer(
                data.instanceCount - first, kStride);
            if (available == 0)
                break;
            bgfx::InstanceDataBuffer idb;
            bgfx::allocInstanceDataBuffer(&idb, available, kStride);
            auto *gpu = reinterpret_cast<MeshInstance *>(idb.data);
            std::memcpy(gpu, data.instances + first,
                        sizeof(MeshInstance) * idb.num);

            bgfx::setState(state);
            bgfx::setVertexBuffer(0, buffer);
            bgfx::setInstanceDataBuffer(&idb);
            bgfx::submit(view, program, sortDepth);
            first += idb.num;
        }
    };


    if (modeFlags.hiddenLine && modeFlags.meshFill)
    {
        bgfx::setUniform(m_meshEdgeOverride, edgeOff);
        submitChunks(kViewDepthPrepass, m_meshInstanceProgram, meshBuffer, prepassState);
    }
    if (modeFlags.meshFill && !modeFlags.hiddenLine)
    {
        bgfx::setUniform(m_cadStyleParams, fillStyleParams);
        bgfx::setUniform(m_cadFlatShade, flatShadeParams);
        submitChunks(kViewSolidFill, m_cadAlgorithmProgram, meshBuffer, fillState);
    }
    if (modeFlags.show3dEdges)
    {
        // Hardware PT_LINES had unreliable AA on D3D11.  Expand feature edges
        // into screen-space ribbons and reuse the same line AA path.
        drawEdgeRibbonsForInstances(
            data.view, data.projection, data.eye,
            data.instances, data.instanceCount, data.mesh,
            data.layer, data.logDepth,
            std::max(data.edgeHalfWidth, data.material.lineWidth * 0.5f),
            data.edgeSoftness);
    }
}

void BgfxRenderer::setRenderMode(RenderMode mode)
{
    m_renderMode.set(mode);
}

RenderMode BgfxRenderer::renderMode() const
{
    return m_renderMode.mode();
}

RenderModeFlags BgfxRenderer::renderModeFlags() const
{
    return m_renderMode.flags();
}

void BgfxRenderer::drawPolylines(const PolylineRenderData& data) {
    static uint32_t logged = 0;
    if (logged < 10)
    {
        ++logged;
        std::cout << "[POLYLINE_DEBUG] program=" << bgfx::isValid(m_polylineProgram)
                  << " vertexCount=" << data.vertexCount
                  << " vertices=" << (data.vertices != nullptr)
                  << " layer=" << data.layer << std::endl;
        std::cout << "[POLYLINE_DEBUG] p0=(" << data.vertices[0].position.x
                  << ", " << data.vertices[0].position.y
                  << ", " << data.vertices[0].position.z
                  << std::endl;
    }
    if (!bgfx::isValid(m_polylineProgram) || data.vertexCount < 2 || !data.vertices) return;
    const glm::mat4 proj = projectionForDirect3D(data.projection);
    float logDepth[4] = { data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w };
    const float depthStyle = depthStyleForRenderMode(m_renderMode.mode());
    float prim[4] = { 0.15f, data.edgeSoftness, depthStyle, 0.0f };
    const float denom = logDepthDenominator(data.logDepth);
    if (logged < 10)
    {
        const glm::vec4 view0 = data.view *
            glm::vec4(data.vertices[0].position, 1.0f);
        const glm::vec4 clip0 = proj * view0;
        std::cout << "[POLYLINE_DEBUG] view0=(" << view0.x << ", " << view0.y
                  << ", " << view0.z << ", " << view0.w
                  << ") clip0=(" << clip0.x << ", " << clip0.y
                  << ", " << clip0.z << ", " << clip0.w << ")" << std::endl;
    }
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | (data.layer > 0.0f ? BGFX_STATE_DEPTH_TEST_ALWAYS
                             : BGFX_STATE_DEPTH_TEST_LEQUAL)
        | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

    constexpr uint32_t kMaxChunkVertices = 63000;
    for (uint32_t first = 0; first < data.vertexCount;)
    {
        uint32_t count = std::min({data.vertexCount - first,
                                   kMaxChunkVertices,
                                   bgfx::getAvailTransientVertexBuffer(
                                       kMaxChunkVertices, m_polylineLayout)});
        count -= count % 6;
        if (count < 6)
            return;

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, count, m_polylineLayout);
        std::memcpy(tvb.data, data.vertices + first, count * sizeof(PrimVertex));

        glm::mat4 identity = glm::mat4(1.0f);
        bgfx::setTransform(glm::value_ptr(identity));
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setUniform(m_view, glm::value_ptr(data.view));
        bgfx::setUniform(m_projection, glm::value_ptr(proj));
        bgfx::setUniform(m_logDepth, logDepth);
        bgfx::setUniform(m_primParams, prim);

        glm::vec3 centroid(0.0f);
        for (uint32_t i = first; i < first + count; ++i)
            centroid += data.vertices[i].position;
        centroid /= float(count);
        const glm::vec4 centroidView = data.view * glm::vec4(centroid, 1.0f);
        const float centroidDepth = -centroidView.z;
        const float layerOffsetValue =
            layerOffsetUnits(data.layer, centroidDepth, data.logDepth);
        const uint32_t sortDepth = normalizedSortDepth(
            centroidDepth - layerOffsetValue, data.logDepth, denom);
        const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_layerOffset, layerOffset);

        bgfx::setState(state);
        bgfx::submit(kViewOverlay, m_polylineProgram, sortDepth);
        first += count;
    }
}

void BgfxRenderer::drawLineInstances(const LineInstancesRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_lineInstanceProgram) ||
        !bgfx::isValid(m_lineInstanceQuadBuffer) ||
        !data.instances || data.instanceCount == 0)
    {
        return;
    }

    const glm::mat4 proj = projectionForDirect3D(data.projection);
    const float logDepth[4] = {
        data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w};
    const float depthStyle = depthStyleForRenderMode(m_renderMode.mode());
    const float prim[4] = {0.15f, data.edgeSoftness, depthStyle, data.dash};
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        (data.layer > 0.0f ? BGFX_STATE_DEPTH_TEST_ALWAYS
                           : BGFX_STATE_DEPTH_TEST_LEQUAL) |
        BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

    constexpr uint16_t kStride = sizeof(LineInstance);
    static_assert(kStride == 64,
                  "LineInstance must match the GPU instance stride");
    const float denom = logDepthDenominator(data.logDepth);

    uint32_t first = 0;
    while (first < data.instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            data.instanceCount - first, kStride);
        if (available == 0)
            break;

        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, available, kStride);
        std::memcpy(idb.data, data.instances + first,
                    size_t(idb.num) * kStride);

        glm::vec3 centroid(0.0f);
        for (uint32_t i = 0; i < idb.num; ++i)
        {
            const LineInstance &instance = data.instances[first + i];
            centroid += 0.5f * (glm::vec3(instance.start) + glm::vec3(instance.end));
        }
        centroid /= float(idb.num);
        const glm::vec4 centroidView = data.view * glm::vec4(centroid, 1.0f);
        const float centroidDepth = -centroidView.z;
        const float layerOffsetValue =
            layerOffsetUnits(data.layer, centroidDepth, data.logDepth);
        const uint32_t sortDepth = normalizedSortDepth(
            centroidDepth - layerOffsetValue, data.logDepth, denom);
        const float layerOffset[4] = {layerOffsetValue, 0.0f, 0.0f, 0.0f};

        bgfx::setUniform(m_view, glm::value_ptr(data.view));
        bgfx::setUniform(m_projection, glm::value_ptr(proj));
        bgfx::setUniform(m_logDepth, logDepth);
        bgfx::setUniform(m_primParams, prim);
        bgfx::setUniform(m_layerOffset, layerOffset);
        bgfx::setVertexBuffer(0, m_lineInstanceQuadBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::setState(state);
        bgfx::submit(kViewOverlay, m_lineInstanceProgram, sortDepth);

        first += idb.num;
    }
}

void BgfxRenderer::drawFilledTriangles(const FilledTrianglesRenderData& data)
{
    const RenderModeFlags modeFlags = m_renderMode.flags();
    if (!data.is3DFace && !modeFlags.show2dSolidFills)
        return;
    if (data.is3DFace && !modeFlags.face3dFill && !modeFlags.hiddenLine)
        return;
    if (!bgfx::isValid(m_fillProgram) || data.vertexCount < 3 || !data.vertices)
        return;
    const uint32_t vertSize = sizeof(FillVertex);
    const glm::mat4 proj = projectionForDirect3D(data.projection);
    float logDepth[4] = { data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w };
    const float fillStyle[4] = { depthStyleForRenderMode(m_renderMode.mode()), 0.0f, 0.0f, 0.0f };
    const float denom = logDepthDenominator(data.logDepth);
    const uint64_t prepassState = BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_WRITE_Z | BGFX_STATE_MSAA;
    const uint64_t fillState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

    constexpr uint32_t kMaxChunkVertices = 63000;
    for (uint32_t first = 0; first < data.vertexCount;)
    {
        uint32_t count = std::min({data.vertexCount - first,
                                   kMaxChunkVertices,
                                   bgfx::getAvailTransientVertexBuffer(
                                       kMaxChunkVertices, m_fillLayout)});
        count -= count % 3;
        if (count < 3)
            return;

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, count, m_fillLayout);
        std::memcpy(tvb.data, data.vertices + first, count * vertSize);

        glm::mat4 identity = glm::mat4(1.0f);
        bgfx::setTransform(glm::value_ptr(identity));
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setUniform(m_view, glm::value_ptr(data.view));
        bgfx::setUniform(m_projection, glm::value_ptr(proj));
        bgfx::setUniform(m_logDepth, logDepth);
        bgfx::setUniform(m_primParams, fillStyle);
        bgfx::setUniform(m_realisticMaterial,
                         glm::value_ptr(data.material.baseColor));

        glm::vec3 centroid(0.0f);
        for (uint32_t i = first; i < first + count; ++i)
            centroid += data.vertices[i].position;
        centroid /= float(count);
        const glm::vec4 centroidView = data.view * glm::vec4(centroid, 1.0f);
        const float centroidDepth = -centroidView.z;
        const float layerOffsetValue =
            layerOffsetUnits(data.layer, centroidDepth, data.logDepth);
        const uint32_t sortDepth = normalizedSortDepth(
            centroidDepth - layerOffsetValue, data.logDepth, denom);
        const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_layerOffset, layerOffset);

        if (data.is3DFace && modeFlags.hiddenLine)
        {
            bgfx::setState(prepassState);
            bgfx::submit(kViewDepthPrepass, m_fillProgram, sortDepth);
        }
        else if (!data.is3DFace || modeFlags.face3dFill)
        {
            bgfx::setState(fillState | BGFX_STATE_WRITE_Z);
            bgfx::submit(kViewSolidFill, m_fillProgram, sortDepth);
        }

        first += count;
    }
}

void BgfxRenderer::drawCurves(const CurveRenderData& data)
{
    if (!m_initialized || !bgfx::isValid(m_curveProgram) ||
        data.sampleCount < 2)
        return;

    struct CurveVertex
    {
        float t;
        float unused;
    };

    const glm::mat4 proj = projectionForDirect3D(data.projection);
    const float logDepth[4] = {
        data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w };
    const uint32_t sampleCount = std::min<uint32_t>(data.sampleCount, 512);
    const uint32_t available = bgfx::getAvailTransientVertexBuffer(
        sampleCount, m_curveLayout);
    if (available < sampleCount)
        return;

    glm::vec3 center(0.0f);
    for (const glm::vec4 &point : data.controlPoints)
        center += glm::vec3(point);
    center /= float(data.controlPoints.size());
    const glm::vec4 centerView = data.view * glm::vec4(center, 1.0f);
    const float centerDepth = -centerView.z;
    const float denom = logDepthDenominator(data.logDepth);
    const float layerOffsetValue =
        layerOffsetUnits(data.layer, centerDepth, data.logDepth);
    const uint32_t sortDepth = normalizedSortDepth(
        centerDepth - layerOffsetValue, data.logDepth, denom);
    const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, sampleCount, m_curveLayout);
    auto *vertices = reinterpret_cast<CurveVertex*>(tvb.data);
    for (uint32_t i = 0; i < sampleCount; ++i)
    {
        vertices[i].t = float(i) / float(sampleCount - 1);
        vertices[i].unused = 0.0f;
    }

    bgfx::setTransform(glm::value_ptr(glm::mat4(1.0f)));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(proj));
    bgfx::setUniform(m_logDepth, logDepth);
    bgfx::setUniform(m_layerOffset, layerOffset);
    bgfx::setUniform(m_curveCP, glm::value_ptr(data.controlPoints.front()), 16);
    bgfx::setUniform(m_curveKnot, glm::value_ptr(data.knots.front()), 4);
    bgfx::setUniform(m_curveParams, glm::value_ptr(data.params));
    bgfx::setUniform(m_curveColor, glm::value_ptr(data.color));
    bgfx::setUniform(m_curveArc, glm::value_ptr(data.arc));
    // Curves are line-like overlay geometry. They must use the same ordered
    // overlay pass as their outlines; otherwise the later overlay pass draws
    // the wider outline on top of the original curve.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINESTRIP |
                   BGFX_STATE_LINEAA | BGFX_STATE_MSAA);
    bgfx::submit(kViewOverlay, m_curveProgram, sortDepth);
}

void BgfxRenderer::drawAabb(const AabbRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_cubeProgram) ||
        !bgfx::isValid(m_aabbBuffer))
        return;

    const glm::vec3 size = glm::max(data.relativeMax - data.relativeMin,
                                    glm::vec3(1e-5f));
    const glm::vec3 center = (data.relativeMax + data.relativeMin) * 0.5f;
    const glm::mat4 model = glm::scale(glm::mat4(1.0f), size);
    const glm::mat4 projection = projectionForDirect3D(data.projection);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LEQUAL |
                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_PT_LINES |
                   BGFX_STATE_LINEAA | BGFX_STATE_MSAA);
    bgfx::setTransform(glm::value_ptr(model));
    bgfx::setVertexBuffer(0, m_aabbBuffer);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_cubeRelativePosition,
                     glm::value_ptr(glm::vec4(data.object.high, 1.0f)));
    bgfx::setUniform(m_cubeRelativePositionLow,
                     glm::value_ptr(glm::vec4(data.object.low, 0.0f)));
    bgfx::setUniform(m_eyeHigh,
                     glm::value_ptr(glm::vec4(data.eye.high, 0.0f)));
    bgfx::setUniform(m_eyeLow,
                     glm::value_ptr(glm::vec4(data.eye.low, 0.0f)));
    bgfx::setUniform(m_cubeOpacity,
                     glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_cubeColor,
                     glm::value_ptr(glm::vec4(data.color, 1.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    const float aabbStyle[4] = { depthStyleForRenderMode(m_renderMode.mode()), 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_primParams, aabbStyle);
    const glm::vec4 aabbCenterView =
        data.view * glm::vec4((data.relativeMin + data.relativeMax) * 0.5f, 1.0f);
    const float layerOffsetValue = layerOffsetUnits(
        data.layer, -aabbCenterView.z, data.logDepth);
    const float layerOffset[4] = { layerOffsetValue, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_layerOffset, layerOffset);
    bgfx::submit(kViewWire, m_cubeProgram);
}

glm::mat4 BgfxRenderer::gpuPickProjection(const GpuPickRequest &request) const
{
    const bool orthographic = request.projection[3][3] > 0.5f;
    double left = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    double top = 0.0;
    double nearDepth = request.nearDepth;
    double farDepth = std::max(request.farDepth, nearDepth + 0.01);

    if (orthographic)
    {
        // The shared ortho depth slab can intentionally start behind the
        // camera. Preserve it so the pick pass clips identically to the
        // main render pass.
        const double halfWidth =
            1.0 / std::max(std::abs(request.projection[0][0]), 1.0e-12f);
        const double halfHeight =
            1.0 / std::max(std::abs(request.projection[1][1]), 1.0e-12f);
        const double pixelWidth = halfWidth / std::max(1, int(m_width));
        const double pixelHeight = halfHeight / std::max(1, int(m_height));
        left = request.ndcX * halfWidth - pixelWidth;
        right = request.ndcX * halfWidth + pixelWidth;
        bottom = request.ndcY * halfHeight - pixelHeight;
        top = request.ndcY * halfHeight + pixelHeight;
        return glm::ortho(left, right, bottom, top, nearDepth, farDepth);
    }

    nearDepth = std::max(nearDepth, 0.01);
    farDepth = std::max(farDepth, nearDepth + 0.01);

    const double halfWidthAtNear =
        nearDepth / std::max(std::abs(request.projection[0][0]), 1.0e-12f);
    const double halfHeightAtNear =
        nearDepth / std::max(std::abs(request.projection[1][1]), 1.0e-12f);
    const double pixelWidthAtNear = halfWidthAtNear / std::max(1, int(m_width));
    const double pixelHeightAtNear = halfHeightAtNear / std::max(1, int(m_height));
    left = request.ndcX * halfWidthAtNear - pixelWidthAtNear;
    right = request.ndcX * halfWidthAtNear + pixelWidthAtNear;
    bottom = request.ndcY * halfHeightAtNear - pixelHeightAtNear;
    top = request.ndcY * halfHeightAtNear + pixelHeightAtNear;
    return glm::frustum(left, right, bottom, top, nearDepth, farDepth);
}

bool BgfxRenderer::gpuPickInstanceIsCandidate(
    const MeshInstance &instance) const
{
    const glm::vec3 center =
        glm::vec3(instance.positionHigh) - m_gpuPickRequest.eye.high +
        glm::vec3(instance.positionLow) - m_gpuPickRequest.eye.low;
    const glm::vec4 clip = m_gpuPickRequest.projection *
        m_gpuPickRequest.view * glm::vec4(center, 1.0f);
    if (!(clip.w > std::numeric_limits<float>::epsilon()))
        return false;

    const float radius = std::max({
        glm::length(glm::vec3(instance.transformColumn0)),
        glm::length(glm::vec3(instance.transformColumn1)),
        glm::length(glm::vec3(instance.transformColumn2))}) * 1.5f;
    const float radiusNdcX =
        radius * std::abs(m_gpuPickRequest.projection[0][0]) / clip.w;
    const float radiusNdcY =
        radius * std::abs(m_gpuPickRequest.projection[1][1]) / clip.w;
    const glm::vec2 delta(
        clip.x / clip.w - float(m_gpuPickRequest.ndcX),
        clip.y / clip.w - float(m_gpuPickRequest.ndcY));
    return std::abs(delta.x) <= radiusNdcX &&
           std::abs(delta.y) <= radiusNdcY;
}

bool BgfxRenderer::gpuPickVerticesAreCandidate(
    const FillVertex *vertices, uint32_t vertexCount) const
{
    if (!vertices || vertexCount == 0)
        return false;

    glm::vec3 center(0.0f);
    for (uint32_t i = 0; i < vertexCount; ++i)
        center += vertices[i].position;
    center /= float(vertexCount);

    float radiusSquared = 0.0f;
    for (uint32_t i = 0; i < vertexCount; ++i)
        radiusSquared = std::max(radiusSquared,
                                 glm::dot(vertices[i].position - center,
                                          vertices[i].position - center));
    const float radius = std::sqrt(radiusSquared) * 1.5f;

    const glm::vec4 clip = m_gpuPickRequest.projection *
        m_gpuPickRequest.view * glm::vec4(center, 1.0f);
    if (!(clip.w > std::numeric_limits<float>::epsilon()))
        return false;

    const float radiusNdcX =
        radius * std::abs(m_gpuPickRequest.projection[0][0]) / clip.w;
    const float radiusNdcY =
        radius * std::abs(m_gpuPickRequest.projection[1][1]) / clip.w;
    const glm::vec2 delta(
        clip.x / clip.w - float(m_gpuPickRequest.ndcX),
        clip.y / clip.w - float(m_gpuPickRequest.ndcY));
    return std::abs(delta.x) <= radiusNdcX &&
           std::abs(delta.y) <= radiusNdcY;
}

bool BgfxRenderer::gpuPickCachedVerticesAreCandidate(
    const GpuTrianglePickGeometry &geometry,
    const glm::mat4 &view) const
{
    const glm::vec4 clip = m_gpuPickRequest.projection *
        view * glm::vec4(geometry.center, 1.0f);
    if (!(clip.w > std::numeric_limits<float>::epsilon()))
        return false;

    const float radiusNdcX =
        geometry.radius * std::abs(m_gpuPickRequest.projection[0][0]) /
        clip.w;
    const float radiusNdcY =
        geometry.radius * std::abs(m_gpuPickRequest.projection[1][1]) /
        clip.w;
    const glm::vec2 delta(
        clip.x / clip.w - float(m_gpuPickRequest.ndcX),
        clip.y / clip.w - float(m_gpuPickRequest.ndcY));
    return std::abs(delta.x) <= radiusNdcX &&
           std::abs(delta.y) <= radiusNdcY;
}

uint32_t BgfxRenderer::requestGpuPick(const GpuPickRequest &request)
{
    if (!m_initialized || !bgfx::isValid(m_gpuPickProgram))
        return 0;

    completeGpuPickReadback();
    // The full-scene ID view does not consume the one-pixel result, so it may
    // refresh every frame without waiting for debug-texture readback.
    const bool sceneRequest =
        (m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) && request.ndcX == 0.0 && request.ndcY == 0.0;
    if (m_gpuPickReadPending && !sceneRequest)
        return 0;

    m_gpuPickRequest = request;
    m_gpuPickPrimitives.clear();
    m_gpuPickQueueStats = {};
    m_gpuPickQueueStats.meshCapacity = kMaxGpuPickInstances;
    m_gpuPickQueueStats.triangleCapacity = kMaxGpuPickTriangleBatches;
    m_gpuPickActive = true;
    m_gpuPickLastResult.ready = false;
    m_gpuPickLastResult.hit = false;
    m_gpuPickLastResult.requestToken = m_gpuPickNextToken++;
    m_gpuPickLastResult.objectId = 0;
    m_gpuPickLastResult.faceIndex = 0;
    return m_gpuPickLastResult.requestToken;
}

void BgfxRenderer::queueGpuMeshPick(const MeshInstance &instance,
                                    MeshType mesh, uint32_t objectId)
{
    const RenderModeFlags modeFlags = m_renderMode.flags();
    const GpuPickPrimitive::Kind kind =
        (!modeFlags.meshFill && modeFlags.show3dEdges)
            ? GpuPickPrimitive::Kind::Edge
            : GpuPickPrimitive::Kind::Mesh;

    // The outlined entity must always reach the ID buffer or its
    // outline silently disappears, so it may exceed the soft cap.
    const bool meshCapacityFull =
        kind == GpuPickPrimitive::Kind::Mesh &&
        m_gpuPickQueueStats.queuedMeshes >= kMaxGpuPickInstances &&
        objectId != m_selectionOutlineId;
    if (meshCapacityFull)
        ++m_gpuPickQueueStats.droppedMeshes;

    if (!m_gpuPickActive || objectId == 0 || objectId == 0xffffffffu ||
        meshCapacityFull ||
        (!(m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) && !gpuPickInstanceIsCandidate(instance)))
    {
        return;
    }

    GpuPickPrimitive &primitive = m_gpuPickPrimitives.emplace_back();
    primitive.kind = kind;
    primitive.meshInstance = instance;
    primitive.meshType = mesh;
    primitive.objectId = objectId;
    primitive.occlusionRank =
        primitive.kind == GpuPickPrimitive::Kind::Edge
            ? 1
            : (instance.positionHigh.w >= 0.999f ? 0 : 1);

    // The pick pass does not need mesh color or opacity, so the four unused
    // instance scalars carry the objectId directly to the shader.
    primitive.meshInstance.transformColumn0.w =
        float((objectId >> 16) & 0xff) / 255.0f;
    primitive.meshInstance.transformColumn1.w =
        float((objectId >> 8) & 0xff) / 255.0f;
    primitive.meshInstance.transformColumn2.w =
        float(objectId & 0xff) / 255.0f;
    primitive.meshInstance.positionHigh.w =
        float((objectId >> 24) & 0xff) / 255.0f;

    if (kind == GpuPickPrimitive::Kind::Mesh)
        ++m_gpuPickQueueStats.queuedMeshes;
    else
        ++m_gpuPickQueueStats.queuedEdges;
}
GpuPickQueueStats BgfxRenderer::gpuPickQueueStats() const
{
    return m_gpuPickQueueStats;
}

void BgfxRenderer::queueGpuTrianglePick(uint64_t geometryKey,
                                        const FillVertex *vertices,
                                        uint32_t vertexCount,
                                        const glm::mat4 &view,
                                        const glm::mat4 &projection,
                                        const glm::vec4 &logDepth,
                                        uint32_t objectId,
                                        uint8_t occlusionRank)
{
    const size_t triangleCount = m_gpuPickQueueStats.queuedTriangles;
    // The outlined entity must always reach the ID buffer or its
    // outline silently disappears, so it may exceed the soft cap.
    const bool triangleCapacityFull =
        triangleCount >= kMaxGpuPickTriangleBatches &&
        objectId != m_selectionOutlineId;
    if (triangleCapacityFull)
        ++m_gpuPickQueueStats.droppedTriangles;

    const bool transient = geometryKey == 0;
    if (!m_gpuPickActive || objectId == 0 || objectId == 0xffffffffu ||
        !vertices || vertexCount < 3 || vertexCount > 65535 ||
        triangleCapacityFull)
    {
        return;
    }

    if (transient)
    {
        if (!(m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) &&
            !gpuPickVerticesAreCandidate(vertices, vertexCount))
            return;

        GpuPickPrimitive &primitive = m_gpuPickPrimitives.emplace_back();
        primitive.kind = GpuPickPrimitive::Kind::Triangle;
        primitive.geometryKey = 0;
        primitive.view = view;
        primitive.projection = projection;
        primitive.objectId = objectId;
        primitive.occlusionRank = std::min<uint8_t>(occlusionRank, 2);
        primitive.transientVertices.assign(vertices, vertices + vertexCount);
        ++m_gpuPickQueueStats.queuedTriangles;
        return;
    }

    auto [geometryIt, inserted] =
        m_gpuPickTriangleGeometry.try_emplace(geometryKey);
    if (inserted)
    {
        glm::vec3 center(0.0f);
        for (uint32_t i = 0; i < vertexCount; ++i)
            center += vertices[i].position;
        center /= float(vertexCount);

        float radiusSquared = 0.0f;
        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            const glm::vec3 delta = vertices[i].position - center;
            radiusSquared = std::max(radiusSquared, glm::dot(delta, delta));
        }

        geometryIt->second.center = center;
        geometryIt->second.radius = std::sqrt(radiusSquared) * 1.5f;
        if (!(m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) &&
            !gpuPickCachedVerticesAreCandidate(geometryIt->second, view))
        {
            m_gpuPickTriangleGeometry.erase(geometryIt);
            return;
        }

        geometryIt->second.buffer = bgfx::createVertexBuffer(
            bgfx::copy(vertices, vertexCount * sizeof(FillVertex)),
            m_fillLayout);
        if (!bgfx::isValid(geometryIt->second.buffer))
        {
            m_gpuPickTriangleGeometry.erase(geometryIt);
            ++m_gpuPickQueueStats.resourceFailures;
            return;
        }
    }
    else if (!(m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) &&
             !gpuPickCachedVerticesAreCandidate(geometryIt->second, view))
    {
        return;
    }

    GpuPickPrimitive &primitive = m_gpuPickPrimitives.emplace_back();
    primitive.kind = GpuPickPrimitive::Kind::Triangle;
    primitive.geometryKey = geometryKey;
    primitive.view = view;
    primitive.projection = projection;
    primitive.objectId = objectId;
    primitive.occlusionRank = std::min<uint8_t>(occlusionRank, 2);
    ++m_gpuPickQueueStats.queuedTriangles;
}

GpuPickResult BgfxRenderer::pollGpuPick()
{
    completeGpuPickReadback();
    return m_gpuPickLastResult;
}

void BgfxRenderer::cancelGpuPick()
{
    m_gpuPickActive = false;
    m_gpuPickReadPending = false;
    m_gpuPickPrimitives.clear();
    m_gpuPickQueueStats = {};
}

void BgfxRenderer::renderGpuPickPass()
{
    if (!m_gpuPickActive || !bgfx::isValid(m_gpuPickProgram) ||
        !bgfx::isValid(m_gpuPickFrameBuffer) ||
        !bgfx::isValid(m_gpuPickReadback))
    {
        return;
    }

    // A 1x1 readback is already queued. Submitting the same request again
    // overwrites m_gpuPickReadFrame every frame, so its completion frame
    // keeps moving forward and pollGpuPick() times out after 16 frames.
    if (m_gpuPickReadPending)
    {
        return;
    }

    // Full-scene debug does not need the 1x1 pick target. Skipping it halves
    // transient buffer pressure and keeps the visible texture current-frame.
    if ((m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) &&
        m_gpuPickRequest.ndcX == 0.0 && m_gpuPickRequest.ndcY == 0.0)
    {
        renderGpuPickDebugPass(projectionForDirect3D(m_gpuPickRequest.projection));
        return;
    }

    bgfx::setViewName(kViewGpuPick, "CAD Gpu Pick");
    bgfx::setViewName(kViewGpuPickBlit, "CAD Gpu Pick Blit");
    bgfx::setViewFrameBuffer(kViewGpuPick, m_gpuPickFrameBuffer);
    bgfx::setViewRect(kViewGpuPick, 0, 0, 1, 1);
    bgfx::setViewRect(kViewGpuPickBlit, 0, 0, 1, 1);
    bgfx::setViewClear(kViewGpuPick,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0xffffffff, 1.0f, 0);
    bgfx::setViewTransform(kViewGpuPick,
                           glm::value_ptr(m_gpuPickRequest.view),
                           glm::value_ptr(m_gpuPickRequest.projection));

    const glm::mat4 projection = projectionForDirect3D(
        gpuPickProjection(m_gpuPickRequest));
    if (m_gpuPickPrimitives.empty())
        bgfx::touch(kViewGpuPick);
    else
        submitGpuPickPrimitives(kViewGpuPick, projection);

    bgfx::blit(kViewGpuPickBlit, m_gpuPickReadback, 0, 0,
               bgfx::getTexture(m_gpuPickFrameBuffer));
    bgfx::touch(kViewGpuPickBlit);
    m_gpuPickReadPending = true;
    m_gpuPickReadFrame = bgfx::readTexture(
        m_gpuPickReadback, m_gpuPickReadbackData.data());

    if (m_gpuPickDebugVisible)
        renderGpuPickDebugPass(projection);
}

// GRID_DEBUG_PICK=1: dump the pick payload once per visual-style change so
// a broken ID pass can be compared across modes (instance matrices, eye
// rebasing, and the first primitive of each kind).
static bool gpuPickDumpEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("GRID_DEBUG_PICK");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

void BgfxRenderer::submitGpuPickPrimitives(bgfx::ViewId view,
                                           const glm::mat4 &projection)
{
    bgfx::setUniform(m_view, glm::value_ptr(m_gpuPickRequest.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_eyeHigh,
                     glm::value_ptr(glm::vec4(m_gpuPickRequest.eye.high, 0.0f)));
    bgfx::setUniform(m_eyeLow,
                     glm::value_ptr(glm::vec4(m_gpuPickRequest.eye.low, 0.0f)));
    bgfx::setUniform(m_logDepth,
                     glm::value_ptr(m_gpuPickRequest.logDepth));

    // Match visual compositing: opaque surfaces first, non-depth-writing
    // transparent surfaces/edges next, CAD stroke overlays last. A line drawn
    // over a translucent mesh must therefore win the ID test too.
    auto pickPrimitiveCameraDepth =
        [&](const GpuPickPrimitive &primitive) {
            glm::vec3 center(0.0f);
            if (primitive.kind == GpuPickPrimitive::Kind::Mesh ||
                primitive.kind == GpuPickPrimitive::Kind::Edge)
            {
                center = glm::vec3(primitive.meshInstance.positionHigh) -
                         m_gpuPickRequest.eye.high +
                         glm::vec3(primitive.meshInstance.positionLow) -
                         m_gpuPickRequest.eye.low;
            }
            else if (primitive.geometryKey == 0)
            {
                if (primitive.transientVertices.empty())
                    return 0.0f;
                for (const FillVertex &vertex : primitive.transientVertices)
                    center += vertex.position;
                center /= float(primitive.transientVertices.size());
            }
            else
            {
                const auto geometry =
                    m_gpuPickTriangleGeometry.find(primitive.geometryKey);
                if (geometry == m_gpuPickTriangleGeometry.end())
                    return 0.0f;
                center = geometry->second.center;
            }

            const glm::vec4 viewPosition =
                m_gpuPickRequest.view * glm::vec4(center, 1.0f);
            return viewPosition.w > std::numeric_limits<float>::epsilon()
                       ? -viewPosition.z / viewPosition.w
                       : 0.0f;
        };

    struct OrderedPickPrimitive
    {
        const GpuPickPrimitive *primitive = nullptr;
        float depth = 0.0f;
    };

    static std::vector<OrderedPickPrimitive> orderedPickPrimitives;
    orderedPickPrimitives.clear();
    orderedPickPrimitives.reserve(m_gpuPickPrimitives.size());
    for (const GpuPickPrimitive &primitive : m_gpuPickPrimitives)
        orderedPickPrimitives.push_back({&primitive, pickPrimitiveCameraDepth(primitive)});

    std::stable_sort(
        orderedPickPrimitives.begin(), orderedPickPrimitives.end(),
        [](const OrderedPickPrimitive &lhs, const OrderedPickPrimitive &rhs) {
            if (lhs.primitive->occlusionRank != rhs.primitive->occlusionRank)
                return lhs.primitive->occlusionRank < rhs.primitive->occlusionRank;

            // Non-depth-writing primitives are composited in submission order.
            // Sort each rank back-to-front so the nearest visible surface/edge
            // is submitted last and therefore owns the ID, matching blending.
            if (lhs.primitive->occlusionRank == 0)
                return false;

            constexpr float kDepthEpsilon = 1.0e-4f;
            return lhs.depth > rhs.depth + kDepthEpsilon;
        });

    if (gpuPickDumpEnabled())
    {
        static RenderMode dumpedMode = RenderMode(0xff);
        if (m_renderMode.mode() != dumpedMode)
        {
            dumpedMode = m_renderMode.mode();
            size_t meshCount = 0, edgeCount = 0, triangleCount = 0;
            const GpuPickPrimitive *firstMesh = nullptr;
            const GpuPickPrimitive *firstEdge = nullptr;
            const GpuPickPrimitive *firstTriangle = nullptr;
            for (const OrderedPickPrimitive &ordered : orderedPickPrimitives)
            {
                const GpuPickPrimitive &candidate = *ordered.primitive;
                if (candidate.kind == GpuPickPrimitive::Kind::Mesh)
                {
                    ++meshCount;
                    if (!firstMesh) firstMesh = &candidate;
                }
                else if (candidate.kind == GpuPickPrimitive::Kind::Edge)
                {
                    ++edgeCount;
                    if (!firstEdge) firstEdge = &candidate;
                }
                else
                {
                    ++triangleCount;
                    if (!firstTriangle) firstTriangle = &candidate;
                }
            }
            std::cout << "[PICK_DUMP] style="
                      << renderModeLabel(m_renderMode.mode())
                      << " meshes=" << meshCount << " edges=" << edgeCount
                      << " triangles=" << triangleCount << std::endl;
            std::cout << "[PICK_DUMP] eyeHigh=("
                      << m_gpuPickRequest.eye.high.x << ","
                      << m_gpuPickRequest.eye.high.y << ","
                      << m_gpuPickRequest.eye.high.z << ") eyeLow=("
                      << m_gpuPickRequest.eye.low.x << ","
                      << m_gpuPickRequest.eye.low.y << ","
                      << m_gpuPickRequest.eye.low.z << ")" << std::endl;
            auto dumpInstance = [&](const char *label,
                                    const GpuPickPrimitive &candidate) {
                const MeshInstance &m = candidate.meshInstance;
                std::cout << "[PICK_DUMP] " << label
                          << " objectId=" << candidate.objectId
                          << " rank=" << unsigned(candidate.occlusionRank)
                          << std::endl;
                std::cout << "  col0=(" << m.transformColumn0.x << ","
                          << m.transformColumn0.y << ","
                          << m.transformColumn0.z << "|"
                          << m.transformColumn0.w << ") col1=("
                          << m.transformColumn1.x << ","
                          << m.transformColumn1.y << ","
                          << m.transformColumn1.z << "|"
                          << m.transformColumn1.w << ") col2=("
                          << m.transformColumn2.x << ","
                          << m.transformColumn2.y << ","
                          << m.transformColumn2.z << "|"
                          << m.transformColumn2.w << ")" << std::endl;
                std::cout << "  posHigh=(" << m.positionHigh.x << ","
                          << m.positionHigh.y << "," << m.positionHigh.z
                          << "|" << m.positionHigh.w << ") posLow=("
                          << m.positionLow.x << "," << m.positionLow.y
                          << "," << m.positionLow.z << "|"
                          << m.positionLow.w << ")" << std::endl;
            };
            if (firstMesh)
                dumpInstance("firstMesh", *firstMesh);
            if (firstEdge)
                dumpInstance("firstEdge", *firstEdge);
            if (firstTriangle)
            {
                if (firstTriangle->geometryKey != 0)
                {
                    std::cout << "[PICK_DUMP] firstTriangle cached key="
                              << firstTriangle->geometryKey << " view3=("
                              << firstTriangle->view[3][0] << ","
                              << firstTriangle->view[3][1] << ","
                              << firstTriangle->view[3][2] << ")" << std::endl;
                }
                else if (!firstTriangle->transientVertices.empty())
                {
                    const FillVertex &v =
                        firstTriangle->transientVertices.front();
                    std::cout << "[PICK_DUMP] firstTriangle transient n="
                              << firstTriangle->transientVertices.size()
                              << " v0=(" << v.position.x << ","
                              << v.position.y << "," << v.position.z
                              << ") c=(" << v.color.x << "," << v.color.y
                              << "," << v.color.z << "," << v.color.w
                              << ")" << std::endl;
                }
            }
        }
    }
    constexpr uint16_t kStride = sizeof(MeshInstance);
    for (size_t primitiveIndex = 0; primitiveIndex < orderedPickPrimitives.size();)
    {
        const GpuPickPrimitive &primitive =
            *orderedPickPrimitives[primitiveIndex].primitive;

        if (primitive.kind == GpuPickPrimitive::Kind::Mesh ||
            primitive.kind == GpuPickPrimitive::Kind::Edge)
        {
            size_t batchSize = 1;
            while (primitiveIndex + batchSize < orderedPickPrimitives.size())
            {
                const GpuPickPrimitive &nextPrimitive =
                    *orderedPickPrimitives[primitiveIndex + batchSize].primitive;
                if (nextPrimitive.kind != primitive.kind ||
                    nextPrimitive.meshType != primitive.meshType ||
                    nextPrimitive.occlusionRank != primitive.occlusionRank)
                {
                    break;
                }
                ++batchSize;
            }

            batchSize = std::min(batchSize, size_t(kMaxGpuPickInstances));
            const bool edgePass = primitive.kind == GpuPickPrimitive::Kind::Edge;
            bgfx::VertexBufferHandle buffer = m_gpuPickCubeBuffer;
            bgfx::VertexBufferHandle edgeBuffer = m_gpuPickCubeEdgeBuffer;
            switch (primitive.meshType)
            {
            case MeshType::Sphere:
                buffer = m_gpuPickSphereBuffer;
                edgeBuffer = m_gpuPickSphereEdgeBuffer;
                break;
            case MeshType::Cone:
                buffer = m_gpuPickConeBuffer;
                edgeBuffer = m_gpuPickConeEdgeBuffer;
                break;
            case MeshType::Torus:
                buffer = m_gpuPickTorusBuffer;
                edgeBuffer = m_gpuPickTorusEdgeBuffer;
                break;
            case MeshType::Cube:
                break;
            }
            if (edgePass)
                buffer = edgeBuffer;

            bgfx::InstanceDataBuffer instanceBuffer;
            bgfx::allocInstanceDataBuffer(
                &instanceBuffer, uint16_t(batchSize), kStride);
            auto *instances =
                reinterpret_cast<MeshInstance *>(instanceBuffer.data);
            for (size_t instanceIndex = 0;
                 instanceIndex < batchSize;
                 ++instanceIndex)
            {
                instances[instanceIndex] =
                    orderedPickPrimitives[primitiveIndex + instanceIndex]
                        .primitive->meshInstance;
            }

            bgfx::setUniform(m_view,
                             glm::value_ptr(m_gpuPickRequest.view));
            bgfx::setUniform(m_projection, glm::value_ptr(projection));
            bgfx::setVertexBuffer(0, buffer);
            bgfx::setInstanceDataBuffer(&instanceBuffer);
            const uint64_t primitiveState =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                (primitive.occlusionRank == 0 ? BGFX_STATE_WRITE_Z : uint64_t(0)) |
                BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;
            const uint64_t edgeState =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                BGFX_STATE_DEPTH_TEST_LEQUAL |
                BGFX_STATE_PT_LINES | BGFX_STATE_LINEAA | BGFX_STATE_MSAA;
            bgfx::setState(edgePass ? edgeState : primitiveState);
            bgfx::submit(view, m_gpuPickProgram);

            primitiveIndex += batchSize;
            continue;
        }

        const bool transient = primitive.geometryKey == 0;
        const auto geometryIt = transient
            ? m_gpuPickTriangleGeometry.end()
            : m_gpuPickTriangleGeometry.find(primitive.geometryKey);
        const bool validBuffer = transient
            ? !primitive.transientVertices.empty()
            : geometryIt != m_gpuPickTriangleGeometry.end() &&
              bgfx::isValid(geometryIt->second.buffer);
        if (!validBuffer || !bgfx::isValid(m_fillProgram))
        {
            ++primitiveIndex;
            continue;
        }

        bgfx::VertexBufferHandle persistentBuffer = BGFX_INVALID_HANDLE;
        bgfx::TransientVertexBuffer tvb;
        if (transient)
        {
            const uint32_t vertexCount32 =
                uint32_t(primitive.transientVertices.size());
            const uint32_t availableVertices =
                bgfx::getAvailTransientVertexBuffer(
                    vertexCount32, m_fillLayout);
            const size_t requiredBytes =
                primitive.transientVertices.size() * sizeof(FillVertex);
            if (availableVertices < vertexCount32)
            {
                ++m_gpuPickQueueStats.droppedTriangles;
                ++primitiveIndex;
                continue;
            }

            bgfx::allocTransientVertexBuffer(&tvb, vertexCount32,
                                             m_fillLayout);
            if (!tvb.data || tvb.stride != sizeof(FillVertex) ||
                size_t(tvb.size) < requiredBytes)
            {
                ++m_gpuPickQueueStats.droppedTriangles;
                ++primitiveIndex;
                continue;
            }

            std::memcpy(tvb.data, primitive.transientVertices.data(),
                        requiredBytes);
        }
        else
        {
            persistentBuffer = geometryIt->second.buffer;
        }

        glm::mat4 identity = glm::mat4(1.0f);
        bgfx::setTransform(glm::value_ptr(identity));
        if (transient)
            bgfx::setVertexBuffer(0, &tvb);
        else
            bgfx::setVertexBuffer(0, persistentBuffer);
        // The caller already supplies either the full-scene projection or the
        // 1x1 pick frustum. Triangle primitives may carry a camera-independent
        // view (for cached CAD fills), but they must never override the
        // current pick projection: doing so renders the full screen into the
        // 1x1 target and reads a fixed corner pixel.
        const glm::mat4 primitiveProjection = projection;
        // Triangle vertices are expressed in the primitive's view frame.
        // CAD fills use an anchor-relative view, so using the global pick
        // view here shifts the ID geometry away from the visible fill.
        bgfx::setUniform(m_view, glm::value_ptr(primitive.view));
        bgfx::setUniform(m_projection,
                         glm::value_ptr(primitiveProjection));
        const float logDepth[4] = {
            m_gpuPickRequest.logDepth.x,
            m_gpuPickRequest.logDepth.y,
            m_gpuPickRequest.logDepth.z,
            m_gpuPickRequest.logDepth.w};
        bgfx::setUniform(m_logDepth, logDepth);
        const float primParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(m_primParams, primParams);
        const float layerOffset[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(m_layerOffset, layerOffset);
        const float identityMaterial[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        bgfx::setUniform(m_realisticMaterial, identityMaterial);
        const uint64_t triangleState =
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
            (primitive.occlusionRank == 0 ? BGFX_STATE_WRITE_Z
                                          : uint64_t(0)) |
            BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA;
        bgfx::setState(triangleState);
        bgfx::submit(view, m_fillProgram);
        ++primitiveIndex;
    }
}

void BgfxRenderer::renderGpuPickDebugPass(const glm::mat4 &projection)
{
    if (!bgfx::isValid(m_gpuPickDebugFrameBuffer) ||
        !bgfx::isValid(m_gpuPickDebugReadback) ||
        !bgfx::isValid(m_gpuPickDebugVisualTexture))
    {
        return;
    }

    const glm::mat4 debugProjection = (m_gpuPickSceneDebug || m_gpuPickScenePassEnabled)
        ? projectionForDirect3D(m_gpuPickRequest.projection)
        : projection;
    const uint16_t debugWidth = std::max<uint16_t>(1, m_gpuPickDebugWidth);
    const uint16_t debugHeight = std::max<uint16_t>(1, m_gpuPickDebugHeight);
    bgfx::setViewName(kViewGpuPickDebug, "CAD Gpu Pick Debug");
    bgfx::setViewName(kViewGpuPickDebugBlit, "CAD Gpu Pick Debug Blit");
    bgfx::setViewFrameBuffer(kViewGpuPickDebug, m_gpuPickDebugFrameBuffer);
    bgfx::setViewRect(kViewGpuPickDebug, 0, 0, debugWidth, debugHeight);
    bgfx::setViewRect(kViewGpuPickDebugBlit, 0, 0, debugWidth, debugHeight);
    bgfx::setViewClear(kViewGpuPickDebug,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0xffffffff, 1.0f, 0);
    bgfx::setViewTransform(kViewGpuPickDebug,
                           glm::value_ptr(m_gpuPickRequest.view),
                           glm::value_ptr(debugProjection));

    if (m_gpuPickPrimitives.empty())
        bgfx::touch(kViewGpuPickDebug);
    else
        submitGpuPickPrimitives(kViewGpuPickDebug, debugProjection);

    // A continuous full-scene ID view must not block on full-frame CPU
    // readback. Its present shader normalizes with the CPU-assigned range.
    if ((m_gpuPickSceneDebug || m_gpuPickScenePassEnabled) &&
        m_gpuPickRequest.ndcX == 0.0 && m_gpuPickRequest.ndcY == 0.0)
        return;

    bgfx::blit(kViewGpuPickDebugBlit, m_gpuPickDebugReadback, 0, 0,
               bgfx::getTexture(m_gpuPickDebugFrameBuffer));
    bgfx::touch(kViewGpuPickDebugBlit);
    m_gpuPickDebugReadPending = true;
    m_gpuPickDebugReadFrame = bgfx::readTexture(
        m_gpuPickDebugReadback, m_gpuPickDebugReadbackData.data());
}

void BgfxRenderer::completeGpuPickReadback()
{
    completeGpuPickDebugReadback();
    if (!m_gpuPickReadPending || m_frame <= m_gpuPickReadFrame)
        return;

    m_gpuPickReadPending = false;
    const uint32_t packed =
        uint32_t(m_gpuPickReadbackData[0]) |
        (uint32_t(m_gpuPickReadbackData[1]) << 8) |
        (uint32_t(m_gpuPickReadbackData[2]) << 16) |
        (uint32_t(m_gpuPickReadbackData[3]) << 24);
    m_gpuPickLastResult.ready = true;
    m_gpuPickLastResult.hit = packed != 0xffffffffu && packed != 0;
    m_gpuPickLastResult.objectId = m_gpuPickLastResult.hit ? packed : 0;
    m_gpuPickLastResult.faceIndex = 0;
    m_gpuPickActive = false;
}

void BgfxRenderer::completeGpuPickDebugReadback()
{
    if (!m_gpuPickDebugReadPending || m_frame <= m_gpuPickDebugReadFrame)
        return;

    m_gpuPickDebugReadPending = false;
    const uint16_t width = std::max<uint16_t>(1, m_gpuPickDebugWidth);
    const uint16_t height = std::max<uint16_t>(1, m_gpuPickDebugHeight);
    const size_t pixelCount = size_t(width) * height;
    if (m_gpuPickDebugReadbackData.size() != pixelCount * 4u ||
        !bgfx::isValid(m_gpuPickDebugVisualTexture))
    {
        return;
    }

    // Normalize the captured ID range before colorizing.  This makes the
    // debug texture show relative entity ordering instead of raw byte codes.
    uint32_t minId = std::numeric_limits<uint32_t>::max();
    uint32_t maxId = 0u;
    bool hasId = false;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        const size_t offset = pixel * 4u;
        const uint32_t packed =
            uint32_t(m_gpuPickDebugReadbackData[offset + 0]) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 1]) << 8) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 2]) << 16) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 3]) << 24);
        if (packed != 0u && packed != 0xffffffffu)
        {
            minId = std::min(minId, packed);
            maxId = std::max(maxId, packed);
            hasId = true;
        }
    }

    if (hasId)
    {
        m_gpuPickDebugMinId = minId;
        m_gpuPickDebugMaxId = maxId;
    }

    std::vector<uint8_t> visual(m_gpuPickDebugReadbackData.size(), 0u);
    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        const size_t offset = pixel * 4u;
        const uint32_t packed =
            uint32_t(m_gpuPickDebugReadbackData[offset + 0]) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 1]) << 8) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 2]) << 16) |
            (uint32_t(m_gpuPickDebugReadbackData[offset + 3]) << 24);

        uint8_t r = 255u;
        uint8_t g = 255u;
        uint8_t b = 255u;
        if (packed != 0u && packed != 0xffffffffu)
        {
            const float normalized = maxId > minId
                ? float(packed - minId) / float(maxId - minId)
                : 0.5f;
            const float clamped = std::clamp(normalized, 0.0f, 1.0f);
            r = uint8_t(std::clamp(255.0f * (0.15f + 0.85f * clamped), 0.0f, 255.0f));
            g = uint8_t(std::clamp(255.0f * (0.15f + 0.85f * (1.0f - clamped)), 0.0f, 255.0f));
            b = uint8_t(std::clamp(255.0f * (0.30f + 0.70f * (1.0f - std::abs(clamped * 2.0f - 1.0f))), 0.0f, 255.0f));
        }

        visual[offset + 0] = b;
        visual[offset + 1] = g;
        visual[offset + 2] = r;
        visual[offset + 3] = 255u;
    }

    const bgfx::Memory *memory = bgfx::copy(
        visual.data(), uint32_t(visual.size()));
    bgfx::updateTexture2D(m_gpuPickDebugVisualTexture, 0, 0, 0, 0,
                          width, height, memory, width * 4u);
}

void BgfxRenderer::drawTargetPoint(const TargetPointRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_pointProgram))
        return;

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const glm::vec4 clip = projection * data.view *
                           glm::vec4(data.relativePosition, 1.0f);
    if (!(clip.w > 0.0f))
        return;

    const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
    // The target is a screen-space quad standing in for GL_POINTS. The
    // perspective bias is applied in world units (half a pixel at the
    // point's depth), so it stays sub-pixel at every distance instead of
    // growing with the log-depth mapping at large world coordinates. Ortho
    // depth is linear and must match the reference exactly.
    const glm::vec4 viewPosition =
        data.view * glm::vec4(data.relativePosition, 1.0f);
    float depth = clip.z / clip.w;
    if (data.isOrtho <= 0.5f)
    {
        const float viewDepth = -viewPosition.z;
        const float biasWorld =
            std::max(0.5f * data.pixelSizeWorld, 0.01f);
        depth = normalizedLogDepth(
            std::max(viewDepth - biasWorld, data.logDepth.y), data.logDepth);
    }
    const glm::vec4 position(
        ndc,
        depth,
        1.0f);
    const glm::vec4 pointSize(data.pointSize * 2.0f / float(m_width),
                              data.pointSize * 2.0f / float(m_height),
                              0.0f, 0.0f);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
    bgfx::setUniform(m_primParams,
                     glm::value_ptr(glm::vec4(
                         m_renderMode.mode() == RenderMode::DepthBuffer ? 1.0f : 0.0f,
                         0.0f, 0.0f, 0.0f)));
    bgfx::setVertexBuffer(0, m_pointBuffer);
    bgfx::setUniform(m_pointPosition, glm::value_ptr(position));
    bgfx::setUniform(m_pointSize, glm::value_ptr(pointSize));
    bgfx::setUniform(m_pointColor,
                     glm::value_ptr(glm::vec4(data.color, 1.0f)));
    bgfx::submit(kViewOverlay, m_pointProgram);
}

bool BgfxRenderer::createRenderResources()
{
    const auto createShader = [](const uint8_t *data, uint32_t size,
                                 const char *name) {
        const bgfx::ShaderHandle handle = bgfx::createShader(bgfx::copy(data, size));
        if (bgfx::isValid(handle))
            bgfx::setName(handle, name);
        return handle;
    };

    const auto gridVertexBinary = SELECT_SHADER_BINARY(GridShaders, vertex);
    const auto gridFragmentBinary = SELECT_SHADER_BINARY(GridShaders, frag);
    const bgfx::ShaderHandle gridVertex = createShader(
        gridVertexBinary.data, gridVertexBinary.size, "grid_vs");
    const bgfx::ShaderHandle gridFragment = createShader(
        gridFragmentBinary.data, gridFragmentBinary.size, "grid_fs");
    m_gridProgram = bgfx::createProgram(gridVertex, gridFragment, true);

    const auto cubeVertexBinary = SELECT_SHADER_BINARY(CubeShaders, vertex);
    const auto cubeFragmentBinary = SELECT_SHADER_BINARY(CubeShaders, frag);
    const bgfx::ShaderHandle cubeVertex = createShader(
        cubeVertexBinary.data, cubeVertexBinary.size, "cube_vs");
    const bgfx::ShaderHandle cubeFragment = createShader(
        cubeFragmentBinary.data, cubeFragmentBinary.size, "cube_fs");
    m_cubeProgram = bgfx::createProgram(cubeVertex, cubeFragment, true);

    const auto meshInstanceVertexBinary =
        SELECT_SHADER_BINARY(MeshInstanceShaders, vertex);
    const auto meshInstanceFragmentBinary =
        SELECT_SHADER_BINARY(MeshInstanceShaders, frag);
    const bgfx::ShaderHandle meshInstanceVertex = createShader(
        meshInstanceVertexBinary.data, meshInstanceVertexBinary.size,
        "mesh_instance_vs");
    const bgfx::ShaderHandle meshInstanceFragment = createShader(
        meshInstanceFragmentBinary.data, meshInstanceFragmentBinary.size,
        "mesh_instance_fs");
    m_meshInstanceProgram = bgfx::createProgram(
        meshInstanceVertex, meshInstanceFragment, true);

    const auto pbrMeshVertexBinary =
        SELECT_SHADER_BINARY(RealisticMeshShaders, vs_realistic_mesh);
    const auto pbrMeshFragmentBinary =
        SELECT_SHADER_BINARY(RealisticMeshShaders, fs_pbr_mesh);
    const bgfx::ShaderHandle pbrMeshVertex = createShader(
        pbrMeshVertexBinary.data, pbrMeshVertexBinary.size,
        "realistic_mesh_vs");
    const bgfx::ShaderHandle pbrMeshFragment = createShader(
        pbrMeshFragmentBinary.data, pbrMeshFragmentBinary.size,
        "realistic_mesh_fs");
    m_pbrMeshProgram = bgfx::createProgram(pbrMeshVertex, pbrMeshFragment, true);

    const auto pointVertexBinary = SELECT_SHADER_BINARY(PointShaders, vertex);
    const auto pointFragmentBinary = SELECT_SHADER_BINARY(PointShaders, frag);
    const bgfx::ShaderHandle pointVertex = createShader(
        pointVertexBinary.data, pointVertexBinary.size, "point_vs");
    const bgfx::ShaderHandle pointFragment = createShader(
        pointFragmentBinary.data, pointFragmentBinary.size, "point_fs");
    m_pointProgram = bgfx::createProgram(pointVertex, pointFragment, true);

    const auto pointInstanceVertexBinary =
        SELECT_SHADER_BINARY(PointInstanceShaders, vertex);
    const auto pointInstanceFragmentBinary =
        SELECT_SHADER_BINARY(PointInstanceShaders, frag);
    const bgfx::ShaderHandle pointInstanceVertex = createShader(
        pointInstanceVertexBinary.data, pointInstanceVertexBinary.size,
        "point_instance_vs");
    const bgfx::ShaderHandle pointInstanceFragment = createShader(
        pointInstanceFragmentBinary.data, pointInstanceFragmentBinary.size,
        "point_instance_fs");
    m_pointInstanceProgram = bgfx::createProgram(
        pointInstanceVertex, pointInstanceFragment, true);

    const auto cadAlgorithmVertexBinary =
        SELECT_SHADER_BINARY(CadAlgorithmShaders, vs_cad_instance);
    const auto cadAlgorithmFragmentBinary =
        SELECT_SHADER_BINARY(CadAlgorithmShaders, fs_cad_styles);
    const bgfx::ShaderHandle cadAlgorithmVertex = createShader(
        cadAlgorithmVertexBinary.data, cadAlgorithmVertexBinary.size,
        "cad_algorithm_vs");
    const bgfx::ShaderHandle cadAlgorithmFragment = createShader(
        cadAlgorithmFragmentBinary.data, cadAlgorithmFragmentBinary.size,
        "cad_algorithm_fs");
    m_cadAlgorithmProgram = bgfx::createProgram(
        cadAlgorithmVertex, cadAlgorithmFragment, true);

    const auto polylineVertexBinary =
        SELECT_SHADER_BINARY(PrimPolylineShaders, vs_polyline);
    const auto polylineFragmentBinary =
        SELECT_SHADER_BINARY(PrimPolylineShaders, fs_polyline);
    const bgfx::ShaderHandle polylineVertex = createShader(
        polylineVertexBinary.data, polylineVertexBinary.size,
        "prim_polyline_vs");
    const bgfx::ShaderHandle polylineFragment = createShader(
        polylineFragmentBinary.data, polylineFragmentBinary.size,
        "prim_polyline_fs");
    m_polylineProgram = bgfx::createProgram(polylineVertex, polylineFragment, true);

    const auto lineInstanceVertexBinary =
        SELECT_SHADER_BINARY(LineInstanceShaders, vs_line_instance);
    const bgfx::ShaderHandle lineInstanceVertex = createShader(
        lineInstanceVertexBinary.data, lineInstanceVertexBinary.size,
        "line_instance_vs");
    m_lineInstanceProgram = bgfx::createProgram(
        lineInstanceVertex, polylineFragment, true);
    const auto meshEdgeRibbonVertexBinary =
        SELECT_SHADER_BINARY(MeshEdgeRibbonShaders, vs_mesh_edge_ribbon);
    const bgfx::ShaderHandle meshEdgeRibbonVertex = createShader(
        meshEdgeRibbonVertexBinary.data, meshEdgeRibbonVertexBinary.size,
        "mesh_edge_ribbon_vs");
    m_meshEdgeRibbonProgram = bgfx::createProgram(
        meshEdgeRibbonVertex, polylineFragment, true);

    const auto presentVertexBinary =
        SELECT_SHADER_BINARY(PresentShaders, vs_present);
    const auto presentFragmentBinary =
        SELECT_SHADER_BINARY(PresentShaders, fs_present);
    const bgfx::ShaderHandle presentVertex = createShader(
        presentVertexBinary.data, presentVertexBinary.size, "cad_present_vs");
    const bgfx::ShaderHandle presentFragment = createShader(
        presentFragmentBinary.data, presentFragmentBinary.size, "cad_present_fs");
    m_presentProgram = bgfx::createProgram(presentVertex, presentFragment, true);

    // The generated header uses the source filename, not a manual
    // "_fragment" suffix. selectShaderBinary() already handles Vulkan vs.
    // the other backends.
    const auto selectionFragmentBinary =
        SELECT_SHADER_BINARY(SelectionOutlineShaders, fs_outline_overlay);
    const bgfx::ShaderHandle selectionFragment = createShader(
        selectionFragmentBinary.data, selectionFragmentBinary.size,
        "selection_outline_fs");
    const bgfx::ShaderHandle selectionVertex = createShader(
        presentVertexBinary.data, presentVertexBinary.size,
        "selection_outline_vs");
    m_selectionOutlineProgram = bgfx::createProgram(
        selectionVertex, selectionFragment, true);
    const auto fillVertexBinary =
        SELECT_SHADER_BINARY(PrimFilledShaders, vs_filled);
    const auto fillFragmentBinary =
        SELECT_SHADER_BINARY(PrimFilledShaders, fs_filled);
    const bgfx::ShaderHandle fillVertex = createShader(
        fillVertexBinary.data, fillVertexBinary.size, "prim_fill_vs");
    const bgfx::ShaderHandle fillFragment = createShader(
        fillFragmentBinary.data, fillFragmentBinary.size, "prim_fill_fs");
    m_fillProgram = bgfx::createProgram(fillVertex, fillFragment, true);

    const auto curveVertexBinary =
        SELECT_SHADER_BINARY(PrimCurveShaders, vs_curve);
    const auto curveFragmentBinary =
        SELECT_SHADER_BINARY(PrimCurveShaders, fs_curve);
    const bgfx::ShaderHandle curveVertex = createShader(
        curveVertexBinary.data, curveVertexBinary.size, "cad_curve_vs");
    const bgfx::ShaderHandle curveFragment = createShader(
        curveFragmentBinary.data, curveFragmentBinary.size, "cad_curve_fs");
    m_curveProgram = bgfx::createProgram(curveVertex, curveFragment, true);
    createGpuPickResources();
    m_polylineLayout.begin()
    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
    .end();
    m_lineInstanceLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .end();
    constexpr std::array<float, 12> lineQuad{
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };
    m_lineInstanceQuadBuffer = bgfx::createVertexBuffer(
        bgfx::copy(lineQuad.data(), sizeof(lineQuad)), m_lineInstanceLayout);
m_fillLayout.begin()
    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
    .end();
m_curveLayout.begin()
    .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
    .end();


    bool ready = bgfx::isValid(m_gridProgram) &&
                 bgfx::isValid(m_meshInstanceProgram) &&
                 bgfx::isValid(m_pbrMeshProgram) &&
                 bgfx::isValid(m_pointInstanceProgram) &&
                 bgfx::isValid(m_cadAlgorithmProgram) &&
                 bgfx::isValid(m_polylineProgram) &&
                 bgfx::isValid(m_lineInstanceProgram) &&
                 bgfx::isValid(m_meshEdgeRibbonProgram) &&
                 bgfx::isValid(m_fillProgram) &&
                 bgfx::isValid(m_curveProgram) &&
                 bgfx::isValid(m_cubeProgram) &&
                bgfx::isValid(m_pointProgram);
    if (!ready)
        std::cerr << "Failed to create one or more bgfx shader programs." << std::endl;
    if (!bgfx::isValid(m_gridProgram)) std::cerr << "Invalid program: grid" << std::endl;
    if (!bgfx::isValid(m_meshInstanceProgram)) std::cerr << "Invalid program: mesh instance" << std::endl;
    if (!bgfx::isValid(m_pbrMeshProgram)) std::cerr << "Invalid program: PBR mesh" << std::endl;
    if (!bgfx::isValid(m_pointInstanceProgram)) std::cerr << "Invalid program: point instance" << std::endl;
    if (!bgfx::isValid(m_cadAlgorithmProgram)) std::cerr << "Invalid program: CAD algorithm" << std::endl;
    if (!bgfx::isValid(m_polylineProgram)) std::cerr << "Invalid program: polyline" << std::endl;
    if (!bgfx::isValid(m_lineInstanceProgram)) std::cerr << "Invalid program: line instance" << std::endl;
    if (!bgfx::isValid(m_fillProgram)) std::cerr << "Invalid program: fill" << std::endl;
    if (!bgfx::isValid(m_curveProgram)) std::cerr << "Invalid program: curve" << std::endl;
    if (!bgfx::isValid(m_cubeProgram)) std::cerr << "Invalid program: cube" << std::endl;
    if (!bgfx::isValid(m_pointProgram)) std::cerr << "Invalid program: point" << std::endl;

    if (ready)
    {
        m_gridInvViewProj = createUniformHandle("uInvViewProj", bgfx::UniformType::Mat4);
        m_gridViewProj = createUniformHandle("uViewProj", bgfx::UniformType::Mat4);
        m_gridCamFront = createUniformHandle("uCamFront", bgfx::UniformType::Vec4);
        m_gridOrthoPlaneCenter = createUniformHandle("uOrthoPlaneCenter", bgfx::UniformType::Vec4);
        m_gridOrthoRight = createUniformHandle("uOrthoRight", bgfx::UniformType::Vec4);
        m_gridOrthoUp = createUniformHandle("uOrthoUp", bgfx::UniformType::Vec4);
        m_gridOriginRelative = createUniformHandle("uOriginRelative", bgfx::UniformType::Vec4);
        m_gridPlaneNormal = createUniformHandle("uPlaneNormal", bgfx::UniformType::Vec4);
        m_gridPlaneTangentU = createUniformHandle("uPlaneTangentU", bgfx::UniformType::Vec4);
        m_gridPlaneTangentV = createUniformHandle("uPlaneTangentV", bgfx::UniformType::Vec4);
        m_gridAxisColorU = createUniformHandle("uAxisColorU", bgfx::UniformType::Vec4);
        m_gridAxisColorV = createUniformHandle("uAxisColorV", bgfx::UniformType::Vec4);
        m_gridStartAxisOrigin = createUniformHandle("uStartAxisOrigin", bgfx::UniformType::Vec4);
        m_gridStartAxisDirection = createUniformHandle("uStartAxisDirection", bgfx::UniformType::Vec4);
        m_gridStartAxisVisible = createUniformHandle("uStartAxisVisible", bgfx::UniformType::Vec4);
        m_gridStartAxisLine = createUniformHandle("uStartAxisLine", bgfx::UniformType::Vec4);
        m_gridAxisOriginGridRelative = createUniformHandle(
            "uAxisOriginGridRelative", bgfx::UniformType::Vec4);
        m_gridAxisLineX = createUniformHandle("uAxisLineX", bgfx::UniformType::Vec4);
        m_gridAxisLineZ = createUniformHandle("uAxisLineZ", bgfx::UniformType::Vec4);
        m_gridIsOrtho = createUniformHandle("uIsOrtho", bgfx::UniformType::Vec4);
        m_gridGroundRelativeY = createUniformHandle("uGroundRelativeY", bgfx::UniformType::Vec4);
        m_gridStep = createUniformHandle("uStep", bgfx::UniformType::Vec4);
        m_gridAxisVisible = createUniformHandle("uAxisVisible", bgfx::UniformType::Vec4);
        m_gridScreenHeight = createUniformHandle("uScreenHeight", bgfx::UniformType::Vec4);
        m_gridScreenWidth = createUniformHandle("uScreenWidth", bgfx::UniformType::Vec4);
        m_gridColorMajor = createUniformHandle("uGridColorMajor", bgfx::UniformType::Vec4);
        m_gridColorMinor = createUniformHandle("uGridColorMinor", bgfx::UniformType::Vec4);
        m_gridOpacity = createUniformHandle("uGridOpacity", bgfx::UniformType::Vec4);
        m_gridOrthoPlaneValid = createUniformHandle("uOrthoPlaneValid", bgfx::UniformType::Vec4);
        m_logDepth = createUniformHandle("uLogDepth", bgfx::UniformType::Vec4);
        m_layerOffset = createUniformHandle("uLayerOffset", bgfx::UniformType::Vec4);
        m_view = createUniformHandle("uView", bgfx::UniformType::Mat4);
        m_projection = createUniformHandle("projection", bgfx::UniformType::Mat4);
        m_meshEdgeOverride = createUniformHandle("uEdgeOverride", bgfx::UniformType::Vec4);
        m_meshEdgeRibbonParams = createUniformHandle("uEdgeRibbon", bgfx::UniformType::Vec4);
        m_cubeRelativePosition = createUniformHandle("uModelRelativePosition", bgfx::UniformType::Vec4);
        m_cubeRelativePositionLow = createUniformHandle("uModelRelativePositionLow", bgfx::UniformType::Vec4);
        m_eyeHigh = createUniformHandle("uEyeHigh", bgfx::UniformType::Vec4);
        m_eyeLow = createUniformHandle("uEyeLow", bgfx::UniformType::Vec4);
        m_cubeOpacity = createUniformHandle("uCubeOpacity", bgfx::UniformType::Vec4);
        m_cubeColor = createUniformHandle("uObjectColor", bgfx::UniformType::Vec4);
        m_pointPosition = createUniformHandle("uRelativePosition", bgfx::UniformType::Vec4);
        m_pointSize = createUniformHandle("uPointSize", bgfx::UniformType::Vec4);
        m_pointColor = createUniformHandle("uColor", bgfx::UniformType::Vec4);
        // Uniform names must exactly match the names declared in the CAD
        // shader sources; bgfx binds program uniforms by name.
        m_cadView = createUniformHandle("u_cadView", bgfx::UniformType::Mat4);
        m_cadProjection = createUniformHandle("u_cadProjection", bgfx::UniformType::Mat4);
        m_cadCameraPos = createUniformHandle("u_cameraPos", bgfx::UniformType::Vec4);
        m_cadBaseColor = createUniformHandle("u_baseColor", bgfx::UniformType::Vec4);
        m_cadLightDir = createUniformHandle("u_lightDir", bgfx::UniformType::Vec4);
        m_cadStyleParams = createUniformHandle("u_styleParams", bgfx::UniformType::Vec4);
        m_cadWireframeColor = createUniformHandle("u_wireframeColor", bgfx::UniformType::Vec4);
        m_cadStrokeParams = createUniformHandle("u_strokeParams", bgfx::UniformType::Vec4);
        m_cadFlatShade = createUniformHandle("u_flatShade", bgfx::UniformType::Vec4);
        m_presentSampler = createUniformHandle("s_texColor", bgfx::UniformType::Sampler);
        m_presentParams = createUniformHandle("uPresentParams", bgfx::UniformType::Vec4);
        m_selectionOutlineParams = createUniformHandle(
            "u_outline_params", bgfx::UniformType::Vec4);
        m_selectionOutlineColor = createUniformHandle(
            "u_outline_color", bgfx::UniformType::Vec4);
        m_primParams = createUniformHandle("uPrimParams", bgfx::UniformType::Vec4);
        m_depthDisplayParams =
            createUniformHandle("uDepthDisplay", bgfx::UniformType::Vec4);
        m_curveCP = createUniformHandle("uCurveCP", bgfx::UniformType::Vec4, 16);
        m_curveKnot = createUniformHandle("uCurveKnot", bgfx::UniformType::Vec4, 4);
        m_curveParams = createUniformHandle("uCurveParams", bgfx::UniformType::Vec4);
        m_curveColor = createUniformHandle("uCurveColor", bgfx::UniformType::Vec4);
        m_curveArc = createUniformHandle("uArc", bgfx::UniformType::Vec4);
        m_meshSurface = createUniformHandle("uMeshSurface", bgfx::UniformType::Vec4);
        m_albedoSampler = createUniformHandle("s_albedo", bgfx::UniformType::Sampler);
        m_realisticMaterial = createUniformHandle("u_material", bgfx::UniformType::Vec4);
        m_rAmbient = createUniformHandle("u_rAmbient", bgfx::UniformType::Vec4);
        m_rDirection = createUniformHandle("u_rDirection", bgfx::UniformType::Vec4);
        m_rDirectionColor = createUniformHandle("u_rDirectionColor", bgfx::UniformType::Vec4);
        m_rPointPositions = createUniformHandle(
            "u_rPointPositions", bgfx::UniformType::Vec4, 4);
        m_rPointColors = createUniformHandle(
            "u_rPointColors", bgfx::UniformType::Vec4, 4);
        m_rParams = createUniformHandle("u_rParams", bgfx::UniformType::Vec4);

        ready = bgfx::isValid(m_gridInvViewProj) &&
                bgfx::isValid(m_gridViewProj) &&
                bgfx::isValid(m_gridCamFront) &&
                bgfx::isValid(m_gridOrthoPlaneCenter) &&
                bgfx::isValid(m_gridOrthoRight) &&
                bgfx::isValid(m_gridOrthoUp) &&
                bgfx::isValid(m_gridOriginRelative) &&
                bgfx::isValid(m_gridPlaneNormal) &&
                bgfx::isValid(m_gridPlaneTangentU) &&
                bgfx::isValid(m_gridPlaneTangentV) &&
                bgfx::isValid(m_gridAxisColorU) &&
                bgfx::isValid(m_gridAxisColorV) &&
                bgfx::isValid(m_gridStartAxisOrigin) &&
                bgfx::isValid(m_gridStartAxisDirection) &&
                bgfx::isValid(m_gridStartAxisVisible) &&
                bgfx::isValid(m_gridStartAxisLine) &&
                bgfx::isValid(m_gridAxisOriginGridRelative) &&
                bgfx::isValid(m_gridAxisLineX) &&
                bgfx::isValid(m_gridAxisLineZ) &&
                bgfx::isValid(m_gridIsOrtho) &&
                bgfx::isValid(m_gridGroundRelativeY) &&
                bgfx::isValid(m_gridStep) &&
                bgfx::isValid(m_gridAxisVisible) &&
                bgfx::isValid(m_gridScreenHeight) &&
                bgfx::isValid(m_gridScreenWidth) &&
                bgfx::isValid(m_gridColorMajor) &&
                bgfx::isValid(m_gridColorMinor) &&
                bgfx::isValid(m_gridOpacity) &&
                bgfx::isValid(m_gridOrthoPlaneValid) &&
                bgfx::isValid(m_logDepth) &&
                bgfx::isValid(m_layerOffset) &&
                bgfx::isValid(m_view) && bgfx::isValid(m_projection) &&
        bgfx::isValid(m_meshEdgeOverride) &&
                bgfx::isValid(m_cubeRelativePosition) &&
                bgfx::isValid(m_cubeRelativePositionLow) &&
                bgfx::isValid(m_eyeHigh) && bgfx::isValid(m_eyeLow) &&
                bgfx::isValid(m_cubeOpacity) && bgfx::isValid(m_cubeColor) &&
                bgfx::isValid(m_pointPosition) && bgfx::isValid(m_pointSize) &&
                bgfx::isValid(m_pointColor) &&
                bgfx::isValid(m_cadView) &&
                bgfx::isValid(m_cadProjection) &&
                bgfx::isValid(m_cadCameraPos) &&
                bgfx::isValid(m_cadBaseColor) &&
                bgfx::isValid(m_cadLightDir) &&
                bgfx::isValid(m_cadStyleParams) &&
                bgfx::isValid(m_cadWireframeColor) &&
                bgfx::isValid(m_cadStrokeParams) &&
                bgfx::isValid(m_cadFlatShade) &&
                bgfx::isValid(m_presentSampler) &&
                bgfx::isValid(m_primParams) &&
                bgfx::isValid(m_presentParams) &&
                bgfx::isValid(m_selectionOutlineParams) &&
                bgfx::isValid(m_selectionOutlineColor) &&
                bgfx::isValid(m_meshSurface) &&
                bgfx::isValid(m_albedoSampler);
        ready = ready && bgfx::isValid(m_curveCP) &&
                bgfx::isValid(m_curveKnot) &&
                bgfx::isValid(m_curveParams) &&
                bgfx::isValid(m_curveColor) &&
                bgfx::isValid(m_curveArc);
        ready = ready && bgfx::isValid(m_realisticMaterial) &&
                bgfx::isValid(m_rAmbient) &&
                bgfx::isValid(m_rDirection) &&
                bgfx::isValid(m_rDirectionColor) &&
                bgfx::isValid(m_rPointPositions) &&
                bgfx::isValid(m_rPointColors) &&
                bgfx::isValid(m_rParams);
    }

    if (ready)
    {


        bgfx::VertexLayout cubeLayout;
        cubeLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .end();
        const std::array<CubeVertex, 36> cubeVertices = makeCubeVertices();
        m_sphereBuffer = createMeshBuffer(proceduralMeshVertices(MeshType::Sphere));
        m_coneBuffer = createMeshBuffer(proceduralMeshVertices(MeshType::Cone));
        m_torusBuffer = createMeshBuffer(proceduralMeshVertices(MeshType::Torus));
        m_instanceCubeBuffer =
            createMeshBuffer(proceduralMeshVertices(MeshType::Cube));
        m_cubeBuffer = bgfx::createVertexBuffer(
            bgfx::copy(cubeVertices.data(), sizeof(cubeVertices)), cubeLayout);
        constexpr std::array<uint8_t, 4> whitePixel{255, 255, 255, 255};
        m_whiteTexture = bgfx::createTexture2D(
            1, 1, false, 1, bgfx::TextureFormat::BGRA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(whitePixel.data(), sizeof(whitePixel)));
        m_cadCubeBuffer = createCadCubeBuffer(cubeVertices);
        m_cadSphereBuffer =
            createCadMeshBuffer(proceduralMeshVertices(MeshType::Sphere));
        m_cadConeBuffer =
            createCadMeshBuffer(proceduralMeshVertices(MeshType::Cone));
        m_cadTorusBuffer =
            createCadMeshBuffer(proceduralMeshVertices(MeshType::Torus));
        m_cubeEdgeBuffer = createFeatureEdgeLineBuffer(makeCubeFeatureEdges());
        m_sphereEdgeBuffer = createFeatureEdgeLineBuffer(makeSphereFeatureEdges());
        m_coneEdgeBuffer = createFeatureEdgeLineBuffer(makeConeFeatureEdges());
        m_torusEdgeBuffer = createFeatureEdgeLineBuffer(makeTorusFeatureEdges());
        m_cubeEdgeRibbonBuffer = createEdgeRibbonBuffer(makeEdgeSegments(rendering::MeshType::Cube));
        m_sphereEdgeRibbonBuffer = createEdgeRibbonBuffer(makeEdgeSegments(rendering::MeshType::Sphere));
        m_coneEdgeRibbonBuffer = createEdgeRibbonBuffer(makeEdgeSegments(rendering::MeshType::Cone));
        m_torusEdgeRibbonBuffer = createEdgeRibbonBuffer(makeEdgeSegments(rendering::MeshType::Torus));
        const std::array<CubeVertex, 24> aabbVertices = makeCubeEdgeVertices();
        m_aabbBuffer = bgfx::createVertexBuffer(
            bgfx::copy(aabbVertices.data(), sizeof(aabbVertices)), cubeLayout);

        bgfx::VertexLayout pointLayout;
        pointLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
        const std::array<float, 6> pointDisc{-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
        m_pointBuffer = bgfx::createVertexBuffer(
            bgfx::copy(pointDisc.data(), sizeof(pointDisc)), pointLayout);

        bgfx::VertexLayout presentLayout;
        presentLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        constexpr std::array<float, 24> presentQuad{
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
        };
        m_presentQuadBuffer = bgfx::createVertexBuffer(
            bgfx::copy(presentQuad.data(), sizeof(presentQuad)), presentLayout);




        ready = bgfx::isValid(m_cubeBuffer) &&
                bgfx::isValid(m_cadCubeBuffer) &&
                bgfx::isValid(m_cadSphereBuffer) &&
                bgfx::isValid(m_cadConeBuffer) &&
                bgfx::isValid(m_cadTorusBuffer) &&
                bgfx::isValid(m_cubeEdgeBuffer) &&
                bgfx::isValid(m_sphereEdgeBuffer) &&
                bgfx::isValid(m_coneEdgeBuffer) &&
                bgfx::isValid(m_torusEdgeBuffer) &&
                bgfx::isValid(m_cubeEdgeRibbonBuffer) &&
                bgfx::isValid(m_sphereEdgeRibbonBuffer) &&
                bgfx::isValid(m_coneEdgeRibbonBuffer) &&
                bgfx::isValid(m_torusEdgeRibbonBuffer) &&
                bgfx::isValid(m_instanceCubeBuffer) &&
                bgfx::isValid(m_whiteTexture) &&
                bgfx::isValid(m_aabbBuffer) &&
                bgfx::isValid(m_pointBuffer) &&
                bgfx::isValid(m_presentQuadBuffer) &&
                bgfx::isValid(m_presentProgram) &&
                bgfx::isValid(m_selectionOutlineProgram) &&
                bgfx::isValid(m_gpuPickProgram) &&
                createSceneFrameBuffer();
        if (!ready)
            std::cerr << "Failed to create bgfx vertex buffers." << std::endl;
    }

    return ready;
}

uint32_t BgfxRenderer::uploadGlyphSdf(const unsigned char *sdf, int width,
                                      int height)
{
    if (!m_initialized || !sdf || width <= 0 || height <= 0)
        return 0;

    if (!bgfx::isValid(m_textProgram))
    {
        const auto vertexBinary =
            SELECT_SHADER_BINARY(TextSdfShaders, vs_text_sdf);
        const auto fragmentBinary =
            SELECT_SHADER_BINARY(TextSdfShaders, fs_text_sdf);
        auto createTextShader = [](const uint8_t *data, uint32_t size,
                                   const char *name) {
            const bgfx::ShaderHandle handle =
                bgfx::createShader(bgfx::copy(data, size));
            if (bgfx::isValid(handle))
                bgfx::setName(handle, name);
            return handle;
        };
        const bgfx::ShaderHandle vertexShader = createTextShader(
            vertexBinary.data, vertexBinary.size, "text_sdf_vs");
        const bgfx::ShaderHandle fragmentShader = createTextShader(
            fragmentBinary.data, fragmentBinary.size, "text_sdf_fs");
        m_textProgram =
            bgfx::createProgram(vertexShader, fragmentShader, true);
        m_textSampler =
            createUniformHandle("s_texColor", bgfx::UniformType::Sampler);
        m_textParams =
            createUniformHandle("u_params0", bgfx::UniformType::Vec4);
        m_textLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
            .end();
    }
    if (!bgfx::isValid(m_textProgram))
        return 0;

    // R8 single-channel distance field, clamp sampling.
    const bgfx::TextureHandle texture = bgfx::createTexture2D(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), false,
        1, bgfx::TextureFormat::R8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(sdf, static_cast<uint32_t>(width * height)));
    if (!bgfx::isValid(texture))
        return 0;
    m_glyphTextures.push_back(texture);
    return uint32_t(m_glyphTextures.size()); // 1-based id
}

void BgfxRenderer::drawSdfGlyphQuad(const glm::mat4 &view,
                                    const glm::mat4 &projection,
                                    uint32_t textureId,
                                    const float *vertices)
{
    if (!m_initialized || !bgfx::isValid(m_textProgram) ||
        textureId == 0 || textureId > m_glyphTextures.size() || !vertices)
    {
        return;
    }

    bgfx::setViewFrameBuffer(kViewText, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(kViewText, 0, 0, m_width, m_height);
    bgfx::setViewClear(kViewText, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    // D3D-family backends expect NDC depth in [0, 1]; without this remap
    // the near half of the ortho slab clips at a straight line (text
    // glyphs vanished below a horizontal screen line).
    const glm::mat4 textProjection = projectionForDirect3D(projection);
    bgfx::setViewTransform(kViewText, glm::value_ptr(view),
                           glm::value_ptr(textProjection));

    bgfx::TransientVertexBuffer tvb;
    if (6 != bgfx::getAvailTransientVertexBuffer(6, m_textLayout))
        return;
    bgfx::allocTransientVertexBuffer(&tvb, 6, m_textLayout);
    std::memcpy(tvb.data, vertices, size_t(6) * sizeof(float) * 9);

    // Overlay pass: backbuffer depth is not the scene depth, so text
    // draws without a depth test after the present view.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setTexture(0, m_textSampler,
                     m_glyphTextures[textureId - 1]);
    const float params[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // textureLod 0
    bgfx::setUniform(m_textParams, params);
    bgfx::submit(kViewText, m_textProgram);
}

} // namespace rendering
