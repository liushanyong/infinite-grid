#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "RenderMode.h"

namespace rendering
{

// Double-single encoding: high is the nearest f32 to the f64 value and low is
// the f64 remainder after high is subtracted.  The pair preserves enough
// precision for world-space translations at CAD/survey coordinates.
struct DoubleSingleVec3
{
    glm::vec3 high{0.0f};
    glm::vec3 low{0.0f};
};

inline DoubleSingleVec3 encodeDoubleSingle(const glm::dvec3 &value)
{
    DoubleSingleVec3 encoded;
    encoded.high = glm::vec3(value);
    encoded.low = glm::vec3(value - glm::dvec3(encoded.high));
    return encoded;
}


enum class BackendType
{
    Bgfx,
    WebGpu,
    WebGpuMigration,
};

enum class GraphicsApi
{
    Auto,
    Direct3D11,
    Direct3D12,
    WebGPU,
    OpenGL,
    Vulkan,
};

struct GridRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 invViewProj;
    glm::mat4 viewProj;
    glm::vec3 camFront;
    glm::vec3 orthoPlaneCenter;
    glm::vec3 orthoRight;
    glm::vec3 orthoUp;
    glm::vec3 planeOriginRelative;
    float plane;
    glm::vec3 planeNormal;
    glm::vec3 planeTangentU;
    glm::vec3 planeTangentV;
    glm::vec3 axisColorU;
    glm::vec3 axisColorV;
    glm::vec3 startAxisOrigin;
    glm::vec3 startAxisDirection;
    float startAxisVisible;
    glm::vec3 startAxisLine;
    glm::vec2 axisOriginGridRelative;
    glm::vec3 axisLineX;
    glm::vec3 axisLineZ;
    float orthoPlaneValid;
    float groundRelativeY;
    float isOrtho;
    float step;
    glm::vec2 axisVisible;
    float screenHeight;
    float screenWidth;
    glm::vec3 gridColorMajor;
    glm::vec3 gridColorMinor;
    float gridOpacity;
    glm::vec4 logDepth;
};

enum class MeshType
{
    Cube,
    Sphere,
    Cone,
    Torus,
};

struct CubeRenderData
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 modelRelativePosition;
    glm::vec3 objectColor;
    float opacity;
    // Rendering layer.  0 keeps every object in the single default
    // layer; values > 0 composite the object above lower layers.
    float layer = 0.0f;
    MeshType mesh = MeshType::Cube;
    glm::vec4 logDepth;
    DoubleSingleVec3 eye;
    DoubleSingleVec3 object;
};

struct AabbRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 relativeMin;
    glm::vec3 relativeMax;
    glm::vec3 color;
    float opacity;
    float layer = 0.0f;
    glm::vec4 logDepth;
    DoubleSingleVec3 eye;
    DoubleSingleVec3 object;
};

struct MeshInstance
{
    // Column-major 3x3 transform.  The .w components carry RGB because bgfx
    // exposes fewer instance attributes when the mesh uses packed UVs.
    glm::vec4 transformColumn0;
    glm::vec4 transformColumn1;
    glm::vec4 transformColumn2;

    // Double-single encoded translation; high.w is opacity.
    glm::vec4 positionHigh;
    glm::vec4 positionLow;
};

struct MeshInstancesRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    const MeshInstance *instances = nullptr;
    uint32_t instanceCount = 0;
    MeshType mesh = MeshType::Cube;
    bool opaque = false;
    float layer = 0.0f;
    glm::vec4 logDepth;
    DoubleSingleVec3 eye;
    uint32_t diffuseTextureIndex = 0;
    float headlight = 1.0f;
    float triplanarUv = 0.0f;
    bool realistic = false;
    glm::vec4 material = glm::vec4(0.0f, 0.35f, 0.0f, 0.5f);
};

enum class SurfaceAlgorithm
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

struct SurfaceMaterial
{
    SurfaceAlgorithm algorithm = SurfaceAlgorithm::Shaded;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 accentColor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.35f;
    float transparency = 0.0f;
    float lineWidth = 1.0f;
};

enum class CurveAlgorithm
{
    Bezier,
    BSpline,
    NURBS,
    Arc
};

struct CurveRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 logDepth;
    std::array<glm::vec4, 16> controlPoints{};
    std::array<glm::vec4, 4> knots{};
    glm::vec4 params{0.0f, 3.0f, 4.0f, 0.0f};
    glm::vec4 arc{1.0f, 0.0f, 6.2831853f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t sampleCount = 64;
    float layer = 0.0f;
};

struct RealisticLight
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius = 0.0f;
};

struct RealisticLightsRenderData
{
    glm::vec3 ambient{0.08f, 0.09f, 0.11f};
    glm::vec3 direction{0.4f, 0.8f, 0.55f};
    glm::vec3 directionColor{1.0f, 0.97f, 0.90f};
    float directionIntensity = 1.0f;
    std::array<RealisticLight, 4> pointLights;
    uint32_t pointLightCount = 0;
};

struct CadAlgorithmDemoRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    const MeshInstance *instances = nullptr;
    uint32_t instanceCount = 0;
    MeshType mesh = MeshType::Cube;
    RenderMode renderMode = RenderMode::Wireframe2D;
    glm::vec3 cameraPos;
    glm::vec3 lightDir;
    glm::vec3 baseColor;
    float metallic = 0.0f;
    float roughness = 0.35f;
    float transparency = 0.5f;
    float strokeWidth = 1.0f;
    float strokeDensity = 1.0f;
    float layer = 0.0f;
    glm::vec4 logDepth;
    DoubleSingleVec3 eye;
    SurfaceMaterial material;
};

// A one-pixel GPU id pass. The full-camera projection is kept here for
// conservative culling; the renderer derives the tiny pick frustum itself.
struct GpuPickRequest
{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    DoubleSingleVec3 eye;
    double ndcX = 0.0;
    double ndcY = 0.0;
    double nearDepth = 0.01;
    double farDepth = 1.0;
    glm::vec4 logDepth{0.0f};
};

struct GpuPickQueueStats
{
    size_t meshCapacity = 0;
    size_t triangleCapacity = 0;
    size_t droppedMeshes = 0;
    size_t droppedTriangles = 0;

    bool capacityExceeded() const
    {
        return droppedMeshes != 0 || droppedTriangles != 0;
    }
};

struct GpuPickResult
{
    bool ready = false;
    bool hit = false;
    uint32_t requestToken = 0;
    uint32_t objectId = 0;
    uint32_t faceIndex = 0;
};

// CAD vector primitives ported from CADplatformer's lines_pass: wide
// polylines with round joins/caps and edge anti-aliasing, plus arbitrary
// filled triangle soups. Vertices are already rebase-relative.
struct PrimVertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv; // x: along-ribbon parameter; y: across-ribbon in [0, 1]
};

struct FillVertex
{
    glm::vec3 position;
    glm::vec4 color;
};

struct PolylineRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    const PrimVertex *vertices = nullptr;
    uint32_t vertexCount = 0;
    glm::vec4 logDepth;
    float edgeSoftness = 0.15f; // ribbon units faded at the edges
    float layer = 0.0f;
};

struct FilledTrianglesRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    const FillVertex *vertices = nullptr;
    uint32_t vertexCount = 0;
    bool is3DFace = false;
    float layer = 0.0f;
    glm::vec4 logDepth;
    SurfaceMaterial material;
};

struct TargetPointInstance
{
    glm::vec3 relativePosition;
    glm::vec3 color;
    float pointSize = 0.0f;
};

struct TargetPointInstancesRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    const TargetPointInstance *instances = nullptr;
    uint32_t instanceCount = 0;
    float pointSize = 2.0f;
    float pixelSizeWorld = 0.0f;
    float isOrtho = 0.0f;
    glm::vec4 logDepth;
};

struct TargetPointRenderData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 relativePosition;
    float pointSize;
    // World units per screen pixel at this point (perspective).  Used for a
    // world-scale depth bias instead of a fixed normalized-depth epsilon.
    float pixelSizeWorld;
    glm::vec3 color;
    float isOrtho;
    glm::vec4 logDepth;
};

class RendererBackend
{
public:
    virtual ~RendererBackend() = default;

    virtual const char *name() const = 0;
    // The API that actually owns the swapchain. A migration backend can be
    // selected as WebGPU while reporting the native compatibility API it uses.
    virtual const char *graphicsApiName() const = 0;
    virtual Uint32 windowFlags() const = 0;
    virtual bool configureSDL() = 0;
    virtual bool initialize(SDL_Window *window) = 0;
    virtual void shutdown() = 0;
    virtual bool beginFrame(const glm::vec4 &clearColor) = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;
    virtual void drawGrid(const GridRenderData &data) = 0;
    virtual void drawCube(const CubeRenderData &data) = 0;
    virtual void drawMeshInstances(const MeshInstancesRenderData &data) = 0;
    virtual void drawAabb(const AabbRenderData &data) = 0;
    virtual void drawTargetPoint(const TargetPointRenderData &data) = 0;
    virtual void drawTargetPointInstances(const TargetPointInstancesRenderData &data) = 0;
    virtual void drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data) = 0;
    virtual void setRenderMode(RenderMode mode) = 0;
    virtual RenderMode renderMode() const = 0;
    virtual RenderModeFlags renderModeFlags() const = 0;
    virtual void drawPolylines(const PolylineRenderData &data) = 0;
    virtual void drawFilledTriangles(const FilledTrianglesRenderData &data) = 0;
    virtual void drawCurves(const CurveRenderData &data) {}
    virtual void requestDebugScreenShot(const std::string &) {}

    // Optional asynchronous mesh picking. Calls to queueGpuMeshPick are valid
    // only between requestGpuPick() and the next endFrame().
    // Returns the token for an accepted request, or zero while the previous
    // asynchronous readback is still pending.
    virtual uint32_t requestGpuPick(const GpuPickRequest &request) { return 0; }
    virtual void queueGpuMeshPick(const MeshInstance &instance,
                                  MeshType mesh, uint32_t objectId) {}
    virtual GpuPickQueueStats gpuPickQueueStats() const { return {}; }
    // Triangle vertices are expressed in the coordinate space represented by
    // view. A non-zero geometryKey marks reusable static geometry; transient
    // soups use zero and are copied for the pick request.
    // The fragment color is the entity ID; this lets lines, fills, and point
    // impostors share one small asynchronous ID submission.
    virtual void queueGpuTrianglePick(uint64_t geometryKey,
                                      const FillVertex *vertices,
                                      uint32_t vertexCount,
                                      const glm::mat4 &view,
                                      const glm::mat4 &projection,
                                      const glm::vec4 &logDepth,
                                      uint32_t objectId) {}
    virtual GpuPickResult pollGpuPick() { return {}; }

    // Zero is a renderer-provided white texture; other ids are allocated by
    // the active backend and remain valid until shutdown().
    virtual uint32_t loadMeshTexture(const std::string &path) { return 0; }
    virtual void setRealisticLights(const RealisticLightsRenderData &lights) {}
};

std::unique_ptr<RendererBackend> createRenderer(
    BackendType type, GraphicsApi api = GraphicsApi::Auto);

} // namespace rendering
