#include <SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "coordinate/WorldRebase.h"
#include "camera/orbit.h"
#include "rendering/RendererBackend.h"
#include "entities/tessellate.h"
#include "entities/world_draw.h"
#include "libredwg/include/dwg.h"
#include "entities/dwg_bridge.h"
#include "acgi/AcGiTextQueue.h"
#include "acgi/AcGiTextEngine.h"
#include "util/resource_path.h"
#include "acgi/AcGiLineType.h"
#include "scene/DrawContext.h"
#include "scene/SceneDrawList.h"
#include "rendering/ProceduralMesh.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <array>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>
#include <limits>
#include <chrono>
#include <functional>

#include "text/text_font.h"

namespace
{

constexpr int SCREEN_WIDTH = 1200;
constexpr int SCREEN_HEIGHT = 768;

bool &useOrthoProjection()
{
    static bool enabled = false;
    return enabled;
}

WorldRebase &worldRebase()
{
    static WorldRebase instance;
    return instance;
}

struct RequestedRenderer
{
  rendering::BackendType type = rendering::BackendType::Bgfx;
  rendering::GraphicsApi api = rendering::GraphicsApi::Vulkan;//Auto;//
};

RequestedRenderer resolveRequestedBackend()
{
  const char *backend = std::getenv("WINDOW_RENDERER");
  if (!backend)
    return {};

  const std::string_view backendName(backend);
  RequestedRenderer requested;
  if (backendName == "bgfx")
  {
    requested.api = rendering::GraphicsApi::Auto;
  }
  else if (backendName == "bgfx-d3d11" || backendName == "dx11")
  {
    requested.api = rendering::GraphicsApi::Direct3D11;
  }
  else if (backendName == "bgfx-d3d12" || backendName == "dx12")
  {
    requested.api = rendering::GraphicsApi::Direct3D12;
  }
  else if (backendName == "bgfx-opengl" || backendName == "opengl" ||
           backendName == "gl")
  {
    requested.api = rendering::GraphicsApi::OpenGL;
  }
  else if (backendName == "bgfx-vulkan" || backendName == "vulkan" ||
           backendName == "vk")
  {
    requested.api = rendering::GraphicsApi::Vulkan;
  }
  else if (backendName == "bgfx-webgpu" || backendName == "webgpu")
  {
    requested.type = rendering::BackendType::WebGpu;
    requested.api = rendering::GraphicsApi::WebGPU;
  }
  else
  {
    std::cerr << "Unknown WINDOW_RENDERER value '" << backendName
              << "'. Supported: bgfx, dx11, dx12, webgpu, gl, vk."
              << std::endl;
  }
  return requested;
}

std::unique_ptr<rendering::RendererBackend> rendererBackend;

constexpr glm::vec4 kClearColor(0.1f, 0.1f, 0.1f, 1.0f);

// Keep CAD content readable even when the authored color is close to the
// clear color.  A squared RGB distance of 0.04 corresponds to a 0.2 channel
// delta, which is enough to catch near-black/near-background overlays while
// avoiding needless color changes for clearly distinct hues.
static bool colorIsCloseToBackground(const glm::vec4 &color)
{
  const glm::vec3 delta = glm::vec3(color) - glm::vec3(kClearColor);
  return glm::dot(delta, delta) <= 0.04f;
}

static glm::vec4 contrastAgainstBackground(const glm::vec4 &color)
{
  if (!colorIsCloseToBackground(color))
    return color;
  return glm::vec4(1.0f - color.r, 1.0f - color.g, 1.0f - color.b,
                   color.a);
}

constexpr glm::vec4 kOutlineColor(1.0f, 0.55f, 0.05f, 1.0f);

// Global geometric outline expansion. For centered line ribbons this is the
// extra half-width added to each side, so the full width grows by two copies.
constexpr float kOutlineWidthPixels = 3.0f;

static float outlineWidthWorld(float pixelSizeWorld)
{
    return kOutlineWidthPixels * pixelSizeWorld;
}

static rendering::RenderModeManager visualStyleManager;

// Demo hook: set GRID_MESH_LAYER / GRID_FILL_LAYER to move demo objects onto
// explicit compositing layers (default 0 = everything in one layer).
static float envLayer(const char *name)
{
  const char *value = std::getenv(name);
  return value ? float(std::atof(value)) : 0.0f;
}

static bool debugValidationMeshesEnabled()
{
  const char *value = std::getenv("GRID_VALIDATION_MESHES");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

static bool demoMeshesEnabled()
{
  const char *value = std::getenv("GRID_DEMO_MESHES");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

static bool centerCubeForced()
{
  const char *value = std::getenv("GRID_CENTER_CUBE");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

static uint32_t gMeshTextureIndex = 0;

// Loaded font subsystem (SDF TTF + AutoCAD SHX), consolidated behind the
// AcGi text engine (see AcGiTextEngine.h).  Populated at startup.
static bool gSdfFontReady = false;
static bool gShxFontReady = false;

static float meshHeadlight()
{
  const char *value = std::getenv("GRID_MESH_HEADLIGHT");
  return value ? float(std::clamp(std::atof(value), 0.0, 1.0)) : 1.0f;
}
static float meshTriplanar()
{
  const char *value = std::getenv("GRID_MESH_TRIPLANAR");
  return value ? float(std::clamp(std::atof(value), 0.0, 1.0)) : 0.0f;
}

static bool realisticMeshEnabled()
{
  const char *value = std::getenv("GRID_REALISTIC");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

static rendering::SurfaceMaterial toSurfaceMaterial(
    const scene::AcGiMaterial &material)
{
  rendering::SurfaceMaterial result;
  result.algorithm = static_cast<rendering::SurfaceAlgorithm>(
      material.algorithm);
  result.baseColor = material.baseColor;
  result.accentColor = material.accentColor;
  result.metallic = material.metallic;
  result.roughness = material.roughness;
  result.transparency = material.transparency;
  result.lineWidth = material.lineWidth;
  return result;
}

static rendering::MeshInstance makeMeshInstance(
    float scale, const glm::vec3 &color, float opacity,
    const rendering::DoubleSingleVec3 &position,
    const glm::vec4 &material = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f))
{
  rendering::MeshInstance instance;
  instance.transformColumn0 = glm::vec4(scale, 0.0f, 0.0f, color.r);
  instance.transformColumn1 = glm::vec4(0.0f, scale, 0.0f, color.g);
  instance.transformColumn2 = glm::vec4(0.0f, 0.0f, scale, color.b);
  instance.positionHigh = glm::vec4(position.high, opacity);
  instance.positionLow = glm::vec4(position.low, material.r);
  return instance;
}
SDL_Window *window = nullptr;

// Every demo path funnels renderer submissions through this helper.  The
// protocol keeps instancing compact; only the submitter knows backend details.
void submitMeshBatch(scene::MeshBatchCommand &command,
                     const glm::mat4 &view, const glm::mat4 &projection,
                     const glm::vec4 &logDepth,
                     const rendering::DoubleSingleVec3 &eye,
                     float pixelSizeWorld = 0.0f,
                     float edgeSoftness = 0.15f)
{
  if (!rendererBackend || command.instances.empty())
    return;

  if (command.cadAlgorithm)
  {
    const rendering::CadAlgorithmDemoRenderData renderData{
        .view = view,
        .projection = projection,
        .instances = command.instances.data(),
        .instanceCount = static_cast<uint32_t>(command.instances.size()),
        .mesh = command.prototype,
        .renderMode = visualStyleManager.mode(),
        .cameraPos = eye.high + eye.low,
        .lightDir = glm::vec3(0.4f, 0.8f, 0.55f),
        .metallic = command.acgiMaterial.metallic,
        .roughness = command.acgiMaterial.roughness,
        .transparency = command.acgiMaterial.transparency,
        .strokeWidth = command.acgiMaterial.lineWidth,
        .strokeDensity = 1.0f,
        .layer = envLayer("GRID_MESH_LAYER"),
        .logDepth = logDepth,
        .eye = eye,
        .material = toSurfaceMaterial(command.acgiMaterial),
        .edgeHalfWidth = std::max(command.acgiMaterial.lineWidth * 0.5f,
                                  pixelSizeWorld),
        .edgeSoftness = edgeSoftness,
    };
    rendererBackend->drawCadAlgorithmDemo(renderData);
    return;
  }

  const rendering::MeshInstancesRenderData renderData{
      .view = view,
      .projection = projection,
      .instances = command.instances.data(),
      .instanceCount = static_cast<uint32_t>(command.instances.size()),
      .mesh = command.prototype,
      .opaque = command.opaque,
      .layer = envLayer("GRID_MESH_LAYER"),
      .logDepth = logDepth,
      .eye = eye,
      .diffuseTextureIndex = gMeshTextureIndex,
      .headlight = meshHeadlight(),
      .triplanarUv = meshTriplanar(),
      .realistic = command.realistic,
      .material = command.material,
      .edgeHalfWidth = std::max(command.acgiMaterial.lineWidth * 0.5f,
                                pixelSizeWorld),
      .edgeSoftness = edgeSoftness,
  };
  rendererBackend->drawMeshInstances(renderData);
}

// All world-space object positions stay double precision on the CPU.
// The GPU never sees these; only (objWorld - worldRebase().origin()) does.
glm::dvec3 cubeWorldPosition(0.0);

bool largeCoordinateCameraView = false;
bool frustumCaptureRequested = false;
bool frustumWireframeVisible = false;
bool targetPlaneConstraintEnabled = false;
glm::dvec3 frustumCorners[8];
bool gridVisibleQuadValid = false;
glm::dvec3 gridVisibleQuad[8];
int gridVisibleQuadCount = 0;
enum class GridPlaneType
{
    XZ,
    XY,
    YZ,
    Custom,
};

GridPlaneType gridPlane = GridPlaneType::XY;
glm::dvec3 gridPlaneOrigin(0.0);
glm::dvec3 gridPlaneNormal(0.0, 0.0, 1.0);
glm::dvec3 gridPlaneStartAxisOrigin(0.0);
glm::dvec3 gridPlaneStartAxisDirection(1.0, 0.0, 0.0);

// Keep the CPU visibility test, slab construction, and grid shader discard
// aligned.  A plane within five degrees of the view direction is hidden.
constexpr double kMinGridPlaneCos = 0.087155743; // sin(5 degrees)

// Edit these to define a non-axis-aligned infinite grid.  Key 4 activates it;
// R rebuilds them from the current camera frame.
glm::dvec3 customGridPlaneOrigin(1.0, 0.5, -0.5);
glm::dvec3 customGridPlaneNormal =
    glm::normalize(glm::dvec3(0.25, 1.0, 0.15));
glm::dvec3 customGridPlaneStartAxisOrigin(1.0, 0.5, -0.5);
glm::dvec3 customGridPlaneStartAxisDirection =
    glm::normalize(glm::dvec3(1.0, 0.0, 0.25));

void enforceTargetPlaneConstraint();

// Projection matrices are float32, while slab bounds are accumulated in
// double.  Convert in the outward direction so rounding can never move a
// near/far plane inside a bounds that was calculated to contain it.
float floatExpandOutward(double value, bool downward)
{
    constexpr float kNegativeInfinity = -std::numeric_limits<float>::infinity();
    constexpr float kPositiveInfinity = std::numeric_limits<float>::infinity();
    const float direction = downward ? kNegativeInfinity : kPositiveInfinity;
    float result = static_cast<float>(value);

    if ((downward && static_cast<double>(result) > value) ||
        (!downward && static_cast<double>(result) < value))
    {
        result = std::nextafterf(result, direction);
    }


    for (int i = 0; i < 3 && std::isfinite(result); ++i)
        result = std::nextafterf(result, direction);
    return result;
}
// Depth-slab hysteresis: expansion is applied immediately so nothing is
// clipped while moving, but shrinkage is delayed until the candidate slab
// has been stable for twenty frames.  This prevents one-ULP per-frame noise
// from making the near/far planes (and therefore the log-depth mapping)
// ping-pong between two values.
struct SlabStabilizer
{
    bool initialized = false;
    double stableNear = 0.0;
    double stableFar = 0.0;
    double pendingNear = 0.0;
    double pendingFar = 0.0;
    int stableFrames = 0;

    void reset()
    {
        initialized = false;
        stableFrames = 0;
    }

    void apply(double candidateNear, double candidateFar,
               double &outNear, double &outFar)
    {
        if (!initialized)
        {
            stableNear = pendingNear = candidateNear;
            stableFar = pendingFar = candidateFar;
            stableFrames = 0;
            initialized = true;
        }
        else if (candidateNear < stableNear || candidateFar > stableFar)
        {
            stableNear = std::min(stableNear, candidateNear);
            stableFar = std::max(stableFar, candidateFar);
            pendingNear = stableNear;
            pendingFar = stableFar;
            stableFrames = 0;
        }
        else
        {
            const double magnitude = std::max(
                {std::abs(stableNear), std::abs(stableFar),
                 std::abs(candidateNear), std::abs(candidateFar)});
            const double epsilon = std::max(1.0e-4, magnitude * 1.0e-4);
            if (std::abs(candidateNear - pendingNear) > epsilon ||
                std::abs(candidateFar - pendingFar) > epsilon)
            {
                pendingNear = candidateNear;
                pendingFar = candidateFar;
                stableFrames = 0;
            }
            else
            {
                ++stableFrames;
            }
            if (stableFrames >= 20)
            {
                stableNear = pendingNear;
                stableFar = pendingFar;
                stableFrames = 0;
            }
        }
        outNear = stableNear;
        outFar = stableFar;
    }
};

SlabStabilizer g_orthoSlabStabilizer;
SlabStabilizer g_perspectiveSlabStabilizer;
SlabStabilizer g_overlaySlabStabilizer;

void resetSlabStabilizers()
{
    g_orthoSlabStabilizer.reset();
    g_perspectiveSlabStabilizer.reset();
    g_overlaySlabStabilizer.reset();
}



const char *gridPlaneName(GridPlaneType plane)
{
    switch (plane)
    {
    case GridPlaneType::XY: return "XY";
    case GridPlaneType::YZ: return "YZ";
    case GridPlaneType::Custom: return "CUSTOM";
    default: return "XZ";
    }
}

constexpr glm::vec3 kAxisColorX(0.8f, 0.2f, 0.2f);
constexpr glm::vec3 kAxisColorY(0.2f, 0.8f, 0.2f);
constexpr glm::vec3 kAxisColorZ(0.2f, 0.2f, 0.8f);

bool isSpecialGridPlane(GridPlaneType plane)
{
    return plane != GridPlaneType::Custom;
}

void applyGridPlane(GridPlaneType plane)
{
    gridPlane = plane;
    switch (plane)
    {
    case GridPlaneType::XY:
        gridPlaneOrigin = glm::dvec3(0.0);
        gridPlaneNormal = glm::dvec3(0.0, 0.0, 1.0);
        gridPlaneStartAxisOrigin = glm::dvec3(0.0);
        gridPlaneStartAxisDirection = glm::dvec3(1.0, 0.0, 0.0);
        break;
    case GridPlaneType::YZ:
        gridPlaneOrigin = glm::dvec3(0.0);
        gridPlaneNormal = glm::dvec3(1.0, 0.0, 0.0);
        gridPlaneStartAxisOrigin = glm::dvec3(0.0);
        gridPlaneStartAxisDirection = glm::dvec3(0.0, 1.0, 0.0);
        break;
    case GridPlaneType::Custom:
        gridPlaneOrigin = customGridPlaneOrigin;
        gridPlaneNormal = customGridPlaneNormal;
        gridPlaneStartAxisOrigin = customGridPlaneStartAxisOrigin;
        gridPlaneStartAxisDirection = customGridPlaneStartAxisDirection;
        break;
    case GridPlaneType::XZ:
    default:
        gridPlaneOrigin = glm::dvec3(0.0);
        gridPlaneNormal = glm::dvec3(0.0, 1.0, 0.0);
        gridPlaneStartAxisOrigin = glm::dvec3(0.0);
        gridPlaneStartAxisDirection = glm::dvec3(1.0, 0.0, 0.0);
        break;
    }
    if (window)
        SDL_SetWindowTitle(
            window,
            (std::string("grid plane - ") + gridPlaneName(plane) +
             "  [1]XY [2]XZ [3]YZ [4]CUSTOM")
                .c_str());
    std::cout << "Grid plane: " << gridPlaneName(plane)
              << " (1=XY, 2=XZ, 3=YZ, 4=CUSTOM; origin=("
              << gridPlaneOrigin.x << ", " << gridPlaneOrigin.y << ", "
              << gridPlaneOrigin.z << "))" << std::endl;
    resetSlabStabilizers();
    if (targetPlaneConstraintEnabled)
        enforceTargetPlaneConstraint();
}

void activePlaneTangents(glm::dvec3 &tangentU, glm::dvec3 &tangentV)
{
    const glm::dvec3 planeNormal = glm::normalize(gridPlaneNormal);
    if (gridPlane == GridPlaneType::XY)
    {
        tangentU = glm::dvec3(1.0, 0.0, 0.0);
        tangentV = glm::dvec3(0.0, 1.0, 0.0);
    }
    else if (gridPlane == GridPlaneType::XZ)
    {
        tangentU = glm::dvec3(1.0, 0.0, 0.0);
        tangentV = glm::dvec3(0.0, 0.0, 1.0);
    }
    else if (gridPlane == GridPlaneType::YZ)
    {
        tangentU = glm::dvec3(0.0, 1.0, 0.0);
        tangentV = glm::dvec3(0.0, 0.0, 1.0);
    }
    else
    {
        tangentU = gridPlaneStartAxisDirection -
                   planeNormal * glm::dot(gridPlaneStartAxisDirection,
                                          planeNormal);
        if (glm::length(tangentU) < 1e-9)
        {
            tangentU = glm::dvec3(1.0, 0.0, 0.0) -
                       planeNormal * planeNormal.x;
            if (glm::length(tangentU) < 1e-9)
                tangentU = glm::dvec3(0.0, 1.0, 0.0) -
                           planeNormal * planeNormal.y;
        }
        tangentU = glm::normalize(tangentU);
        tangentV = glm::normalize(glm::cross(planeNormal, tangentU));
    }
}
// Validation scene adapted from external/large-coordinate-rendering.
//
  // The base point is 1e7 so the far cluster exercises large coordinates.
  // The rebased camera/target stay bounded by the 1e4 chunk; distant
  // objects sit far from the rebase origin, but at those distances they
  // render below the impostor threshold, so float32 quantization stays
  // sub-pixel.  Bump to ~1e9 for a true precision-stress demonstration:
const glm::dvec3 LARGE_COORDINATE_BASE_POINT(1e7, 0.0, 1e7);
const glm::dvec3 LARGE_COORDINATE_DETAIL_OFFSET(1536.0, 0.0, -1024.0);

OrbitCamera orbitCam(
    glm::vec3(0.0f), // Target is origin
    15.0f,           // Radius distance from target
    -45.0f,          // Yaw
    20.0f            // Pitch
);

// An orbit drag locks its pivot when it starts.  Until this demo has a
// selection system, use the same fallback as OpenCADStudio: the camera target.
std::optional<glm::dvec3> orbitPivot;

// Project the orbit focus onto the active grid plane. Translating the eye by
// the same plane correction preserves the view direction and orbit distance;
// ordinary orbiting may then still place the eye off the plane.
void enforceTargetPlaneConstraint()
{
    const glm::dvec3 planeNormal = glm::normalize(gridPlaneNormal);
    const double targetOnNormal =
        glm::dot(orbitCam.Target - gridPlaneOrigin, planeNormal);
    const glm::dvec3 correction = planeNormal * -targetOnNormal;
    orbitCam.setTarget(orbitCam.Target + correction);
}

void setTargetPlaneConstraint(bool enabled)
{
    if (targetPlaneConstraintEnabled == enabled)
        return;

    targetPlaneConstraintEnabled = enabled;
    if (enabled)
        enforceTargetPlaneConstraint();

    std::cout << "Target constraint: "
              << (enabled ? "GRID PLANE" : "FREE")
              << (enabled ? " (C disables)" : " (C enables)")
              << std::endl;
}

// Re-anchor the turntable and working plane to the current view: camera Up
// becomes the new gravity axis, while the plane passes through the orbit
// target and is aligned with camera Right.  Preset planes 1/2/3 remain
// untouched and can restore a world-axis working plane.
void resetWorldUpAndPlaneFromCamera()
{
    orbitCam.setWorldUp(orbitCam.Up);

    const glm::dvec3 horizontalAxis =
        glm::normalize(orbitCam.Right);
    customGridPlaneOrigin = orbitCam.Target;
    customGridPlaneNormal = glm::normalize(orbitCam.WorldUp);
    customGridPlaneStartAxisOrigin = orbitCam.Target;
    customGridPlaneStartAxisDirection = horizontalAxis;

    applyGridPlane(GridPlaneType::Custom);
    std::cout << "World up reset from camera: ("
              << orbitCam.WorldUp.x << ", " << orbitCam.WorldUp.y << ", "
              << orbitCam.WorldUp.z << ")" << std::endl;
}

} // namespace

void close();

bool init()
{
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return false;
  }

  const auto requestedRenderer = resolveRequestedBackend();
  rendererBackend = rendering::createRenderer(
      requestedRenderer.type, requestedRenderer.api);

  if (!rendererBackend->configureSDL())
  {
    std::cerr << "Failed to configure render backend: "
              << rendererBackend->name() << std::endl;
    close();
    return false;
  }

  window = SDL_CreateWindow("grid plane", SCREEN_WIDTH, SCREEN_HEIGHT,
                            rendererBackend->windowFlags());
  if (!window)
  {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    close();
    return false;
  }
  SDL_SetWindowTitle(
      window, "grid plane - XY  [1]XY [2]XZ [3]YZ [4]CUSTOM");

  if (!rendererBackend->initialize(window))
  {
    std::cerr << "Failed to initialize render backend: "
              << rendererBackend->name() << std::endl;
    close();
    return false;
  }

  std::cout << "Renderer backend: " << rendererBackend->name()
            << " (" << rendererBackend->graphicsApiName() << ")"
            << std::endl;

  // Optional startup visual style (0=Wireframe2D .. 5=DepthBuffer), used by
  // tooling/screenshots to render specific styles without key input.
  if (const char *styleEnv = std::getenv("GRID_START_STYLE"))
  {
    const int styleIndex = std::atoi(styleEnv);
    if (styleIndex > 0 && styleIndex < 6)
    {
      visualStyleManager.set(static_cast<rendering::RenderMode>(styleIndex));
      rendererBackend->setRenderMode(visualStyleManager.mode());
      std::cout << "Visual style: "
              << rendering::renderModeLabel(visualStyleManager.mode())
              << std::endl;
    }
  }
  std::string meshTexturePath;
  if (const char *textureEnv = std::getenv("GRID_MESH_TEXTURE"))
  {
    if (textureEnv[0] != '\0')
      meshTexturePath = textureEnv;
  }
  if (meshTexturePath.empty() && util::resourceExists("textures/grid-uv.png"))
  {
    meshTexturePath = util::resourcePath("textures/grid-uv.png").string();
  }
  if (!meshTexturePath.empty())
  {
    gMeshTextureIndex = rendererBackend->loadMeshTexture(meshTexturePath);
    std::cout << "Mesh texture: " << meshTexturePath
              << " (index=" << gMeshTextureIndex << ")" << std::endl;
  }

  // SDF + SHX font loaders.  WenQuanWeiMiHei-1 is the Chinese SDF source;
  // whgdtxt is the AutoCAD SHX (vector big-font) companion.  Both fall back
  // to the bundled NotoSansLatin if the requested files are missing.
  if (util::resourceExists("fonts/WenQuanWeiMiHei-1.ttf"))
  {
    gSdfFontReady = acgi::textEngine().loadSdfFont(
        util::resourcePath("fonts/WenQuanWeiMiHei-1.ttf").string());
    std::cout << "SDF font (WenQuanWeiMiHei-1): "
              << (gSdfFontReady ? "loaded" : "failed") << std::endl;
  }
  else
  {
    gSdfFontReady = acgi::textEngine().loadSdfFont(
        util::resourcePath("fonts/NotoSansLatin.ttf").string());
    std::cout << "SDF font (NotoSansLatin fallback): "
              << (gSdfFontReady ? "loaded" : "failed") << std::endl;
  }
  if (gSdfFontReady)
  {
    // Glyphs are available: Text/MText entities render through the SDF
    // queue instead of their baked layout frames.
    acgi::textFrameFallback() = false;
  }
  // AutoCAD bigfont pairing: txt.shx provides ASCII glyphs, gbcbig.shx
  // provides the double-byte CJK glyphs (full GB2312 coverage; whgdtxt
  // tops out at 0xC8FE and misses many common characters).
  bool shxRegularReady = false;
  if (util::resourceExists("fonts/txt.shx"))
  {
    shxRegularReady = acgi::textEngine().loadShxRegularFont(
        util::resourcePath("fonts/txt.shx").string());
    std::cout << "SHX regular font (txt): "
              << (shxRegularReady ? "loaded" : "failed") << std::endl;
  }
  if (util::resourceExists("fonts/gbcbig.shx"))
  {
    const bool bigReady = acgi::textEngine().loadShxBigFont(
        util::resourcePath("fonts/gbcbig.shx").string());
    std::cout << "SHX big font (bigfont): "
              << (bigReady ? "loaded" : "failed") << std::endl;
    gShxFontReady = shxRegularReady || bigReady;
  }
  else
  {
    gShxFontReady = shxRegularReady;
  }
  std::cout << "Grid plane: " << gridPlaneName(gridPlane)
            << " (1=XY, 2=XZ, 3=YZ, 4=CUSTOM; XYZ=red/green/blue, "
               "custom start axis=yellow)"
            << std::endl;
  std::cout << "Keys: V=cycle visual style, P=projection, "
               "L=scene teleport" << std::endl;

  return true;
}

void close()
{
  if (rendererBackend)
    rendererBackend->shutdown();
  rendererBackend.reset();
  if (window)
    SDL_DestroyWindow(window);
  window = nullptr;
  SDL_Quit();
}

void logCameraTargetIfChanged(const glm::dvec3 &target)
{
  static glm::dvec3 lastTarget(std::numeric_limits<double>::quiet_NaN());
  if (!(std::abs(target.x - lastTarget.x) < 1e-9 &&
        std::abs(target.y - lastTarget.y) < 1e-9 &&
        std::abs(target.z - lastTarget.z) < 1e-9))
  {
    std::cout << std::fixed << std::setprecision(6)
              << "Camera Target: ("
              << target.x << ", " << target.y << ", " << target.z << ")"
              << std::endl;
    lastTarget = target;
  }
}

void logCameraStateIfChanged(const glm::dvec3 &target,
                             double nearPlane, double farPlane,
                             bool isOrtho)
{
  static glm::dvec3 lastTarget(std::numeric_limits<double>::quiet_NaN());
  static double lastNear = std::numeric_limits<double>::quiet_NaN();
  static double lastFar  = std::numeric_limits<double>::quiet_NaN();
  static bool lastOrtho  = false;
  static bool initialized = false;

  const bool targetChanged =
      !(std::abs(target.x - lastTarget.x) < 1e-9 &&
        std::abs(target.y - lastTarget.y) < 1e-9 &&
        std::abs(target.z - lastTarget.z) < 1e-9);
  const bool planesChanged =
      !initialized ||
      std::abs(nearPlane - lastNear) > 1e-6 * std::max(1.0, std::abs(lastNear)) ||
      std::abs(farPlane  - lastFar)  > 1e-6 * std::max(1.0, std::abs(lastFar))  ||
      isOrtho != lastOrtho;

  if (targetChanged || planesChanged)
  {
    // Console I/O on Windows is synchronous and expensive; throttling to
    // ~10 Hz keeps pan from spending most of its frame budget on logging.
    static auto lastLogTime = std::chrono::steady_clock::now() -
                              std::chrono::milliseconds(1000);
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastLogTime).count() < 100)
      return;
    lastLogTime = now;

    std::cout << std::fixed << std::setprecision(6)
              << "Camera Target: ("
              << target.x << ", " << target.y << ", " << target.z << ")"
              << "  [" << (isOrtho ? "ORTHO" : "PERSP") << "]"
              << "  near=" << nearPlane
              << "  far="  << farPlane
              << std::endl;
    lastTarget = target;
    lastNear = nearPlane;
    lastFar  = farPlane;
    lastOrtho = isOrtho;
    initialized = true;
  }
}

// GRID_DEBUG_SLAB=1 prints stabilized slab changes so a static camera can be
// verified to produce at most one immediate expansion and one delayed shrink.
void logSlabIfChanged(bool isOrtho, double nearPlane, double farPlane,
                      double overlayNearPlane, double overlayFarPlane)
{
    static const bool enabled = [] {
        const char *flag = std::getenv("GRID_DEBUG_SLAB");
        return flag && flag[0] != '0';
    }();
    if (!enabled)
        return;

    static double last[2][4];
    static bool initialized[2] = {false, false};
    static auto lastLogTime = std::chrono::steady_clock::now();
    const int slot = isOrtho ? 1 : 0;
    const double values[4] = {nearPlane, farPlane,
                              overlayNearPlane, overlayFarPlane};
    bool changed = !initialized[slot];
    if (!changed)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (std::abs(values[i] - last[slot][i]) >
                1e-6 * std::max(1.0, std::abs(last[slot][i])))
            {
                changed = true;
                break;
            }
        }
    }
    if (!changed)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastLogTime).count() < 100)
        return;
    lastLogTime = now;
    std::cout << std::scientific << std::setprecision(6) << "[SLAB]"
              << (isOrtho ? " ortho" : " persp")
              << " near=" << nearPlane << " far=" << farPlane
              << " overlayNear=" << overlayNearPlane
              << " overlayFar=" << overlayFarPlane << std::endl;
    for (int i = 0; i < 4; ++i)
        last[slot][i] = values[i];
    initialized[slot] = true;
}

void appendSceneLine(scene::SceneDrawList &drawList,
                     const glm::dvec3 &start, const glm::dvec3 &end,
                     const glm::vec3 &color, float opacity,
                     double lineWeight = 2.0)
{
  scene::WorldDraw draw(drawList.geometry());
  draw.subEntityTraits().setColor(contrastAgainstBackground(glm::vec4(color, opacity)));
  draw.subEntityTraits().setLineWeight(lineWeight);

  entities::Line line;
  line.start = start;
  line.end = end;
  entities::worldDraw(line, draw);
}

bool lineDebugEnabled()
{
  static const bool enabled = [] {
    const char *value = std::getenv("GRID_LINE_DEBUG");
    return value && *value && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

void appendScenePoint(scene::SceneDrawList &drawList,
                      const glm::dvec3 &location, const glm::vec3 &color,
                      double pointSize)
{
  scene::WorldDraw draw(drawList.geometry());
  draw.subEntityTraits().setColor(contrastAgainstBackground(glm::vec4(color, 1.0f)));
  draw.subEntityTraits().setLineWeight(pointSize);

  entities::Point point;
  point.location = location;
  entities::worldDraw(point, draw);
}

// Dynamic overlays stay in the AcGi-lite protocol but are never placed in the
// immutable CAD draw-list cache.  The submitter preserves their cheap
// view-space line and point pipelines.
glm::vec3 ribbonSide(const glm::vec3 &direction, const glm::vec3 &front,
                     float halfWidth)
{
  glm::vec3 sideAxis = glm::cross(direction, front);
  if (glm::length(sideAxis) < 1.0e-5f)
    sideAxis = glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f));
  if (glm::length(sideAxis) < 1.0e-5f)
    sideAxis = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
  return glm::normalize(sideAxis) * halfWidth;
}

float strokeHalfWidth(const entities::Stroke &stroke, float fallback = 2.0f)
{
  return stroke.lineWeight > 0.0
             ? static_cast<float>(stroke.lineWeight) * 0.5f
             : fallback;
}

// Current overlay depth slab, published by render() each frame before any
// stroke submission.  CAD strokes are frustum-clipped against the slab of
// the pass that consumes them, so infinite entities (Ray/XLine) emit only
// their visible span instead of pushing megameter ribbons through the GPU.
double g_renderSlabNear = 0.0;
double g_renderSlabFar = 1.0e9;

int currentDrawableWidth();
int currentDrawableHeight();
bool clipReferenceSegmentToOrtho(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearPlane, double farPlane, double halfWidth,
    double halfHeight, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd);
bool clipReferenceSegmentToPerspective(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearPlane, double farPlane, double tanHalfVertical,
    double tanHalfHorizontal, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd);
bool clipSemiInfiniteRayToView(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearDepth, double farDepth, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd, bool flattenToSlabCenter = true);

// Clip one stroke segment to the frustum of the pass that will draw it.
// Hardware clips at exactly these planes, so the clipped segment renders
// identically while every emitted camera-relative vertex stays bounded and
// dash patterns never subdivide off-screen geometry.
static bool clipStrokeSegmentToView(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearDepth, double farDepth,
    glm::dvec3 &clippedStart, glm::dvec3 &clippedEnd,
    bool semiInfiniteRay = false)
{
  // Keep a tiny safety band outside the hardware clip planes.  Geometry
  // that lies exactly on a plane (reference-line endpoints, the debug
  // frustum wireframe, the grid visible quad) must never be rejected by
  // floating-point jitter; the band stays pixel-exact because hardware
  // still clips at the true planes.
  const double slabEpsilon =
      std::max(1.0e-6, (farDepth - nearDepth) * 1.0e-6);
  nearDepth -= slabEpsilon;
  farDepth += slabEpsilon;
  const double angularEpsilon = 1.0e-6;
  if (semiInfiniteRay)
  {
    // A Ray must not stop at its tessellation proxy length.  Clip the
    // analytic half-line directly against the same slab and viewport.
    return clipSemiInfiniteRayToView(
        startWorld, endWorld, cameraPosition, cameraRight, cameraUp,
        cameraFront, nearDepth, farDepth, clippedStart, clippedEnd);
  }
  if (useOrthoProjection())
  {
    const double halfHeight = orbitCam.orthoSize() * (1.0 + angularEpsilon);
    return clipReferenceSegmentToOrtho(
        startWorld, endWorld, cameraPosition, cameraRight, cameraUp,
        cameraFront, nearDepth, farDepth,
        halfHeight * (1.0 + angularEpsilon) *
            (double)currentDrawableWidth() /
            std::max(1, currentDrawableHeight()),
        halfHeight, clippedStart, clippedEnd);
  }
  const double tanHalfVertical =
      std::tan(glm::radians(45.0) * 0.5) * (1.0 + angularEpsilon);
  const double tanHalfHorizontal =
      tanHalfVertical * (double)currentDrawableWidth() /
      std::max(1, currentDrawableHeight());
  return clipReferenceSegmentToPerspective(
      startWorld, endWorld, cameraPosition, cameraRight, cameraUp,
      cameraFront, nearDepth, farDepth, tanHalfVertical,
      tanHalfHorizontal, clippedStart, clippedEnd);
}

std::vector<glm::dvec3> sampleCurveBatch(const scene::CurveBatchCommand &curve);

void submitAcGiDrawable(scene::SceneDrawList &drawList,
                         const glm::mat4 &view,
                         const glm::mat4 &projection,
                         const glm::mat4 &overlayProjection,
                         const glm::dvec3 &rebase,
                         const glm::dvec3 &cameraPosition,
                         const glm::dvec3 &cameraRight,
                         const glm::dvec3 &cameraUp,
                         const glm::dvec3 &cameraFront,
                         const glm::vec4 &logDepth,
                         float pixelSizeWorld = 0.0f,
                         float edgeSoftness = 0.15f,
                         float pointSize = 2.0f)
{
  if (!rendererBackend)
    return;

  if (drawList.lights())
    rendererBackend->setRealisticLights(drawList.lights()->data);

  if (drawList.grid())
    rendererBackend->drawGrid(drawList.grid()->data);

  for (scene::MeshBatchCommand &batch : drawList.meshBatches())
    submitMeshBatch(batch, view, projection, logDepth,
                    rendering::encodeDoubleSingle(cameraPosition),
                    pixelSizeWorld, edgeSoftness);


  static std::vector<rendering::PrimVertex> polylineVertices;
  polylineVertices.clear();
  static std::vector<rendering::LineInstance> lineInstances;
  lineInstances.clear();
  auto appendAcGiRibbon = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                              const glm::vec4 &color, float halfWidth,
                              float u0, float u1) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const float minimumHalfWidth =
        pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
    halfWidth = std::max(halfWidth, minimumHalfWidth);
    // The polyline fragment shader treats v=[0,1] as symmetric edges, so the
    // visible opaque core lies at the quad midpoint.  Emit a centered ribbon;
    // a one-sided quad would shift every rendered line by half its width.
    const glm::vec4 strokeColor = contrastAgainstBackground(color);
    const glm::vec3 side = ribbonSide(direction, cameraFront, halfWidth);
    polylineVertices.push_back({ra - side, strokeColor, {u0, 0.0f}});
    polylineVertices.push_back({ra + side, strokeColor, {u0, 1.0f}});
    polylineVertices.push_back({rb + side, strokeColor, {u1, 1.0f}});
    polylineVertices.push_back({ra - side, strokeColor, {u0, 0.0f}});
    polylineVertices.push_back({rb + side, strokeColor, {u1, 1.0f}});
    polylineVertices.push_back({rb - side, strokeColor, {u1, 0.0f}});
  };
  auto appendLineInstance = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                               const glm::vec4 &color, float halfWidth) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const float minimumHalfWidth =
        pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
    halfWidth = std::max(halfWidth, minimumHalfWidth);
    const glm::vec4 strokeColor = contrastAgainstBackground(color);
    lineInstances.push_back({
        glm::vec4(ra, 0.0f),
        glm::vec4(rb, 1.0f),
        glm::vec4(glm::vec3(strokeColor), halfWidth),
        glm::vec4(strokeColor.a, 0.0f, 0.0f, 0.0f),
    });
  };

  // Curves were previously hardware PT_LINESTRIPs, which do not give reliable
  // line AA on D3D11.  Sample on CPU using the same evaluator as picking/ID,
  // frustum-clip each segment, and reuse the screen-space ribbon pipeline.
  for (const scene::CurveBatchCommand &curve : drawList.curveBatches())
  {
    if (curve.controlPoints.empty() &&
        curve.algorithm != rendering::CurveAlgorithm::Arc)
    {
      continue;
    }
    const std::vector<glm::dvec3> points = sampleCurveBatch(curve);
    if (points.size() < 2)
      continue;
    const float halfWidth = std::max(
        curve.acgiMaterial.lineWidth * 0.5f, pixelSizeWorld);
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
      glm::dvec3 clippedStart, clippedEnd;
      if (!clipStrokeSegmentToView(
              points[i], points[i + 1], cameraPosition,
              cameraRight, cameraUp, cameraFront,
              g_renderSlabNear, g_renderSlabFar,
              clippedStart, clippedEnd, false))
      {
        continue;
      }
      appendLineInstance(
          glm::vec3(clippedStart - cameraPosition),
          glm::vec3(clippedEnd - cameraPosition),
          curve.acgiMaterial.baseColor, halfWidth);
    }
  }
  auto appendLinePatternSegment = [&](const glm::vec3 &ra,
                                      const glm::vec3 &rb,
                                      const entities::Stroke &stroke) {
    const std::string &type = stroke.common.lineType;
    const AcGiLineType *lineType = acgiFindLineType(type.c_str());
    if (!lineType)
    {
      appendAcGiRibbon(ra, rb, stroke.common.color, 0.0f, 0.0f, 0.0f);
      return;
    }
    const std::vector<AcGiLineTypeMark> pattern =
        acgiLineTypeMarks(*lineType);

    const float halfWidth = strokeHalfWidth(stroke);
    const glm::dvec3 start(ra);
    const glm::dvec3 end(rb);
    const double total = glm::length(end - start);
    if (total < 1.0e-12)
      return;

    double patternLength = 0.0;
    for (const auto &mark : pattern)
      patternLength += mark.length;
    double distance = 0.0;
    size_t markIndex = 0;
    while (distance < total && !pattern.empty())
    {
      const auto &mark = pattern[markIndex % pattern.size()];
      const double markLength = mark.length;
      const double next = std::min(distance + markLength, total);
      if (mark.stroke && next > distance)
      {
        appendAcGiRibbon(
            glm::vec3(start + (end - start) * (distance / total)),
            glm::vec3(start + (end - start) * (next / total)),
            stroke.common.color, halfWidth,
            float(distance / total), float(next / total));
      }
      distance = next;
      ++markIndex;
    }
  };

  for (const entities::Stroke &stroke : drawList.geometry().strokes)
  {
    if (!stroke.common.visible || stroke.points.size() < 2)
      continue;
    const float halfWidth = strokeHalfWidth(stroke);
    const size_t strokeCount = stroke.points.size();
    const bool patterned = stroke.common.lineType != "ByLayer" &&
                           stroke.common.lineType != "CONTINUOUS";
    // Closed strokes also emit the wrap segment back to their first point.
    const size_t strokeSegmentCount =
        stroke.closed ? strokeCount : strokeCount - 1;
    for (size_t i = 0; i < strokeSegmentCount; ++i)
    {
      // Frustum-clip in double before emitting the ribbon: infinite
      // strokes (Ray/XLine) contribute only their visible span.
      glm::dvec3 clippedStart, clippedEnd;
      if (!clipStrokeSegmentToView(
              stroke.points[i], stroke.points[(i + 1) % strokeCount],
              cameraPosition,
              cameraRight, cameraUp, cameraFront,
              g_renderSlabNear, g_renderSlabFar, clippedStart, clippedEnd,
              stroke.semiInfinite))
        continue;
      if (lineDebugEnabled() && stroke.semiInfinite)
      {
        std::printf(
            "[BODY_RAY] cam=(%.6f,%.6f,%.6f) start=(%.6f,%.6f,%.6f) end=(%.6f,%.6f,%.6f) half=%.6f\n",
            cameraPosition.x, cameraPosition.y, cameraPosition.z,
            clippedStart.x, clippedStart.y, clippedStart.z,
            clippedEnd.x, clippedEnd.y, clippedEnd.z,
            std::max(halfWidth, pixelSizeWorld));
      }
      if (patterned)
      {
        appendLinePatternSegment(
            glm::vec3(clippedStart - cameraPosition),
            glm::vec3(clippedEnd - cameraPosition), stroke);
      }
      else
      {
        appendLineInstance(
            glm::vec3(clippedStart - cameraPosition),
            glm::vec3(clippedEnd - cameraPosition),
            stroke.common.color, halfWidth);
      }
    }
  }

  if (!lineInstances.empty())
  {
    const rendering::LineInstancesRenderData lineData{
        .view = view,
        .projection = overlayProjection,
        .instances = lineInstances.data(),
        .instanceCount = static_cast<uint32_t>(lineInstances.size()),
        .logDepth = logDepth,
        .edgeSoftness = edgeSoftness,
        .layer = envLayer("GRID_LINE_LAYER"),
    };
    rendererBackend->drawLineInstances(lineData);
  }

  if (!polylineVertices.empty())
  {
    if (lineDebugEnabled())
    {
      glm::vec2 ndcMin(std::numeric_limits<float>::max());
      glm::vec2 ndcMax(std::numeric_limits<float>::lowest());
      for (size_t i = 0; i < polylineVertices.size(); i += 6)
      {
        const glm::vec4 projected = overlayProjection * view *
            glm::vec4(polylineVertices[i].position, 1.0f);
        const glm::vec2 ndc = glm::vec2(projected) / projected.w;
        ndcMin = glm::min(ndcMin, ndc);
        ndcMax = glm::max(ndcMax, ndc);
      }
      std::cout << "[LINE_DEBUG] ribbon vertices="
                << polylineVertices.size()
                << " ndcMin=(" << ndcMin.x << ", " << ndcMin.y
                << ") ndcMax=(" << ndcMax.x << ", " << ndcMax.y << ")"
                << std::endl;
    }
    const rendering::PolylineRenderData polylineData{
        .view = view,
        .projection = overlayProjection,
        .vertices = polylineVertices.data(),
        .vertexCount = static_cast<uint32_t>(polylineVertices.size()),
        .logDepth = logDepth,
        .edgeSoftness = edgeSoftness,
        .layer = envLayer("GRID_LINE_LAYER"),
    };
    rendererBackend->drawPolylines(polylineData);
  }

  static std::vector<rendering::FillVertex> fillVertices;
  static std::vector<rendering::FillVertex> surfaceFillVertices;
  fillVertices.clear();
  surfaceFillVertices.clear();
  for (const entities::Triangle &triangle : drawList.geometry().fills)
  {
    if (!triangle.common.visible)
      continue;
    std::vector<rendering::FillVertex> &vertices =
        triangle.is3DFace ? surfaceFillVertices : fillVertices;
    const glm::vec4 fillColor = contrastAgainstBackground(triangle.common.color);
    vertices.push_back({glm::vec3(triangle.a - cameraPosition), fillColor});
    vertices.push_back({glm::vec3(triangle.b - cameraPosition), fillColor});
    vertices.push_back({glm::vec3(triangle.c - cameraPosition), fillColor});
  }
  for (const bool is3DFace : {false, true})
  {
    std::vector<rendering::FillVertex> &vertices =
        is3DFace ? surfaceFillVertices : fillVertices;
    if (vertices.empty())
      continue;
    const rendering::FilledTrianglesRenderData fillData{
        .view = view,
        .projection = projection,
        .vertices = vertices.data(),
        .vertexCount = static_cast<uint32_t>(vertices.size()),
        .is3DFace = is3DFace,
        .layer = envLayer("GRID_FILL_LAYER"),
        .logDepth = logDepth,
        .material = toSurfaceMaterial(scene::AcGiMaterial{}),
    };
    rendererBackend->drawFilledTriangles(fillData);
  }

  static std::vector<rendering::TargetPointInstance> points;
  points.clear();
  for (const entities::TessellatedPoint &point : drawList.geometry().points)
  {
    if (!point.common.visible)
      continue;
    points.push_back({glm::vec3(point.location - cameraPosition),
                      glm::vec3(contrastAgainstBackground(point.common.color)),
                      float(point.pointSize)});
  }
  if (!points.empty())
  {
    const rendering::TargetPointInstancesRenderData pointData{
        .view = view,
        .projection = overlayProjection,
        .instances = points.data(),
        .instanceCount = static_cast<uint32_t>(points.size()),
        .pointSize = pointSize,
        .pixelSizeWorld = pixelSizeWorld,
        .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
        .logDepth = logDepth,
    };
    rendererBackend->drawTargetPointInstances(pointData);
  }
}

struct MeshEntityRecord
{
  entities::Mesh entity;
  glm::dvec3 worldPosition;
  float size;
  rendering::MeshType mesh = rendering::MeshType::Cube;

  bool realistic() const
  {
    return entity.style == entities::MeshStyle::Realistic;
  }

  const std::string &displayName() const
  {
    return entity.common.name;
  }
};

using LargeCoordinateObject = MeshEntityRecord;

enum class VisibilityKind
{
  MeshObject,
  CenterCube,
  CadMesh,
  CadStroke,
  CadFill,
  CadPoint,
  CadCurve
};

struct CadEntityRange;

struct GpuPickEntity
{
  VisibilityKind kind = VisibilityKind::MeshObject;
  const MeshEntityRecord *mesh = nullptr;
  const CadEntityRange *cadRange = nullptr;
  const scene::CurveBatchCommand *curve = nullptr;

  bool operator==(const GpuPickEntity &other) const
  {
    return kind == other.kind && mesh == other.mesh &&
           cadRange == other.cadRange &&
           curve == other.curve;
  }
};

std::unordered_map<uint32_t, GpuPickEntity> &gpuPickRegistry()
{
  static std::unordered_map<uint32_t, GpuPickEntity> registry;
  return registry;
}

uint32_t gpuPickNextEntityId = 2;

static bool gpuPickEnabled()
{
  // GPU rough-picking is the default; GRID_GPU_PICK=0 selects the CPU path.
  const char *value = std::getenv("GRID_GPU_PICK");
  return value == nullptr || std::strcmp(value, "0") != 0;
}

uint32_t registerGpuPickEntity(GpuPickEntity entity)
{
  auto &registry = gpuPickRegistry();
  for (uint32_t count = 0; count < 0xfffffffcu; ++count)
  {
    const uint32_t id = gpuPickNextEntityId;
    gpuPickNextEntityId = gpuPickNextEntityId >= 0xfffffffeu
                              ? 2
                              : gpuPickNextEntityId + 1;
    if (id == 0 || id == 0xffffffffu || id == 1)
      continue;

    auto [existing, inserted] = registry.emplace(id, entity);
    if (inserted || existing->second == entity)
      return id;
  }
  return 0;
}

glm::vec4 encodeGpuPickId(uint32_t id)
{
  return {
      float((id >> 16) & 0xff) / 255.0f,
      float((id >> 8) & 0xff) / 255.0f,
      float(id & 0xff) / 255.0f,
      float((id >> 24) & 0xff) / 255.0f};
}

const GpuPickEntity *findGpuPickEntity(uint32_t id)
{
  auto &registry = gpuPickRegistry();
  const auto found = registry.find(id);
  return found != registry.end() ? &found->second : nullptr;
}

constexpr uint32_t kGpuPickCenterCubeId = 1;

struct GpuPickCameraBasis
{
  glm::dvec3 position;
  glm::dvec3 front;
  glm::dvec3 right;
  glm::dvec3 up;
  bool ortho = false;
  double orthoHalfHeight = 0.0;
};

struct GpuPickFocusState
{
  std::optional<GpuPickCameraBasis> camera;
  double ndcX = 0.0;
  double ndcY = 0.0;
  uint32_t requestToken = 0;
  uint32_t pendingFrames = 0;
  bool pendingNdc = false;
  bool waitingResult = false;
};

GpuPickFocusState gpuPickFocus;
bool gpuPickSceneDebug = false;
bool gpuPickSceneDebugQueueActive = false;
std::optional<GpuPickEntity> outlineEntity;
bool outlineLockTest = false;
bool outlineAllTest = false;
uint32_t lockedOutlineId = 0;

// FNV-1a over raw bytes; used to detect when the full-scene GPU ID
// buffer must be re-rendered for the selection outline overlay.
static uint64_t hashGpuPickSceneBytes(uint64_t hash, const void *data,
                                      size_t size)
{
  const unsigned char *bytes = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i)
  {
    hash ^= bytes[i];
    hash *= 0x100000001b3ull;
  }
  return hash;
}

uint32_t findGpuPickObjectIdForEntity(const GpuPickEntity &entity)
{
  if (entity.kind == VisibilityKind::CenterCube)
    return kGpuPickCenterCubeId;
  for (const auto &[objectId, registered] : gpuPickRegistry())
    if (registered == entity)
      return objectId;
  return 0;
}

static bool gpuPickFocusWaiting()
{
  return gpuPickEnabled() && gpuPickFocus.waitingResult;
}

glm::vec4 meshEntityColor(const MeshEntityRecord &entity)
{
  return contrastAgainstBackground(entity.entity.common.color);
}

bool meshEntityVisible(const MeshEntityRecord &entity)
{
  return entity.entity.common.visible && entity.entity.common.color.a > 0.0f;
}

static rendering::MeshInstance &cachedMeshInstance(
    const MeshEntityRecord &entity,
    const glm::vec4 &material = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f))
{
  struct CacheEntry
  {
    rendering::MeshInstance instance;
    glm::dvec3 position;
    float size = 0.0f;
    glm::vec4 color;
    glm::vec4 material;
  };

  static std::unordered_map<const MeshEntityRecord *, CacheEntry> cache;
  const glm::vec4 color = meshEntityColor(entity);
  auto found = cache.find(&entity);
  if (found != cache.end() && found->second.position == entity.worldPosition &&
      found->second.size == entity.size && found->second.color == color &&
      found->second.material == material)
  {
    return found->second.instance;
  }

  CacheEntry entry;
  entry.position = entity.worldPosition;
  entry.size = entity.size;
  entry.color = color;
  entry.instance = makeMeshInstance(
      entity.size, glm::vec3(color), color.a,
      rendering::encodeDoubleSingle(entity.worldPosition), material);
  auto inserted = cache.emplace(&entity, std::move(entry));
  return inserted.first->second.instance;
}

static void queueGpuMeshEntity(const MeshEntityRecord &entity,
                               uint32_t objectId)
{
  if (!rendererBackend ||
      !(gpuPickFocusWaiting() || gpuPickSceneDebugQueueActive) ||
      !meshEntityVisible(entity) || objectId == 0)
    return;

  const rendering::DoubleSingleVec3 objectPosition =
      rendering::encodeDoubleSingle(entity.worldPosition);
  const glm::vec4 color = meshEntityColor(entity);
  const rendering::MeshInstance instance = makeMeshInstance(
      entity.size, glm::vec3(color), color.a, objectPosition);
  rendererBackend->queueGpuMeshPick(instance, entity.mesh, objectId);
}

static void queueGpuMeshEntity(const MeshEntityRecord *entity)
{
  if (!entity)
    return;
  queueGpuMeshEntity(*entity,
                     registerGpuPickEntity({VisibilityKind::MeshObject,
                                            entity, nullptr}));
}

glm::vec4 meshEntityRenderMaterial(const MeshEntityRecord &entity);

void appendMeshEntityToScene(const MeshEntityRecord &entity,
                             scene::SceneDrawList &drawList,
                             bool cadAlgorithm = false)
{
  if (!meshEntityVisible(entity))
    return;

  const glm::vec4 color = meshEntityColor(entity);
  const glm::vec4 material = meshEntityRenderMaterial(entity);
  scene::MeshBatchCommand &batch = drawList.addMeshBatch(
      entity.mesh, color.a >= 1.0f, entity.realistic(), material,
      cadAlgorithm);
  batch.acgiMaterial.algorithm = entity.realistic()
      ? scene::AcGiShaderAlgorithm::Realistic
      : scene::AcGiShaderAlgorithm::Shaded;
  batch.acgiMaterial.baseColor = color;
  batch.acgiMaterial.accentColor = entity.entity.material.specularFactor;
  batch.acgiMaterial.metallic = entity.entity.material.metallicFactor;
  batch.acgiMaterial.roughness = entity.entity.material.roughnessFactor;
  batch.acgiMaterial.transparency = 1.0f - color.a;
  batch.acgiMaterial.lineWidth = float(entity.entity.common.lineWeight);
  batch.instances.push_back(cachedMeshInstance(entity, material));
}

glm::vec4 meshEntityRenderMaterial(const MeshEntityRecord &entity)
{
  return entity.realistic()
      ? glm::vec4(entity.entity.material.metallicFactor,
                  entity.entity.material.roughnessFactor, 0.0f, 0.0f)
      : glm::vec4(0.0f, 0.35f, 0.0f, 0.0f);
}


const char *meshDisplayName(rendering::MeshType mesh)
{
  switch (mesh)
  {
  case rendering::MeshType::Sphere: return "Sphere";
  case rendering::MeshType::Cone: return "Cone";
  case rendering::MeshType::Torus: return "Torus";
  case rendering::MeshType::Cube: break;
  }
  return "Cube";
}

std::string indexedDisplayName(const char *prefix, size_t index)
{
  std::string name(prefix);
  const size_t firstDigit = name.size();
  do
  {
    name.push_back(char('0' + index % 10));
    index /= 10;
  } while (index != 0);
  std::reverse(name.begin() + firstDigit, name.end());
  return name;
}

const std::vector<LargeCoordinateObject> &getLargeCoordinateObjects()
{
  static const std::vector<LargeCoordinateObject> objects = [] {
    const glm::dvec3 detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    struct ValidationPoint
    {
      glm::dvec3 offset;
      glm::vec3 color;
      float size;
      rendering::MeshType mesh;
    };
    const ValidationPoint validationPoints[] = {
        {glm::dvec3(0.0, 256.0, 0.0), glm::vec3(0.43f, 0.91f, 0.98f),
         512.0f, rendering::MeshType::Cube},
        {glm::dvec3(0.0, 224.0, 0.0), glm::vec3(1.0f, 0.58f, 0.25f),
         448.0f, rendering::MeshType::Cube},
        {glm::dvec3(1920.0, 64.0, 0.0), glm::vec3(0.95f, 0.95f, 0.95f),
         128.0f, rendering::MeshType::Cube},
        {glm::dvec3(0.0, 64.0, -1920.0), glm::vec3(0.95f, 0.95f, 0.95f),
         128.0f, rendering::MeshType::Cube},
        {glm::dvec3(-288.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {glm::dvec3(-96.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {glm::dvec3(96.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {glm::dvec3(288.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
    };
    std::vector<LargeCoordinateObject> objects;
    if (debugValidationMeshesEnabled())
    {
      objects.reserve(std::size(validationPoints));
      for (const ValidationPoint &point : validationPoints)
      {
        MeshEntityRecord object;
        object.worldPosition = detailCenter + point.offset;
        object.entity.common.color = glm::vec4(point.color, 0.45f);
        object.size = point.size;
        object.mesh = point.mesh;
        object.entity.common.name =
            indexedDisplayName(meshDisplayName(object.mesh), objects.size());
        objects.push_back(std::move(object));
      }
    }
    if (realisticMeshEnabled())
    {
      for (size_t i = 0; i < objects.size(); ++i)
      {
        objects[i].entity.style = (i % 2) != 0
                                      ? entities::MeshStyle::Realistic
                                      : entities::MeshStyle::Cad;
        objects[i].entity.material.metallicFactor = 0.82f;
        objects[i].entity.material.roughnessFactor = 0.22f;
      }
    }
    return objects;
  }();
  return objects;
}

MeshEntityRecord getCenterCubeEntity()
{
  MeshEntityRecord entity;
  entity.entity.common.name = "CenterCube";
  entity.entity.common.visible =
      !largeCoordinateCameraView || centerCubeForced();
  entity.entity.common.color = glm::vec4(1.0f, 0.58f, 0.25f, 1.0f);
  entity.worldPosition = cubeWorldPosition;
  entity.size = 1.0f;
  entity.mesh = rendering::MeshType::Cube;
  return entity;
}

static glm::dvec3 vectorPrimitivesAnchor() {
  return LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET +
         glm::dvec3(0.0, 0.0, 0.0);
}

static bool cadEntityDemoEnabled()
{
  const char *value = std::getenv("GRID_CAD_DEMO");
  // CAD vector entities are part of the design-rendering default. Keep the
  // explicit zero value as an escape hatch for debugging the legacy demo.
  return value == nullptr || (std::strcmp(value, "0") != 0);
}

// A/B escape hatch for the ParamSurface stress case.  It is enabled by
// default; set GRID_PARAM_SURFACE=0 to exclude only this entity and its
// isolines from the cached CAD demo without disabling the other CAD entities.
static bool paramSurfaceDemoEnabled()
{
  const char *value = std::getenv("GRID_PARAM_SURFACE");
  return value == nullptr || (std::strcmp(value, "0") != 0);
}

// Lightweight frame-time diagnostics for renderer A/B tests.  Disabled by
// default; set GRID_FRAME_LOG=1 to print the 60-frame average after present.
static bool frameLogEnabled()
{
  const char *value = std::getenv("GRID_FRAME_LOG");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

// The ParamSurface tessellation density is a tuning knob for the fill/geometry
// split.  The default preserves the existing 24x24 mesh; zero or malformed
// values fall back to the default.
static int paramSurfaceSegmentCount()
{
  const char *value = std::getenv("GRID_PARAM_SURFACE_SEGMENTS");
  if (!value || *value == '\0')
    return 24;
  const int segments = std::atoi(value);
  return segments > 1 ? segments : 24;
}

// Separates the surface body from its isolines when profiling.  Enabled by
// default; set GRID_PARAM_SURFACE_ISOLINES=0 to draw only the fill.
static bool paramSurfaceIsolinesEnabled()
{
  const char *value = std::getenv("GRID_PARAM_SURFACE_ISOLINES");
  return value == nullptr || (std::strcmp(value, "0") != 0);
}

enum class CadPickShape
{
  // Use the tessellated primitives themselves.
  Primitives,
  // Two same-length offset strokes define an area (for example MLine).
  // Picking only the two boundary ribbons leaves the visible entity's
  // semantic body zoom-dependent and misses clicks between the edges.
  PairedStrokeBand
};

struct CadEntityRange
{
  std::string name;
  size_t begin = 0;
  size_t count = 0;
  CadPickShape pickShape = CadPickShape::Primitives;
};

enum class VisibilityState
{
  Offscreen,
  Tiny,
  Visible
};

// One CPU-side visibility candidate can describe either a mesh instance or a
// tessellated CAD entity range.  Both projection modes classify these records;
// only the query parameters differ.
struct VisibilityCandidate
{
  VisibilityKind kind = VisibilityKind::MeshObject;
  size_t entityIndex = 0;
  const CadEntityRange *cadRange = nullptr;
  size_t rangeBegin = 0;
  size_t rangeCount = 0;
  const MeshEntityRecord *mesh = nullptr;
  const scene::CurveBatchCommand *curve = nullptr;
  glm::dvec3 min{0.0};
  glm::dvec3 max{0.0};
  glm::dvec3 center{0.0};
  double lodSize = 0.0;
  glm::vec4 overlayColor{1.0f};
  float overlayPointSize = 2.0f;
};

struct VectorPrimitivesTessellation
{
  entities::TessellatedEntity geometry;
  glm::dvec3 anchor{0.0};
  std::vector<MeshEntityRecord> meshes;
  std::vector<CadEntityRange> strokeRanges;
  std::vector<CadEntityRange> fillRanges;
  std::vector<CadEntityRange> pointRanges;
  std::vector<scene::CurveBatchCommand> curves;
};

enum class CadEntityPickShape
{
  Primitives,
  PairedStrokeBand
};

template <typename EntityType>
void appendVectorPrimitive(const EntityType &entity, const char *name,
                           const entities::TesselationOptions &options,
                           VectorPrimitivesTessellation &target,
                           bool fillIs3DFace = false,
                           CadEntityPickShape pickShape =
                               CadEntityPickShape::Primitives)
{
  auto addRange = [name, pickShape](std::vector<CadEntityRange> &ranges,
                         size_t begin, size_t end) {
    if (end != begin)
    {
      ranges.push_back({name, begin, end - begin,
                        pickShape == CadEntityPickShape::PairedStrokeBand
                            ? CadPickShape::PairedStrokeBand
                            : CadPickShape::Primitives});
    }
  };
  const size_t strokeBegin = target.geometry.strokes.size();
  const size_t fillBegin = target.geometry.fills.size();
  const size_t pointBegin = target.geometry.points.size();
  scene::ViewportDraw draw(target.geometry, options);
  draw.subEntityTraits().setFrom(entity.common);
  entities::worldDraw(entity, draw, fillIs3DFace);
  addRange(target.strokeRanges, strokeBegin,
           target.geometry.strokes.size());
  addRange(target.fillRanges, fillBegin, target.geometry.fills.size());
  addRange(target.pointRanges, pointBegin, target.geometry.points.size());
}

// ACI palette indices 1-7 cover the standard drawing colors; anything else
// (true color, ByBlock) falls back to white for the demo renderer.
static glm::vec4 aciColor(int index)
{
  switch (index)
  {
  case 1: return {1.0f, 0.0f, 0.0f, 1.0f};
  case 2: return {1.0f, 1.0f, 0.0f, 1.0f};
  case 3: return {0.0f, 1.0f, 0.0f, 1.0f};
  case 4: return {0.0f, 1.0f, 1.0f, 1.0f};
  case 5: return {0.0f, 0.0f, 1.0f, 1.0f};
  case 6: return {1.0f, 0.0f, 1.0f, 1.0f};
  default: return {1.0f, 1.0f, 1.0f, 1.0f};
  }
}

// GRID_DWG=<path> appends a decoded DWG model next to the authored demo
// entities: libredwg decodes the file, dwg_bridge.h maps each supported
// entity struct, and appendVectorPrimitive routes it through the same
// tessellation/picking pipeline as the authored demo.
static void appendDwgFile(const char *path,
                          const entities::TesselationOptions &options,
                          VectorPrimitivesTessellation &target)
{
  Dwg_Data dwg;
  memset(&dwg, 0, sizeof(dwg));
  if (dwg_read_file(path, &dwg) != 0)
  {
    std::cout << "GRID_DWG: failed to decode " << path << std::endl;
    return;
  }

  size_t appended = 0;
  for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
  {
    const Dwg_Object &object = dwg.object[i];
    if (object.supertype != DWG_SUPERTYPE_ENTITY || !object.tio.entity)
      continue;
    const Dwg_Color &color = object.tio.entity->color;
    const glm::vec4 entityColor = aciColor(static_cast<int>(color.index));

    switch (object.type)
    {
    case DWG_TYPE_LINE:
    {
      entities::Line line = entities::toEntity(*object.tio.entity->tio.LINE);
      line.common.color = entityColor;
      appendVectorPrimitive(line, "DWG_LINE", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_ARC:
    {
      entities::Arc arc = entities::toEntity(*object.tio.entity->tio.ARC);
      arc.common.color = entityColor;
      appendVectorPrimitive(arc, "DWG_ARC", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_CIRCLE:
    {
      entities::Circle circle =
          entities::toEntity(*object.tio.entity->tio.CIRCLE);
      circle.common.color = entityColor;
      appendVectorPrimitive(circle, "DWG_CIRCLE", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_ELLIPSE:
    {
      entities::Ellipse ellipse =
          entities::toEntity(*object.tio.entity->tio.ELLIPSE);
      ellipse.common.color = entityColor;
      appendVectorPrimitive(ellipse, "DWG_ELLIPSE", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_POINT:
    {
      entities::Point point =
          entities::toEntity(*object.tio.entity->tio.POINT);
      point.common.color = entityColor;
      appendVectorPrimitive(point, "DWG_POINT", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_RAY:
    {
      entities::Ray ray = entities::toEntity(*object.tio.entity->tio.RAY);
      ray.common.color = entityColor;
      appendVectorPrimitive(ray, "DWG_RAY", options, target);
      ++appended;
      break;
    }
    case DWG_TYPE_XLINE:
    {
      entities::XLine xline;
      const entities::Ray ray =
          entities::toEntity(*object.tio.entity->tio.RAY);
      xline.point = ray.start;
      xline.direction = ray.direction;
      xline.common.color = entityColor;
      appendVectorPrimitive(xline, "DWG_XLINE", options, target);
      ++appended;
      break;
    }
    default:
      break;
    }
  }
  std::cout << "GRID_DWG: appended " << appended << " entities from "
            << path << std::endl;
  dwg_free(&dwg);
}

// The CAD vector demo is authored as formal entities.  The cached draw list is
// shared by drawing and CPU picking; dynamic documents replace this builder's
// revision with a dirty-document notification.
VectorPrimitivesTessellation buildVectorPrimitivesTessellation()
{
  VectorPrimitivesTessellation target;
  entities::TessellatedEntity &result = target.geometry;
  if (!cadEntityDemoEnabled())
    return target;

    const glm::dvec3 cadAnchor =
        vectorPrimitivesAnchor() + glm::dvec3(1536.0, -1280.0, 0.0);
    target.anchor = cadAnchor;
    const entities::TesselationOptions options;

    entities::Line line;
    line.common.color = glm::vec4(1.0f, 0.24f, 0.20f, 1.0f);
    line.start = cadAnchor;
    line.end = cadAnchor + glm::dvec3(768.0, 0.0, 0.0);
    appendVectorPrimitive(line, "Line", options, target);

    entities::Arc arc;
    arc.common.color = glm::vec4(1.0f, 0.52f, 0.10f, 1.0f);
    arc.center = cadAnchor + glm::dvec3(1024.0, 256.0, 0.0);
    arc.radius = 192.0;
    arc.startAngle = 0.0;
    arc.endAngle = glm::radians(270.0);
    appendVectorPrimitive(arc, "Arc", options, target);

    entities::Circle circle;
    circle.common.color = glm::vec4(0.20f, 0.60f, 0.90f, 1.0f);
    circle.center = cadAnchor + glm::dvec3(256.0, 512.0, 0.0);
    circle.radius = 160.0;
    appendVectorPrimitive(circle, "Circle", options, target);

    entities::Ellipse ellipse;
    ellipse.common.color = glm::vec4(0.65f, 0.30f, 0.85f, 1.0f);
    ellipse.center = cadAnchor + glm::dvec3(768.0, 640.0, 0.0);
    ellipse.majorAxis = glm::dvec3(224.0, 0.0, 0.0);
    ellipse.radiusRatio = 0.55;
    appendVectorPrimitive(ellipse, "Ellipse", options, target);

    // Partial ellipse: the parameter range draws an elliptical arc instead
    // of the closed curve.
    entities::Ellipse ellipseArc;
    ellipseArc.common.color = glm::vec4(0.75f, 0.40f, 0.95f, 1.0f);
    ellipseArc.center = cadAnchor + glm::dvec3(1536.0, -640.0, 0.0);
    ellipseArc.majorAxis = glm::dvec3(160.0, 0.0, 0.0);
    ellipseArc.radiusRatio = 0.55;
    ellipseArc.startParameter = glm::pi<double>() * 0.25;
    ellipseArc.endParameter = glm::pi<double>() * 1.75;
    appendVectorPrimitive(ellipseArc, "EllipseArc", options, target);

    entities::Polyline polyline;
    polyline.common.color = glm::vec4(0.60f, 0.20f, 1.00f, 1.0f);
    polyline.vertices = {
        cadAnchor + glm::dvec3(-384.0, 128.0, 0.0),
        cadAnchor + glm::dvec3(-128.0, 384.0, 0.0),
        cadAnchor + glm::dvec3(128.0, 256.0, 0.0),
        cadAnchor + glm::dvec3(384.0, 512.0, 0.0)};
    polyline.bulges = {0.25, 0.0, -0.35};
    appendVectorPrimitive(polyline, "Polyline", options, target);

    entities::LwPolyline lwpolyline;
    lwpolyline.common.color = glm::vec4(0.25f, 0.80f, 0.45f, 1.0f);
    lwpolyline.vertices = {glm::dvec2(-256.0, -384.0),
                           glm::dvec2(0.0, -192.0),
                           glm::dvec2(256.0, -448.0)};
    lwpolyline.elevation = cadAnchor.z;
    lwpolyline.closed = true;
    for (AcGePoint2d &vertex : lwpolyline.vertices)
      vertex += glm::dvec2(cadAnchor);
    appendVectorPrimitive(lwpolyline, "LwPolyline", options, target);

    entities::Spline spline;
    spline.common.color = glm::vec4(0.90f, 0.70f, 0.20f, 1.0f);
    spline.degree = 3;
    spline.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    spline.controlPoints = {
        cadAnchor + glm::dvec3(-768.0, 512.0, 0.0),
        cadAnchor + glm::dvec3(-512.0, 896.0, 128.0),
        cadAnchor + glm::dvec3(-256.0, 384.0, -128.0),
        cadAnchor + glm::dvec3(0.0, 768.0, 0.0)};
    appendVectorPrimitive(spline, "Spline", options, target);

    entities::Hatch hatch;
    hatch.common.color = glm::vec4(0.30f, 0.80f, 0.50f, 0.75f);
    hatch.outerLoop = {
        cadAnchor + glm::dvec3(1024.0, -384.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -384.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -128.0, 0.0),
        cadAnchor + glm::dvec3(1024.0, -128.0, 0.0)};
    appendVectorPrimitive(hatch, "Hatch", options, target);

    // ANSI31 line pattern at unit scale; the inner loop punches a hole via
    // the even-odd rule.
    entities::Hatch patternHatch;
    patternHatch.common.color = glm::vec4(0.30f, 0.80f, 0.50f, 0.90f);
    patternHatch.solidFill = false;
    patternHatch.patternName = "ANSI31";
    patternHatch.patternScale = 1.0;
    patternHatch.patternAngle = 0.0;
    patternHatch.outerLoop = {
        cadAnchor + glm::dvec3(1024.0, -768.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -768.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -512.0, 0.0),
        cadAnchor + glm::dvec3(1024.0, -512.0, 0.0)};
    patternHatch.innerLoops = {{
        cadAnchor + glm::dvec3(1152.0, -704.0, 0.0),
        cadAnchor + glm::dvec3(1280.0, -704.0, 0.0),
        cadAnchor + glm::dvec3(1280.0, -576.0, 0.0),
        cadAnchor + glm::dvec3(1152.0, -576.0, 0.0)}};
    appendVectorPrimitive(patternHatch, "PatternHatch", options, target);

    // Same pattern family rotated 45 degrees and widened by patternScale.
    entities::Hatch angledHatch;
    angledHatch.common.color = glm::vec4(0.35f, 0.55f, 0.95f, 0.90f);
    angledHatch.solidFill = false;
    angledHatch.patternName = "ANSI31";
    angledHatch.patternScale = 2.0;
    angledHatch.patternAngle = glm::pi<double>() * 0.25;
    angledHatch.outerLoop = {
        cadAnchor + glm::dvec3(1024.0, -1024.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -1024.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -832.0, 0.0),
        cadAnchor + glm::dvec3(1024.0, -832.0, 0.0)};
    appendVectorPrimitive(angledHatch, "AngledHatch", options, target);

    entities::Solid solid;
    solid.common.color = glm::vec4(0.50f, 0.50f, 0.90f, 0.85f);
    solid.firstCorner = cadAnchor + glm::dvec3(0.0, -128.0, 256.0);
    solid.secondCorner = solid.firstCorner + glm::dvec3(512.0, 0.0, 0.0);
    solid.thirdCorner = solid.firstCorner + glm::dvec3(0.0, 384.0, 0.0);
    solid.fourthCorner = solid.firstCorner + glm::dvec3(512.0, 384.0, 0.0);
    appendVectorPrimitive(solid, "Solid", options, target);

    entities::Ray ray;
    ray.common.color = glm::vec4(0.10f, 0.85f, 0.75f, 1.0f);
    ray.start = cadAnchor + glm::dvec3(-640.0, -768.0, 0.0);
    ray.direction = glm::dvec3(1.0, 0.25, 0.0);
    appendVectorPrimitive(ray, "Ray", options, target);

    entities::XLine xline;
    xline.common.color = glm::vec4(0.65f, 0.35f, 0.95f, 1.0f);
    xline.point = cadAnchor + glm::dvec3(256.0, -1152.0, 0.0);
    xline.direction = glm::dvec3(2.0, -1.0, 0.0);
    appendVectorPrimitive(xline, "XLine", options, target);

    // SHX vector-font text: each glyph's stroke list becomes ordinary CAD
    // strokes, so the full pipeline (ribbons, picking, outlines, visual
    // styles) applies to text exactly like any other entity.  whgdtxt is
    // an AutoCAD big-font covering ASCII + CJK punctuation.
    if (gShxFontReady)
    {
      const std::string shxText = "中文 INFINITE - GRID 123";
      // SHX glyphs hang below their anchor (cap line at the anchor,
      // baseline one em down): raise the anchor one em above the ground.
      const glm::dvec3 textOrigin =
          cadAnchor + glm::dvec3(256.0, 128.0, 96.0);
      const double textHeight = 96.0; // world units per em
      const glm::dvec3 textRight(1.0, 0.0, 0.0);
      const glm::dvec3 textUp(0.0, 0.0, 1.0);
      const glm::vec4 shxColor(0.95f, 0.85f, 0.30f, 1.0f);

      // One entities::Stroke PER glyph stroke segment: a single stroke
      // with all points would connect consecutive segments into spurious
      // pen-up lines ("连笔") across and within glyphs.
      const size_t strokeBegin = target.geometry.strokes.size();
      for (const rendering::ShxGlyphStroke &glyphStroke :
           acgi::textEngine().shxStrokes(shxText))
      {
        entities::Stroke segment;
        segment.common.color = shxColor;
        segment.points = {
            textOrigin + textRight * (glyphStroke.fromX * textHeight) +
                textUp * (glyphStroke.fromY * textHeight),
            textOrigin + textRight * (glyphStroke.toX * textHeight) +
                textUp * (glyphStroke.toY * textHeight)};
        target.geometry.strokes.push_back(std::move(segment));
      }
      if (target.geometry.strokes.size() > strokeBegin)
      {
        // Register the range so visibility/pick treat the text as one
        // entity.
        target.strokeRanges.push_back(
            {"ShxText", strokeBegin,
             target.geometry.strokes.size() - strokeBegin,
             CadPickShape::Primitives});
      }
    }

    entities::MLine mline;
    mline.common.color = glm::vec4(0.85f, 0.35f, 0.35f, 0.95f);
    mline.vertices = {
        cadAnchor + glm::dvec3(-256.0, -768.0, 0.0),
        cadAnchor + glm::dvec3(256.0, -704.0, 0.0),
        cadAnchor + glm::dvec3(768.0, -832.0, 0.0)};
    mline.scale = glm::dvec3(24.0, 1.0, 1.0);
    appendVectorPrimitive(
        mline, "MLine", options, target, false,
        CadEntityPickShape::PairedStrokeBand);

    // Closed multi-line: the offset band wraps around and the enclosed
    // strip is filled between the two boundary strokes.
    entities::MLine closedMLine;
    closedMLine.common.color = glm::vec4(0.55f, 0.75f, 0.95f, 0.95f);
    closedMLine.vertices = {
        cadAnchor + glm::dvec3(1472.0, -880.0, 0.0),
        cadAnchor + glm::dvec3(1728.0, -960.0, 0.0),
        cadAnchor + glm::dvec3(1600.0, -1088.0, 0.0)};
    closedMLine.scale = glm::dvec3(40.0, 1.0, 1.0);
    closedMLine.closed = true;
    appendVectorPrimitive(
        closedMLine, "ClosedMLine", options, target, false,
        CadEntityPickShape::PairedStrokeBand);

    // Closed polyline with non-zero thickness: the outline extrudes into
    // wall quads along the normal.
    entities::Polyline borderedPolyline;
    borderedPolyline.common.color = glm::vec4(0.95f, 0.45f, 0.15f, 1.0f);
    borderedPolyline.vertices = {
        cadAnchor + glm::dvec3(1792.0, -576.0, 0.0),
        cadAnchor + glm::dvec3(1984.0, -576.0, 0.0),
        cadAnchor + glm::dvec3(1984.0, -736.0, 0.0),
        cadAnchor + glm::dvec3(1792.0, -736.0, 0.0)};
    borderedPolyline.closed = true;
    borderedPolyline.thickness = 48.0;
    appendVectorPrimitive(
        borderedPolyline, "BorderedPolyline", options, target);

    // Fit-point splines: the C1 fallback interpolates the fit points, open
    // and closed forms.
    entities::Spline fitSpline;
    fitSpline.common.color = glm::vec4(0.90f, 0.70f, 0.20f, 1.0f);
    fitSpline.degree = 3;
    fitSpline.fitPoints = {
        cadAnchor + glm::dvec3(1024.0, -1152.0, 0.0),
        cadAnchor + glm::dvec3(1152.0, -1024.0, 0.0),
        cadAnchor + glm::dvec3(1280.0, -1216.0, 0.0),
        cadAnchor + glm::dvec3(1408.0, -1088.0, 0.0)};
    appendVectorPrimitive(fitSpline, "FitSpline", options, target);

    entities::Spline closedFitSpline;
    closedFitSpline.common.color = glm::vec4(0.55f, 0.85f, 0.25f, 1.0f);
    closedFitSpline.degree = 3;
    closedFitSpline.closed = true;
    closedFitSpline.fitPoints = {
        cadAnchor + glm::dvec3(1024.0, -1408.0, 160.0),
        cadAnchor + glm::dvec3(1152.0, -1296.0, -160.0),
        cadAnchor + glm::dvec3(1312.0, -1424.0, 288.0),
        cadAnchor + glm::dvec3(1152.0, -1520.0, -96.0)};
    appendVectorPrimitive(closedFitSpline, "ClosedFitSpline", options, target);

    // The closed ring through the same points: the interpolation visibly
    // smooths the corners of this reference polygon.
    entities::Polyline closedFitRing;
    closedFitRing.common.color = glm::vec4(0.55f, 0.55f, 0.55f, 0.9f);
    closedFitRing.vertices = closedFitSpline.fitPoints;
    closedFitRing.closed = true;
    appendVectorPrimitive(closedFitRing, "ClosedFitRing", options, target);

    // Non-planar fit spline: the fit points span all three dimensions, so
    // the curve bends out of the ground plane (the fit-point demos above
    // are flat for comparison).
    entities::Spline spaceSpline;
    spaceSpline.common.color = glm::vec4(0.30f, 0.65f, 0.95f, 1.0f);
    spaceSpline.degree = 3;
    spaceSpline.fitPoints = {
        cadAnchor + glm::dvec3(1024.0, -1152.0, 480.0),
        cadAnchor + glm::dvec3(1152.0, -1024.0, -480.0),
        cadAnchor + glm::dvec3(1280.0, -1216.0, 768.0),
        cadAnchor + glm::dvec3(1408.0, -1088.0, -320.0),
        cadAnchor + glm::dvec3(1536.0, -1216.0, 640.0)};
    appendVectorPrimitive(spaceSpline, "SpaceSpline", options, target);

    entities::Point cadPoint;
    cadPoint.common.color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    cadPoint.location = cadAnchor + glm::dvec3(512.0, 0.0, 0.0);
    appendVectorPrimitive(cadPoint, "Point", options, target);

    entities::Text text;
    text.common.color = glm::vec4(0.95f, 0.95f, 0.30f, 1.0f);
    text.insertion = cadAnchor + glm::dvec3(1152.0, 128.0, 384.0);
    text.height = 96.0;
    text.text = "中文 TEXT";
    appendVectorPrimitive(text, "Text", options, target);

    entities::MText mtext;
    mtext.common.color = glm::vec4(0.35f, 0.90f, 0.95f, 1.0f);
    mtext.insertion = cadAnchor + glm::dvec3(1152.0, 320.0, 384.0);
    mtext.direction = glm::dvec3(1.0, 0.0, 0.0);
    mtext.height = 64.0;
    mtext.text = "中文 MTEXT\nDEMO";
    appendVectorPrimitive(mtext, "MText", options, target);

    // Text locators: bright vertical lines pointing at each text insertion
    // point, so the glyph position can be found from any distance while
    // debugging the SDF path.
    entities::Line textLocator;
    textLocator.common.color = glm::vec4(1.0f, 0.20f, 0.90f, 1.0f);
    textLocator.common.lineWeight = 3.0;
    textLocator.start = text.insertion + glm::dvec3(0.0, 0.0, -320.0);
    textLocator.end = text.insertion;
    appendVectorPrimitive(textLocator, "TextLocator", options, target);
    entities::Line mtextLocator;
    mtextLocator.common.color = glm::vec4(1.0f, 0.20f, 0.90f, 1.0f);
    mtextLocator.common.lineWeight = 3.0;
    mtextLocator.start = mtext.insertion + glm::dvec3(0.0, 0.0, -320.0);
    mtextLocator.end = mtext.insertion;
    appendVectorPrimitive(mtextLocator, "MTextLocator", options, target);

    entities::Solid3d solid3d;
    solid3d.common.color = glm::vec4(0.75f, 0.65f, 0.25f, 1.0f);
    solid3d.renderClass = entities::RenderClass::Cad;
    const glm::dvec3 boxMin = cadAnchor + glm::dvec3(1536.0, 128.0, 384.0);
    const glm::dvec3 boxMax = boxMin + glm::dvec3(256.0, 256.0, 256.0);
    solid3d.vertices = {
        glm::dvec3(boxMin.x, boxMin.y, boxMin.z),
        glm::dvec3(boxMax.x, boxMin.y, boxMin.z),
        glm::dvec3(boxMax.x, boxMax.y, boxMin.z),
        glm::dvec3(boxMin.x, boxMax.y, boxMin.z),
        glm::dvec3(boxMin.x, boxMin.y, boxMax.z),
        glm::dvec3(boxMax.x, boxMin.y, boxMax.z),
        glm::dvec3(boxMax.x, boxMax.y, boxMax.z),
        glm::dvec3(boxMin.x, boxMax.y, boxMax.z)
    };
    solid3d.indices = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6,
        3, 0, 4, 3, 4, 7
    };
    appendVectorPrimitive(solid3d, "Solid3d", options, target, true);

    entities::Light light;
    light.common.color = glm::vec4(1.0f, 0.90f, 0.55f, 1.0f);
    light.type = entities::LightType::Spot;
    light.position = cadAnchor + glm::dvec3(-1152.0, 256.0, 512.0);
    light.target = cadAnchor + glm::dvec3(-768.0, 0.0, 0.0);
    light.range = 1024.0f;
    appendVectorPrimitive(light, "Light", options, target);

    // Former in-function demo geometry.  These are now ordinary CAD entities,
    // so the same tessellation drives rendering, names, and autofocus picking.
    const glm::dvec3 demoAnchor = vectorPrimitivesAnchor();

    for (int i = 0; i < 24; ++i)
    {
      entities::Point gridPoint;
      gridPoint.common.color = glm::vec4(
          0.2f + 0.03f * i, 0.9f - 0.025f * i, 0.3f + 0.02f * i, 1.0f);
      gridPoint.common.lineWeight = 6.0;
      gridPoint.location = demoAnchor +
          glm::dvec3((i % 8) * 96.0, (i / 8) * 96.0 - 640.0, 0.0) +
          glm::dvec3(1024.0, 0.0, 0.0);
      appendVectorPrimitive(
          gridPoint, ("PointGrid" + std::to_string(i)).c_str(), options, target);
    }

    entities::Line dashedArrow;
    dashedArrow.common.color = glm::vec4(0.95f, 0.25f, 0.75f, 1.0f);
    dashedArrow.common.lineType = "DASHED";
    dashedArrow.common.lineWeight = 2.5;
    dashedArrow.start = demoAnchor + glm::dvec3(-1024.0, -640.0, -512.0);
    dashedArrow.end = dashedArrow.start + glm::dvec3(1024.0, 256.0, 0.0);
    appendVectorPrimitive(dashedArrow, "DashedArrow", options, target);

    {
      const glm::dvec3 dir = glm::dvec3(
          (dashedArrow.end - dashedArrow.start).normal());
      const glm::dvec3 side =
          glm::normalize(glm::cross(dir, glm::dvec3(0.0, 0.0, 1.0))) * 24.0;
      const glm::dvec3 base = dashedArrow.end - AcGeVector3d(dir) * 48.0;
      entities::Solid arrowHead;
      arrowHead.common = dashedArrow.common;
      arrowHead.firstCorner = dashedArrow.end;
      arrowHead.secondCorner = base - side;
      arrowHead.thirdCorner = base + side;
      arrowHead.fourthCorner = base + side;
      appendVectorPrimitive(arrowHead, "ArrowHead", options, target);
    }

    if (paramSurfaceDemoEnabled())
    {
      const int surfaceSegs = paramSurfaceSegmentCount();
      const glm::dvec3 surfaceOrigin =
          demoAnchor + glm::dvec3(512.0, 768.0, -512.0);
      auto surfacePoint = [&](double u, double v) {
        const double height = glm::sin(glm::pi<double>() * u) *
                              glm::sin(glm::pi<double>() * v);
        return surfaceOrigin +
               glm::dvec3(u * 768.0, height * 224.0, v * 512.0);
      };

      entities::Mesh surface;
      surface.common.color = glm::vec4(0.20f, 0.45f, 0.85f, 0.80f);
      surface.common.layer = "GRID_FILL_LAYER";
      surface.style = entities::MeshStyle::Cad;
      surface.geometry.positions.reserve(
          static_cast<size_t>(surfaceSegs + 1) * (surfaceSegs + 1));
      for (int iy = 0; iy <= surfaceSegs; ++iy)
      {
        for (int ix = 0; ix <= surfaceSegs; ++ix)
        {
          surface.geometry.positions.push_back(surfacePoint(
              double(ix) / surfaceSegs, double(iy) / surfaceSegs));
        }
      }
      for (int iy = 0; iy < surfaceSegs; ++iy)
      {
        for (int ix = 0; ix < surfaceSegs; ++ix)
        {
          const uint32_t v00 = uint32_t(iy * (surfaceSegs + 1) + ix);
          const uint32_t v10 = v00 + 1;
          const uint32_t v01 = v00 + surfaceSegs + 1;
          const uint32_t v11 = v01 + 1;
          surface.geometry.indices.insert(
              surface.geometry.indices.end(),
              {v00, v10, v11, v00, v11, v01});
        }
      }
      // ParamSurface uses solid-fill semantics, so its tessellated face stays
      // visible in 2D wireframe like Solid/Hatch fills instead of being treated
      // as a 3D face.
      appendVectorPrimitive(surface, "ParamSurface", options, target, false);

      if (paramSurfaceIsolinesEnabled())
      for (int k = 0; k <= surfaceSegs; k += 4)
      {
        entities::Polyline isoU;
        entities::Polyline isoV;
        isoU.common.color = glm::vec4(0.05f, 0.10f, 0.25f, 0.85f);
        isoU.common.lineWeight = 2.0;
        isoV = isoU;
        const double g = double(k) / surfaceSegs;
        for (int i = 0; i <= surfaceSegs; ++i)
        {
          const double t = double(i) / surfaceSegs;
          isoU.vertices.push_back(surfacePoint(t, g));
          isoV.vertices.push_back(surfacePoint(g, t));
        }
        appendVectorPrimitive(
            isoU, ("ParamSurfaceIsoU" + std::to_string(k)).c_str(),
            options, target);
        appendVectorPrimitive(
            isoV, ("ParamSurfaceIsoV" + std::to_string(k)).c_str(),
            options, target);
      }
    }

    for (int i = 0; i < 5; ++i)
    {
      entities::Point widthDot;
      widthDot.common.color = glm::vec4(
          0.2f + i * 0.15f, 0.9f - i * 0.1f, 0.5f + i * 0.05f, 1.0f);
      widthDot.common.lineWeight = 12.0;
      widthDot.location =
          demoAnchor + glm::dvec3(i * 256.0, -768.0, 0.0);
      appendVectorPrimitive(
          widthDot, ("WidthDot" + std::to_string(i)).c_str(), options, target);
    }

    {
      const float widths[] = {1.0f, 2.0f, 4.0f, 8.0f};
      const glm::vec4 colors[] = {
          {1.0f, 0.2f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.2f, 1.0f},
          {0.2f, 0.4f, 1.0f, 1.0f}, {1.0f, 0.8f, 0.2f, 1.0f}};
      for (int i = 0; i < 4; ++i)
      {
        entities::Line widthLine;
        widthLine.common.color = colors[i];
        widthLine.common.lineWeight = widths[i];
        widthLine.start =
            demoAnchor + glm::dvec3(-1024.0, -384.0 + i * 192.0, -512.0);
        widthLine.end = widthLine.start + glm::dvec3(1024.0, 0.0, 0.0);
        appendVectorPrimitive(
            widthLine, ("WidthLine" + std::to_string(i)).c_str(),
            options, target);
      }
    }

    {
      const char *lineTypes[] = {"CONTINUOUS", "DASHED", "HIDDEN", "CENTER",
                                 "DOT", "PHANTOM"};
      const glm::vec4 lineColors[] = {
          {0.85f, 0.85f, 0.85f, 1.0f}, {0.95f, 0.25f, 0.75f, 1.0f},
          {0.20f, 0.80f, 0.80f, 1.0f}, {1.00f, 0.70f, 0.20f, 1.0f},
          {0.60f, 0.60f, 1.00f, 1.0f}, {0.80f, 0.20f, 0.20f, 1.0f}};
      for (int i = 0; i < 6; ++i)
      {
        entities::Line specimen;
        specimen.common.color = lineColors[i];
        specimen.common.lineType = lineTypes[i];
        specimen.common.lineWeight = 24.0;
        specimen.start =
            demoAnchor + glm::dvec3(-1024.0, 1536.0 + i * 256.0, -3072.0);
        specimen.end = specimen.start + glm::dvec3(2048.0, 0.0, 0.0);
        appendVectorPrimitive(
            specimen, (std::string(lineTypes[i]) + "Line").c_str(),
            options, target);
      }
    }

    entities::Polyline demoPolyline;
    demoPolyline.common.color = glm::vec4(0.60f, 0.20f, 1.00f, 1.0f);
    demoPolyline.common.lineWeight = 4.0;
    const glm::dvec3 polylineBase =
        demoAnchor + glm::dvec3(-512.0, 0.0, -512.0);
    demoPolyline.vertices = {
        polylineBase,
        polylineBase + glm::dvec3(256.0, 256.0, 0.0),
        polylineBase + glm::dvec3(512.0, 128.0, 256.0),
        polylineBase + glm::dvec3(768.0, 384.0, 0.0)};
    appendVectorPrimitive(demoPolyline, "DemoPolyline", options, target);

    entities::Hatch hexagon;
    hexagon.common.color = glm::vec4(0.30f, 0.80f, 0.50f, 1.0f);
    hexagon.outerLoop.reserve(6);
    for (int i = 0; i < 6; ++i)
    {
      const double angle = glm::two_pi<double>() * i / 6.0;
      hexagon.outerLoop.push_back(
          demoAnchor + glm::dvec3(0.0, 256.0, -512.0) +
          glm::dvec3(128.0 * std::cos(angle), 128.0 * std::sin(angle), 0.0));
    }
    appendVectorPrimitive(hexagon, "Hexagon", options, target);

    {
      entities::Hatch circleFill;
      circleFill.common.color = glm::vec4(0.20f, 0.60f, 0.90f, 1.0f);
      constexpr int circleSegments = 48;
      circleFill.outerLoop.reserve(circleSegments);
      for (int i = 0; i < circleSegments; ++i)
      {
        const double angle = glm::two_pi<double>() * i / circleSegments;
        circleFill.outerLoop.push_back(
            demoAnchor + glm::dvec3(256.0, 512.0, 256.0) +
            glm::dvec3(160.0 * std::cos(angle),
                       160.0 * std::sin(angle), 0.0));
      }
      appendVectorPrimitive(circleFill, "CircleFill", options, target);
    }

    entities::Solid rectangle;
    rectangle.common.color = glm::vec4(0.50f, 0.50f, 0.90f, 1.0f);
    rectangle.firstCorner =
        demoAnchor + glm::dvec3(-256.0, -128.0, 256.0);
    rectangle.secondCorner = rectangle.firstCorner + glm::dvec3(512.0, 0.0, 0.0);
    rectangle.thirdCorner = rectangle.firstCorner + glm::dvec3(0.0, 384.0, 0.0);
    rectangle.fourthCorner =
        rectangle.firstCorner + glm::dvec3(512.0, 384.0, 0.0);
    appendVectorPrimitive(rectangle, "Rectangle", options, target);

    entities::Spline bezier;
    bezier.common.color = glm::vec4(0.90f, 0.70f, 0.20f, 1.0f);
    bezier.common.lineWeight = 6.0;
    bezier.degree = 3;
    bezier.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    const glm::dvec3 bezierBase =
        demoAnchor + glm::dvec3(-768.0, 512.0, 0.0);
    bezier.controlPoints = {
        bezierBase,
        bezierBase + glm::dvec3(256.0, 384.0, 128.0),
        bezierBase + glm::dvec3(512.0, -128.0, -128.0),
        bezierBase + glm::dvec3(768.0, 256.0, 0.0)};
    appendVectorPrimitive(bezier, "Bezier", options, target);

    auto addCurveDemo = [&target](rendering::CurveAlgorithm algorithm,
                                   const char *name,
                                   const glm::vec4 &color,
                                   std::vector<glm::dvec3> controlPoints,
                                   int degree = 3,
                                   std::vector<double> weights = {}) {
      scene::CurveBatchCommand &curve = target.curves.emplace_back();
      curve.algorithm = algorithm;
      curve.name = name;
      curve.degree = degree;
      curve.sampleCount = 192;
      curve.controlPoints = std::move(controlPoints);
      curve.weights = std::move(weights);
      curve.acgiMaterial.algorithm = scene::AcGiShaderAlgorithm::Shaded;
      curve.acgiMaterial.baseColor = color;
    };

    const glm::dvec3 curveCenter = demoAnchor;
    addCurveDemo(rendering::CurveAlgorithm::Bezier, "BezierCurve",
                 glm::vec4(0.15f, 1.0f, 0.55f, 1.0f),
                 {curveCenter + glm::dvec3(-240.0, -120.0, 0.0),
                  curveCenter + glm::dvec3(-80.0, 220.0, 0.0),
                  curveCenter + glm::dvec3(80.0, -220.0, 0.0),
                  curveCenter + glm::dvec3(240.0, 120.0, 0.0)});
    addCurveDemo(rendering::CurveAlgorithm::BSpline, "BSplineCurve",
                 glm::vec4(0.20f, 0.62f, 1.00f, 1.0f),
                 {curveCenter + glm::dvec3(-320.0, -180.0, 0.0),
                  curveCenter + glm::dvec3(-140.0, 180.0, 0.0),
                  curveCenter + glm::dvec3(0.0, -140.0, 0.0),
                  curveCenter + glm::dvec3(140.0, 180.0, 0.0),
                  curveCenter + glm::dvec3(320.0, -180.0, 0.0)});
    addCurveDemo(rendering::CurveAlgorithm::NURBS, "NurbsCurve",
                 glm::vec4(0.95f, 0.82f, 0.25f, 1.0f),
                 {curveCenter + glm::dvec3(-280.0, 260.0, 0.0),
                  curveCenter + glm::dvec3(-110.0, -260.0, 0.0),
                  curveCenter + glm::dvec3(110.0, 260.0, 0.0),
                  curveCenter + glm::dvec3(280.0, -260.0, 0.0)},
                 3, {1.0, 2.0, 2.0, 1.0});

    scene::CurveBatchCommand &curveArc = target.curves.emplace_back();
    curveArc.algorithm = rendering::CurveAlgorithm::Arc;
    curveArc.name = "CurveArc";
    curveArc.sampleCount = 192;
    curveArc.center = curveCenter;
    curveArc.axisU = glm::dvec3(1.0, 0.0, 0.0);
    curveArc.axisV = glm::dvec3(0.0, 1.0, 0.0);
    curveArc.radius = 360.0;
    curveArc.startAngle = -0.35;
    curveArc.sweep = 1.60;
    curveArc.acgiMaterial.algorithm = scene::AcGiShaderAlgorithm::Shaded;
    curveArc.acgiMaterial.baseColor = glm::vec4(0.92f, 0.35f, 0.72f, 1.0f);

    {
      // Demo meshes remain formal entities, but stay out of the default CAD
      // scene.  They are available only for explicit rendering/pick debugging.
      if (!demoMeshesEnabled())
          // Decoded DWG models ride the same pipeline when GRID_DWG names a file.
  if (const char *dwgPath = std::getenv("GRID_DWG"))
    appendDwgFile(dwgPath, options, target);

return target;

      MeshEntityRecord debugCube;
      debugCube.entity.common.name = "RedDebugCube";
      debugCube.entity.common.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      debugCube.worldPosition = demoAnchor;
      debugCube.size = 200.0f;
      target.meshes.push_back(std::move(debugCube));

      const glm::dvec3 solidBase =
          demoAnchor + glm::dvec3(0.0, 768.0, 512.0);
      const rendering::MeshType meshTypes[] = {
          rendering::MeshType::Cube, rendering::MeshType::Sphere,
          rendering::MeshType::Cone, rendering::MeshType::Torus};
      const char *meshNames[] = {
          "DemoCube", "DemoSphere", "DemoCone", "DemoTorus"};
      for (size_t index = 0; index < std::size(meshTypes); ++index)
      {
        MeshEntityRecord mesh;
        mesh.entity.common.name = meshNames[index];
        mesh.entity.common.color = glm::vec4(0.8f, 0.7f, 0.9f, 0.65f);
        mesh.worldPosition = solidBase +
            glm::dvec3((index % 2) * 512.0, (index / 2) * 512.0, 0.0);
        mesh.size = 192.0f;
        mesh.mesh = meshTypes[index];
        target.meshes.push_back(std::move(mesh));
      }
    }
  return target;
}

const VectorPrimitivesTessellation &getVectorPrimitivesTessellation()
{
  static constexpr std::uint64_t cadDemoRevision = 5;
  static constexpr std::uint64_t cadDemoTraitsVersion = 1;
  static constexpr std::uint32_t cadDemoToleranceBucket = 0;
  const scene::DrawListKey key{
      cadDemoRevision, cadDemoTraitsVersion,
      vectorPrimitivesAnchor(), cadDemoToleranceBucket};
  static scene::DrawListCache cache;
  return cache.get(key, buildVectorPrimitivesTessellation);
}

static uint64_t cadGpuPickGeometryKey(const CadEntityRange *range,
                                      size_t chunkIndex,
                                      uint32_t objectId)
{
  const uint64_t pointer =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(range));
  return pointer * 0x9e3779b97f4a7c15ULL +
         (static_cast<uint64_t>(objectId) << 32) +
         static_cast<uint64_t>(chunkIndex);
}

// CAD fill tessellation is immutable.  Cache each entity's pick soup in
// anchor-relative coordinates so repeated GPU pick requests reuse the same
// renderer-side vertex buffer instead of rebuilding large-coordinate copies.
// The anchor-relative frame is camera-independent, so the cached buffers stay
// valid; cadAnchorView supplies the camera-dependent translation.
// Visible CAD fills are immutable once tessellated.  Cache the renderer-side
// camera-relative vertices per entity range so normal drawing does not repeat
// the per-triangle double-to-float conversion and color contrast pass every
// frame.  The anchor-relative frame is camera-independent.
static const std::vector<rendering::FillVertex> &
cadVisibleFillVertices(const CadEntityRange &range)
{
  static std::map<const CadEntityRange *, std::vector<rendering::FillVertex>>
      cache;
  auto [it, inserted] = cache.try_emplace(&range);
  if (!inserted)
    return it->second;

  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  std::vector<rendering::FillVertex> &vertices = it->second;
  vertices.reserve(range.count * 3);
  for (size_t i = range.begin; i < range.begin + range.count; ++i)
  {
    const entities::Triangle &triangle = tessellation.geometry.fills[i];
    if (!triangle.common.visible)
      continue;
    const glm::vec4 fillColor =
        contrastAgainstBackground(triangle.common.color);
    vertices.push_back(
        {glm::vec3(triangle.a - tessellation.anchor), fillColor});
    vertices.push_back(
        {glm::vec3(triangle.b - tessellation.anchor), fillColor});
    vertices.push_back(
        {glm::vec3(triangle.c - tessellation.anchor), fillColor});
  }
  return vertices;
}

static const std::vector<rendering::FillVertex> &
cadGpuPickFillVertices(const CadEntityRange &range,
                       const glm::vec4 &idColor, uint32_t objectId)
{
  static std::map<std::pair<const CadEntityRange *, uint32_t>,
                  std::vector<rendering::FillVertex>>
      cache;
  auto [it, inserted] = cache.try_emplace({&range, objectId});
  if (!inserted)
    return it->second;

  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  std::vector<rendering::FillVertex> &vertices = it->second;
  vertices.reserve(range.count * 3);
  for (size_t i = range.begin; i < range.begin + range.count; ++i)
  {
    const entities::Triangle &triangle = tessellation.geometry.fills[i];
    if (!triangle.common.visible)
      continue;
    vertices.push_back(
        {glm::vec3(triangle.a - tessellation.anchor), idColor});
    vertices.push_back(
        {glm::vec3(triangle.b - tessellation.anchor), idColor});
    vertices.push_back(
        {glm::vec3(triangle.c - tessellation.anchor), idColor});
  }
  return vertices;
}

std::vector<glm::dvec3> sampleCurveBatch(const scene::CurveBatchCommand &curve);
int currentDrawableHeight();


struct CadPairedBandPoints
{
    const entities::Stroke *left = nullptr;
    const entities::Stroke *right = nullptr;
    size_t segmentCount = 0;
};

bool cadPairedBandPoints(const CadEntityRange &range,
                         const entities::TessellatedEntity &tess,
                         CadPairedBandPoints &band);

static void appendOutlineRibbon(
    std::vector<rendering::PrimVertex> &vertices, const glm::vec3 &start,
    const glm::vec3 &end, const glm::vec3 &front, float halfWidth, float u0,
    float u1, bool centered,
    const glm::vec4 &color = kOutlineColor);

static void drawVectorPrimitivesDemo(const glm::mat4 &view,
                                     const glm::mat4 &projection,
                                     const glm::mat4 &overlayProjection,
                                     const glm::dvec3 &rebase,
                                     const glm::vec4 &logDepth,
                                     const glm::dvec3 &cameraPos,
                                     const glm::dvec3 &cameraFront,
                                     const glm::dvec3 &cameraRightD,
                                     const glm::dvec3 &cameraUpD,
                                     float pixelSizeWorld,
                                     const std::vector<const VisibilityCandidate *> &visibleCad,
                                     const std::vector<const VisibilityCandidate *> &tinyCad) {
  if (visibleCad.empty() && tinyCad.empty()) return;

  const glm::vec3 camRight = glm::normalize(glm::vec3(cameraRightD));
  const glm::vec3 camUp = glm::normalize(glm::vec3(cameraUpD));
  const glm::vec3 camFront = glm::normalize(glm::vec3(cameraFront));
  // Visible point impostors are exact screen-space discs.  Use the
  // world-per-pixel value at the point itself so the ID quad has the same
  // projected pixel radius in perspective views, not merely at the camera
  // target.
  auto pointWorldPerPixel = [&](const glm::dvec3 &worldPoint) {
    if (useOrthoProjection())
      return 2.0 * orbitCam.orthoSize() / currentDrawableHeight();
    const double viewDepth = std::max(1.0e-9,
        glm::dot(worldPoint - cameraPos, cameraFront));
    return 2.0 * viewDepth * std::tan(glm::radians(45.0) * 0.5) /
           currentDrawableHeight();
  };

  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  const glm::mat4 cadAnchorView = view * glm::translate(
      glm::mat4(1.0f), glm::vec3(tessellation.anchor - cameraPos));
  static scene::SceneDrawList cadDrawList;
  cadDrawList.clear();
  static std::vector<bool> strokeVisible;
  static std::vector<bool> fillVisible;
  static std::vector<bool> pointVisible;

  const entities::TessellatedEntity &tess = tessellation.geometry;
  static std::vector<rendering::FillVertex> gpuPickVertices;
  const bool gpuPickQueueActive =
      rendererBackend && gpuPickEnabled() &&
      (gpuPickFocus.waitingResult || gpuPickSceneDebugQueueActive);
  const rendering::RenderModeFlags renderFlags = rendererBackend
      ? rendererBackend->renderModeFlags()
      : rendering::RenderModeFlags{};
  const bool queueSolidFillPicks = renderFlags.show2dSolidFills;
  auto queueGpuSoup = [&](const glm::mat4 &pickProjection, uint32_t objectId) {
    if (!gpuPickQueueActive || objectId == 0 || gpuPickVertices.size() < 3)
      return;

    constexpr size_t kMaxPickChunkVertices = 3 * 21000;
    for (size_t first = 0; first < gpuPickVertices.size();
         first += kMaxPickChunkVertices)
    {
      const size_t count = std::min(kMaxPickChunkVertices,
                                    gpuPickVertices.size() - first);
      rendererBackend->queueGpuTrianglePick(
          0, gpuPickVertices.data() + first, uint32_t(count), view,
          pickProjection, logDepth, objectId);
    }
  };
  // Match the centered visible AcGi ribbon and its symmetric edge shader.
  // A one-sided pick quad would offset the ID buffer from the rendered pixels.
  auto appendPickRibbon = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                              const glm::vec4 &color, float halfWidth) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const float minimumHalfWidth =
        pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
    halfWidth = std::max(halfWidth, minimumHalfWidth);
    const glm::vec3 side = ribbonSide(direction, camFront, halfWidth);
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({ra + side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({rb - side, color});
  };
  // CurveBatchCommand is drawn by a centered 1-pixel line strip, unlike the
  // one-sided AcGi ribbon above.  Its ID ribbon must therefore also be
  // centered on the curve, not offset to the rendered line's side.
  auto appendCenteredPickRibbon = [&](const glm::vec3 &ra,
                                      const glm::vec3 &rb,
                                      const glm::vec4 &color) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const glm::dvec3 worldMidpoint =
        cameraPos + glm::dvec3((ra + rb) * 0.5f);
    // Hardware AA line strips rasterize wider than their nominal 1-pixel
    // centerline, so the ID ribbon uses the same 2-pixel floor as AcGi
    // stroke ribbons; otherwise the visible curve is easier to see than to
    // pick.
    const float halfWidth = float(pointWorldPerPixel(worldMidpoint));
    const glm::vec3 side = ribbonSide(direction, camFront, halfWidth);
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({ra + side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({rb - side, color});
  };
  auto appendPickPoint = [&](const glm::vec3 &center, float radius,
                             const glm::vec4 &color) {
    const glm::vec3 right = camRight * radius;
    const glm::vec3 up = camUp * radius;
    gpuPickVertices.push_back({center - right - up, color});
    gpuPickVertices.push_back({center + right - up, color});
    gpuPickVertices.push_back({center + right + up, color});
    gpuPickVertices.push_back({center - right - up, color});
    gpuPickVertices.push_back({center + right + up, color});
    gpuPickVertices.push_back({center - right + up, color});
  };
  auto queueCadFill = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const CadEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadFill, nullptr, &range});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    const std::vector<rendering::FillVertex> &pickVertices =
        cadGpuPickFillVertices(range, idColor, objectId);
    // CAD surfaces participate in occlusion. Transparent CAD fills behave like
    // transparent meshes: they are visible through, but still receive picks.
    const uint8_t fillOcclusionRank =
        range.count && tess.fills[range.begin].common.color.a >= 0.999f ? 0 : 1;
    constexpr size_t kMaxPickChunkVertices = 3 * 21000;
    for (size_t chunk = 0, first = 0; first < pickVertices.size();
         ++chunk, first += kMaxPickChunkVertices)
    {
      const size_t count = std::min(kMaxPickChunkVertices,
                                    pickVertices.size() - first);
      rendererBackend->queueGpuTrianglePick(
          cadGpuPickGeometryKey(&range, chunk, objectId),
          pickVertices.data() + first, uint32_t(count), cadAnchorView,
          projection, logDepth, objectId, fillOcclusionRank);
    }
  };
  auto queueCadStroke = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const CadEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadStroke, nullptr, &range});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    CadPairedBandPoints band;
    if (cadPairedBandPoints(range, tess, band))
    {
      // The GPU ID buffer must represent the same semantic area as CPU
      // picking.  Otherwise a click between the paired boundary strokes is
      // zoom-dependent (it can miss the 1x1 ID pixel but hit the CPU band).
      for (size_t segment = 0; segment < band.segmentCount; ++segment)
      {
        const size_t next =
            (segment + 1) % band.left->points.size();
        gpuPickVertices.push_back(
            {glm::vec3(band.left->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {glm::vec3(band.right->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {glm::vec3(band.right->points[next] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {glm::vec3(band.left->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {glm::vec3(band.right->points[next] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {glm::vec3(band.left->points[next] - cameraPos), idColor});
      }
    }
    else
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::Stroke &stroke = tess.strokes[i];
      const size_t count = stroke.points.size();
      if (!stroke.common.visible || count < 2)
        continue;
      const float halfWidth = strokeHalfWidth(stroke);
      const size_t segmentCount =
          stroke.closed ? count : count - 1;
      for (size_t segment = 0; segment < segmentCount; ++segment)
      {
        // The scene ID pass and the 1x1 pick pass both hardware-clip at
        // the overlay slab, so clipping the soup there is pixel-exact.
        glm::dvec3 clippedStart, clippedEnd;
        if (!clipStrokeSegmentToView(
                stroke.points[segment],
                stroke.points[(segment + 1) % count],
                cameraPos, cameraRightD, cameraUpD, cameraFront,
                g_renderSlabNear, g_renderSlabFar,
                clippedStart, clippedEnd,
                stroke.semiInfinite))
          continue;
        appendPickRibbon(
            glm::vec3(clippedStart - cameraPos),
            glm::vec3(clippedEnd - cameraPos),
            idColor, halfWidth);
      }
    }
    queueGpuSoup(overlayProjection, objectId);
  };
  auto queueCadPoint = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const CadEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadPoint, nullptr, &range});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::TessellatedPoint &point = tess.points[i];
      if (!point.common.visible)
        continue;
      const float radius = std::max(
          float(point.pointSize) * float(pointWorldPerPixel(point.location)),
          float(3.0 * pointWorldPerPixel(point.location)));
      appendPickPoint(glm::vec3(point.location - cameraPos), radius, idColor);
    }
    queueGpuSoup(overlayProjection, objectId);
  };
  auto queueCadCurve = [&](const VisibilityCandidate &candidate) {
    if (!candidate.curve)
      return;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadCurve, nullptr, nullptr, candidate.curve});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    const std::vector<glm::dvec3> points = sampleCurveBatch(*candidate.curve);
    for (size_t i = 0; i + 1 < points.size(); ++i)
      appendCenteredPickRibbon(
          glm::vec3(points[i] - cameraPos),
          glm::vec3(points[i + 1] - cameraPos), idColor);
    queueGpuSoup(overlayProjection, objectId);
  };
  auto queueTinyCadPoint = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const CadEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {candidate.kind, nullptr, &range});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    const float minimumRadius =
        float(3.0 * pointWorldPerPixel(candidate.center));
    const float pointRadius = candidate.overlayPointSize > 0.0f
        ? candidate.overlayPointSize *
              float(pointWorldPerPixel(candidate.center))
        : minimumRadius;
    appendPickPoint(glm::vec3(candidate.center - cameraPos),
                    std::max(pointRadius, minimumRadius), idColor);
    queueGpuSoup(overlayProjection, objectId);
  };

  strokeVisible.assign(tess.strokes.size(), false);
  fillVisible.assign(tess.fills.size(), false);
  pointVisible.assign(tess.points.size(), false);

  auto markRange = [&](VisibilityKind kind, size_t begin, size_t count) {
    std::vector<bool> &flags = kind == VisibilityKind::CadStroke ? strokeVisible
                               : kind == VisibilityKind::CadFill ? fillVisible
                                                                 : pointVisible;
    const size_t limit = kind == VisibilityKind::CadStroke ? tess.strokes.size()
                             : kind == VisibilityKind::CadFill ? tess.fills.size()
                                                               : tess.points.size();
    const size_t first = std::min(begin, limit);
    const size_t last = std::min(begin + count, limit);
    for (size_t i = first; i < last; ++i)
      flags[i] = true;
  };

  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (!candidate)
      continue;
    if (candidate->kind == VisibilityKind::CadStroke ||
        candidate->kind == VisibilityKind::CadFill ||
        candidate->kind == VisibilityKind::CadPoint)
    {
      markRange(candidate->kind, candidate->rangeBegin, candidate->rangeCount);
    }
  }

  for (const entities::Stroke &stroke : tess.strokes)
  {
    if (strokeVisible.empty() || strokeVisible[&stroke - tess.strokes.data()])
      cadDrawList.geometry().strokes.push_back(stroke);
  }
  // CAD fills bypass the generic SceneDrawList copy.  Visible ranges reuse
  // their cached anchor-relative renderer vertices and submit as one batch.
  static std::vector<rendering::FillVertex> visibleFillVertices;
  visibleFillVertices.clear();
  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (!candidate || candidate->kind != VisibilityKind::CadFill ||
        !candidate->cadRange || !candidate->cadRange->count)
      continue;
    const std::vector<rendering::FillVertex> &rangeVertices =
        cadVisibleFillVertices(*candidate->cadRange);
    visibleFillVertices.insert(visibleFillVertices.end(),
                               rangeVertices.begin(), rangeVertices.end());
  }
  if (!visibleFillVertices.empty())
  {
    const rendering::FilledTrianglesRenderData cadFillData{
        .view = cadAnchorView,
        .projection = projection,
        .vertices = visibleFillVertices.data(),
        .vertexCount =
            static_cast<uint32_t>(visibleFillVertices.size()),
        .is3DFace = false,
        .layer = envLayer("GRID_FILL_LAYER"),
        .logDepth = logDepth,
        .material = toSurfaceMaterial(scene::AcGiMaterial{}),
    };
    rendererBackend->drawFilledTriangles(cadFillData);
  }

  // Wireframe modes suppress solid fills, which would make fill-only
  // entities (Solid, Rectangle, Hatch, CircleFill) vanish.  Draw their
  // per-range boundary edges (edges used by a single triangle, the same
  // shared-edge rule as the selection outline) as colored ribbons instead.
  const rendering::RenderModeFlags fillRenderFlags = rendererBackend
      ? rendererBackend->renderModeFlags()
      : rendering::RenderModeFlags{};
  if (!fillRenderFlags.show2dSolidFills && !fillRenderFlags.face3dFill &&
      !fillRenderFlags.hiddenLine)
  {
    struct FillEdgeKey
    {
      double v[6];
      bool operator<(const FillEdgeKey &other) const
      {
        for (int i = 0; i < 6; ++i)
          if (v[i] != other.v[i]) return v[i] < other.v[i];
        return false;
      }
    };
    static std::vector<rendering::PrimVertex> wireBoundaryVertices;
    wireBoundaryVertices.clear();
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (!candidate || candidate->kind != VisibilityKind::CadFill ||
          !candidate->cadRange || !candidate->cadRange->count)
        continue;
      const CadEntityRange &range = *candidate->cadRange;
      const size_t last = std::min(range.begin + range.count,
                                   tess.fills.size());
      std::map<FillEdgeKey, int> useCounts;
      std::map<FillEdgeKey, std::pair<glm::dvec3, glm::dvec3>> edgeEnds;
      glm::vec4 rangeColor(1.0f);
      bool hasColor = false;
      for (size_t i = range.begin; i < last; ++i)
      {
        const entities::Triangle &triangle = tess.fills[i];
        if (!triangle.common.visible)
          continue;
        if (!hasColor)
        {
          rangeColor = contrastAgainstBackground(triangle.common.color);
          hasColor = true;
        }
        const glm::dvec3 corners[3] = {triangle.a, triangle.b, triangle.c};
        for (int e = 0; e < 3; ++e)
        {
          const glm::dvec3 &p = corners[e];
          const glm::dvec3 &q = corners[(e + 1) % 3];
          FillEdgeKey key;
          const glm::dvec3 *first = &p;
          const glm::dvec3 *second = &q;
          if (std::tie(q.x, q.y, q.z) < std::tie(p.x, p.y, p.z))
            std::swap(first, second);
          key.v[0] = first->x; key.v[1] = first->y; key.v[2] = first->z;
          key.v[3] = second->x; key.v[4] = second->y; key.v[5] = second->z;
          ++useCounts[key];
          edgeEnds[key] = {*first, *second};
        }
      }
      if (!hasColor)
        continue;
      const float boundaryHalfWidth =
          0.5f * outlineWidthWorld(pixelSizeWorld);
      for (const auto &entry : useCounts)
      {
        if (entry.second != 1)
          continue; // shared edges stay hidden in wireframe
        const auto &ends = edgeEnds[entry.first];
        appendOutlineRibbon(
            wireBoundaryVertices,
            glm::vec3(ends.first - cameraPos),
            glm::vec3(ends.second - cameraPos),
            glm::vec3(cameraFront), boundaryHalfWidth, 0.0f, 1.0f,
            true, rangeColor);
      }
    }
    if (!wireBoundaryVertices.empty())
    {
      const rendering::PolylineRenderData boundaryData{
          .view = view,
          .projection = overlayProjection,
          .vertices = wireBoundaryVertices.data(),
          .vertexCount =
              static_cast<uint32_t>(wireBoundaryVertices.size()),
          .logDepth = logDepth,
          .edgeSoftness = 0.15f,
          .layer = envLayer("GRID_LINE_LAYER"),
      };
      rendererBackend->drawPolylines(boundaryData);
    }
  }
  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (candidate && candidate->kind == VisibilityKind::CadCurve && candidate->curve)
      cadDrawList.addCurveBatch() = *candidate->curve;
  }

  for (const entities::TessellatedPoint &point : tess.points)
  {
    if (!pointVisible.empty() && !pointVisible[&point - tess.points.data()])
      continue;
    if (!point.common.visible)
      continue;
    cadDrawList.geometry().points.push_back(point);
  }

  // A whole stroke, face, point group, or mesh below the pixel threshold is
  // represented by one stable impostor instead of submitting invisible pixels.
  for (const VisibilityCandidate *candidate : tinyCad)
  {
    if (!candidate)
      continue;
    appendScenePoint(cadDrawList, candidate->center,
                     glm::vec3(candidate->overlayColor),
                     double(candidate->overlayPointSize));
  }

  if (gpuPickQueueActive)
  {
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadFill &&
          queueSolidFillPicks)
        queueCadFill(*candidate);
    }
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadStroke)
        queueCadStroke(*candidate);
    }
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadCurve)
        queueCadCurve(*candidate);
    }
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadPoint)
        queueCadPoint(*candidate);
    }
    for (const VisibilityCandidate *candidate : tinyCad)
    {
      if (candidate && candidate->cadRange)
        queueTinyCadPoint(*candidate);
    }
  }

  // GRID_DEBUG_PICK=1: per-second composition of the GPU pick queue.  The
  // queue branches on the visual style (meshFill modes represent meshes as
  // surfaces, wireframes as feature edges), so this log verifies what each
  // style actually submits to the ID pass.
  static const bool pickDebugEnabled = [] {
    const char *value = std::getenv("GRID_DEBUG_PICK");
    return value != nullptr && std::strcmp(value, "0") != 0;
  }();
  if (pickDebugEnabled && gpuPickQueueActive)
  {
    static auto lastPickStatsLog =
        std::chrono::steady_clock::now() - std::chrono::seconds(2);
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastPickStatsLog).count() >= 1000)
    {
      lastPickStatsLog = now;
      const rendering::GpuPickQueueStats stats =
          rendererBackend->gpuPickQueueStats();
      std::cout << "[PICK_QUEUE] style="
                << rendering::renderModeLabel(
                       visualStyleManager.mode())
                << " meshes=" << stats.queuedMeshes
                << " edges=" << stats.queuedEdges
                << " triangles=" << stats.queuedTriangles
                << " droppedMeshes=" << stats.droppedMeshes
                << " droppedTriangles=" << stats.droppedTriangles
                << std::endl;
    }
  }

  submitAcGiDrawable(cadDrawList, view, projection, overlayProjection,
                     rebase, cameraPos, cameraRightD, cameraUpD, cameraFront,
                     logDepth, pixelSizeWorld, 2.0f, 7.0f);

  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (!candidate || candidate->kind != VisibilityKind::CadMesh)
      continue;
    if (candidate->entityIndex >= tessellation.meshes.size())
      continue;
    const MeshEntityRecord &mesh =
        tessellation.meshes[candidate->entityIndex];
    if (!meshEntityVisible(mesh))
      continue;
    scene::SceneDrawList meshDrawList;
    appendMeshEntityToScene(mesh, meshDrawList);
    for (scene::MeshBatchCommand &batch : meshDrawList.meshBatches())
      submitMeshBatch(batch, view, projection, logDepth,
                      rendering::encodeDoubleSingle(rebase),
                      pixelSizeWorld);
    queueGpuMeshEntity(&mesh);
  }
}

int stressObjectCount();
const std::vector<LargeCoordinateObject> &getStressObjects();

static bool cadAlgorithmDemoEnabled()
{
  // The CAD instancing path is the design default. Keep the older stylized
  // shader experiment opt-in.
  const char *value = std::getenv("GRID_CAD_SHADER_DEMO");
  return value != nullptr && *value != '\0' &&
         std::strcmp(value, "0") != 0;
}



void drawLargeCoordinateObjects(const glm::mat4 &view,
                                const glm::mat4 &projection,
                                const glm::dvec3 &rebaseOrigin,
                                const std::vector<const LargeCoordinateObject *> &drawOrder,
                                const glm::vec4 &logDepth,
                                float pixelSizeWorld = 0.0f,
                                float edgeSoftness = 0.15f)
{
  const glm::dvec3 cameraPos(orbitCam.Position);
  const glm::dvec3 &cameraFront = orbitCam.Front;

  // Depth values feed both sort comparisons, so cache them once per frame.
  static std::vector<std::pair<double, const LargeCoordinateObject *>>
      sortedDraws;
  sortedDraws.clear();
  sortedDraws.reserve(drawOrder.size());
  for (const LargeCoordinateObject *object : drawOrder)
  {
    sortedDraws.emplace_back(
        glm::dot(object->worldPosition - cameraPos, cameraFront), object);
  }
  std::sort(sortedDraws.begin(), sortedDraws.end(),
            [](const auto &lhs, const auto &rhs) {
              return lhs.first > rhs.first;
            });

  rendering::RealisticLightsRenderData realisticLights;
  realisticLights.pointLights[0].position =
      glm::vec3(LARGE_COORDINATE_BASE_POINT +
                LARGE_COORDINATE_DETAIL_OFFSET + glm::dvec3(-1024.0, 640.0, 512.0) -
                cameraPos);
  realisticLights.pointLights[0].radius = 1024.0f;
  realisticLights.pointLights[0].color = glm::vec3(1.0f, 0.88f, 0.72f);
  realisticLights.pointLights[1].position =
      glm::vec3(LARGE_COORDINATE_BASE_POINT +
                LARGE_COORDINATE_DETAIL_OFFSET + glm::dvec3(1024.0, 384.0, -512.0) -
                cameraPos);
  realisticLights.pointLights[1].radius = 1024.0f;
  realisticLights.pointLights[1].color = glm::vec3(0.52f, 0.74f, 1.0f);
  realisticLights.pointLightCount = 2;
  if (cadAlgorithmDemoEnabled())
  {
    static scene::SceneDrawList cadDrawList;
    cadDrawList.clearKeepCapacity();
    cadDrawList.setLights(realisticLights);
    constexpr size_t kCadMeshCount = 4;
    static std::array<std::vector<rendering::MeshInstance>, kCadMeshCount>
        cadGroups;
    for (auto &group : cadGroups)
      group.clear();
    for (const auto &entry : sortedDraws)
    {
        const LargeCoordinateObject *object = entry.second;
      if (!meshEntityVisible(*object))
        continue;
      const size_t meshIndex = static_cast<size_t>(object->mesh);
      cadGroups[meshIndex].push_back(cachedMeshInstance(*object, meshEntityRenderMaterial(*object)));
      queueGpuMeshEntity(object);
    }

    const rendering::MeshType cadMeshTypes[kCadMeshCount] = {
        rendering::MeshType::Cube, rendering::MeshType::Sphere,
        rendering::MeshType::Cone, rendering::MeshType::Torus};
    for (size_t meshIndex = 0; meshIndex < kCadMeshCount; ++meshIndex)
    {
      auto &instances = cadGroups[meshIndex];
      if (instances.empty())
        continue;
      scene::MeshBatchCommand batch;
      batch.prototype = cadMeshTypes[meshIndex];
      batch.cadAlgorithm = true;
      batch.instances = std::move(instances);
      cadDrawList.meshBatches().push_back(std::move(batch));
    }
    submitAcGiDrawable(cadDrawList, view, projection, projection,
                       rebaseOrigin, cameraPos, orbitCam.Right, orbitCam.Up,
                       cameraFront, logDepth, pixelSizeWorld);
    return;
  }
  // CAD and PBR meshes share the global far-to-near painter order. Depth
  // buckets preserve that order across compatible instancing groups while
  // keeping the number of draw calls bounded.
  constexpr size_t kDepthBucketCount = 16;
  struct InstanceGroup
  {
    rendering::MeshType mesh;
    bool realistic;
    glm::vec4 material;
    float opacity;
    std::vector<rendering::MeshInstance> instances;
  };
  static std::array<std::vector<InstanceGroup>, kDepthBucketCount> buckets;
  for (auto &bucket : buckets)
    bucket.clear();
  static scene::SceneDrawList meshDrawList;
  meshDrawList.clearKeepCapacity();
  meshDrawList.setLights(realisticLights);

    const size_t orderCount = drawOrder.size();
    for (size_t orderIndex = 0; orderIndex < orderCount; ++orderIndex)
    {
        const LargeCoordinateObject *object = sortedDraws[orderIndex].second;
        const size_t bucketIndex = orderIndex * kDepthBucketCount /
                                   std::max(orderCount, size_t(1));
        if (!meshEntityVisible(*object))
          continue;

        const bool realistic = object->realistic();
        const glm::vec4 material = meshEntityRenderMaterial(*object);
        const glm::vec4 color = meshEntityColor(*object);

        std::vector<InstanceGroup> &groups = buckets[bucketIndex];
        auto groupIt = std::find_if(
            groups.begin(), groups.end(),
            [&](const InstanceGroup &group) {
              return group.mesh == object->mesh &&
                     group.realistic == realistic &&
                     group.material == material &&
                     group.opacity == color.a;
            });
        if (groupIt == groups.end())
        {
          groups.push_back({object->mesh, realistic, material, color.a, {}});
          groupIt = groups.end() - 1;
        }
        groupIt->instances.push_back(cachedMeshInstance(*object, material));
        queueGpuMeshEntity(object);
    }

  for (auto &bucket : buckets)
    for (InstanceGroup &group : bucket)
    {
      if (group.instances.empty())
        continue;
        scene::MeshBatchCommand batch;
        batch.prototype = group.mesh;
        batch.opaque = group.opacity >= 1.0f;
        batch.realistic = group.realistic;
        batch.material = group.material;
        batch.acgiMaterial.algorithm = group.realistic
            ? scene::AcGiShaderAlgorithm::Realistic
            : scene::AcGiShaderAlgorithm::Shaded;
        batch.acgiMaterial.metallic = group.material.x;
        batch.acgiMaterial.roughness = group.material.y;
        batch.acgiMaterial.transparency = 1.0f - group.opacity;
        batch.instances = std::move(group.instances);
        meshDrawList.meshBatches().push_back(std::move(batch));
    }
  submitAcGiDrawable(meshDrawList, view, projection, projection,
                     rebaseOrigin, cameraPos, orbitCam.Right, orbitCam.Up,
                     cameraFront, logDepth, pixelSizeWorld);
}

int stressObjectCount()
{
  static const int count = [] {
    const char *value = std::getenv("GRID_STRESS_COUNT");
    const int requested = value ? std::atoi(value) : 1000;
    return std::clamp(requested, 0, 1000000);
  }();
  return count;
}

const std::vector<LargeCoordinateObject> &getStressObjects()
{
  static const std::vector<LargeCoordinateObject> objects = [] {
    std::vector<LargeCoordinateObject> result;
    const int count = stressObjectCount();
    if (count <= 0)
      return result;

    // Grid layout on the XZ plane around the large-coordinate base point,
    // offset so the stress field does not overlap the validation cluster.
    const int cols =
        static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    constexpr double kSpacing = 256.0;
    constexpr double kObjectSize = 128.0;
    const double centerOffset = (cols - 1) * 0.5 * kSpacing;
    static constexpr rendering::MeshType kMeshCycle[] = {
        rendering::MeshType::Sphere, rendering::MeshType::Cone,
        rendering::MeshType::Torus,  rendering::MeshType::Cube,
    };
    static constexpr glm::vec3 kPalette[] = {
        {0.43f, 0.91f, 0.98f}, {1.00f, 0.58f, 0.25f},
        {0.55f, 0.85f, 0.45f}, {0.95f, 0.95f, 0.95f},
        {0.90f, 0.45f, 0.75f}, {0.98f, 0.90f, 0.35f},
    };
    result.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
      const int row = i / cols;
      const int col = i % cols;
      LargeCoordinateObject object;
      object.worldPosition =
          LARGE_COORDINATE_BASE_POINT +
          glm::dvec3(col * kSpacing - centerOffset, kObjectSize * 0.5,
                     row * kSpacing - centerOffset - 4096.0);
      object.entity.common.color = glm::vec4(kPalette[i % 6], 0.45f);
      object.size = static_cast<float>(kObjectSize);
      object.mesh = kMeshCycle[i % 4];
      object.entity.style = realisticMeshEnabled() && (i % 2) != 0
                                ? entities::MeshStyle::Realistic
                                : entities::MeshStyle::Cad;
      object.entity.material.metallicFactor = 0.78f;
      object.entity.material.roughnessFactor = 0.28f;
      object.entity.common.name =
          indexedDisplayName("StressMesh", size_t(i));
      result.push_back(object);
    }
    return result;
  }();
  return objects;
}

struct WorldAabb
{
  glm::dvec3 min;
  glm::dvec3 max;
  bool valid = false;
};

void expandWorldAabb(WorldAabb &bounds, const glm::dvec3 &center,
                     const glm::dvec3 &halfExtent)
{
  const glm::dvec3 minimum = center - halfExtent;
  const glm::dvec3 maximum = center + halfExtent;
  if (!bounds.valid)
  {
    bounds.min = minimum;
    bounds.max = maximum;
    bounds.valid = true;
    return;
  }

  bounds.min = glm::min(bounds.min, minimum);
  bounds.max = glm::max(bounds.max, maximum);
}

void expandCadTessellationBounds(WorldAabb &bounds,
                                 const VectorPrimitivesTessellation &tess)
{
  for (const CadEntityRange &range : tess.strokeRanges)
  {
    for (size_t index = range.begin; index < range.begin + range.count; ++index)
    {
      const entities::Stroke &stroke = tess.geometry.strokes[index];
      // Infinite entities (Ray/XLine) carry only a tessellation proxy
      // endpoint; their unbounded geometry must not inflate scene bounds.
      if (stroke.semiInfinite)
        continue;
      for (const glm::dvec3 &point : stroke.points)
        expandWorldAabb(bounds, point, glm::dvec3(0.0));
    }
  }
  for (const entities::Triangle &triangle : tess.geometry.fills)
  {
    expandWorldAabb(bounds, triangle.a, glm::dvec3(0.0));
    expandWorldAabb(bounds, triangle.b, glm::dvec3(0.0));
    expandWorldAabb(bounds, triangle.c, glm::dvec3(0.0));
  }
  for (const entities::TessellatedPoint &point : tess.geometry.points)
    expandWorldAabb(bounds, point.location, glm::dvec3(0.0));
  for (const scene::CurveBatchCommand &curve : tess.curves)
  {
    if (curve.algorithm == rendering::CurveAlgorithm::Arc)
    {
      const double radius = glm::max(curve.radius, 0.0);
      expandWorldAabb(bounds, curve.center, glm::dvec3(radius));
      continue;
    }
    for (const glm::dvec3 &point : curve.controlPoints)
      expandWorldAabb(bounds, point, glm::dvec3(0.0));
  }
  for (const MeshEntityRecord &mesh : tess.meshes)
    expandWorldAabb(bounds, mesh.worldPosition,
                    glm::dvec3(mesh.size * 0.5));
}

// Immutable scene bounds are computed once; the per-frame pass only converts
// this one conservative box to camera space before doing exact object culling.
static WorldAabb stressFieldBounds()
{
  WorldAabb bounds;
  expandCadTessellationBounds(bounds, getVectorPrimitivesTessellation());
  for (const LargeCoordinateObject &object : getStressObjects())
    expandWorldAabb(bounds, object.worldPosition, glm::dvec3(object.size * 0.5));

  // Text entities must join the content bounds: their glyph quads are
  // drawn with the main ortho projection, so the depth slab has to cover
  // them or the near/far planes clip the glyphs when the slab tightens
  // around meshes alone.  Expand conservatively from the insertion point
  // by the text's own metrics (length for x/y extent, height for z).
  for (const acgi::TextRequest &request : acgi::textRequests())
  {
    const double extent =
        double(request.message.size()) * request.height + request.height;
    expandWorldAabb(bounds, request.position,
                    glm::dvec3(extent, extent, request.height * 2.0));
  }
  return bounds;
}

const WorldAabb &immutableObjectBounds()
{
  static const WorldAabb bounds = [] {
    WorldAabb result = stressFieldBounds();
    for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
    {
      expandWorldAabb(result, object.worldPosition,
                      glm::dvec3(object.size * 0.5));
    }
    return result;
  }();
  return bounds;
}


const WorldAabb &immutableSceneBounds()
{
  static const WorldAabb bounds = [] {
    WorldAabb result = immutableObjectBounds();

    // The reference line is always drawn and may connect distant clusters.
    expandWorldAabb(result, glm::dvec3(0.0), glm::dvec3(0.0));
    expandWorldAabb(result, LARGE_COORDINATE_BASE_POINT, glm::dvec3(0.0));
    return result;
  }();
  return bounds;
}

// Put the initial orbit camera around a bounding sphere of every renderable
// object.  This is a fit-all view, not a request to bypass per-frame culling:
// all objects are therefore submitted on the first perspective frame, while
// later views still avoid drawing geometry outside the CAD frustum.
// Shared drawable-aspect helper for camera fit operations.
static double currentDrawableAspect()
{
  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  return static_cast<double>(drawableWidth) /
         static_cast<double>(drawableHeight);
}

// Shared fit-to-bounds-and-report helper.
static void fitCameraToBounds(const WorldAabb &bounds, const char *label)
{
  orbitCam.fitToBounds(bounds.min, bounds.max, currentDrawableAspect());
  std::cout << label << " center=(" << orbitCam.Target.x << ", "
            << orbitCam.Target.y << ", " << orbitCam.Target.z
            << ") distance=" << orbitCam.Distance
            << std::endl;
}

void fitCameraToRenderableObjects()
{
  const WorldAabb &bounds = immutableObjectBounds();
  if (!bounds.valid)
    return;
  fitCameraToBounds(bounds, "Initial fit-all camera:");
}

void fitCameraToStressField()
{
  const WorldAabb bounds = stressFieldBounds();
  if (!bounds.valid)
  {
    const glm::dvec3 detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    orbitCam.setOrbit(detailCenter, 6000.0);
    orbitCam.fitDepthToBounds(detailCenter, detailCenter);
    return;
  }
  fitCameraToBounds(bounds, "Stress-field camera:");
}

void printLargeCoordinateValidation()
{
  const glm::dvec3 detailCenter =
      LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
  const std::array<double, 4> microOffsets{-288.0, -96.0, 96.0, 288.0};
  const float floatReferenceX = static_cast<float>(detailCenter.x);

  std::cout << "Large-coordinate validation\n"
            << std::fixed << std::setprecision(0)
            << "  base: (" << LARGE_COORDINATE_BASE_POINT.x << ", "
            << LARGE_COORDINATE_BASE_POINT.y << ", "
            << LARGE_COORDINATE_BASE_POINT.z << ")\n"
            << "  detail: (" << detailCenter.x << ", " << detailCenter.y
            << ", " << detailCenter.z << ")\n"
            << "  micro X offsets [double, float32 world, CPU double rebase]:"
            << std::endl;

  std::cout << "  stress objects: " << stressObjectCount() << std::endl;

  for (const double offset : microOffsets)
  {
    const float floatObjectX =
        static_cast<float>(detailCenter.x + offset);
    const float floatOffset = floatObjectX - floatReferenceX;
    const float rebasedOffset = static_cast<float>(
        (detailCenter.x + offset) - detailCenter.x);

    std::cout << "    " << std::setw(6) << offset
              << " -> " << std::setw(6) << floatOffset
              << " -> " << std::setw(6) << rebasedOffset
              << std::endl;
  }
}

bool cameraDebugEnabled()
{
  static const bool enabled = [] {
    const char *value = std::getenv("GRID_CAMERA_DEBUG");
    return value != nullptr && *value != '\0' &&
           std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

bool shiftKeyDown()
{
  const bool *keys = SDL_GetKeyboardState(NULL);
  return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
}

// World-per-pixel must follow the actual drawable size, not the compile-time
// window constants, so resize / HiDPI changes keep pan speed correct.
int currentDrawableHeight()
{
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;
    if (window)
        SDL_GetWindowSizeInPixels(window, &width, &height);
    return std::max(1, height);
}

void handleOrbitMouseMovement(SDL_Event event, bool middleMouseDrag)
{
  if (event.type == SDL_EVENT_MOUSE_MOTION)
  {
    if (!middleMouseDrag)
      return;

    if (shiftKeyDown())
    {
      // Shift + middle-drag orbits the camera.
      orbitCam.orbitAroundPivot(
          event.motion.xrel, -event.motion.yrel,
          orbitPivot.value_or(orbitCam.Target));
      return;
    }

    // OpenCADStudio pans on the camera image plane in both projections.
    // The eye moves with the target, preserving orientation and distance.
    orbitCam.panScreen(event.motion.xrel, event.motion.yrel,
                       (float)currentDrawableHeight());
  }
}

void handleOrbitZoom(SDL_Event event)
{
  if (event.type != SDL_EVENT_MOUSE_WHEEL)
    return;

  const float delta = static_cast<float>(event.wheel.y);

  // Keep the orbit target (tag point) pinned to the viewport center. Wheel
  // zoom changes Distance/ortho size without cursor-plane target compensation.
  orbitCam.zoom(delta);
  if (cameraDebugEnabled())
  {
    std::cout << "Ortho half-height: " << std::scientific
              << std::setprecision(4) << orbitCam.orthoSize() << std::endl;
  }
}

// OpenCADStudio's projection toggle does not mutate the camera state.  Both
// modes share Target, Rotation, and Distance; only the projection differs.
void switchProjectionMode()
{
  bool &isOrtho = useOrthoProjection();
  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  orbitCam.setProjectionPreservingFrame(isOrtho, !isOrtho, aspect);
  isOrtho = !isOrtho;
  resetSlabStabilizers();

  std::cout << "Projection: "
            << (isOrtho ? "ORTHOGRAPHIC" : "PERSPECTIVE")
            << std::endl;
}

glm::dvec3 normalizedNdcLine(const glm::dvec3 &line)
{
  const double scale = std::max(std::abs(line.x), std::abs(line.y));
  if (!(scale > 0.0) || !std::isfinite(scale))
    return glm::dvec3(0.0);
  return line / scale;
}

bool lineIntersectsNdcSquare(const glm::dvec3 &line)
{
  const double f00 = -line.x - line.y + line.z;
  const double f10 = line.x - line.y + line.z;
  const double f01 = -line.x + line.y + line.z;
  const double f11 = line.x + line.y + line.z;
  const double minValue = std::min(std::min(f00, f10), std::min(f01, f11));
  const double maxValue = std::max(std::max(f00, f10), std::max(f01, f11));
  return minValue <= 0.0f && maxValue >= 0.0f;
}

glm::vec3 anchoredNdcLine(const glm::dvec3 &line)
{
  if (!lineIntersectsNdcSquare(line))
    return glm::vec3(line);

  // Re-anchor the infinite line at the midpoint of its segment inside the
  // NDC square.  The original constant can still contain a large world
  // coordinate; evaluating that constant in float32 causes cancellation and
  // flicker while panning.
  glm::dvec2 anchors[4];
  int anchorCount = 0;
  const double edgeEpsilon = 1e-12;
  const auto addAnchor = [&](double x, double y) {
    if (anchorCount < 4 &&
        x >= -1.0 - edgeEpsilon && x <= 1.0 + edgeEpsilon &&
        y >= -1.0 - edgeEpsilon && y <= 1.0 + edgeEpsilon)
    {
      anchors[anchorCount++] = glm::dvec2(glm::clamp(x, -1.0, 1.0),
                                           glm::clamp(y, -1.0, 1.0));
    }
  };

  if (std::abs(line.y) > edgeEpsilon)
  {
    addAnchor(-1.0, (-line.z + line.x) / line.y);
    addAnchor(1.0, (-line.z - line.x) / line.y);
  }
  if (std::abs(line.x) > edgeEpsilon)
  {
    addAnchor((-line.z + line.y) / line.x, -1.0);
    addAnchor((-line.z - line.y) / line.x, 1.0);
  }
  if (anchorCount == 0)
    return glm::vec3(line);

  glm::dvec2 anchor(0.0);
  for (int index = 0; index < anchorCount; ++index)
    anchor += anchors[index] / static_cast<double>(anchorCount);

  return glm::vec3(line.x, line.y,
                   -(line.x * anchor.x + line.y * anchor.y));
}

struct CameraSpacePoint
{
  double x;
  double y;
  double depth;
};

struct CameraSpaceAabb
{
  double minX;
  double maxX;
  double minY;
  double maxY;
  double minDepth;
  double maxDepth;
};

// Equivalent to transforming by the inverse view matrix, but evaluated in
// double precision from the camera basis.  This is important for the
// rebased large-coordinate scene: converting an absolute world position
// through a float mat4 would lose precision before the depth bounds are
// calculated.
CameraSpacePoint toCameraSpace(const glm::dvec3 &worldPosition,
                               const glm::dvec3 &cameraPosition,
                               const glm::dvec3 &cameraRight,
                               const glm::dvec3 &cameraUp,
                               const glm::dvec3 &cameraFront)
{
  const glm::dvec3 delta = worldPosition - cameraPosition;
  return {glm::dot(delta, cameraRight),
          glm::dot(delta, cameraUp),
          glm::dot(delta, cameraFront)};
}

// The exact camera-space bounds of an axis-aligned box.  The support radius
// along each camera basis vector is dot(abs(basis), halfExtent); summing all
// three world-axis contributions is required for a rotated camera basis.
CameraSpaceAabb cameraAabbBounds(const glm::dvec3 &worldCenter,
                                 const glm::dvec3 &halfExtent,
                                 const glm::dvec3 &cameraPosition,
                                 const glm::dvec3 &cameraRight,
                                 const glm::dvec3 &cameraUp,
                                 const glm::dvec3 &cameraFront)
{
  const glm::dvec3 delta = worldCenter - cameraPosition;
  const double rightRadius = glm::dot(glm::abs(cameraRight), halfExtent);
  const double upRadius = glm::dot(glm::abs(cameraUp), halfExtent);
  const double frontRadius = glm::dot(glm::abs(cameraFront), halfExtent);
  const double centerX = glm::dot(delta, cameraRight);
  const double centerY = glm::dot(delta, cameraUp);
  const double centerDepth = glm::dot(delta, cameraFront);

  return {centerX - rightRadius, centerX + rightRadius,
          centerY - upRadius,      centerY + upRadius,
          centerDepth - frontRadius, centerDepth + frontRadius};
}

bool aabbIntersectsOrthoViewport(const CameraSpaceAabb &bounds,
                                 double halfWidth,
                                 double halfHeight)
{
  return bounds.maxX >= -halfWidth && bounds.minX <= halfWidth &&
         bounds.maxY >= -halfHeight && bounds.minY <= halfHeight;
}

// WorldAabb without the valid flag: BVH node bounds are always set.
using WorldAabb2 = WorldAabb;

CameraSpaceAabb cameraAabbBounds(const WorldAabb2 &worldBounds,
                                 const glm::dvec3 &cameraPosition,
                                 const glm::dvec3 &cameraRight,
                                 const glm::dvec3 &cameraUp,
                                 const glm::dvec3 &cameraFront)
{
    const glm::dvec3 center = (worldBounds.min + worldBounds.max) * 0.5;
    const glm::dvec3 halfExtent = (worldBounds.max - worldBounds.min) * 0.5;
    return cameraAabbBounds(center, halfExtent, cameraPosition, cameraRight,
                            cameraUp, cameraFront);
}

// Generic median-split BVH over immutable scene objects.  Subclasses provide
// objectBounds()/objectCentroid() so the mesh scene and CAD range picking
// share one build/traverse implementation.
template <typename ObjectT>
class MedianSplitBvh
{
public:
    struct Node
    {
        WorldAabb2 bounds{};
        uint32_t leftChild = 0;
        uint32_t rightChild = 0;
        uint32_t begin = 0;
        uint32_t end = 0;
        bool leaf = false;
    };

    void build(std::vector<const ObjectT *> sceneObjects)
    {
        objects = std::move(sceneObjects);
        nodes.clear();
        if (objects.empty())
            return;

        nodes.reserve(objects.size() * 2);
        buildRange(0, static_cast<uint32_t>(objects.size()));
    }

    template <typename Visitor>
    void collect(Visitor &&nodeIsVisible,
                 std::vector<const ObjectT *> &output) const
    {
        output.clear();
        if (nodes.empty())
            return;

        std::array<uint32_t, 128> stack{};
        int stackTop = 0;
        stack[stackTop++] = 0;
        while (stackTop > 0)
        {
            const Node &node = nodes[stack[--stackTop]];
            if (!nodeIsVisible(node.bounds))
                continue;
            if (node.leaf)
            {
                for (uint32_t i = node.begin; i < node.end; ++i)
                    output.push_back(objects[i]);
            }
            else
            {
                stack[stackTop++] = node.rightChild;
                stack[stackTop++] = node.leftChild;
            }
        }
    }

protected:
    virtual WorldAabb2 objectBounds(const ObjectT &object) const = 0;
    virtual glm::dvec3 objectCentroid(const ObjectT &object) const = 0;

private:
    static WorldAabb2 mergeBounds(const WorldAabb2 &a, const WorldAabb2 &b)
    {
        return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
    }

    uint32_t buildRange(uint32_t begin, uint32_t end)
    {
        WorldAabb2 bounds = objectBounds(*objects[begin]);
        for (uint32_t i = begin + 1; i < end; ++i)
            bounds = mergeBounds(bounds, objectBounds(*objects[i]));

        const uint32_t nodeIndex = static_cast<uint32_t>(nodes.size());
        nodes.push_back({bounds, 0, 0, begin, end, false});
        const uint32_t count = end - begin;
        if (count <= 4)
        {
            nodes[nodeIndex].leaf = true;
            return nodeIndex;
        }

        const glm::dvec3 extent = bounds.max - bounds.min;
        int axis = 0;
        if (extent.y > extent.x && extent.y >= extent.z)
            axis = 1;
        else if (extent.z > extent.x && extent.z > extent.y)
            axis = 2;

        const uint32_t middle = begin + count / 2;
        std::nth_element(objects.begin() + begin, objects.begin() + middle,
                         objects.begin() + end,
                         [this, axis](const ObjectT *lhs, const ObjectT *rhs) {
                             return objectCentroid(*lhs)[axis] <
                                    objectCentroid(*rhs)[axis];
                         });

        const uint32_t leftChild = buildRange(begin, middle);
        // buildRange can reallocate nodes, so refresh the parent reference.
        nodes[nodeIndex].leftChild = leftChild;
        const uint32_t rightChild = buildRange(middle, end);
        nodes[nodeIndex].rightChild = rightChild;
        nodes[nodeIndex].leaf = false;
        return nodeIndex;
    }

    std::vector<const ObjectT *> objects;
    std::vector<Node> nodes;
};

// Static median-split BVH over the immutable startup scene.  World positions
// remain double precision; the camera-space conversion happens only for a
// visited node.  This removes the per-frame O(objects) broad-phase transform
// pass while preserving exact leaf-side slab/LOD decisions.
class SceneObjectBvh final : public MedianSplitBvh<LargeCoordinateObject>
{
protected:
    WorldAabb2 objectBounds(const LargeCoordinateObject &object) const override
    {
        const glm::dvec3 halfExtent(object.size * 0.5);
        return {object.worldPosition - halfExtent,
                object.worldPosition + halfExtent};
    }

    glm::dvec3 objectCentroid(const LargeCoordinateObject &object) const override
    {
        return object.worldPosition;
    }
};

const SceneObjectBvh &getSceneObjectBvh()
{
    static const SceneObjectBvh bvh = [] {
        std::vector<const LargeCoordinateObject *> all;
        all.reserve(getLargeCoordinateObjects().size() +
                    getStressObjects().size());
        for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
            all.push_back(&object);
        for (const LargeCoordinateObject &object : getStressObjects())
            all.push_back(&object);

        SceneObjectBvh result;
        result.build(std::move(all));
        return result;
    }();
    return bvh;
}

// World-per-pixel must follow the actual drawable size.
int currentDrawableWidth()
{
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;
    if (window)
        SDL_GetWindowSizeInPixels(window, &width, &height);
    return std::max(1, width);
}

// VSG "double all the way": all ray construction and intersection math
// uses double precision.  The direction is normalized in double.
struct PickRay
{
    glm::dvec3 origin;
    glm::dvec3 direction;
};

// Construct a picking ray from cursor NDC coordinates.
// Perspective: origin = eye, direction = normalize(front + right*ndcX*tanH + up*ndcY*tanV).
// Ortho:       origin shifts on the image plane, direction = front.
PickRay pickRayFromNdc(double ndcX, double ndcY,
                       const GpuPickCameraBasis &basis)
{
    PickRay ray;
    ray.origin = basis.position;

    if (basis.ortho)
    {
        const double halfH = basis.orthoHalfHeight > 0.0
                                ? basis.orthoHalfHeight
                                : orbitCam.orthoSize();
        const double aspect = (double)currentDrawableWidth() /
                               (double)currentDrawableHeight();
        const double halfW = halfH * aspect;
        ray.origin += basis.right * (ndcX * halfW) +
                       basis.up * (ndcY * halfH);
        ray.direction = glm::normalize(basis.front);
    }
    else
    {
        // Object rendering intentionally keeps a fixed 45 degree perspective
        // FOV; OrbitCamera::Zoom controls only orthographic framing.
        const double tanHalfV =
            std::tan(glm::radians(45.0) * 0.5);
        const double aspect = (double)currentDrawableWidth() /
                               (double)currentDrawableHeight();
        const double tanHalfH = tanHalfV * aspect;
        ray.direction = glm::normalize(
            basis.front +
            basis.right * (ndcX * tanHalfH) +
            basis.up * (ndcY * tanHalfV));
    }
    return ray;
}

PickRay pickRayFromNdc(double ndcX, double ndcY)
{
    const GpuPickCameraBasis liveCamera{
        orbitCam.Position, orbitCam.Front, orbitCam.Right, orbitCam.Up,
        useOrthoProjection(), orbitCam.orthoSize()};
    return pickRayFromNdc(ndcX, ndcY, liveCamera);
}

// Ortho slabs intentionally straddle the camera plane (near can be negative);
// visible and GPU ID passes rasterize that content, so CPU picking must use
// the same slab.  Perspective keeps the classic ray parameter > 0 rule for
// the near side, but the far side is clamped to the rendered slab: hardware
// clips at that far plane, so geometry beyond it cannot be under the cursor.
static double g_pickDepthNear = 0.0;
static double g_pickDepthFar = std::numeric_limits<double>::infinity();

static bool pickIsOrthoProjection()
{
    return useOrthoProjection();
}

static double pickMinDepth()
{
    return pickIsOrthoProjection() ? g_pickDepthNear : 0.0;
}

static double pickMaxDepth()
{
    // Without the far clamp the 1e6-unit Ray stroke stays pickable far
    // outside the rendered slab, and autofocus would then blow up the
    // depth range for the whole scene.
    return g_pickDepthFar;
}

static bool pickDepthInRange(double depth)
{
    return depth >= pickMinDepth() && depth <= pickMaxDepth();
}

// Ray-AABB slab test in double precision.
bool rayIntersectsAabb(const PickRay &ray,
                       const WorldAabb2 &bounds,
                       double &hitDepth)
{
    glm::dvec3 inverseDirection(1.0);
    for (int axis = 0; axis < 3; ++axis)
    {
        const double component = ray.direction[axis];
        inverseDirection[axis] =
            std::abs(component) < 1.0e-20
                ? std::numeric_limits<double>::infinity()
                : 1.0 / component;
    }

    const glm::dvec3 minimum = (bounds.min - ray.origin) * inverseDirection;
    const glm::dvec3 maximum = (bounds.max - ray.origin) * inverseDirection;
    const glm::dvec3 nearDeltas = glm::min(minimum, maximum);
    const glm::dvec3 farDeltas  = glm::max(minimum, maximum);
    const double enter = std::max({nearDeltas.x, nearDeltas.y, nearDeltas.z});
    const double exit  = std::min({farDeltas.x,  farDeltas.y,  farDeltas.z});
    const double minDepth = pickMinDepth();
    if (exit < std::max(enter, minDepth))
        return false;

    hitDepth = std::max(enter, minDepth);
    return true;
}


std::vector<glm::dvec3> sampleCurveBatch(const scene::CurveBatchCommand &curve)
{
    std::vector<glm::dvec3> result;
    const int samples = std::clamp(static_cast<int>(curve.sampleCount), 2, 512);
    if (curve.algorithm == rendering::CurveAlgorithm::Arc)
    {
        // CurveBatchCommand::axisU/axisV are unit directions; the visible
        // curve shader applies radius.  CPU sampling/ID geometry must apply
        // the same radius or an analytic Arc collapses to a point-sized ID
        // trace and disappears from the full-scene ID buffer.
        const double radius = std::max(0.0, curve.radius);
        result.reserve(samples);
        for (int i = 0; i < samples; ++i)
        {
            const double t = static_cast<double>(i) / (samples - 1);
            const double angle = curve.startAngle + curve.sweep * t;
            result.push_back(curve.center +
                             curve.axisU * (radius * std::cos(angle)) +
                             curve.axisV * (radius * std::sin(angle)));
        }
        return result;
    }

        // Match the GPU shader's CAD_CURVE_MAX_CP capacity so CPU picking,
    // ID geometry, and visible rendering evaluate the same curve.  BSpline/
    // NURBS need one spare slot for the clamped knot vector; Bezier can use
    // all 16 control-point slots.
    const size_t controlCount =
        curve.algorithm == rendering::CurveAlgorithm::Bezier
            ? curve.controlPoints.size()
            : std::min(curve.controlPoints.size(), size_t(16 - 1));
    if (controlCount < 2)
        return result;

    if (curve.algorithm == rendering::CurveAlgorithm::Bezier)
    {
        std::vector<glm::dvec4> points;
        points.reserve(controlCount);
        for (size_t i = 0; i < controlCount; ++i)
        {
            const double weight = i < curve.weights.size() ? curve.weights[i] : 1.0;
            points.push_back(glm::dvec4(curve.controlPoints[i] * weight, weight));
        }

        result.reserve(samples);
        for (int sample = 0; sample < samples; ++sample)
        {
            const double t = static_cast<double>(sample) / (samples - 1);
            std::vector<glm::dvec4> value = points;
            while (value.size() > 1)
            {
                for (size_t i = 0; i + 1 < value.size(); ++i)
                    value[i] = glm::mix(value[i], value[i + 1], t);
                value.pop_back();
            }
            const glm::dvec4 &homogeneous = value.front();
            result.push_back(glm::dvec3(homogeneous) /
                             std::max(1.0e-12, homogeneous.w));
        }
        return result;
    }

    const int degree = std::clamp(curve.degree, 1, 3);
    if (controlCount < static_cast<size_t>(degree) + 1)
        return result;

    std::vector<double> knots = curve.knots;
    if (knots.size() < controlCount + degree + 1)
    {
        knots.clear();
        knots.reserve(controlCount + degree + 1);
        const size_t innerCount = controlCount - degree - 1;
        for (size_t i = 0; i <= degree; ++i)
            knots.push_back(0.0);
        for (size_t i = 1; i <= innerCount; ++i)
            knots.push_back(static_cast<double>(i) / (innerCount + 1));
        for (size_t i = 0; i <= degree; ++i)
            knots.push_back(1.0);
    }

    auto findSpan = [&](double t) {
        const size_t firstSpan = static_cast<size_t>(degree);
        if (controlCount < firstSpan + 1)
            return firstSpan;
        // Standard knot-span convention: span s covers
        // [knots[s], knots[s+1]) and uses cp[s-degree..s]; valid spans
        // are [degree, controlCount-1].
        // Match the shader: the final knot is at controlCount + degree,
        // not controlCount - 1.  The old test collapsed interior samples
        // into the last span for clamped knot vectors.
        const size_t lastKnotIndex = std::min(
            controlCount + static_cast<size_t>(degree), knots.size() - 1);
        // Keep span + degree inside the 16-entry knot window shared with the
        // GPU evaluation; mirrors the span clamp in cad_bspline/cad_nurbs.
        const size_t lastSpan =
            std::min(controlCount - 1, size_t(std::max(0, 15 - degree)));
        if (t >= knots[lastKnotIndex])
            return lastSpan;
        if (t <= knots[firstSpan])
            return firstSpan;
        for (size_t span = firstSpan; span < lastSpan; ++span)
        {
            if (t < knots[span + 1])
                return span;
        }
        return lastSpan;
    };

    result.reserve(samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const double t = static_cast<double>(sample) / (samples - 1);
        const size_t span = findSpan(t);
        std::vector<glm::dvec4> points(degree + 1);
        for (int i = 0; i <= degree; ++i)
        {
            const size_t index = span - degree + i;
            const double weight = index < curve.weights.size() ? curve.weights[index] : 1.0;
            points[i] = glm::dvec4(curve.controlPoints[index] * weight, weight);
        }
        for (int r = 1; r <= degree; ++r)
        {
            for (int j = degree; j >= r; --j)
            {
                const size_t index = span - degree + j;
                const double denominator = knots[index + degree - r + 1] -
                                           knots[index];
                const double alpha = denominator > 1.0e-12
                                         ? (t - knots[index]) / denominator
                                         : 0.0;
                points[j] = (1.0 - alpha) * points[j - 1] + alpha * points[j];
            }
        }
        const glm::dvec4 &homogeneous = points[degree];
        result.push_back(glm::dvec3(homogeneous) /
                         std::max(1.0e-12, homogeneous.w));
    }
    return result;
}

VisibilityCandidate makeCurveCandidate(const scene::CurveBatchCommand &curve)
{
    VisibilityCandidate candidate;
    candidate.kind = VisibilityKind::CadCurve;
    candidate.curve = &curve;
    candidate.overlayColor = contrastAgainstBackground(curve.acgiMaterial.baseColor);
    candidate.overlayPointSize = 2.0f;

    const std::vector<glm::dvec3> points = sampleCurveBatch(curve);
    if (points.empty())
        return candidate;

    glm::dvec3 minimum = points.front();
    glm::dvec3 maximum = points.front();
    for (const glm::dvec3 &point : points)
    {
        minimum = glm::min(minimum, point);
        maximum = glm::max(maximum, point);
    }

    candidate.min = minimum;
    candidate.max = maximum;
    candidate.center = (minimum + maximum) * 0.5;
    candidate.lodSize = glm::length(maximum - minimum);
    return candidate;
}

const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates();

// CAD entities are immutable after tessellation, so their world-space bounds
// can back a static BVH. Exact primitive tests remain outside this class: the
// BVH answers which ranges the ray can reach, not which surface wins overlaps.
class CadRangeBvh final : public MedianSplitBvh<VisibilityCandidate>
{
protected:
    WorldAabb2 objectBounds(const VisibilityCandidate &candidate) const override
    {
        return {candidate.min, candidate.max};
    }

    glm::dvec3 objectCentroid(const VisibilityCandidate &candidate) const override
    {
        return candidate.center;
    }
};

const CadRangeBvh &getCadRangeBvh()
{
    static const CadRangeBvh bvh = [] {
        const std::vector<VisibilityCandidate> &candidates =
            cadRangeVisibilityCandidates();
        std::vector<const VisibilityCandidate *> ranges;
        ranges.reserve(candidates.size());
        for (const VisibilityCandidate &candidate : candidates)
        {
            if (candidate.lodSize > 0.0)
                ranges.push_back(&candidate);
        }

        CadRangeBvh result;
        result.build(std::move(ranges));
        return result;
    }();
    return bvh;
}

double cadPickTolerance(const PickRay &ray, const glm::dvec3 &worldPoint);

// Closest approach between a normalized picking ray and a finite segment.
// Keeping every intermediate value in double avoids false hits/misses in the
// large-coordinate CAD demo.
// Closest approach between the pick ray and the segment, honoring the depth
// window.  Returns the clamped ray parameter (NaN for degenerate segments)
// and reports the matching segment parameter so callers can measure the
// true miss distance.
static double raySegmentClosestRayParameter(
    const PickRay &ray,
    const glm::dvec3 &start,
    const glm::dvec3 &end,
    double &segmentParameter,
    double depthNear,
    double depthFar,
    bool semiInfiniteRay)
{
    const glm::dvec3 segment = end - start;
    const double segmentLength2 = glm::dot(segment, segment);
    if (segmentLength2 < 1.0e-24)
    {
        segmentParameter = 0.0;
        return std::numeric_limits<double>::quiet_NaN();
    }

    const glm::dvec3 originToStart = ray.origin - start;
    const double uu = glm::dot(ray.direction, ray.direction);
    const double uv = glm::dot(ray.direction, segment);
    const double vv = segmentLength2;
    const double wd = glm::dot(originToStart, ray.direction);
    const double we = glm::dot(originToStart, segment);
    const double denominator = uu * vv - uv * uv;

    auto clampSegmentParameter = [&](double value) {
        return semiInfiniteRay ? glm::max(0.0, value)
                               : glm::clamp(value, 0.0, 1.0);
    };

    double segmentParameterValue;
    if (denominator > std::max(1.0e-24, vv * 1.0e-14))
        segmentParameterValue = (uu * we - uv * wd) / denominator;
    else
        segmentParameterValue = we / vv;
    segmentParameterValue = clampSegmentParameter(segmentParameterValue);

    double rayParameter = (uv * segmentParameterValue - wd) / uu;
    // A flattened ortho overlay ray is visible even where its true 3D point is
    // outside the object slab.  Do not let the slab reject that screen-space
    // entity; autofocus still ignores infinite entities.
    if (!semiInfiniteRay || !pickIsOrthoProjection())
        rayParameter = glm::clamp(rayParameter, depthNear, depthFar);
    // The closest point on the segment must follow the clamped ray parameter.
    segmentParameterValue = denominator > std::max(1.0e-24, vv * 1.0e-14)
        ? clampSegmentParameter((uv * rayParameter + we) / vv)
        : clampSegmentParameter(we / vv);
    segmentParameter = segmentParameterValue;
    return rayParameter;
}

// The cursor tolerance must be measured where the pick ray actually
// approaches the geometry.  Evaluating it at a fixed reference point (for
// example a long stroke midpoint, which sits 5e5 units away for the
// semi-infinite Ray) inflates it to thousands of world units and lets that
// stroke steal every pick in the view.
bool rayIntersectsSegmentWithTolerance(
    const PickRay &ray,
    const glm::dvec3 &start,
    const glm::dvec3 &end,
    const std::function<double(double rayDepth)> &toleranceAtDepth,
    double &hitDepth,
    double depthNear = pickMinDepth(),
    double depthFar = pickMaxDepth(),
    bool semiInfiniteRay = false)
{
    double segmentParameter = 0.0;
    const double rayParameter = raySegmentClosestRayParameter(
        ray, start, end, segmentParameter, depthNear, depthFar,
        semiInfiniteRay);
    if (!std::isfinite(rayParameter))
        return false;

    const glm::dvec3 rayPoint = ray.origin + ray.direction * rayParameter;
    const glm::dvec3 segmentPoint = start + (end - start) * segmentParameter;
    if (glm::distance(rayPoint, segmentPoint) >
        toleranceAtDepth(rayParameter))
        return false;

    hitDepth = rayParameter;
    return true;
}

bool rayIntersectsSegment(const PickRay &ray,
                          const glm::dvec3 &start,
                          const glm::dvec3 &end,
                          double tolerance,
                          double &hitDepth,
                          double depthNear = pickMinDepth(),
                          double depthFar = pickMaxDepth())
{
    return rayIntersectsSegmentWithTolerance(
        ray, start, end,
        [tolerance](double) { return tolerance; },
        hitDepth, depthNear, depthFar);
}

bool rayIntersectsTriangle(const PickRay &ray,
                           const glm::dvec3 &a,
                           const glm::dvec3 &b,
                           const glm::dvec3 &c,
                           double &hitDepth,
                           double depthNear = pickMinDepth(),
                           double depthFar = pickMaxDepth())
{
    const glm::dvec3 edge1 = b - a;
    const glm::dvec3 edge2 = c - a;
    const glm::dvec3 pvec = glm::cross(ray.direction, edge2);
    const double determinant = glm::dot(edge1, pvec);
    if (std::abs(determinant) < 1.0e-24)
        return false;

    const double inverseDeterminant = 1.0 / determinant;
    const glm::dvec3 tvec = ray.origin - a;
    const double u = glm::dot(tvec, pvec) * inverseDeterminant;
    if (u < 0.0 || u > 1.0)
        return false;

    const glm::dvec3 qvec = glm::cross(tvec, edge1);
    const double v = glm::dot(ray.direction, qvec) * inverseDeterminant;
    if (v < 0.0 || u + v > 1.0)
        return false;

    const double depth = glm::dot(edge2, qvec) * inverseDeterminant;
    if (depth < depthNear || depth > depthFar)
        return false;

    hitDepth = depth;
    return true;
}

bool cadPairedBandPoints(const CadEntityRange &range,
                         const entities::TessellatedEntity &tess,
                         CadPairedBandPoints &band)
{
    if (range.pickShape != CadPickShape::PairedStrokeBand ||
        range.count != 2 || range.begin + 1 >= tess.strokes.size())
    {
        return false;
    }

    band.left = &tess.strokes[range.begin];
    band.right = &tess.strokes[range.begin + 1];
    if (!band.left->common.visible || !band.right->common.visible ||
        band.left->points.size() < 2 ||
        band.left->points.size() != band.right->points.size())
    {
        return false;
    }

    band.segmentCount = band.left->closed
        ? band.left->points.size()
        : band.left->points.size() - 1;
    return band.segmentCount > 0;
}

bool rayIntersectsCadPairedBand(const PickRay &ray,
                                const entities::TessellatedEntity &tess,
                                const CadEntityRange &range,
                                double &hitDepth)
{
    CadPairedBandPoints band;
    if (!cadPairedBandPoints(range, tess, band))
        return false;

    bool hit = false;
    double best = std::numeric_limits<double>::infinity();
    auto consider = [&](const glm::dvec3 &a, const glm::dvec3 &b,
                        const glm::dvec3 &c) {
        double depth = 0.0;
        if (rayIntersectsTriangle(ray, a, b, c, depth) && depth < best)
        {
            best = depth;
            hit = true;
        }
    };

    for (size_t i = 0; i < band.segmentCount; ++i)
    {
        const size_t next = (i + 1) % band.left->points.size();
        const glm::dvec3 &li = band.left->points[i];
        const glm::dvec3 &ln = band.left->points[next];
        const glm::dvec3 &ri = band.right->points[i];
        const glm::dvec3 &rn = band.right->points[next];
        consider(li, ri, rn);
        consider(li, rn, ln);
    }

    hitDepth = best;
    return hit;
}

bool rayIntersectsPoint(const PickRay &ray,
                        const glm::dvec3 &location,
                        double tolerance,
                        double &hitDepth)
{
    const double depth = glm::dot(location - ray.origin, ray.direction);
    if (!pickDepthInRange(depth))
        return false;
    const glm::dvec3 rayPoint = ray.origin + ray.direction * depth;
    if (glm::distance(rayPoint, location) > tolerance)
        return false;

    hitDepth = depth;
    return true;
}

bool rayIntersectsMeshFeatureEdges(const PickRay &ray,
                                   const PickRay &localRay,
                                   const LargeCoordinateObject &object,
                                   double directionLength, double scale,
                                   double localDepthNear, double localDepthFar,
                                   double &hitDepth)
{
    const std::vector<float> &edges =
        rendering::proceduralMeshFeatureEdges(object.mesh);
    const glm::dvec3 objectCenter(object.worldPosition);
    bool hit = false;
    double best = std::numeric_limits<double>::infinity();

    for (size_t vertex = 0; vertex + 1 < edges.size() / 8; vertex += 2)
    {
        auto position = [&](size_t index) {
            const size_t first = index * 8;
            return glm::dvec3(edges[first], edges[first + 1],
                              edges[first + 2]);
        };
        const glm::dvec3 a = position(vertex);
        const glm::dvec3 b = position(vertex + 1);
        const glm::dvec3 worldPoint =
            object.worldPosition + ((a + b) * 0.5) * scale;
        const double tolerance =
            cadPickTolerance(ray, worldPoint) / scale;
        double depth = 0.0;
        if (rayIntersectsSegment(localRay, a, b, tolerance, depth,
                                 localDepthNear, localDepthFar) &&
            depth < best)
        {
            best = depth;
            hit = true;
        }
    }

    if (!hit)
        return false;
    hitDepth = best;
    return true;
}

// Mesh instances are unrotated and uniformly scaled. Refine the candidate AABB
// to the same triangle faces submitted to the renderer; an empty corner of a
// transparent mesh box must not steal focus from a nearby CAD vector.
bool rayIntersectsRenderedMesh(const PickRay &ray,
                               const LargeCoordinateObject &object,
                               double &hitDepth, size_t *faceIndex = nullptr)
{
    const double scale = std::max(1.0e-12, static_cast<double>(object.size));
    const double directionLength = glm::length(ray.direction);
    if (directionLength < 1.0e-20)
        return false;

    const bool ortho = pickIsOrthoProjection();
    const double localDepthNear = ortho
        ? g_pickDepthNear * directionLength / scale : 0.0;
    const double localDepthFar = ortho
        ? g_pickDepthFar * directionLength / scale
        : std::numeric_limits<double>::infinity();
    const glm::dvec3 origin = (ray.origin - object.worldPosition) / scale;
    const glm::dvec3 direction = ray.direction / directionLength;
    double best = std::numeric_limits<double>::infinity();
    const PickRay localRay{origin, direction};

    const rendering::RenderModeFlags renderFlags = rendererBackend
        ? rendererBackend->renderModeFlags()
        : rendering::RenderModeFlags{};
    const bool pickFeatureEdges = !renderFlags.meshFill && renderFlags.show3dEdges;

    // Wireframe/edge modes draw only feature edges in the GPU ID pass. CPU
    // refinement must use the same visibility rule; otherwise an invisible
    // solid face can still satisfy the GPU edge ID and steal a pick.
    if (!pickFeatureEdges)
    {
        const std::vector<float> &soup =
            rendering::proceduralMeshVertices(object.mesh);
        constexpr size_t kVertexFloats =
            rendering::kProceduralMeshFloatStride;

        for (size_t vertex = 0; vertex + 2 < soup.size() / kVertexFloats;
             vertex += 3)
        {
            const auto position = [&](size_t index) {
                const size_t first = index * kVertexFloats;
                return glm::dvec3(soup[first], soup[first + 1],
                                  soup[first + 2]);
            };

            double depth = 0.0;
            if (rayIntersectsTriangle(localRay, position(vertex),
                                      position(vertex + 1),
                                      position(vertex + 2), depth,
                                      localDepthNear, localDepthFar) &&
                depth < best)
            {
                best = depth;
                if (faceIndex)
                    *faceIndex = vertex / 3;
            }
        }
    }

    double edgeDepth = 0.0;
    if (pickFeatureEdges &&
        rayIntersectsMeshFeatureEdges(ray, localRay, object,
                                      directionLength, scale,
                                      localDepthNear, localDepthFar,
                                      edgeDepth) &&
        edgeDepth < best)
    {
        best = edgeDepth;
        if (faceIndex)
            *faceIndex = std::numeric_limits<size_t>::max();
    }

    if (!std::isfinite(best) ||
        best < localDepthNear || best > localDepthFar)
        return false;
    hitDepth = best * scale / directionLength;
    return true;
}

// CAD strokes and points are visible as screen-space primitives, so use a
// cursor-sized tolerance instead of a fixed radius that disappears when the
// large-coordinate demo is zoomed out.
double cadPickTolerance(const PickRay &ray, const glm::dvec3 &worldPoint)
{
    const double worldPerPixel = useOrthoProjection()
        ? 2.0 * orbitCam.orthoSize() / currentDrawableHeight()
        : 2.0 * std::max(0.0,
              glm::dot(worldPoint - ray.origin, orbitCam.Front)) *
          std::tan(glm::radians(45.0) * 0.5) /
          currentDrawableHeight();
    return worldPerPixel * 5.0;
}

double cadCurvePickTolerance(const PickRay &ray,
                             const glm::dvec3 &worldPoint)
{
    // CurveBatchCommand is projected like every other cursor-sized overlay.
    // A fixed world-space radius is wrong as soon as ortho zoom changes.
    return cadPickTolerance(ray, worldPoint);
}

double cadStrokePickTolerance(const PickRay &ray,
                              const entities::Stroke &stroke,
                              const glm::dvec3 &worldPoint)
{
    const double renderedHalfWidth = strokeHalfWidth(stroke);
    return std::max(cadPickTolerance(ray, worldPoint), renderedHalfWidth);
}

double cadPointPickTolerance(const PickRay &ray,
                             const entities::TessellatedPoint &point)
{
    const double baseTolerance = cadPickTolerance(ray, point.location);
    const double worldPerPixel = baseTolerance / 3.0;
    return std::max(baseTolerance, point.pointSize * worldPerPixel);
}


bool pickDebugEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("GRID_PICK_DEBUG");
        return value != nullptr && *value != '\0' &&
               std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool aabbIntersectsPerspectiveFrustum(const CameraSpaceAabb &bounds,
                                      double nearPlane, double farPlane,
                                      double tanHalfVertical,
                                      double tanHalfHorizontal);

struct UnifiedVisibilityQuery
{
  bool isOrtho = false;
  glm::dvec3 cameraPos{0.0};
  glm::dvec3 right{1.0, 0.0, 0.0};
  glm::dvec3 up{0.0, 1.0, 0.0};
  glm::dvec3 front{0.0, 0.0, -1.0};
  double halfWidth = 1.0;
  double halfHeight = 1.0;
  double nearDepth = 0.05;
  double farDepth = 1.0e9;
  double tanHalfVertical = 0.4142;
  double tanHalfHorizontal = 0.4142;
  double drawableHeight = 1.0;
  double minPixelExtent = 0.1;

  static UnifiedVisibilityQuery makeOrtho(
      const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
      const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
      double halfWidthValue, double halfHeightValue, double pixelsHigh)
  {
    return UnifiedVisibilityQuery{
        true, cameraPosition, cameraRight, cameraUp, cameraFront,
        halfWidthValue, halfHeightValue, 0.0, 0.0, 0.0, 0.0,
        pixelsHigh, 0.1};
  }

  static UnifiedVisibilityQuery makePerspective(
      const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
      const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
      double tanHalfV, double tanHalfH, double pixelsHigh,
      double nearPlane, double farPlane)
  {
    return UnifiedVisibilityQuery{
        false, cameraPosition, cameraRight, cameraUp, cameraFront,
        0.0, 0.0, nearPlane, farPlane, tanHalfV, tanHalfH,
        pixelsHigh, 0.1};
  }

  CameraSpaceAabb cameraAabb(const VisibilityCandidate &candidate) const
  {
    const glm::dvec3 center = (candidate.min + candidate.max) * 0.5;
    const glm::dvec3 halfExtent =
        glm::max((candidate.max - candidate.min) * 0.5,
                 glm::dvec3(0.5e-3));
    return cameraAabbBounds(center, halfExtent, cameraPos, right, up, front);
  }

  bool intersects(const VisibilityCandidate &candidate) const
  {
    const CameraSpaceAabb bounds = cameraAabb(candidate);
    if (isOrtho)
      return aabbIntersectsOrthoViewport(bounds, halfWidth, halfHeight);

    return aabbIntersectsPerspectiveFrustum(
        bounds, nearDepth, farDepth, tanHalfVertical, tanHalfHorizontal);
  }

  double screenExtent(const VisibilityCandidate &candidate) const
  {
    const CameraSpaceAabb bounds = cameraAabb(candidate);
    const double worldSize = candidate.lodSize > 0.0
                                 ? candidate.lodSize
                                 : glm::length(candidate.max - candidate.min);
    if (isOrtho)
      return worldSize * drawableHeight / (2.0 * halfHeight);

    const double nearestDepth =
        std::max(nearDepth, std::max(0.0, bounds.minDepth));
    return worldSize * drawableHeight /
           (2.0 * nearestDepth * tanHalfVertical);
  }

  VisibilityState classify(const VisibilityCandidate &candidate) const
  {
    if (!intersects(candidate))
      return VisibilityState::Offscreen;

    // Orthographic CAD work traditionally presents every in-frame object at a
    // predictable world scale.  Keep that behavior while perspective uses the
    // existing point-impostor threshold.
    if (!isOrtho && screenExtent(candidate) < minPixelExtent)
      return VisibilityState::Tiny;
    return VisibilityState::Visible;
  }
};

void buildVisibilityCandidates(std::vector<VisibilityCandidate> &candidates);

VisibilityCandidate makeMeshCandidate(const MeshEntityRecord &mesh,
                                      VisibilityKind kind,
                                      size_t entityIndex = 0);

VisibilityCandidate makeCadRangeCandidate(
    const VectorPrimitivesTessellation &tessellation,
    const CadEntityRange &range, VisibilityKind kind);

const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates();

UnifiedVisibilityQuery makePickingVisibilityQuery()
{
  const double aspect = (double)currentDrawableWidth() /
                        (double)currentDrawableHeight();
  const glm::dvec3 cameraPosition(orbitCam.Position);
  if (useOrthoProjection())
  {
    return UnifiedVisibilityQuery::makeOrtho(
        cameraPosition, orbitCam.Right, orbitCam.Up, orbitCam.Front,
        orbitCam.orthoSize() * aspect, orbitCam.orthoSize(),
        (double)currentDrawableHeight());
  }

  const double tanHalfVertical = std::tan(glm::radians(45.0) * 0.5);
  return UnifiedVisibilityQuery::makePerspective(
      cameraPosition, orbitCam.Right, orbitCam.Up, orbitCam.Front,
      tanHalfVertical, tanHalfVertical * aspect,
      (double)currentDrawableHeight(), 0.05, 1.0e9);
}

VisibilityState classifyMeshVisibility(
    const UnifiedVisibilityQuery &query, const MeshEntityRecord &mesh,
    VisibilityKind kind)
{
  return query.classify(makeMeshCandidate(mesh, kind));
}

int cadOverlayPriority(VisibilityKind kind)
{
    // Points are smallest and easiest to miss, so they resolve exact-depth
    // overlaps with curves. Curves then resolve against filled surfaces.
    switch (kind)
    {
    case VisibilityKind::CadPoint:
        return 2;
    case VisibilityKind::CadStroke:
    case VisibilityKind::CadCurve:
        return 1;
    default:
        return 0;
    }
}

struct PickDebugTrace
{
    std::string meshName;
    double meshDepth = 0.0;
    bool meshHit = false;
    std::string cadName;
    double cadDepth = 0.0;
    bool cadHit = false;
    std::string cadOverlayName;
    VisibilityKind cadOverlayKind = VisibilityKind::CadStroke;
    double cadOverlayDepth = 0.0;
    bool cadOverlayHit = false;
    size_t cadStrokeCount = 0;
    size_t cadFillCount = 0;
    size_t cadPointCount = 0;
};

// BVH-accelerated ray pick with distance-based pruning.
// The collect() predicate returns false for subtrees the ray misses or
// that cannot improve the current best, reducing traversal from O(n)
// to O(log n) for typical spatially coherent scenes.
struct PickResult
{
    glm::dvec3 pivot;
    double hitDepth = 0.0;
    bool hit = false;
    std::string objectName = "Scene";
    // Zero-based index into the selected procedural mesh's triangle list.
    std::optional<size_t> faceIndex;
};

struct AutofocusResult
{
    std::string entityName;
    glm::dvec3 hitPivot;
    double viewDepth = 0.0;
    std::optional<size_t> faceIndex;
};

PickResult pickObjectAlongRay(const PickRay &ray,
                              PickDebugTrace *debugTrace = nullptr)
{
    PickResult result;
    PickDebugTrace localTrace;
    PickDebugTrace &trace = debugTrace ? *debugTrace : localTrace;
    const UnifiedVisibilityQuery pickVisibility = makePickingVisibilityQuery();
    const auto meshObjectState = [&](const MeshEntityRecord *object) {
        if (!meshEntityVisible(*object))
            return VisibilityState::Offscreen;
        return pickVisibility.classify(
            makeMeshCandidate(*object, VisibilityKind::MeshObject));
    };
    double nearestDepth = std::numeric_limits<double>::infinity();
    double nearestMeshDepth = std::numeric_limits<double>::infinity();
    bool nearestMeshOpaque = true;
    double nearestCadDepth = std::numeric_limits<double>::infinity();
    double nearestCadSurfaceDepth = std::numeric_limits<double>::infinity();
    const auto considerHit = [&](double hitDepth, const std::string &name,
                                 std::optional<size_t> faceIndex = std::nullopt) {
        if (hitDepth >= nearestDepth)
            return;
        nearestDepth = hitDepth;
        result.hitDepth = hitDepth;
        result.pivot = ray.origin + ray.direction * hitDepth;
        result.hit = true;
        result.objectName = name;
        result.faceIndex = faceIndex;
    };
    const auto considerMeshHit = [&](double hitDepth, const std::string &name,
                                     size_t faceIndex, bool opaque = true) {
        if (hitDepth < nearestMeshDepth)
        {
            nearestMeshDepth = hitDepth;
            nearestMeshOpaque = opaque;
            trace.meshHit = true;
            trace.meshDepth = hitDepth;
            trace.meshName = name;
        }

        // rayIntersectsRenderedMesh uses max(size_t) as its internal edge
        // marker. Public pick results represent an edge hit by having no face.
        std::optional<size_t> pickedFace;
        if (faceIndex != std::numeric_limits<size_t>::max())
            pickedFace = faceIndex;
        considerHit(hitDepth, name, pickedFace);
    };
    const auto considerCadHit = [&](double hitDepth, const char *name,
                                    std::optional<size_t> faceIndex = std::nullopt) {
        if (hitDepth < nearestCadDepth)
        {
            nearestCadDepth = hitDepth;
            trace.cadHit = true;
            trace.cadDepth = hitDepth;
            trace.cadName = name;
        }
        considerHit(hitDepth, name, faceIndex);
    };
    const auto considerCadSurfaceHit = [&](double hitDepth,
                                           const char *name,
                                           std::optional<size_t> faceIndex = std::nullopt) {
        if (hitDepth < nearestCadSurfaceDepth)
            nearestCadSurfaceDepth = hitDepth;
        considerCadHit(hitDepth, name, faceIndex);
    };
    // Strokes and points are rendered as cursor-sized screen-space overlays.
    // Since the demo meshes are intentionally translucent, honor their visible
    // hit even when a mesh surface is slightly closer along the same ray.
    const auto considerCadOverlayHit = [&](double hitDepth,
                                           const char *name,
                                           VisibilityKind kind) {
        constexpr double overlayEpsilon = 1.0e-6;
        const int priority = cadOverlayPriority(kind);
        if (!trace.cadOverlayHit ||
            hitDepth < trace.cadOverlayDepth - overlayEpsilon ||
            (hitDepth <= trace.cadOverlayDepth + overlayEpsilon &&
             priority > cadOverlayPriority(trace.cadOverlayKind)))
        {
            trace.cadOverlayHit = true;
            trace.cadOverlayKind = kind;
            trace.cadOverlayDepth = hitDepth;
            trace.cadOverlayName = name;
        }
        considerCadHit(hitDepth, name);
    };

    std::vector<const LargeCoordinateObject *> candidates;
    getSceneObjectBvh().collect(
        [&](const WorldAabb2 &bounds) {
            double hitDepth = 0.0;
            if (!rayIntersectsAabb(ray, bounds, hitDepth))
                return false; // ray misses this subtree entirely
            if (hitDepth >= nearestDepth)
                return false; // subtree entry is already behind the best hit
            return true;      // traverse deeper / collect leaf objects
        },
        candidates);

    // Refine with exact per-object AABBs (leaf bounds may be merged).
    for (const LargeCoordinateObject *object : candidates)
    {
        if (!meshEntityVisible(*object))
            continue;
        const glm::dvec3 halfExtent(object->size * 0.5);
        const WorldAabb2 objBounds{object->worldPosition - halfExtent,
                                   object->worldPosition + halfExtent};
        double hitDepth = 0.0;
        size_t faceIndex = 0;
        if (rayIntersectsAabb(ray, objBounds, hitDepth) &&
            hitDepth < nearestDepth &&
            meshObjectState(object) != VisibilityState::Offscreen &&
            rayIntersectsRenderedMesh(ray, *object, hitDepth, &faceIndex))
        {
            considerMeshHit(hitDepth, object->displayName(), faceIndex,
                            object->entity.common.color.a >= 0.999f);
        }
    }

    // Also test the center cube, which is rendered outside the BVH.
    {
        double hitDepth = 0.0;
        size_t faceIndex = 0;
        const MeshEntityRecord centerCube = getCenterCubeEntity();
        if (classifyMeshVisibility(pickVisibility, centerCube,
                                   VisibilityKind::CenterCube) !=
                VisibilityState::Offscreen &&
            rayIntersectsRenderedMesh(ray, centerCube, hitDepth,
                                      &faceIndex) &&
            hitDepth < nearestDepth)
        {
            considerMeshHit(hitDepth, centerCube.displayName(), faceIndex,
                            centerCube.entity.common.color.a >= 0.999f);
        }
    }

    // CAD vector primitives are CPU-tessellated for drawing; test that same
    // geometry so lines, curves, fills, and points participate in autofocus.
    if (cadEntityDemoEnabled())
    {
        const VectorPrimitivesTessellation &cad =
            getVectorPrimitivesTessellation();
        std::vector<const VisibilityCandidate *> cadHits;
        getCadRangeBvh().collect(
            [&](const WorldAabb2 &bounds) {
                double hitDepth = 0.0;
                return rayIntersectsAabb(ray, bounds, hitDepth);
            },
            cadHits);
        std::array<std::vector<const VisibilityCandidate *>, 4>
            cadCandidates;
        for (const VisibilityCandidate *candidate : cadHits)
        {
            const size_t slot =
                candidate->kind == VisibilityKind::CadStroke ? 0
                : candidate->kind == VisibilityKind::CadFill ? 1
                : candidate->kind == VisibilityKind::CadCurve ? 2
                                                             : 3;
            cadCandidates[slot].push_back(candidate);
        }
        const auto cadMeshState = [&](const MeshEntityRecord &mesh) {
            if (!meshEntityVisible(mesh))
                return VisibilityState::Offscreen;
            return pickVisibility.classify(
                makeMeshCandidate(mesh, VisibilityKind::CadMesh));
        };
        if (debugTrace)
        {
            trace.cadStrokeCount = cad.geometry.strokes.size();
            trace.cadFillCount = cad.geometry.fills.size();
            trace.cadPointCount = cad.geometry.points.size();
        }
        for (const VisibilityCandidate *candidate : cadCandidates[0])
        {
            const CadEntityRange &range = *candidate->cadRange;

            double bandDepth = 0.0;
            if (rayIntersectsCadPairedBand(ray, cad.geometry, range,
                                           bandDepth))
            {
                considerCadOverlayHit(bandDepth, range.name.c_str(),
                                      VisibilityKind::CadStroke);
                continue;
            }

            for (size_t strokeIndex = range.begin;
                 strokeIndex < range.begin + range.count; ++strokeIndex)
            {
                const entities::Stroke &stroke =
                    cad.geometry.strokes[strokeIndex];
                if (!stroke.common.visible || stroke.points.size() < 2)
                    continue;

                const size_t segmentCount =
                    stroke.closed ? stroke.points.size()
                                  : stroke.points.size() - 1;
                for (size_t i = 0; i < segmentCount; ++i)
                {
                    double hitDepth = 0.0;
                    const size_t next = (i + 1) % stroke.points.size();
                    if (rayIntersectsSegmentWithTolerance(
                            ray, stroke.points[i], stroke.points[next],
                            [&](double depth) {
                                return cadStrokePickTolerance(
                                    ray, stroke,
                                    ray.origin + ray.direction * depth);
                            },
                            hitDepth, pickMinDepth(), pickMaxDepth(),
                            stroke.semiInfinite))
                    {
                        considerCadOverlayHit(
                            hitDepth, range.name.c_str(),
                            VisibilityKind::CadStroke);
                    }
                }
            }
        }

        for (const VisibilityCandidate *candidate : cadCandidates[1])
        {
            const CadEntityRange &range = *candidate->cadRange;

            for (size_t fillIndex = range.begin;
                 fillIndex < range.begin + range.count; ++fillIndex)
            {
                const entities::Triangle &triangle =
                    cad.geometry.fills[fillIndex];
                if (!triangle.common.visible)
                    continue;

                double hitDepth = 0.0;
                if (rayIntersectsTriangle(ray, triangle.a, triangle.b,
                                          triangle.c, hitDepth))
                {
                    considerCadSurfaceHit(hitDepth, range.name.c_str());
                }
            }
        }

        for (size_t meshIndex = 0; meshIndex < cad.meshes.size(); ++meshIndex)
        {
            const MeshEntityRecord &mesh = cad.meshes[meshIndex];
            double hitDepth = 0.0;
            size_t faceIndex = 0;
            if (cadMeshState(mesh) != VisibilityState::Offscreen &&
                rayIntersectsRenderedMesh(ray, mesh, hitDepth, &faceIndex))
            {
                if (hitDepth < nearestMeshDepth)
                {
                    nearestMeshDepth = hitDepth;
                    nearestMeshOpaque = mesh.entity.common.color.a >= 0.999f;
                }
                considerCadSurfaceHit(hitDepth, mesh.displayName().c_str(),
                                      faceIndex);
                // Cad meshes go through the CAD surface path, but remember their
                // translucency for overlay resolution below.
            }
        }

        for (const VisibilityCandidate *candidate : cadCandidates[2])
        {
            if (!candidate->curve)
                continue;
            const std::vector<glm::dvec3> points =
                sampleCurveBatch(*candidate->curve);
            for (size_t i = 0; i + 1 < points.size(); ++i)
            {
                double hitDepth = 0.0;
                if (rayIntersectsSegment(ray, points[i], points[i + 1],
                                         cadCurvePickTolerance(ray,
                                         points[i + 1]),
                                         hitDepth))
                {
                    considerCadOverlayHit(
                        hitDepth, candidate->curve->name.c_str(),
                        VisibilityKind::CadCurve);
                }
            }
        }

        for (const VisibilityCandidate *candidate : cadCandidates[3])
        {
            const CadEntityRange &range = *candidate->cadRange;

            for (size_t pointIndex = range.begin;
                 pointIndex < range.begin + range.count; ++pointIndex)
            {
                const entities::TessellatedPoint &point =
                    cad.geometry.points[pointIndex];
                if (!point.common.visible)
                    continue;

                double hitDepth = 0.0;
                if (rayIntersectsPoint(ray, point.location,
                                       cadPointPickTolerance(ray, point),
                                       hitDepth))
                {
                    considerCadOverlayHit(
                        hitDepth, range.name.c_str(),
                        VisibilityKind::CadPoint);
                }
            }
        }
    }

    if (trace.cadOverlayHit)
    {
        const glm::dvec3 overlayPivot =
            ray.origin + ray.direction * trace.cadOverlayDepth;
        // Overlay priority matches non-depth-writing compositing: choose an
        // overlay behind a transparent mesh, but never behind an opaque one.
        const bool nearestMeshTranslucent =
            std::isfinite(nearestMeshDepth) && !nearestMeshOpaque;
        if (!std::isfinite(nearestCadSurfaceDepth) ||
            trace.cadOverlayDepth <= nearestCadSurfaceDepth ||
            nearestMeshTranslucent)
        {
            result.hit = true;
            result.hitDepth = trace.cadOverlayDepth;
            result.pivot = overlayPivot;
            result.objectName = trace.cadOverlayName;
            result.faceIndex.reset();
        }
    }

    return result;
}

// OpenCADStudio-style view-center pivot: casts from the viewport center.
std::optional<glm::dvec3> viewCenterObjectPivot()
{
    const PickRay ray = pickRayFromNdc(0.0, 0.0);
    const PickResult result = pickObjectAlongRay(ray);
    if (result.hit)
        return result.pivot;
    return std::nullopt;
}

// Infinite CAD strokes have no finite pivot that should drive the orbit
// depth.  They may be selected and outlined, but focusing on a point along
// them can move the orthographic slab and make the visible stroke grow, so
// leave target depth unchanged for these entities.
bool entityNameIsInfinite(const std::string &name)
{
  return name == "Ray" || name == "XLine";
}

// Double-click autofocus: focus at the view depth of the nearest object under
// the cursor while keeping the eye fixed.  The orbit target remains on the
// camera's center axis; orthographic zoom is compensated to preserve framing.
std::optional<AutofocusResult> autofocusAtNdc(
    double ndcX, double ndcY, const GpuPickCameraBasis *camera = nullptr)
{
    const PickRay ray =
        camera ? pickRayFromNdc(ndcX, ndcY, *camera)
               : pickRayFromNdc(ndcX, ndcY);
    PickDebugTrace trace;
    const PickResult result = pickObjectAlongRay(ray, &trace);
    if (pickDebugEnabled())
    {
        std::cout << std::fixed << std::setprecision(3)
                  << "Pick: cad={" << trace.cadHit << "," << trace.cadName
                  << "," << trace.cadDepth << "} mesh={" << trace.meshHit
                  << "," << trace.meshName << "," << trace.meshDepth
                  << "} cache=(" << trace.cadStrokeCount << "/"
                  << trace.cadFillCount << "/" << trace.cadPointCount << ")"
                  << std::defaultfloat << std::endl;
    }
    if (!result.hit)
        return std::nullopt;

    // Project the ray hit onto the camera Front axis to get the view depth
    // (distance along the gaze direction, not the slant-ray distance). The
    // orbit target stays on the camera's center axis; it is intentionally not
    // moved to an off-center cursor ray's world-space hit pivot.
    const double viewDepth =
        glm::dot(result.pivot - orbitCam.Position, orbitCam.Front);
    if (!entityNameIsInfinite(result.objectName))
        orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
    return AutofocusResult{result.objectName, result.pivot, viewDepth};
}

// The GPU pass only identifies an entity.  Refine against its CPU geometry so
// every hit point still comes from the same exact primitive tests used by the
// fallback picker.
std::optional<AutofocusResult> autofocusGpuPick(uint32_t objectId,
                                                double ndcX, double ndcY)
{
    // Refine with the camera pose captured at request time so the CPU ray
    // matches the pose the GPU ID pass actually rendered, even when the
    // readback completes several frames later.
    const PickRay ray =
        gpuPickFocus.camera
            ? pickRayFromNdc(ndcX, ndcY, *gpuPickFocus.camera)
            : pickRayFromNdc(ndcX, ndcY);
    const GpuPickEntity *pickEntity = findGpuPickEntity(objectId);
    MeshEntityRecord center;
    const MeshEntityRecord *meshEntity =
        pickEntity && (pickEntity->kind == VisibilityKind::MeshObject ||
                       pickEntity->kind == VisibilityKind::CadMesh)
            ? pickEntity->mesh
            : nullptr;
    if (objectId == kGpuPickCenterCubeId)
    {
        center = getCenterCubeEntity();
        meshEntity = &center;
    }

    if (meshEntity)
    {
        if (!meshEntityVisible(*meshEntity))
            return std::nullopt;

        double hitDepth = 0.0;
        size_t faceIndex = 0;
        if (!rayIntersectsRenderedMesh(ray, *meshEntity, hitDepth,
                                       &faceIndex))
            return std::nullopt;

        const glm::dvec3 hitPivot = ray.origin + ray.direction * hitDepth;
        const double viewDepth =
            glm::dot(hitPivot - orbitCam.Position, orbitCam.Front);
        if (!entityNameIsInfinite(meshEntity->displayName()))
            orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
        std::optional<size_t> pickedFace;
        if (faceIndex != std::numeric_limits<size_t>::max())
            pickedFace = faceIndex;
        return AutofocusResult{meshEntity->displayName(), hitPivot,
                               viewDepth, pickedFace};
    }

    if (pickEntity && pickEntity->kind == VisibilityKind::CadCurve &&
        pickEntity->curve)
    {
        const std::vector<glm::dvec3> points =
            sampleCurveBatch(*pickEntity->curve);
        bool hit = false;
        double bestDepth = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            double depth = 0.0;
            if (rayIntersectsSegment(ray, points[i], points[i + 1],
                                     cadCurvePickTolerance(ray, points[i + 1]), depth) &&
                depth < bestDepth)
            {
                hit = true;
                bestDepth = depth;
            }
        }
        if (!hit)
            return std::nullopt;
        const glm::dvec3 hitPivot = ray.origin + ray.direction * bestDepth;
        const double viewDepth =
            glm::dot(hitPivot - orbitCam.Position, orbitCam.Front);
        if (!entityNameIsInfinite(pickEntity->curve->name))
            orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
        return AutofocusResult{pickEntity->curve->name, hitPivot, viewDepth};
    }

    if (!pickEntity || !pickEntity->cadRange)
        return std::nullopt;

    const VectorPrimitivesTessellation &cad =
        getVectorPrimitivesTessellation();
    const CadEntityRange &range = *pickEntity->cadRange;
    if (!range.count)
        return std::nullopt;

    bool hit = false;
    double bestDepth = std::numeric_limits<double>::infinity();
    size_t primitiveIndex = 0;
    auto considerPrimitive = [&](double depth, size_t index) {
        if (depth < bestDepth)
        {
            hit = true;
            bestDepth = depth;
            primitiveIndex = index;
        }
    };

    if (pickEntity->kind == VisibilityKind::CadStroke)
    {
        if (range.begin >= cad.geometry.strokes.size())
            return std::nullopt;
        double bandDepth = 0.0;
        if (rayIntersectsCadPairedBand(ray, cad.geometry, range,
                                       bandDepth))
        {
            considerPrimitive(bandDepth, range.begin);
        }
        else
        {
            const size_t end = std::min(cad.geometry.strokes.size(),
                                        range.begin + range.count);
            for (size_t strokeIndex = range.begin; strokeIndex < end;
                 ++strokeIndex)
            {
                const entities::Stroke &stroke =
                    cad.geometry.strokes[strokeIndex];
                const size_t pointCount = stroke.points.size();
                if (!stroke.common.visible || pointCount < 2)
                    continue;

                const size_t segmentCount =
                    stroke.closed ? pointCount : pointCount - 1;
                for (size_t segment = 0; segment < segmentCount; ++segment)
                {
                    const size_t next = (segment + 1) % pointCount;
                    double depth = 0.0;
                    if (rayIntersectsSegmentWithTolerance(
                            ray, stroke.points[segment],
                            stroke.points[next],
                            [&](double depthAtHit) {
                                return cadStrokePickTolerance(
                                    ray, stroke,
                                    ray.origin +
                                    ray.direction * depthAtHit);
                            },
                            depth, pickMinDepth(), pickMaxDepth(),
                            stroke.semiInfinite))
                    {
                        considerPrimitive(depth, strokeIndex);
                    }
                }
            }
        }
    }
    else if (pickEntity->kind == VisibilityKind::CadFill)
    {
        if (range.begin >= cad.geometry.fills.size())
            return std::nullopt;
        const size_t end = std::min(cad.geometry.fills.size(),
                                    range.begin + range.count);
        for (size_t fillIndex = range.begin; fillIndex < end; ++fillIndex)
        {
            const entities::Triangle &triangle = cad.geometry.fills[fillIndex];
            if (!triangle.common.visible)
                continue;

            double depth = 0.0;
            if (rayIntersectsTriangle(ray, triangle.a, triangle.b,
                                      triangle.c, depth))
            {
                considerPrimitive(depth, fillIndex);
            }
        }
    }
    else if (pickEntity->kind == VisibilityKind::CadPoint)
    {
        if (range.begin >= cad.geometry.points.size())
            return std::nullopt;
        const size_t end = std::min(cad.geometry.points.size(),
                                    range.begin + range.count);
        for (size_t pointIndex = range.begin; pointIndex < end; ++pointIndex)
        {
            const entities::TessellatedPoint &point =
                cad.geometry.points[pointIndex];
            if (!point.common.visible)
                continue;

            double depth = 0.0;
            if (rayIntersectsPoint(ray, point.location,
                                   cadPointPickTolerance(ray, point), depth))
            {
                considerPrimitive(depth, pointIndex);
            }
        }
    }
    else
    {
        return std::nullopt;
    }

    if (!hit)
        return std::nullopt;

    const glm::dvec3 hitPivot = ray.origin + ray.direction * bestDepth;
    const double viewDepth = glm::dot(hitPivot - orbitCam.Position,
                                      orbitCam.Front);
    if (!entityNameIsInfinite(range.name))
        orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
    return AutofocusResult{range.name, hitPivot, viewDepth, primitiveIndex};
}

void reportAutofocus(const AutofocusResult &selected)
{
    std::cout << std::fixed << std::setprecision(3)
              << "Autofocus: entity=" << selected.entityName
              << " pivot=(" << selected.hitPivot.x << ", "
              << selected.hitPivot.y << ", " << selected.hitPivot.z << ")"
              << " target=(" << orbitCam.Target.x << ", "
              << orbitCam.Target.y << ", " << orbitCam.Target.z << ")"
              << " depth=" << selected.viewDepth
              << " distance=" << orbitCam.Distance
              << " face="
              << (selected.faceIndex ? std::to_string(*selected.faceIndex)
                                     : std::string("edge"))
              << std::endl;
}

const GpuPickEntity *findGpuPickEntityForAutofocusName(
    const std::string &name)
{
  for (const auto &[objectId, registered] : gpuPickRegistry())
  {
    if (registered.mesh && registered.mesh->displayName() == name)
      return &registered;
    if (registered.cadRange && registered.cadRange->name == name)
      return &registered;
    if (registered.curve && registered.curve->name == name)
      return &registered;
  }
  return nullptr;
}

static void reportGpuPickFallback(double ndcX, double ndcY)
{
  if (pickDebugEnabled())
  {
    std::printf("[PICK_DEBUG] fallback ndc=(%f,%f)", ndcX, ndcY);
    std::puts("");
  }

  outlineEntity.reset();
  lockedOutlineId = 0;
  // The captured pose survives until the focus cycle resolves, so a fallback
  // triggered by a failed refinement still refines with the rendered pose.
  const GpuPickCameraBasis *capturedCamera =
      gpuPickFocus.camera ? &*gpuPickFocus.camera : nullptr;
  if (const std::optional<AutofocusResult> selectedEntity =
          autofocusAtNdc(ndcX, ndcY, capturedCamera))
  {
    reportAutofocus(*selectedEntity);
    if (const GpuPickEntity *entity =
            findGpuPickEntityForAutofocusName(selectedEntity->entityName))
    {
      outlineEntity = *entity;
      lockedOutlineId = findGpuPickObjectIdForEntity(*entity);
      if (pickDebugEnabled())
      {
        std::printf("[PICK_DEBUG] fallback outline entity=%s id=%u",
                    selectedEntity->entityName.c_str(), lockedOutlineId);
        std::puts("");
      }
    }
    else if (pickDebugEnabled())
    {
      std::printf("[PICK_DEBUG] fallback outline lookup failed for %s",
                  selectedEntity->entityName.c_str());
      std::puts("");
    }
  }
}

// Deterministic diagnostics for entities that render but lose CPU picking.
// Each range contributes one representative sample (the first segment midpoint,
// stable interior triangle point, or point), probed from several orbit views.
bool runCadPickAudit()
{
  const char *flag = std::getenv("GRID_PICK_AUDIT");
  if (!flag || *flag == '\0' || std::strcmp(flag, "0") == 0)
    return false;

  struct AuditStats
  {
    VisibilityKind kind = VisibilityKind::CadStroke;
    size_t samples = 0;
    size_t matched = 0;
    size_t misses = 0;
    size_t offscreen = 0;
    size_t direct = 0;
    std::map<std::string, size_t> blockers;
  };

  const VectorPrimitivesTessellation &cad = getVectorPrimitivesTessellation();
  struct AuditSample
  {
    std::string name;
    VisibilityKind kind;
    glm::dvec3 location;
    double extent = 0.0;
    VisibilityCandidate candidate;
    const MeshEntityRecord *mesh = nullptr;
  };
  std::vector<AuditSample> samples;
  samples.reserve(cad.strokeRanges.size() + cad.fillRanges.size() +
                  cad.pointRanges.size() + cad.fillRanges.size() +
                  cad.meshes.size() + 4);

  for (const CadEntityRange &range : cad.strokeRanges)
  {
    if (!range.count)
      continue;
    const entities::Stroke &stroke =
        cad.geometry.strokes[range.begin];
    if (!stroke.common.visible || stroke.points.size() < 2)
      continue;
    const double extent = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadStroke).lodSize;
    const glm::dvec3 location =
        (stroke.points[0] + stroke.points[1]) * 0.5;
    AuditSample sample{range.name, VisibilityKind::CadStroke,
                       location, extent};
    sample.candidate = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadStroke);
    samples.push_back(std::move(sample));
  }
  for (const CadEntityRange &range : cad.fillRanges)
  {
    if (!range.count)
      continue;
    const entities::Triangle &fill = cad.geometry.fills[range.begin];
    if (!fill.common.visible)
      continue;
    const double extent = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadFill).lodSize;
    const double localExtent = std::max(
        {glm::distance(fill.a, fill.b),
         glm::distance(fill.b, fill.c),
         glm::distance(fill.c, fill.a)});
    AuditSample sample{range.name, VisibilityKind::CadFill,
                       (fill.a + fill.b + fill.c) / 3.0, localExtent};
    sample.candidate = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadFill);
    AuditSample interiorSample = sample;
    // Avoid the centroid/median: a leader stroke can own that line even when
    // the rest of the fill remains directly selectable.
    interiorSample.location =
        fill.a * 0.20 + fill.b * 0.30 + fill.c * 0.50;
    samples.push_back(std::move(sample));
    samples.push_back(std::move(interiorSample));
  }
  for (const CadEntityRange &range : cad.pointRanges)
  {
    if (!range.count)
      continue;
    const entities::TessellatedPoint &point =
        cad.geometry.points[range.begin];
    if (!point.common.visible)
      continue;
    const double extent = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadPoint).lodSize;
    AuditSample sample{range.name, VisibilityKind::CadPoint,
                       point.location, extent};
    sample.candidate = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadPoint);
    samples.push_back(std::move(sample));
  }

  auto addMeshSample = [&](const MeshEntityRecord &mesh,
                           VisibilityKind kind) {
    if (!meshEntityVisible(mesh))
      return;

    AuditSample sample;
    sample.name = mesh.displayName();
    sample.kind = kind;
    sample.location = mesh.worldPosition;
    sample.extent = mesh.size;
    sample.candidate = makeMeshCandidate(mesh, kind);
    sample.mesh = &mesh;
    if (mesh.mesh == rendering::MeshType::Torus)
    {
      // A ray through the torus center passes through the hole. Aim at a
      // point on the tube so the diagnostic validates the rendered surface.
      sample.location += glm::dvec3(0.325, 0.175, 0.0) *
          static_cast<double>(mesh.size);
    }
    samples.push_back(std::move(sample));
  };

  for (const MeshEntityRecord &mesh : cad.meshes)
    addMeshSample(mesh, VisibilityKind::CadMesh);

  static constexpr rendering::MeshType kAuditMeshTypes[] = {
      rendering::MeshType::Cube, rendering::MeshType::Sphere,
      rendering::MeshType::Cone, rendering::MeshType::Torus};
  for (const rendering::MeshType meshType : kAuditMeshTypes)
  {
    const auto found = std::find_if(
        getStressObjects().begin(), getStressObjects().end(),
        [&](const MeshEntityRecord &mesh) {
          return mesh.mesh == meshType;
        });
    if (found != getStressObjects().end())
      addMeshSample(*found, VisibilityKind::MeshObject);
  }

  std::cout << "CAD pick audit: samples="
            << samples.size() << std::endl;
  if (samples.empty())
    return true;

  const bool previousOrtho = useOrthoProjection();
  const glm::dquat previousRotation = orbitCam.Rotation;
  const glm::dvec3 previousTarget = orbitCam.Target;
  const double previousDistance = orbitCam.Distance;
  const glm::dvec3 previousWorldUp = orbitCam.WorldUp;
  const float previousZoom = orbitCam.Zoom;
  useOrthoProjection() = true;
  orbitCam.Zoom = 45.0f;

  constexpr int kAzimuthCount = 8;
  constexpr double kElevations[] = {
      glm::radians(35.0), glm::radians(70.0)};
  using AuditKey = std::pair<VisibilityKind, std::string>;
  std::map<AuditKey, AuditStats> results;
  for (const AuditSample &sample : samples)
  {
    AuditStats stats;
    stats.kind = sample.kind;
    results.emplace(AuditKey{sample.kind, sample.name}, stats);
  }

  auto setAuditCamera = [&](const glm::dvec3 &target,
                            const glm::dvec3 &viewDirection, double distance) {
    const glm::dvec3 front = glm::normalize(viewDirection);
    glm::dvec3 right(1.0, 0.0, 0.0);
    if (std::abs(front.z) < 0.999)
      right = glm::normalize(
          glm::cross(front, glm::dvec3(0.0, 0.0, 1.0)));
    const glm::dvec3 up = glm::normalize(glm::cross(right, front));
    orbitCam.Rotation = glm::dquat(
        glm::dmat3(right, up, -front));
    orbitCam.setOrbit(target, std::max(1.0, distance));
  };

  auto recordResult = [&](const AuditSample &sample,
                          const PickResult &result) {
    AuditStats &stats =
        results[AuditKey{sample.kind, sample.name}];
    ++stats.samples;
    if (result.hit && result.objectName == sample.name)
    {
      ++stats.matched;
      return;
    }
    if (result.hit)
      ++stats.blockers[result.objectName];
    else
      ++stats.misses;
  };

  auto recordSample = [&](const AuditSample &sample,
                          const PickResult &result) {
    const UnifiedVisibilityQuery visibility = makePickingVisibilityQuery();
    AuditStats &stats =
        results[AuditKey{sample.kind, sample.name}];
    const PickRay ray{orbitCam.Position,
                      glm::normalize(sample.location - orbitCam.Position)};
    bool directHit = false;
    if (sample.kind == VisibilityKind::CadStroke)
    {
      for (size_t i = sample.candidate.rangeBegin;
           i < sample.candidate.rangeBegin + sample.candidate.rangeCount; ++i)
      {
        const entities::Stroke &stroke = cad.geometry.strokes[i];
        for (size_t j = 0; j + 1 < stroke.points.size(); ++j)
        {
          double depth = 0.0;
          directHit |= rayIntersectsSegment(
              ray, stroke.points[j], stroke.points[j + 1],
              cadPickTolerance(ray, sample.location), depth);
        }
      }
    }
    else if (sample.kind == VisibilityKind::CadFill)
    {
      for (size_t i = sample.candidate.rangeBegin;
           i < sample.candidate.rangeBegin + sample.candidate.rangeCount; ++i)
      {
        const entities::Triangle &fill = cad.geometry.fills[i];
        double depth = 0.0;
        directHit |= rayIntersectsTriangle(
            ray, fill.a, fill.b, fill.c, depth);
      }
    }
    else if (sample.mesh)
    {
      double depth = 0.0;
      directHit = rayIntersectsRenderedMesh(ray, *sample.mesh, depth);
    }
    else
    {
      for (size_t i = sample.candidate.rangeBegin;
           i < sample.candidate.rangeBegin + sample.candidate.rangeCount; ++i)
      {
        double depth = 0.0;
        directHit |= rayIntersectsPoint(
            ray, cad.geometry.points[i].location,
            cadPickTolerance(ray, sample.location), depth);
      }
    }
    if (directHit)
      ++stats.direct;
    if (sample.candidate.lodSize > 0.0 &&
        visibility.classify(sample.candidate) == VisibilityState::Offscreen)
      ++stats.offscreen;
    else
      recordResult(sample, result);
  };

  for (const AuditSample &sample : samples)
  {
    const double distance = std::max(
        32.0, std::max(sample.extent * 2.0, sample.extent + 32.0));

    for (int azimuth = 0; azimuth < kAzimuthCount; ++azimuth)
    {
      const double angle = glm::two_pi<double>() * azimuth / kAzimuthCount;
      for (const double elevation : kElevations)
      {
        const glm::dvec3 viewDirection = glm::normalize(glm::dvec3(
            std::cos(elevation) * std::cos(angle),
            std::cos(elevation) * std::sin(angle),
            std::sin(elevation)));
        setAuditCamera(sample.location, viewDirection, distance);
        const PickRay ray{orbitCam.Position,
                          glm::normalize(sample.location - orbitCam.Position)};
        recordSample(sample, pickObjectAlongRay(ray));
      }
    }

    setAuditCamera(sample.location, glm::dvec3(0.0, 0.0, -1.0), distance);
    recordSample(sample, pickObjectAlongRay(
        {orbitCam.Position, glm::dvec3(0.0, 0.0, -1.0)}));
    setAuditCamera(sample.location, glm::dvec3(0.0, 0.0, 1.0), distance);
    recordSample(sample, pickObjectAlongRay(
        {orbitCam.Position, glm::dvec3(0.0, 0.0, 1.0)}));

  }

  for (const auto &[key, stats] : results)
  {
    if (stats.matched != 0)
      continue;

    std::cout << std::fixed << std::setprecision(3)
              << "CAD pick audit FAIL name=" << key.second
              << " kind="
              << (key.first == VisibilityKind::CadStroke ? "stroke"
                    : key.first == VisibilityKind::CadFill ? "fill"
                    : key.first == VisibilityKind::CadPoint ? "point"
                    : "mesh")
              << " samples=" << stats.samples
              << " matched=" << stats.matched
              << " offscreen=" << stats.offscreen
              << " direct=" << stats.direct
              << " misses=" << stats.misses << " blockers={";
    bool firstBlocker = true;
    for (const auto &[blockerName, count] : stats.blockers)
    {
      if (!firstBlocker)
        std::cout << ", ";
      firstBlocker = false;
      std::cout << blockerName << "=" << count;
    }
    std::cout << "}" << std::defaultfloat << std::endl;
  }

  useOrthoProjection() = previousOrtho;
  orbitCam.Zoom = previousZoom;
  orbitCam.WorldUp = previousWorldUp;
  orbitCam.Rotation = previousRotation;
  orbitCam.setOrbit(previousTarget, previousDistance);
  return true;
}

// Convert SDL window coordinates to NDC [-1, 1]. Integer event coordinates
// address a pixel cell; sampling that cell on the GPU uses its center, so add
// the half-pixel offset here. Without it a one-pixel GPU pick frustum can fall
// on the neighboring cell for coordinates exactly on the cell boundary.
double cursorToNdcX(double windowX, int windowWidth)
{
    return windowWidth > 0 ? 2.0 * (windowX + 0.5) / windowWidth - 1.0 : 0.0;
}
double cursorToNdcY(double windowY, int windowHeight)
{
    return windowHeight > 0 ? 1.0 - 2.0 * (windowY + 0.5) / windowHeight : 0.0;
}

// Move the target to the nearest object AABB along the view ray.  The ray
// origin/eye and image do not move; only the focus depth changes.  Do not
// reset slab hysteresis here: preserving the eye preserves camera-space
// content depths, so the stabilizer should still be allowed to shrink.
bool aabbIntersectsPerspectiveFrustum(const CameraSpaceAabb &bounds,
                                      double nearPlane, double farPlane,
                                      double tanHalfVertical,
                                      double tanHalfHorizontal)
{
  // Test the six frustum half-spaces directly.  For each plane, the largest
  // signed distance being negative is the only possible AABB rejection.
  const auto outside = [&](double x, double y, double z, double offset) {
    const double minimum =
        (x < 0.0 ? bounds.maxX : bounds.minX) * x +
        (y < 0.0 ? bounds.maxY : bounds.minY) * y +
        (z < 0.0 ? bounds.maxDepth : bounds.minDepth) * z + offset;
    const double maximum =
        (x < 0.0 ? bounds.minX : bounds.maxX) * x +
        (y < 0.0 ? bounds.minY : bounds.maxY) * y +
        (z < 0.0 ? bounds.minDepth : bounds.maxDepth) * z + offset;
    return maximum < 0.0;
  };

  return !outside( 0.0,  0.0,  1.0, -nearPlane) &&       // z >= near
         !outside( 0.0,  0.0, -1.0,  farPlane) &&        // z <= far
         !outside( 1.0,  0.0,  tanHalfHorizontal, 0.0) && // left
         !outside(-1.0,  0.0,  tanHalfHorizontal, 0.0) && // right
         !outside( 0.0,  1.0,  tanHalfVertical,   0.0) && // bottom
         !outside( 0.0, -1.0,  tanHalfVertical,   0.0);   // top
}

void expandWorldAabb(glm::dvec3 &minimum, glm::dvec3 &maximum,
                     const glm::dvec3 &point)
{
  minimum = glm::min(minimum, point);
  maximum = glm::max(maximum, point);
}

VisibilityCandidate makeMeshCandidate(const MeshEntityRecord &mesh,
                                      VisibilityKind kind,
                                      size_t entityIndex)
{
  const glm::dvec3 halfExtent(mesh.size * 0.5);
  VisibilityCandidate candidate;
  candidate.kind = kind;
  candidate.entityIndex = entityIndex;
  candidate.mesh = &mesh;
  candidate.min = mesh.worldPosition - halfExtent;
  candidate.max = mesh.worldPosition + halfExtent;
  candidate.center = mesh.worldPosition;
  candidate.lodSize = mesh.size;
  candidate.overlayColor = meshEntityColor(mesh);
  candidate.overlayPointSize = 2.0f;
  return candidate;
}

// Classification extent for a semi-infinite stroke: the analytic half-line
// must stay visible no matter how far the camera navigates along it, so the
// candidate bound reaches far past any scene scale instead of stopping at
// the tessellation proxy endpoint.
constexpr double kSemiInfiniteClassificationLength = 1.0e12;

VisibilityCandidate makeCadRangeCandidate(
    const VectorPrimitivesTessellation &tessellation,
    const CadEntityRange &range, VisibilityKind kind)
{
  VisibilityCandidate candidate;
  candidate.kind = kind;
  candidate.entityIndex = 0;
  candidate.cadRange = &range;
  candidate.rangeBegin = range.begin;
  candidate.rangeCount = range.count;
  candidate.overlayPointSize = 2.0f;

  bool hasPoint = false;
  glm::dvec3 minimum(0.0);
  glm::dvec3 maximum(0.0);
  auto include = [&](const glm::dvec3 &point) {
    if (!hasPoint)
    {
      minimum = maximum = point;
      hasPoint = true;
    }
    else
    {
      expandWorldAabb(minimum, maximum, point);
    }
  };

  if (kind == VisibilityKind::CadStroke)
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::Stroke &stroke = tessellation.geometry.strokes[i];
      for (const glm::dvec3 &point : stroke.points)
        include(point);
      if (stroke.semiInfinite && stroke.points.size() >= 2)
      {
        // The stored endpoint is only a tessellation proxy; classification
        // must follow the analytic half-line instead, or the entity is
        // culled as Offscreen the moment the camera pans past the proxy
        // end even though the infinite ray still crosses the viewport.
        // The render and slab paths clip the ray analytically, so this
        // bound only has to contain the geometry, which it does out to
        // any navigable distance.
        const glm::dvec3 direction =
            stroke.points[1] - stroke.points.front();
        const double length = glm::length(direction);
        if (length > 1.0e-18)
          include(stroke.points.front() + direction / length *
                      kSemiInfiniteClassificationLength);
      }
      candidate.overlayColor = contrastAgainstBackground(stroke.common.color);
      candidate.overlayPointSize = float(std::max(stroke.lineWeight, 2.0));
    }
  }
  else if (kind == VisibilityKind::CadFill)
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::Triangle &fill = tessellation.geometry.fills[i];
      include(fill.a);
      include(fill.b);
      include(fill.c);
      candidate.overlayColor = contrastAgainstBackground(fill.common.color);
    }
  }
  else
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::TessellatedPoint &point =
          tessellation.geometry.points[i];
      include(point.location);
      candidate.overlayColor = contrastAgainstBackground(point.common.color);
      candidate.overlayPointSize = float(point.pointSize);
    }
  }

  if (!hasPoint)
    return candidate;

  const double padding = 1.0e-3;
  minimum -= glm::dvec3(padding);
  maximum += glm::dvec3(padding);
  candidate.min = minimum;
  candidate.max = maximum;
  candidate.center = (minimum + maximum) * 0.5;
  const glm::dvec3 extent = maximum - minimum;
  candidate.lodSize = glm::max(extent.x, glm::max(extent.y, extent.z));
  return candidate;
}

const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates()
{
  static const std::vector<VisibilityCandidate> candidates = [] {
    const VectorPrimitivesTessellation &cad =
        getVectorPrimitivesTessellation();
    std::vector<VisibilityCandidate> result;
    result.reserve(cad.strokeRanges.size() + cad.fillRanges.size() +
                   cad.pointRanges.size() + cad.curves.size());
    for (const CadEntityRange &range : cad.strokeRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadStroke));
    }
    for (const CadEntityRange &range : cad.fillRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadFill));
    }
    for (const CadEntityRange &range : cad.pointRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadPoint));
    }
    for (const scene::CurveBatchCommand &curve : cad.curves)
      result.push_back(makeCurveCandidate(curve));
    return result;
  }();
  return candidates;
}

void buildVisibilityCandidates(std::vector<VisibilityCandidate> &candidates)
{
  candidates.clear();
  size_t reserveCount = getLargeCoordinateObjects().size() +
                        getStressObjects().size() + 1;
  const VectorPrimitivesTessellation &cad = getVectorPrimitivesTessellation();
  const std::vector<VisibilityCandidate> &cadRangeCandidates =
      cadRangeVisibilityCandidates();
  reserveCount += cad.meshes.size() + cadRangeCandidates.size();
  candidates.reserve(reserveCount);

  for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
  {
    if (meshEntityVisible(object))
      candidates.push_back(makeMeshCandidate(
          object, VisibilityKind::MeshObject));
  }
  for (const LargeCoordinateObject &object : getStressObjects())
  {
    if (meshEntityVisible(object))
      candidates.push_back(makeMeshCandidate(
          object, VisibilityKind::MeshObject));
  }
  if (const MeshEntityRecord centerCube = getCenterCubeEntity();
      meshEntityVisible(centerCube))
  {
    VisibilityCandidate centerCandidate = makeMeshCandidate(
        centerCube, VisibilityKind::CenterCube);
    centerCandidate.mesh = nullptr;
    candidates.push_back(std::move(centerCandidate));
  }

  for (size_t i = 0; i < cad.meshes.size(); ++i)
  {
    if (meshEntityVisible(cad.meshes[i]))
      candidates.push_back(makeMeshCandidate(
          cad.meshes[i], VisibilityKind::CadMesh, i));
  }
  for (const VisibilityCandidate &candidate : cadRangeCandidates)
    candidates.push_back(candidate);
}

// Liang-Barsky style 2D clipping of a world-space line segment against the
// orthographic viewport rectangle [-halfWidth, halfWidth] x
// [-halfHeight, halfHeight] in camera space.  Only the portion of the
// segment that survives clipping contributes to minDepth / maxDepth, so
// very long reference segments (e.g. the world-origin -> (1e7, 0, 1e7)
// line) do not miss near/far when both endpoints fall outside the
// viewport but the middle of the segment crosses it, and their off-screen
// endpoints do not inflate the depth slab either.
void includeSegmentCameraDepth(const glm::dvec3 &startWorldPosition,
                               const glm::dvec3 &endWorldPosition,
                               const glm::dvec3 &cameraPosition,
                               const glm::dvec3 &cameraRight,
                               const glm::dvec3 &cameraUp,
                               const glm::dvec3 &cameraFront,
                               double halfWidth,
                               double halfHeight,
                               double &minDepth,
                               double &maxDepth)
{
  const CameraSpacePoint p0 = toCameraSpace(
      startWorldPosition, cameraPosition, cameraRight, cameraUp, cameraFront);
  const CameraSpacePoint p1 = toCameraSpace(
      endWorldPosition, cameraPosition, cameraRight, cameraUp, cameraFront);

  const double dx = p1.x - p0.x;
  const double dy = p1.y - p0.y;

  double tEnter = 0.0;
  double tExit  = 1.0;

  const double edges[4][2] = {
      {-dx, p0.x - (-halfWidth)},   // left
      { dx, halfWidth - p0.x},      // right
      {-dy, p0.y - (-halfHeight)},  // bottom
      { dy, halfHeight - p0.y},     // top
  };

  for (int i = 0; i < 4; ++i)
  {
    const double p = edges[i][0];
    const double q = edges[i][1];
    if (std::abs(p) < 1e-18)
    {
      // Segment parallel to this edge.  If it lies entirely outside the
      // slab (q < 0), reject.
      if (q < 0.0)
        return;
      continue;
    }
    const double t = q / p;
    if (p < 0.0)
    {
      if (t > tExit)  return;
      if (t > tEnter) tEnter = t;
    }
    else
    {
      if (t < tEnter) return;
      if (t < tExit)  tExit = t;
    }
  }

  if (tEnter > tExit)
    return;

  const double depthEnter = p0.depth + (p1.depth - p0.depth) * tEnter;
  const double depthExit  = p0.depth + (p1.depth - p0.depth) * tExit;
  minDepth = std::min(minDepth, std::min(depthEnter, depthExit));
  maxDepth = std::max(maxDepth, std::max(depthEnter, depthExit));
}

// Clip a camera-space segment to the four perspective side planes and a
// minimum forward depth.  Unlike projecting both endpoints, this reports the
// depth interval of the portion that can actually enter the view volume.
bool includeSegmentPerspectiveDepth(
    const CameraSpacePoint &start, const CameraSpacePoint &end,
    double minimumDepth, double tanHalfVertical, double tanHalfHorizontal,
    double &minDepth, double &maxDepth)
{
  const double dx = end.x - start.x;
  const double dy = end.y - start.y;
  const double dz = end.depth - start.depth;

  // Each half-space has the form value(t) = a + b * t >= 0.
  const double constraints[5][2] = {
      {start.depth - minimumDepth, dz},
      {start.x + start.depth * tanHalfHorizontal,
       dx + dz * tanHalfHorizontal},
      {-start.x + start.depth * tanHalfHorizontal,
       -dx + dz * tanHalfHorizontal},
      {start.y + start.depth * tanHalfVertical,
       dy + dz * tanHalfVertical},
      {-start.y + start.depth * tanHalfVertical,
       -dy + dz * tanHalfVertical},
  };

  double tEnter = 0.0;
  double tExit = 1.0;
  for (const auto &constraint : constraints)
  {
    const double a = constraint[0];
    const double b = constraint[1];
    if (a < 0.0)
    {
      if (b <= 0.0)
        return false;
      tEnter = std::max(tEnter, -a / b);
    }
    else if (b < 0.0)
    {
      tExit = std::min(tExit, a / -b);
    }
  }

  if (tEnter > tExit)
    return false;

  const double depthEnter = start.depth + dz * tEnter;
  const double depthExit = start.depth + dz * tExit;
  minDepth = std::min({minDepth, depthEnter, depthExit});
  maxDepth = std::max({maxDepth, depthEnter, depthExit});
  return true;
}

// Clip a world segment to camera-space half spaces of the form
// a + b * t >= 0.  Keeping the interpolation parameter in double lets the
// caller draw only the visible portion of a very long line; each endpoint is
// then close enough to the frame's rebase origin for the float32 GPU path.
template <size_t N>
bool clipWorldSegmentToHalfSpaces(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const CameraSpacePoint &startCamera,
    const CameraSpacePoint &endCamera,
    const double (&constraints)[N][2],
    glm::dvec3 &clippedStart, glm::dvec3 &clippedEnd)
{
  const glm::dvec3 deltaWorld = endWorld - startWorld;
  const double dx = endCamera.x - startCamera.x;
  const double dy = endCamera.y - startCamera.y;
  const double dz = endCamera.depth - startCamera.depth;

  double tEnter = 0.0;
  double tExit = 1.0;
  for (const auto &constraint : constraints)
  {
    const double a = constraint[0];
    const double b = constraint[1];
    if (a < 0.0)
    {
      if (b <= 0.0)
        return false;
      tEnter = std::max(tEnter, -a / b);
    }
    else if (b < 0.0)
    {
      tExit = std::min(tExit, a / -b);
    }
  }

  if (tEnter > tExit)
    return false;

  clippedStart = startWorld + deltaWorld * tEnter;
  clippedEnd = startWorld + deltaWorld * tExit;
  return true;
}

bool clipReferenceSegmentToOrtho(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearPlane, double farPlane, double halfWidth,
    double halfHeight, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd)
{
  const CameraSpacePoint start = toCameraSpace(
      startWorld, cameraPosition, cameraRight, cameraUp, cameraFront);
  const CameraSpacePoint end = toCameraSpace(
      endWorld, cameraPosition, cameraRight, cameraUp, cameraFront);
  const double dx = end.x - start.x;
  const double dy = end.y - start.y;
  const double dz = end.depth - start.depth;

  const double constraints[6][2] = {
      {start.depth - nearPlane, dz},
      {farPlane - start.depth, -dz},
      {start.x + halfWidth, dx},
      {halfWidth - start.x, -dx},
      {start.y + halfHeight, dy},
      {halfHeight - start.y, -dy},
  };
  return clipWorldSegmentToHalfSpaces(startWorld, endWorld, start, end,
                                      constraints, clippedStart,
                                      clippedEnd);
}

bool clipReferenceSegmentToPerspective(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearPlane, double farPlane, double tanHalfVertical,
    double tanHalfHorizontal, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd)
{
  const CameraSpacePoint start = toCameraSpace(
      startWorld, cameraPosition, cameraRight, cameraUp, cameraFront);
  const CameraSpacePoint end = toCameraSpace(
      endWorld, cameraPosition, cameraRight, cameraUp, cameraFront);
  const double dx = end.x - start.x;
  const double dy = end.y - start.y;
  const double dz = end.depth - start.depth;

  const double constraints[6][2] = {
      {start.depth - nearPlane, dz},
      {farPlane - start.depth, -dz},
      {start.x + start.depth * tanHalfHorizontal,
       dx + dz * tanHalfHorizontal},
      {-start.x + start.depth * tanHalfHorizontal,
       -dx + dz * tanHalfHorizontal},
      {start.y + start.depth * tanHalfVertical,
       dy + dz * tanHalfVertical},
      {-start.y + start.depth * tanHalfVertical,
       -dy + dz * tanHalfVertical},
  };
  return clipWorldSegmentToHalfSpaces(startWorld, endWorld, start, end,
                                      constraints, clippedStart,
                                      clippedEnd);
}

// Clip a semi-infinite world ray (points[0] toward points[1]) directly to the
// camera frustum.  This avoids both an arbitrary tessellation endpoint and a
// huge proxy segment: the visible span is defined by the same near/far and
// screen planes that the GPU uses.  All interval math remains in double.
bool clipSemiInfiniteRayToView(
    const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
    const glm::dvec3 &cameraPosition, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
    double nearPlane, double farPlane, glm::dvec3 &clippedStart,
    glm::dvec3 &clippedEnd, bool flattenToSlabCenter)
{
  const CameraSpacePoint start = toCameraSpace(
      startWorld, cameraPosition, cameraRight, cameraUp, cameraFront);
  const CameraSpacePoint end = toCameraSpace(
      endWorld, cameraPosition, cameraRight, cameraUp, cameraFront);

  const glm::dvec3 directionWorld = endWorld - startWorld;
  const double directionLength = glm::length(directionWorld);
  if (directionLength <= 1.0e-18)
    return false;

  const glm::dvec3 direction = directionWorld / directionLength;
  const double dx = (end.x - start.x) / directionLength;
  const double dy = (end.y - start.y) / directionLength;
  const double dz = (end.depth - start.depth) / directionLength;

  double tEnter = 0.0;
  double tExit = std::numeric_limits<double>::infinity();
  bool rejected = false;
  auto accumulate = [&](double value, double slope) {
    // value + slope * t >= 0
    if (slope > 1.0e-15)
      tEnter = std::max(tEnter, -value / slope);
    else if (slope < -1.0e-15)
      tExit = std::min(tExit, -value / slope);
    else if (value < 0.0)
      rejected = true;
  };

  if (useOrthoProjection())
  {
    // In orthographic view an infinite CAD ray is a screen overlay: lateral
    // position is independent of camera depth.  Clip it to the viewport sides
    // only, then place the visible span at the stable ortho slab center.  If
    // it were clipped by the object slab too, a grazing ray could shorten
    // while OrthoSize grows because the slab changes independently of the
    // screen rectangle.
    const double halfHeight = orbitCam.orthoSize();
    const double halfWidth = halfHeight *
        (double)currentDrawableWidth() /
        std::max(1, currentDrawableHeight());
    accumulate(start.x + halfWidth, dx);
    accumulate(halfWidth - start.x, -dx);
    accumulate(start.y + halfHeight, dy);
    accumulate(halfHeight - start.y, -dy);
  }
  else
  {
    const double tanHalfVertical =
        std::tan(glm::radians(45.0) * 0.5);
    const double tanHalfHorizontal = tanHalfVertical *
        (double)currentDrawableWidth() /
        std::max(1, currentDrawableHeight());
    accumulate(start.depth - nearPlane, dz);
    accumulate(farPlane - start.depth, -dz);
    accumulate(start.x + start.depth * tanHalfHorizontal,
               dx + dz * tanHalfHorizontal);
    accumulate(-start.x + start.depth * tanHalfHorizontal,
               -dx + dz * tanHalfHorizontal);
    accumulate(start.y + start.depth * tanHalfVertical,
               dy + dz * tanHalfVertical);
    accumulate(-start.y + start.depth * tanHalfVertical,
               -dy + dz * tanHalfVertical);
  }

  if (rejected || !std::isfinite(tExit) || tEnter > tExit)
    return false;

  clippedStart = startWorld + direction * tEnter;
  clippedEnd = startWorld + direction * tExit;

  if (flattenToSlabCenter && useOrthoProjection())
  {
    // Moving a point along Front does not move its ortho projection, but it
    // puts the emitted ribbon safely inside the hardware depth range.
    const double slabCenter = std::max(0.001,
        glm::length(orbitCam.Position - orbitCam.Target));
    const double depthMargin =
        std::max(1.0e-3, (farPlane - nearPlane) * 1.0e-3);
    const double minimumDepth = nearPlane + depthMargin;
    const double maximumDepth = farPlane - depthMargin;
    if (minimumDepth > maximumDepth)
        return false;
    const double renderDepth =
        glm::clamp(slabCenter, minimumDepth, maximumDepth);

    auto flattenDepth = [&](const glm::dvec3 &worldPoint) {
      const double depth = glm::dot(worldPoint - cameraPosition, cameraFront);
      return worldPoint + cameraFront * (renderDepth - depth);
    };
    clippedStart = flattenDepth(clippedStart);
    clippedEnd = flattenDepth(clippedEnd);
  }

  return true;
}

// Clip a world-space polygon against one camera-depth plane.  Camera depth
// uses the same +Z-forward convention as the depth slab.  This keeps the
// debug wireframe aligned with what the grid shader actually accepts:
// fragments outside the [near, far] slab are discarded by depth test.
void clipPolygonAgainstDepth(const glm::dvec3 *polygon, int &count,
                             glm::dvec3 *clipped, int maxClipped,
                             const glm::dvec3 &cameraPos,
                             const glm::dvec3 &front,
                             bool keepAtMost, double depthLimit)
{
    if (count <= 0)
    {
        count = 0;
        return;
    }

    std::array<glm::dvec3, 16> output{};
    int outputCount = 0;

    auto cameraDepth = [&](const glm::dvec3 &p) {
        return glm::dot(p - cameraPos, front);
    };
    auto inside = [&](double depth) {
        return keepAtMost ? depth <= depthLimit
                          : depth >= depthLimit;
    };
    auto addPoint = [&](const glm::dvec3 &p) {
        if (outputCount >= (int)output.size())
            return;
        output[outputCount++] = p;
    };
    auto addIntersection = [&](const glm::dvec3 &a,
                               const glm::dvec3 &b) {
        const double da = cameraDepth(a) - depthLimit;
        const double db = cameraDepth(b) - depthLimit;
        const double denom = da - db;
        const double t = std::abs(denom) > 1e-18 ? da / denom : 0.0;
        addPoint(a + (b - a) * glm::clamp(t, 0.0, 1.0));
    };

    bool previousInside = inside(cameraDepth(polygon[count - 1]));
    for (int i = 0; i < count; ++i)
    {
        const glm::dvec3 &current = polygon[i];
        const bool currentInside = inside(cameraDepth(current));
        if (currentInside)
        {
            if (!previousInside)
                addIntersection(polygon[i - 1 < 0 ? count - 1 : i - 1], current);
            addPoint(current);
        }
        else if (previousInside)
        {
            addIntersection(polygon[i - 1 < 0 ? count - 1 : i - 1], current);
        }
        previousInside = currentInside;
    }

    count = std::min(outputCount, maxClipped);
    std::copy_n(output.begin(), count, clipped);
}


bool outlineIsLineLike(const GpuPickEntity &entity)
{
    return entity.kind == VisibilityKind::CadStroke ||
           entity.kind == VisibilityKind::CadCurve;
}

bool outlineUsesGeometry(const GpuPickEntity &entity)
{
    return outlineIsLineLike(entity) ||
           entity.kind == VisibilityKind::CadFill ||
           entity.kind == VisibilityKind::CadPoint;
}

static void appendOutlineRibbon(
    std::vector<rendering::PrimVertex> &vertices, const glm::vec3 &start,
    const glm::vec3 &end, const glm::vec3 &front, float halfWidth, float u0,
    float u1, bool centered, const glm::vec4 &color)
{
    const glm::vec3 direction = end - start;
    if (glm::length(direction) < 1.0e-5f)
        return;

    const glm::vec3 side = ribbonSide(direction, front, halfWidth);
    if (centered)
    {
        vertices.push_back({start - side, color, {u0, 0.0f}});
        vertices.push_back({start + side, color, {u0, 1.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({start - side, color, {u0, 0.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({end - side, color, {u1, 0.0f}});
    }
    else
    {
        vertices.push_back({start, color, {u0, 0.0f}});
        vertices.push_back({start + side, color, {u0, 1.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({start, color, {u0, 0.0f}});
        vertices.push_back({end + side, color, {u1, 1.0f}});
        vertices.push_back({end, color, {u1, 0.0f}});
    }
}


// Draw a solid fill outline from the same tessellated boundary used by the
// renderer.  Shared triangle edges are skipped, so triangulated hatches and
// solids get one clean silhouette instead of internal mesh edges.  The
// boundary is redrawn as camera-facing ribbons (like the line-like
// outlines), so the visible outline width stays constant at every viewing
// angle; an in-plane outward offset would foreshorten to zero when the fill
// is viewed edge-on.  The fill itself is submitted after this pass in the
// sequential overlay view and covers the inner half of each ribbon.
static void drawSolidFillOutline(const glm::mat4 &view,
                                 const glm::mat4 &overlayProjection,
                                 const glm::dvec3 &cameraPos,
                                 const glm::vec3 &cameraFront,
                                 float pixelSizeWorld,
                                 const glm::vec4 &logDepth)
{
    if (!rendererBackend || !outlineEntity ||
        outlineEntity->kind != VisibilityKind::CadFill ||
        !outlineEntity->cadRange)
    {
        return;
    }

    const CadEntityRange &range = *outlineEntity->cadRange;
    const entities::TessellatedEntity &tess =
        getVectorPrimitivesTessellation().geometry;
    if (range.begin >= tess.fills.size())
        return;
    const size_t last = std::min(range.begin + range.count,
                                 tess.fills.size());

    struct BoundaryKey
    {
        glm::dvec3 a;
        glm::dvec3 b;
        bool operator<(const BoundaryKey &other) const
        {
            if (a.x != other.a.x) return a.x < other.a.x;
            if (a.y != other.a.y) return a.y < other.a.y;
            if (a.z != other.a.z) return a.z < other.a.z;
            if (b.x != other.b.x) return b.x < other.b.x;
            if (b.y != other.b.y) return b.y < other.b.y;
            return b.z < other.b.z;
        }
    };
    struct BoundaryUse
    {
        glm::dvec3 start;
        glm::dvec3 end;
        glm::dvec3 normal;
        glm::dvec3 center;
        bool is3DFace;
    };

    std::map<BoundaryKey, std::vector<BoundaryUse>> boundaryUses;
    auto canonical = [](const glm::dvec3 &p, const glm::dvec3 &q) {
        return p.x < q.x || (p.x == q.x && (p.y < q.y ||
            (p.y == q.y && p.z <= q.z)))
            ? BoundaryKey{p, q} : BoundaryKey{q, p};
    };
    auto addTriangle = [&](const entities::Triangle &triangle) {
        if (!triangle.common.visible)
            return;
        const glm::dvec3 normal = glm::cross(triangle.b - triangle.a,
                                             triangle.c - triangle.a);
        const bool degenerate =
            glm::dot(normal, normal) <= 1.0e-24;
        const glm::dvec3 unit =
            degenerate ? glm::dvec3(0.0) : glm::normalize(normal);
        const glm::dvec3 center =
            (triangle.a + triangle.b + triangle.c) / 3.0;
        const std::pair<glm::dvec3, glm::dvec3> edges[3] = {
            {triangle.a, triangle.b},
            {triangle.b, triangle.c},
            {triangle.c, triangle.a},
        };
        for (const auto &edge : edges)
            boundaryUses[canonical(edge.first, edge.second)].push_back(
                {edge.first, edge.second, unit, center, triangle.is3DFace});
    };
    for (size_t i = range.begin; i < last; ++i)
        addTriangle(tess.fills[i]);

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    // The fill covers the ribbon's inner half, so only the outward half is
    // visible. Use two copies of the shared half-width to keep the visible
    // border at 2 * kOutlineWidthPixels, matching the previous in-plane
    // offset thickness.
    const float outlineWidth = 2.0f * outlineWidthWorld(pixelSizeWorld);
    // A closed 3D solid has no used-once boundary edges; its selection
    // outline is the view-dependent silhouette instead: edges whose two
    // adjacent triangles face opposite sides of the camera.  These ribbons
    // are drawn twice as wide so the solid reads as boldly selected as a
    // planar fill.
    const float silhouetteWidth = 2.0f * outlineWidth;
    const bool ortho = useOrthoProjection();
    auto facesCamera = [&](const BoundaryUse &use) {
        if (ortho)
            return glm::dot(use.normal, glm::dvec3(cameraFront)) < 0.0;
        return glm::dot(use.normal, use.center - cameraPos) > 0.0;
    };
    for (const auto &[key, uses] : boundaryUses)
    {
        (void)key;
        if (uses.size() == 1)
        {
            appendOutlineRibbon(
                outlineVertices,
                glm::vec3(uses.front().start - cameraPos),
                glm::vec3(uses.front().end - cameraPos),
                cameraFront, outlineWidth, 0.0f, 1.0f, true);
        }
        else if (uses.size() == 2 && uses.front().is3DFace &&
                 uses.back().is3DFace)
        {
            const BoundaryUse &first = uses.front();
            const BoundaryUse &second = uses.back();
            if (facesCamera(first) == facesCamera(second))
                continue; // both sides face the same way: interior edge
            appendOutlineRibbon(
                outlineVertices,
                glm::vec3(first.start - cameraPos),
                glm::vec3(first.end - cameraPos),
                cameraFront, silhouetteWidth, 0.0f, 1.0f, true);
        }
    }

    if (outlineVertices.empty())
        return;

    const rendering::PolylineRenderData outlineData{
        .view = view,
        .projection = overlayProjection,
        .vertices = outlineVertices.data(),
        .vertexCount = static_cast<uint32_t>(outlineVertices.size()),
        .logDepth = logDepth,
        .edgeSoftness = 0.15f,
        .layer = 1.0f,
    };
    rendererBackend->drawPolylines(outlineData);
}

// Draw a camera-facing annulus around each CAD point.  The inner radius
// matches the visible point impostor and the outer radius adds a fixed
// screen-space outline, so the source point is never covered by the outline.
static void drawCadPointOutline(const glm::mat4 &view,
                                const glm::mat4 &overlayProjection,
                                const glm::dvec3 &cameraPos,
                                const glm::vec3 &cameraFront,
                                float pixelSizeWorld,
                                const glm::vec4 &logDepth)
{
    if (!rendererBackend || !outlineEntity ||
        outlineEntity->kind != VisibilityKind::CadPoint ||
        !outlineEntity->cadRange)
    {
        return;
    }

    const CadEntityRange &range = *outlineEntity->cadRange;
    const entities::TessellatedEntity &tess =
        getVectorPrimitivesTessellation().geometry;
    if (range.begin >= tess.points.size())
        return;
    const size_t last = std::min(range.begin + range.count,
                                 tess.points.size());

    auto worldPerPixel = [&](const glm::dvec3 &worldPoint) {
        if (useOrthoProjection())
            return 2.0 * orbitCam.orthoSize() /
                   double(currentDrawableHeight());
        const double viewDepth = std::max(1.0e-9,
            glm::dot(worldPoint - cameraPos, glm::dvec3(cameraFront)));
        return 2.0 * viewDepth * std::tan(glm::radians(45.0) * 0.5) /
               double(currentDrawableHeight());
    };

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    const glm::vec3 right(orbitCam.Right);
    const glm::vec3 up(orbitCam.Up);
    constexpr int kCircleSegments = 32;
    for (size_t i = range.begin; i < last; ++i)
    {
        const entities::TessellatedPoint &point = tess.points[i];
        if (!point.common.visible)
            continue;

        const double pixelsPerWorldUnit = 1.0 /
            std::max(worldPerPixel(point.location), 1.0e-12);
        // The point impostor shader uses 2 * pointSize as the visible
        // screen-space radius, so the outline inner edge must match it.
        const double innerPixels = double(point.pointSize);
        const double outerPixels = innerPixels + kOutlineWidthPixels;
        const double innerRadius = innerPixels / pixelsPerWorldUnit;
        const double outerRadius = outerPixels / pixelsPerWorldUnit;
        const glm::vec3 center(point.location - cameraPos);
        for (int segment = 0; segment < kCircleSegments; ++segment)
        {
            const double angle0 = glm::two_pi<double>() * double(segment) /
                                  double(kCircleSegments);
            const double angle1 = glm::two_pi<double>() * double(segment + 1) /
                                  double(kCircleSegments);
            const glm::vec3 inner0 = center +
                right * float(innerRadius * std::cos(angle0)) +
                up * float(innerRadius * std::sin(angle0));
            const glm::vec3 outer0 = center +
                right * float(outerRadius * std::cos(angle0)) +
                up * float(outerRadius * std::sin(angle0));
            const glm::vec3 inner1 = center +
                right * float(innerRadius * std::cos(angle1)) +
                up * float(innerRadius * std::sin(angle1));
            const glm::vec3 outer1 = center +
                right * float(outerRadius * std::cos(angle1)) +
                up * float(outerRadius * std::sin(angle1));

            outlineVertices.push_back({inner0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer1, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({inner0, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({outer1, kOutlineColor, {0.5f, 0.5f}});
            outlineVertices.push_back({inner1, kOutlineColor, {0.5f, 0.5f}});
        }
    }

    if (outlineVertices.empty())
        return;

    const rendering::PolylineRenderData outlineData{
        .view = view,
        .projection = overlayProjection,
        .vertices = outlineVertices.data(),
        .vertexCount = static_cast<uint32_t>(outlineVertices.size()),
        .logDepth = logDepth,
        .edgeSoftness = 0.15f,
        .layer = 1.0f,
    };
    rendererBackend->drawPolylines(outlineData);
}

static void drawLineLikeOutline(const glm::mat4 &view,
                                const glm::mat4 &overlayProjection,
                                const glm::dvec3 &cameraPos,
                                const glm::dvec3 &cameraRight,
                                const glm::dvec3 &cameraUp,
                                const glm::dvec3 &cameraFront,
                                float pixelSizeWorld,
                                const glm::vec4 &logDepth)
{
    if (!rendererBackend || !outlineEntity ||
        !outlineIsLineLike(*outlineEntity))
    {
        return;
    }

    static std::vector<rendering::PrimVertex> outlineVertices;
    outlineVertices.clear();
    static std::vector<rendering::LineInstance> outlineLineInstances;
    outlineLineInstances.clear();

    if (outlineEntity->kind == VisibilityKind::CadStroke &&
        outlineEntity->cadRange)
    {
        const CadEntityRange &range = *outlineEntity->cadRange;
        const entities::TessellatedEntity &tess =
            getVectorPrimitivesTessellation().geometry;
        for (size_t i = range.begin;
             i < range.begin + range.count && i < tess.strokes.size(); ++i)
        {
            const entities::Stroke &stroke = tess.strokes[i];
            const size_t pointCount = stroke.points.size();
            if (!stroke.common.visible || pointCount < 2)
                continue;

            // Match the visible ribbon's pixel-size floor, then add one shared
            // outline half-width. The full width grows by two copies of
            // kOutlineWidthPixels.
            const float sourceHalfWidth =
                std::max(strokeHalfWidth(stroke), pixelSizeWorld);
            const float halfWidth = sourceHalfWidth +
                outlineWidthWorld(pixelSizeWorld);
            const size_t segmentCount = stroke.closed ? pointCount : pointCount - 1;
            for (size_t j = 0; j < segmentCount; ++j)
            {
                const size_t next = (j + 1) % pointCount;
                // The outline must use the same visible span as the source
                // stroke.  In particular a semi-infinite Ray must not draw an
                // outline around its finite tessellation proxy only.
                glm::dvec3 clippedStart, clippedEnd;
                if (!clipStrokeSegmentToView(
                        stroke.points[j], stroke.points[next], cameraPos,
                        cameraRight, cameraUp, cameraFront,
                        g_renderSlabNear, g_renderSlabFar,
                        clippedStart, clippedEnd, stroke.semiInfinite))
                {
                    continue;
                }
                if (lineDebugEnabled() && stroke.semiInfinite)
                {
                    std::printf(
                        "[OUTLINE_RAY] cam=(%.6f,%.6f,%.6f) start=(%.6f,%.6f,%.6f) end=(%.6f,%.6f,%.6f) half=%.6f\n",
                        cameraPos.x, cameraPos.y, cameraPos.z,
                        clippedStart.x, clippedStart.y, clippedStart.z,
                        clippedEnd.x, clippedEnd.y, clippedEnd.z,
                        halfWidth);
                }
                // Visible CAD strokes use the screen-space line-instance
                // pipeline.  A world-space CPU ribbon can look offset from the
                // body in perspective, especially for long semi-infinite rays.
                // Keep outline and body on the same centerline/expansion path.
                outlineLineInstances.push_back({
                    glm::vec4(glm::vec3(clippedStart - cameraPos), 0.0f),
                    glm::vec4(glm::vec3(clippedEnd - cameraPos), 1.0f),
                    glm::vec4(glm::vec3(kOutlineColor), halfWidth),
                    glm::vec4(kOutlineColor.a, 0.0f, 0.0f, 0.0f),
                });
            }
        }
    }
    else if (outlineEntity->kind == VisibilityKind::CadCurve &&
             outlineEntity->curve)
    {
        const std::vector<glm::dvec3> points =
            sampleCurveBatch(*outlineEntity->curve);
        const size_t segmentCount = points.size() > 1 ? points.size() - 1 : 0;
        const float halfWidth =
            std::max(outlineEntity->curve->acgiMaterial.lineWidth * 0.5f,
                     1.0f) +
            outlineWidthWorld(pixelSizeWorld);
        for (size_t i = 0; i < segmentCount; ++i)
        {
            appendOutlineRibbon(
                outlineVertices, glm::vec3(points[i] - cameraPos),
                glm::vec3(points[i + 1] - cameraPos),
                glm::vec3(cameraFront), halfWidth,
                float(i) / float(std::max<size_t>(segmentCount, 1)),
                float(i + 1) / float(std::max<size_t>(segmentCount, 1)),
                true);
        }
    }

    if (!outlineLineInstances.empty())
    {
        const rendering::LineInstancesRenderData outlineData{
            .view = view,
            .projection = overlayProjection,
            .instances = outlineLineInstances.data(),
            .instanceCount = static_cast<uint32_t>(outlineLineInstances.size()),
            .logDepth = logDepth,
            .edgeSoftness = 2.0f,
            .layer = 0.0f,
        };
        rendererBackend->drawLineInstances(outlineData);
        return;
    }

    if (outlineVertices.empty())
        return;

    const rendering::PolylineRenderData outlineData{
        .view = view,
        .projection = overlayProjection,
        .vertices = outlineVertices.data(),
        .vertexCount = static_cast<uint32_t>(outlineVertices.size()),
        .logDepth = logDepth,
        .edgeSoftness = 0.15f,
        .layer = 1.0f,
    };
    rendererBackend->drawPolylines(outlineData);
}

void render()
{
  if (!rendererBackend)
    return;

  if (!rendererBackend->beginFrame(kClearColor))
    return;

  if (gpuPickEnabled() && gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickQueueStats queueStats =
        rendererBackend->gpuPickQueueStats();
    if (queueStats.capacityExceeded())
    {
      gpuPickFocus.waitingResult = false;
      gpuPickFocus.pendingFrames = 0;
      std::cout << "GPU pick capacity exceeded meshes="
                << queueStats.droppedMeshes << "/"
                << queueStats.meshCapacity
                << " triangles=" << queueStats.droppedTriangles
                << "/" << queueStats.triangleCapacity
                << "; using CPU fallback" << std::endl;
      reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
      gpuPickFocus.camera.reset();
    }
  }

  if (gpuPickEnabled() && gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickResult gpuResult = rendererBackend->pollGpuPick();
    const bool resultMatches = gpuResult.ready &&
        gpuResult.requestToken == gpuPickFocus.requestToken;
    ++gpuPickFocus.pendingFrames;
    // Vulkan readbacks can take several frames to become CPU visible. Keep
    // a bounded retry window, then fall back to the CPU raycast.
    if (resultMatches || gpuPickFocus.pendingFrames >= 16)
    {
      if (pickDebugEnabled() && !resultMatches)
      {
        std::printf("[PICK_DEBUG] timeout pendingFrames=%u last=(ready=%d id=%u token=%u)",
                    gpuPickFocus.pendingFrames, gpuResult.ready ? 1 : 0,
                    gpuResult.objectId, gpuResult.requestToken);
        std::puts("");
      }
      gpuPickFocus.waitingResult = false;
      gpuPickFocus.pendingFrames = 0;
      if (!resultMatches)
      {
        // Stop the renderer from re-submitting the stalled pick every frame;
        // a stuck Vulkan readback otherwise loops until the next request.
        rendererBackend->cancelGpuPick();
      }
      std::optional<AutofocusResult> selectedEntity;
      if (resultMatches)
      {
        selectedEntity = autofocusGpuPick(gpuResult.objectId,
                                          gpuPickFocus.ndcX,
                                          gpuPickFocus.ndcY);
        if (pickDebugEnabled())
        {
          std::printf("[PICK_DEBUG] result ready id=%u token=%u",
                      gpuResult.objectId, gpuResult.requestToken);
          std::puts("");
        }
        if (pickDebugEnabled())
        {
          const auto &registry = gpuPickRegistry();
          std::printf("[PICK_DEBUG] registry size=%zu nextId=%u found=%d",
                      registry.size(), gpuPickNextEntityId,
                      registry.count(gpuResult.objectId) ? 1 : 0);
          std::puts("");
          if (const GpuPickEntity *pickEntity = findGpuPickEntity(gpuResult.objectId))
          {
            const char *kindName = "unknown";
            switch (pickEntity->kind)
            {
            case VisibilityKind::CadStroke: kindName = "CadStroke"; break;
            case VisibilityKind::CadFill: kindName = "CadFill"; break;
            case VisibilityKind::CadPoint: kindName = "CadPoint"; break;
            case VisibilityKind::CadCurve: kindName = "CadCurve"; break;
            case VisibilityKind::MeshObject: kindName = "MeshObject"; break;
            case VisibilityKind::CadMesh: kindName = "CadMesh"; break;
            default: break;
            }
            std::printf("[PICK_DEBUG] entity kind=%s range=%p count=%zu mesh=%p",
                        kindName,
                        static_cast<const void *>(pickEntity->cadRange),
                        pickEntity->cadRange ? pickEntity->cadRange->count : size_t(0),
                        static_cast<const void *>(pickEntity->mesh));
            std::puts("");
          }
        }
      }
      if (!selectedEntity)
      {
        // reportGpuPickFallback() may select a neighboring CPU object when the
        // one-pixel GPU ID readback misses a thin wireframe/edge. Preserve its
        // outline target instead of clearing it here.
        reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
      }
      else
      {
        reportAutofocus(*selectedEntity);
        lockedOutlineId = gpuResult.objectId;
        if (const GpuPickEntity *entity = findGpuPickEntity(gpuResult.objectId))
        {
          outlineEntity = *entity;
        }
      }
      // The focus cycle is fully resolved; release the frozen pose.
      gpuPickFocus.camera.reset();
    }
  }

  // �� Rebase layer ����������������������������������������������������������
  // Snap the world origin to the current camera chunk once per frame.
  // Every GPU-bound coordinate (view matrix translation, per-object
  // uModelRelativePosition, grid uOriginRelative) is computed relative
  // to this anchor, so its magnitude never exceeds chunkSize/2 even at
  // 1e9 world coordinates.
  worldRebase().update(orbitCam.Position);
  const glm::dvec3 rebase = worldRebase().origin();

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const float aspect = static_cast<float>(drawableWidth) /
                       static_cast<float>(drawableHeight);
  // RTE step: pass the rebase origin so the view matrix is built from
  // (camera - rebase, target - rebase).  Rotation is preserved; only the
  // translation column shifts, and the same shift is applied to every
  // object uniform downstream.
  glm::mat4 view = orbitCam.getViewMatrix(rebase);
  // Object pipelines use the strict RTE view: rotation only, zero translation.
  // Grid/overlay keeps the old rebase view because its ray shaders reconstruct
  // world-space rays from the inverse view-projection.
  const glm::mat4 viewRte = orbitCam.getViewRotationMatrix();
  glm::mat4 projection;
  glm::mat4 overlayProjection;
  glm::mat4 gridProjection;
  float pixelSize = 0.0f;
  double activeNear = 0.0;
  double activeFar  = 0.0;
  float overlayNear = 0.0f;
  float overlayFar = 0.0f;
  glm::vec4 logDepth(0.0f);
  static std::vector<const LargeCoordinateObject *> drawOrder;
  drawOrder.clear();
  drawOrder.reserve(getLargeCoordinateObjects().size() +
                    getStressObjects().size());
  static std::vector<const LargeCoordinateObject *> tinyDraws;
  tinyDraws.clear();
  tinyDraws.reserve(getLargeCoordinateObjects().size() +
                    getStressObjects().size());
  static std::vector<VisibilityCandidate> visibilityCandidates;
  buildVisibilityCandidates(visibilityCandidates);
  static std::vector<const VisibilityCandidate *> visibleCadDraws;
  visibleCadDraws.clear();
  visibleCadDraws.reserve(visibilityCandidates.size());
  static std::vector<const VisibilityCandidate *> tinyCadDraws;
  tinyCadDraws.clear();
  tinyCadDraws.reserve(visibilityCandidates.size());
  bool centerCubeInFrame = false;
  const glm::dvec3 worldLineEnd = LARGE_COORDINATE_BASE_POINT;
  glm::dvec3 referenceLineStart = glm::dvec3(0.0);
  glm::dvec3 referenceLineEnd = worldLineEnd;
  bool referenceLineVisible = false;

  const glm::dvec3 cameraPos(orbitCam.Position);
  const glm::dvec3 &frontVec = orbitCam.Front;
  glm::dvec3 planeNormal = glm::normalize(gridPlaneNormal);
  glm::dvec3 tangentU;
  glm::dvec3 tangentV;
  activePlaneTangents(tangentU, tangentV);

  const double frontOnNormal = glm::dot(frontVec, planeNormal);
  glm::dvec3 planeCenter;
  if (std::abs(frontOnNormal) > 1e-6)
  {
      const double planeDistance = glm::dot(gridPlaneOrigin - cameraPos, planeNormal);
      planeCenter = cameraPos + frontVec * (planeDistance / frontOnNormal);
  }
  else
  {
      planeCenter = cameraPos -
                    planeNormal * glm::dot(cameraPos - gridPlaneOrigin, planeNormal);
  }

  if (useOrthoProjection())
  {
    const double halfH = orbitCam.orthoSize();
    const double halfW = (double)halfH * (double)aspect;
    const glm::dvec3 &front = orbitCam.Front;
    const glm::dvec3 &right = orbitCam.Right;
    const glm::dvec3 &up = orbitCam.Up;

    // Start with the depth interval covered by the visible ortho image.
    // The scene bounds below can make near negative; that is intentional
    // because orthographic geometry can straddle the camera plane.
    // Clamping near to zero would hide the lower half of the center cube.
    const CameraSpacePoint targetCamera =
        toCameraSpace(orbitCam.Target, cameraPos, right, up, front);
    const double targetDepth = targetCamera.depth;

    const double imageRadius = std::sqrt(halfW * halfW + halfH * halfH);
    double slabMinDepth = targetDepth - imageRadius;
    double slabMaxDepth = targetDepth + imageRadius;

    // Content-weighted slab center: each frustum-intersecting object votes
    // with its camera-space depth midpoint, weighted by the area of its
    // projected ortho footprint.  Centering the slab on what is visible
    // instead of the camera distance keeps the covered interval identical
    // while shrinking the radius that log-depth precision has to span.  An
    // edge-on object contributes zero area and only the reach term.
    double weightedDepthSum = 0.0;
    double weightSum = 0.0;
    size_t contentAabbCount = 0;
    auto includeContentDepth = [&](const CameraSpaceAabb &bounds) {
      const double weight =
          std::max(0.0, bounds.maxX - bounds.minX) *
          std::max(0.0, bounds.maxY - bounds.minY);
      weightedDepthSum +=
          (bounds.minDepth + bounds.maxDepth) * 0.5 * weight;
      weightSum += weight;
      ++contentAabbCount;
    };

    auto includeObjectDepth = [&](const LargeCoordinateObject &object) {
      if (!meshEntityVisible(object))
        return;
      const double halfSize = (double)object.size * 0.5;
      const CameraSpaceAabb bounds = cameraAabbBounds(
          object.worldPosition, glm::dvec3(halfSize), cameraPos, right,
          up, front);
      if (aabbIntersectsOrthoViewport(bounds, halfW, halfH))
      {
        includeContentDepth(bounds);
        slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
        slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        drawOrder.push_back(&object);
      }
    };

    const UnifiedVisibilityQuery orthoVisibility =
        UnifiedVisibilityQuery::makeOrtho(
            cameraPos, right, up, front, halfW, halfH,
            (double)drawableHeight);
    for (const VisibilityCandidate &candidate : visibilityCandidates)
    {
      const VisibilityState state = orthoVisibility.classify(candidate);
      if (state == VisibilityState::Offscreen)
        continue;

      if (candidate.kind == VisibilityKind::MeshObject)
      {
        includeObjectDepth(*candidate.mesh);
      }
      else if (candidate.kind == VisibilityKind::CenterCube)
      {
        const CameraSpaceAabb bounds = orthoVisibility.cameraAabb(candidate);
        includeContentDepth(bounds);
        slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
        slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        centerCubeInFrame = true;
      }
      else if (candidate.kind == VisibilityKind::CadMesh ||
               candidate.kind == VisibilityKind::CadFill)
      {
        const CameraSpaceAabb bounds = orthoVisibility.cameraAabb(candidate);
        includeContentDepth(bounds);
        slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
        slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        visibleCadDraws.push_back(&candidate);
      }
      else
      {
        // Strokes, points, and curves are visible content too.  They draw
        // with the main ortho projection, so leaving them out of the slab
        // lets the weighted content center clip them at near/far whenever
        // zooming tightens the interval around fills and meshes.
        bool haveBounds = false;
        CameraSpaceAabb bounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        auto unionBounds = [&](const CameraSpaceAabb &part) {
          if (haveBounds)
          {
            bounds.minX = std::min(bounds.minX, part.minX);
            bounds.maxX = std::max(bounds.maxX, part.maxX);
            bounds.minY = std::min(bounds.minY, part.minY);
            bounds.maxY = std::max(bounds.maxY, part.maxY);
            bounds.minDepth = std::min(bounds.minDepth, part.minDepth);
            bounds.maxDepth = std::max(bounds.maxDepth, part.maxDepth);
          }
          else
          {
            bounds = part;
            haveBounds = true;
          }
        };

        bool semiInfiniteRange = false;
        if (candidate.kind == VisibilityKind::CadStroke &&
            candidate.cadRange && candidate.rangeCount)
        {
          const VectorPrimitivesTessellation &tess =
              getVectorPrimitivesTessellation();
          for (size_t i = candidate.rangeBegin;
               i < candidate.rangeBegin + candidate.rangeCount; ++i)
          {
            if (tess.geometry.strokes[i].semiInfinite)
            {
              semiInfiniteRange = true;
              break;
            }
          }
        }

        if (!semiInfiniteRange)
        {
          unionBounds(orthoVisibility.cameraAabb(candidate));
        }
        else
        {
          // A semi-infinite ray's stored endpoint is only a tessellation
          // proxy (rayLength ahead along the direction), so its candidate
          // AABB would push a huge fake depth span into the slab.  Clip
          // the analytic half-line to the ortho viewport sides instead.
          // The ortho side planes are parallel to the gaze, so a ray that
          // runs parallel inside the viewport never exits them and
          // contributes nothing here; it renders flattened onto the slab
          // center, safely inside any slab.
          const VectorPrimitivesTessellation &tess =
              getVectorPrimitivesTessellation();
          for (size_t i = candidate.rangeBegin;
               i < candidate.rangeBegin + candidate.rangeCount; ++i)
          {
            const entities::Stroke &stroke = tess.geometry.strokes[i];
            if (stroke.points.size() < 2)
              continue;
            if (stroke.semiInfinite)
            {
              glm::dvec3 clippedStart, clippedEnd;
              if (!clipSemiInfiniteRayToView(
                      stroke.points.front(), stroke.points.back(), cameraPos,
                      right, up, front, 0.0, 0.0, clippedStart, clippedEnd,
                      false))
                continue;
              const CameraSpacePoint a = toCameraSpace(
                  clippedStart, cameraPos, right, up, front);
              const CameraSpacePoint b = toCameraSpace(
                  clippedEnd, cameraPos, right, up, front);
              unionBounds({std::min(a.x, b.x), std::max(a.x, b.x),
                           std::min(a.y, b.y), std::max(a.y, b.y),
                           std::min(a.depth, b.depth),
                           std::max(a.depth, b.depth)});
            }
            else
            {
              for (const glm::dvec3 &point : stroke.points)
              {
                const CameraSpacePoint c = toCameraSpace(
                    point, cameraPos, right, up, front);
                unionBounds({c.x, c.x, c.y, c.y, c.depth, c.depth});
              }
            }
          }
        }

        if (haveBounds)
        {
          includeContentDepth(bounds);
          slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
          slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        }
        visibleCadDraws.push_back(&candidate);
      }
    }

    // The slab center is the projected-area weighted mean depth of the
    // contributing objects; the infinite grid stays excluded on purpose:
    // at grazing angles its horizon depths are unbounded and would destroy
    // depth precision.  The radius is measured from that center to both
    // ends of the accumulated interval, so it still covers every
    // contributing object plus the target ± imageRadius seed.
    const double cameraDistance =
        glm::length(orbitCam.Position - orbitCam.Target);
    double slabCenterDepth;
    double slabRadius;
    // Minimum slab span: keeps extreme zoom from starving the depth
    // range below the visible geometry's own extent.
    constexpr double kMinDepthSpan = 1024.0;
    if (contentAabbCount)
    {
      slabCenterDepth = weightSum > 0.0
          ? weightedDepthSum / weightSum
          : std::max(0.001, cameraDistance);
      const double contentReach = std::max(
          slabMaxDepth - slabCenterDepth, slabCenterDepth - slabMinDepth);
      const double frameRadius = std::max(
          {contentReach, imageRadius, kMinDepthSpan * 0.5,
           orbitCam.orthoSize() * 3.0});
      // The stored model bounds are a conservative fit-all fallback, not a
      // per-frame visibility request.  A previous scene can leave them
      // millions of units away from the active target; cap that historical
      // contribution by the currently visible slab so the stabilizer can
      // actually converge.  Rotation changes still get headroom, and
      // genuinely large visible content raises frameRadius before this
      // ceiling is applied.
      constexpr double kModelDepthRadiusHeadroom = 4.0;
      const double modelDepthRadiusCeiling =
          std::max(kMinDepthSpan, frameRadius * kModelDepthRadiusHeadroom);
      const double modelDepthRadius =
          std::min(orbitCam.orthoDepthRadius(), modelDepthRadiusCeiling);
      slabRadius = std::max(frameRadius, modelDepthRadius);
    }
    else
    {
      // Empty view: the only remaining content is the infinite grid, so
      // the slab shrinks to the smallest interval that still covers the
      // visible image around the camera.  The 512 and orthoSize*3 floors
      // and the fit-all model fallback would thicken an already
      // content-free slab for no visible benefit.
      slabCenterDepth = std::max(0.001, cameraDistance);
      // Minimum span floor applies here as well: an unbounded shrink at
      // extreme zoom left text outside the near/far planes.
      slabRadius = std::max(imageRadius, kMinDepthSpan * 0.5);
    }
    // Text glyph quads draw with the main ortho projection, so their
    // depths must join the final slab in BOTH branches: zooming onto a
    // text entity tightens the slab around meshes and strokes alone, and
    // the near/far planes would clip the glyphs.  Expand the centered
    // interval with each request's depth ± its own extent.
    for (const acgi::TextRequest &request : acgi::textRequests())
    {
      const CameraSpacePoint center =
          toCameraSpace(request.position, cameraPos, right, up, front);
      const double extent =
          double(request.message.size()) * request.height +
          request.height * 2.0;
      const double low =
          std::min(slabCenterDepth - slabRadius, center.depth - extent);
      const double high =
          std::max(slabCenterDepth + slabRadius, center.depth + extent);
      slabCenterDepth = 0.5 * (low + high);
      slabRadius = 0.5 * (high - low);
    }

    // A slab centered on content straddling the camera plane could end up
    // entirely behind depth zero; keep the far plane strictly positive.
    constexpr double kMinFarDepth = 0.001;
    if (slabCenterDepth + slabRadius < kMinFarDepth)
      slabCenterDepth = kMinFarDepth - slabRadius;

    // Hysteresis: expand immediately, shrink only after twenty stable frames.
    double stableOrthoNear = 0.0;
    double stableOrthoFar = 0.0;
    g_orthoSlabStabilizer.apply(slabCenterDepth - slabRadius,
                                slabCenterDepth + slabRadius,
                                stableOrthoNear, stableOrthoFar);
    const double near = floatExpandOutward(stableOrthoNear, true);
    const double far = floatExpandOutward(stableOrthoFar, false);

    const glm::dmat4 orthoDouble = glm::ortho(
        -halfH * (double)aspect, halfH * (double)aspect,
        -halfH,                  halfH, near, far);
    projection = glm::mat4(orthoDouble);
    activeNear = near;
    activeFar  = far;
    g_pickDepthNear = activeNear;
    g_pickDepthFar  = activeFar;

    // Analytic ortho pixel size (doc section 4.2): the vertical frustum
    // extent 2 * halfH maps onto the drawable viewport height.
    pixelSize = static_cast<float>((2.0 * halfH) / (double)drawableHeight);
  }
  else
  {
    const glm::dvec3 &right = orbitCam.Right;
    const glm::dvec3 &up = orbitCam.Up;
    const double tanHalfVertical =
        std::tan(glm::radians(45.0) * 0.5);
    const double tanHalfHorizontal = tanHalfVertical * aspect;

    constexpr double kNearDepthFloor = 0.05;
    constexpr double kMaxPerspectiveFar = 1.0e9;
    constexpr double kMinPerspectiveSpan = 1.0;
    constexpr double kMinObjectPixelExtent = 0.1;
    // A single conservative pass over the cached scene AABB gives an initial
    // depth interval without using the final near/far as input.  Exact AABB
    // culling below therefore cannot reject an object that would later have
    // expanded the final depth range.
    const WorldAabb &sceneBounds = immutableSceneBounds();
    double provisionalNear = kNearDepthFloor;
    double provisionalFar = kMaxPerspectiveFar;
    if (sceneBounds.valid)
    {
      const glm::dvec3 center =
          (sceneBounds.min + sceneBounds.max) * 0.5;
      const glm::dvec3 halfExtent =
          (sceneBounds.max - sceneBounds.min) * 0.5;
      const CameraSpaceAabb sceneCamera =
          cameraAabbBounds(center, halfExtent, cameraPos, right, up,
                           frontVec);
      provisionalNear = std::min(provisionalNear, sceneCamera.minDepth);
      provisionalFar = std::max(sceneCamera.maxDepth,
                                provisionalNear + kMinPerspectiveSpan);
    }

    const CameraSpacePoint targetCamera =
        toCameraSpace(orbitCam.Target, cameraPos, right, up, frontVec);
    double overlayMinDepth = targetCamera.depth;
    double overlayMaxDepth = targetCamera.depth;
    double objectMinDepth = targetCamera.depth;
    double objectMaxDepth = targetCamera.depth;

    const UnifiedVisibilityQuery perspectiveVisibility =
        UnifiedVisibilityQuery::makePerspective(
            cameraPos, right, up, frontVec, tanHalfVertical,
            tanHalfHorizontal, (double)drawableHeight,
            provisionalNear, provisionalFar);
    for (const VisibilityCandidate &candidate : visibilityCandidates)
    {
      const VisibilityState state = perspectiveVisibility.classify(candidate);
      if (state == VisibilityState::Offscreen)
        continue;

      if (candidate.kind == VisibilityKind::MeshObject)
      {
        const CameraSpaceAabb bounds =
            perspectiveVisibility.cameraAabb(candidate);
        if (state == VisibilityState::Tiny)
        {
          tinyDraws.push_back(candidate.mesh);
          continue;
        }

        overlayMinDepth = std::min(overlayMinDepth, bounds.minDepth);
        overlayMaxDepth = std::max(overlayMaxDepth, bounds.maxDepth);
        objectMinDepth = std::min(objectMinDepth, bounds.minDepth);
        objectMaxDepth = std::max(objectMaxDepth, bounds.maxDepth);
        drawOrder.push_back(candidate.mesh);
      }
      else if (candidate.kind == VisibilityKind::CenterCube)
      {
        const CameraSpaceAabb bounds = perspectiveVisibility.cameraAabb(candidate);
        if (state == VisibilityState::Visible)
        {
          overlayMinDepth = std::min(overlayMinDepth, bounds.minDepth);
          overlayMaxDepth = std::max(overlayMaxDepth, bounds.maxDepth);
          objectMinDepth = std::min(objectMinDepth, bounds.minDepth);
          objectMaxDepth = std::max(objectMaxDepth, bounds.maxDepth);
          centerCubeInFrame = true;
        }
        else
        {
          const double centerDepth = glm::dot(
              candidate.center - cameraPos, frontVec);
          overlayMinDepth = std::min(overlayMinDepth, centerDepth);
          overlayMaxDepth = std::max(overlayMaxDepth, centerDepth);
          tinyCadDraws.push_back(&candidate);
        }
      }
      else if (candidate.kind == VisibilityKind::CadMesh ||
               candidate.kind == VisibilityKind::CadFill)
      {
        const CameraSpaceAabb bounds = perspectiveVisibility.cameraAabb(candidate);
        if (state == VisibilityState::Visible)
        {
          objectMinDepth = std::min(objectMinDepth, bounds.minDepth);
          objectMaxDepth = std::max(objectMaxDepth, bounds.maxDepth);
          visibleCadDraws.push_back(&candidate);
        }
        else
        {
          const double centerDepth = glm::dot(
              candidate.center - cameraPos, frontVec);
          overlayMinDepth = std::min(overlayMinDepth, centerDepth);
          overlayMaxDepth = std::max(overlayMaxDepth, centerDepth);
          tinyCadDraws.push_back(&candidate);
        }
      }
      else
      {
        const CameraSpaceAabb bounds = perspectiveVisibility.cameraAabb(candidate);
        const double useDepth = state == VisibilityState::Visible
                                    ? bounds.minDepth
                                    : glm::dot(candidate.center - cameraPos,
                                               frontVec);
        const double maxUseDepth = state == VisibilityState::Visible
                                       ? bounds.maxDepth
                                       : glm::dot(candidate.center - cameraPos,
                                                  frontVec);
        overlayMinDepth = std::min(overlayMinDepth, useDepth);
        overlayMaxDepth = std::max(overlayMaxDepth, maxUseDepth);
        if (state == VisibilityState::Visible)
          visibleCadDraws.push_back(&candidate);
        else
          tinyCadDraws.push_back(&candidate);
      }
    }

    // The reference line is drawn without an AABB, so clip the segment to
    // the exact perspective side frusta.  It is a non-depth-writing overlay
    // and must not consume the depth precision reserved for solid geometry.
    includeSegmentPerspectiveDepth(
        toCameraSpace(glm::dvec3(0.0), cameraPos, right, up, frontVec),
        toCameraSpace(worldLineEnd, cameraPos, right, up, frontVec),
        kNearDepthFloor, tanHalfVertical, tanHalfHorizontal,
        overlayMinDepth, overlayMaxDepth);

    // The infinite grid does not take part in camera depth-slab selection.
    // It still writes depth for sorting, so give it a stable dedicated frustum
    // and let its shader put distant background lines at depth one.
    gridProjection = glm::mat4(glm::perspective(
        glm::radians(45.0), (double)aspect,
        kNearDepthFloor, kMaxPerspectiveFar));

    // Solid geometry receives a compact depth slab.  Infinite ground and the
    // 1e7 reference line are transparent overlays; if they share this slab,
    // a close cube maps to float32 NDC depth 1.0 and is clipped by the GPU.
    const double depthMagnitude = std::max(
        {kNearDepthFloor, std::abs(objectMinDepth),
         std::abs(objectMaxDepth)});
    // A few float32 ULPs are the real precision floor because projection is
    // float; the extra world-unit keeps matrix conversion from being
    // marginal.
    const double depthMargin =
        std::max(1.0, 8.0 * std::numeric_limits<float>::epsilon() *
                           depthMagnitude);

    const double candidateObjectNear = std::max(
        kNearDepthFloor, objectMinDepth - depthMargin);
    const double candidateObjectFar = std::min(
        kMaxPerspectiveFar,
        std::max(objectMaxDepth + depthMargin,
                 candidateObjectNear + kMinPerspectiveSpan));
    double stablePerspectiveNear = 0.0;
    double stablePerspectiveFar = 0.0;
    g_perspectiveSlabStabilizer.apply(candidateObjectNear,
                                      candidateObjectFar,
                                      stablePerspectiveNear,
                                      stablePerspectiveFar);
    const double near = floatExpandOutward(stablePerspectiveNear, true);
    const double far = floatExpandOutward(stablePerspectiveFar, false);

    const glm::dmat4 perspectiveDouble = glm::perspective(
        glm::radians(45.0), (double)aspect, near, far);
    projection = glm::mat4(perspectiveDouble);
    activeNear = near;
    activeFar  = far;
    g_pickDepthNear = activeNear;
    g_pickDepthFar  = activeFar;

    const double overlayDepthMagnitude = std::max(
        {kNearDepthFloor, std::abs(overlayMinDepth),
         std::abs(overlayMaxDepth)});
    const double overlayDepthMargin =
        std::max(1.0, 8.0 * std::numeric_limits<float>::epsilon() *
                           overlayDepthMagnitude);
    const double candidateOverlayFar = std::min(
        kMaxPerspectiveFar,
        std::max(overlayMaxDepth + overlayDepthMargin,
                 kNearDepthFloor + kMinPerspectiveSpan));
    double stableOverlayNear = 0.0;
    double stableOverlayFar = 0.0;
    g_overlaySlabStabilizer.apply(
        (double)kNearDepthFloor, candidateOverlayFar,
        stableOverlayNear, stableOverlayFar);
    overlayNear = floatExpandOutward(stableOverlayNear, true);
    overlayFar = floatExpandOutward(stableOverlayFar, false);
    const glm::dmat4 overlayDouble = glm::perspective(
        glm::radians(45.0), (double)aspect, (double)overlayNear, (double)overlayFar);
    overlayProjection = glm::mat4(overlayDouble);

    // Rough estimate around the orbit target, only used to seed the LOD
    // step; the shader computes exact per-fragment sizes for perspective.
    pixelSize = (2.0f * (float)glm::length(orbitCam.Position - orbitCam.Target)
                  * tan(glm::radians(22.5f)))
               / static_cast<float>(drawableHeight);
  }

  if (useOrthoProjection())
  {
    overlayProjection = projection;
    gridProjection = projection;
    overlayNear = static_cast<float>(activeNear);
    overlayFar = static_cast<float>(activeFar);
    logDepth = glm::vec4(0.0f, overlayNear, overlayFar, 0.0f);
  }
  else
  {
    // One shared mapping is required because every depth-tested fragment
    // compares against the same depth buffer.  The object/overlay projection
    // planes remain independent and are used only for hardware clipping.
    const float depthNear =
        std::min(static_cast<float>(activeNear), overlayNear);
    const float depthFar =
        std::max(static_cast<float>(activeFar), overlayFar);
    logDepth = glm::vec4(1.0f, depthNear, depthFar, 0.0f);
  }

  // Publish this frame's slabs for stroke frustum clipping.
  g_renderSlabNear = overlayNear;
  g_renderSlabFar = overlayFar;

  logSlabIfChanged(useOrthoProjection(), activeNear, activeFar,
                   overlayNear, overlayFar);

  gpuPickSceneDebugQueueActive = false;

  // The full-scene ID pass re-renders every visible entity a second time,
  // so only refresh it when something that can change the ID buffer
  // changes: view/projection, depth slab, viewport size, the outlined
  // entity, or the debug view modes. While the camera is idle the outline
  // keeps sampling the last rendered ID texture and skips the extra pass.
  static uint64_t lastSceneIdSignature = 0;
  static bool lastSceneIdSignatureValid = false;
  static uint32_t cachedOutlineObjectId = 0;
  uint64_t sceneIdSignature = 0xcbf29ce484222325ull;
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, glm::value_ptr(viewRte), 16 * sizeof(float));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, glm::value_ptr(projection), 16 * sizeof(float));
  // Strict-RTE viewRte intentionally has no translation.  Pan/rebase can
  // therefore leave viewRte and projection unchanged while every RTE vertex
  // (object - camera/rebase) changes.  Hash the camera world pose/rebase too,
  // or the selection outline keeps sampling the previous frame ID texture.
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, glm::value_ptr(rebase), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, glm::value_ptr(orbitCam.Target), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, glm::value_ptr(orbitCam.Position), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &activeNear, sizeof(activeNear));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &activeFar, sizeof(activeFar));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &logDepth, sizeof(logDepth));
  {
    const bool ortho = useOrthoProjection();
    const int viewport[2] = {drawableWidth, drawableHeight};
    sceneIdSignature = hashGpuPickSceneBytes(
        sceneIdSignature, &ortho, sizeof(ortho));
    sceneIdSignature = hashGpuPickSceneBytes(
        sceneIdSignature, viewport, sizeof(viewport));
  }
  {
    const void *outlineKey = nullptr;
    if (outlineEntity)
    {
      outlineKey = outlineEntity->mesh
                       ? static_cast<const void *>(outlineEntity->mesh)
                       : outlineEntity->cadRange
                             ? static_cast<const void *>(outlineEntity->cadRange)
                             : static_cast<const void *>(outlineEntity->curve);
      sceneIdSignature = hashGpuPickSceneBytes(
          sceneIdSignature, &outlineEntity->kind,
          sizeof(outlineEntity->kind));
    }
    sceneIdSignature = hashGpuPickSceneBytes(
        sceneIdSignature, &outlineKey, sizeof(outlineKey));
    sceneIdSignature = hashGpuPickSceneBytes(
        sceneIdSignature, &outlineAllTest, sizeof(outlineAllTest));
    sceneIdSignature = hashGpuPickSceneBytes(
        sceneIdSignature, &gpuPickSceneDebug, sizeof(gpuPickSceneDebug));
  }
  const bool sceneIdSignatureChanged =
      !lastSceneIdSignatureValid || sceneIdSignature != lastSceneIdSignature;
  lastSceneIdSignature = sceneIdSignature;
  lastSceneIdSignatureValid = true;
  bool gpuSceneIdPassRequested = gpuPickSceneDebug || outlineAllTest;
  if (outlineEntity.has_value() && sceneIdSignatureChanged)
    gpuSceneIdPassRequested = true;
  if (rendererBackend)
    rendererBackend->setGpuPickScenePassEnabled(gpuSceneIdPassRequested);
  if (gpuPickEnabled() && gpuPickFocus.pendingNdc)
  {
    const rendering::GpuPickRequest request{
        .view = viewRte,
        .projection = overlayProjection,
        .eye = rendering::encodeDoubleSingle(orbitCam.Position),
        .ndcX = gpuPickFocus.ndcX,
        .ndcY = gpuPickFocus.ndcY,
        .nearDepth = activeNear,
        .farDepth = activeFar,
        .logDepth = logDepth,
    };
    // Freeze the pose the 1x1 ID pass will render with; the result handler
    // rebuilds its refinement ray from this exact basis.
    gpuPickFocus.camera = GpuPickCameraBasis{
        orbitCam.Position, orbitCam.Front, orbitCam.Right, orbitCam.Up,
        useOrthoProjection(), orbitCam.orthoSize()};
    const uint32_t requestToken = rendererBackend->requestGpuPick(request);
    if (requestToken != 0)
    {
      if (pickDebugEnabled())
      {
        std::printf("[PICK_DEBUG] request token=%u ndc=(%f,%f)",
                    requestToken, gpuPickFocus.ndcX, gpuPickFocus.ndcY);
        std::puts("");
      }
      gpuPickRegistry().clear();
      gpuPickNextEntityId = 2;
      gpuPickFocus.requestToken = requestToken;
      gpuPickFocus.pendingFrames = 0;
      gpuPickFocus.pendingNdc = false;
      gpuPickFocus.waitingResult = true;
    }
    else if (++gpuPickFocus.pendingFrames >= 3)
    {
      gpuPickFocus.pendingNdc = false;
      gpuPickFocus.pendingFrames = 0;
      reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
    }
  }

  if (gpuSceneIdPassRequested && rendererBackend && gpuPickEnabled() &&
      !gpuPickFocus.pendingNdc && !gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickRequest sceneRequest{
        .view = viewRte,
        .projection = overlayProjection,
        .eye = rendering::encodeDoubleSingle(orbitCam.Position),
        .ndcX = 0.0,
        .ndcY = 0.0,
        .nearDepth = activeNear,
        .farDepth = activeFar,
        .logDepth = logDepth,
    };
    // Publish the outline id before queueing so the renderer can
    // prioritize the outlined entity if the queue ever hits capacity.
    // Ids are stable across frames for the same scene state, and the
    // post-queue lookup below corrects the value before endFrame.
    if (outlineEntity)
      cachedOutlineObjectId =
          findGpuPickObjectIdForEntity(*outlineEntity);
    rendererBackend->setSelectionOutlineId(
        outlineEntity && !outlineUsesGeometry(*outlineEntity)
            ? cachedOutlineObjectId
            : 0);
    gpuPickSceneDebugQueueActive =
        rendererBackend->requestGpuPick(sceneRequest) != 0;
    gpuPickRegistry().clear();
    gpuPickNextEntityId = 2;
  }

  const glm::dvec3 cameraRight(orbitCam.Right);
  const glm::dvec3 cameraUp(orbitCam.Up);
  if (useOrthoProjection())
  {
    referenceLineVisible = clipReferenceSegmentToOrtho(
        glm::dvec3(0.0), worldLineEnd, cameraPos, cameraRight, cameraUp,
        frontVec, activeNear, activeFar,
        orbitCam.orthoSize() * (double)aspect, orbitCam.orthoSize(),
        referenceLineStart, referenceLineEnd);
  }
  else
  {
    const double tanHalfVertical = std::tan(glm::radians(45.0) * 0.5);
    referenceLineVisible = clipReferenceSegmentToPerspective(
        glm::dvec3(0.0), worldLineEnd, cameraPos, cameraRight, cameraUp,
        frontVec, overlayNear, overlayFar, tanHalfVertical,
        tanHalfVertical * (double)aspect, referenceLineStart,
        referenceLineEnd);
  }

  if (cameraDebugEnabled())
  {
    static int lastLineState = -2;
    const int lineState = (useOrthoProjection() ? 2 : 0) |
                          (referenceLineVisible ? 1 : 0);
    if (lineState != lastLineState)
    {
      lastLineState = lineState;
    std::cout << std::scientific << std::setprecision(4)
              << "[LINE] mode=" << (useOrthoProjection() ? "ortho" : "persp")
              << " visible=" << referenceLineVisible
              << " slab=[" << activeNear << ", " << activeFar << "]"
              << " overlay=[" << overlayNear << ", " << overlayFar << "]"
              << " seg=[(" << referenceLineStart.x << "," << referenceLineStart.y << "," << referenceLineStart.z
              << ") -> (" << referenceLineEnd.x << "," << referenceLineEnd.y << "," << referenceLineEnd.z << ")]"
              << std::endl;
    }
  }

  // LOD step with hysteresis (doc section 4.5).  In orthographic mode the
  // step is frozen below an OrthoSize of 1 so extreme close-up zoom keeps a
  // stable grid size instead of continuously introducing finer cells.
  static float step = 1.0f;
  const float baseStep = 1.0f;
  const bool freezeStep =
      useOrthoProjection() && orbitCam.orthoSize() < 1.0;
  if (!freezeStep)
  {
    float cellPx = step / pixelSize;
    while (cellPx < 40.0f && step < baseStep * 1e6f)
    {
      step *= 10.0f;
      cellPx = step / pixelSize;
    }
    while (cellPx > 400.0f && step > baseStep * 1e-6f)
    {
      step *= 0.1f;
      cellPx = step / pixelSize;
    }
  }

  // Snap the grid origin to the current step in double precision (doc
  // sections 3 and 8.2): the grid stays aligned to world multiples of the
  // step while every value sent to the shader remains small.
  // cameraPos already declared at the top of render() for the shared
  // planeCenter computation -- reused here.
  const double s = (double)step;
  glm::dvec3 originWorld(0.0);
  glm::vec3 planeOriginRelative(0.0f);
  glm::vec3 orthoPlaneCenter(0.0f);
  glm::vec3 orthoRight(0.0f);
  glm::vec3 orthoUp(0.0f);
  glm::vec2 axisVisible(0.0f);
  glm::vec2 axisOriginGridRelative(0.0f);
  glm::vec3 axisLineX(0.0f);
  glm::vec3 axisLineZ(0.0f);
  glm::vec3 planeTangentU(0.0f);
  glm::vec3 planeTangentV(0.0f);
  glm::vec3 axisColorU(0.0f);
  glm::vec3 axisColorV(0.0f);
  glm::vec3 startAxisOrigin(0.0f);
  glm::vec3 startAxisDirection(0.0f);
  float startAxisVisible = 0.0f;
  glm::vec3 startAxisLine(0.0f);
  glm::dvec3 normalizedAxisLineX(0.0);
  glm::dvec3 normalizedAxisLineZ(0.0);
  bool orthoPlaneValid = false;

  {
      const glm::dvec3 planeDelta = planeCenter - gridPlaneOrigin;
      const double centerU = glm::dot(planeDelta, tangentU);
      const double centerV = glm::dot(planeDelta, tangentV);
      const double snappedU = std::round(centerU / s) * s;
      const double snappedV = std::round(centerV / s) * s;
      originWorld = gridPlaneOrigin +
                    tangentU * snappedU +
                    tangentV * snappedV;
  }

  if (useOrthoProjection())
  {
    const glm::dvec3 front(orbitCam.Front);
    const glm::dvec3 right(orbitCam.Right);
    const glm::dvec3 up(orbitCam.Up);
    const double halfH = orbitCam.orthoSize();
    const double halfW = halfH * (double)aspect;

    // Resolve the orthographic plane mapping in double precision on the CPU.
    // At OrthoSize=1e-4, one pixel is about 2.6e-7 world units; performing
    // this division and subtraction with mediump/highp float in the fragment
    // shader quantizes the result by multiple pixels.
    //
    // Reuse planeCenter already computed once at the top of render() so
    // the ortho and perspective branches share one ground anchor.
    // For an orthographic camera every pixel shares the same view direction.
    // Hide the plane when that direction is within five degrees of being
    // parallel to it, matching the perspective shader's grazing cutoff.
    orthoPlaneValid = std::abs(frontOnNormal) > kMinGridPlaneCos;
    {
      const glm::dvec3 imageRight = right * halfW;
      const glm::dvec3 imageUp = up * halfH;
      const glm::dvec3 groundRight =
          imageRight - front * (glm::dot(imageRight, planeNormal) / frontOnNormal);
      const glm::dvec3 groundUp =
          imageUp - front * (glm::dot(imageUp, planeNormal) / frontOnNormal);
      orthoRight = glm::vec3(groundRight);
      orthoUp = glm::vec3(groundUp);
      // The point is relative to the snapped plane anchor.  Its normal-axis
      // component also cancels the rebase translation (the anchor has normal
      // coordinate zero), which is exactly what the shader depth path adds
      // back when it reconstructs a rebased clip position.
      orthoPlaneCenter = glm::vec3(planeCenter - originWorld);

      // World axes are projected as exact NDC lines while their plane
      // mapping is still in CPU doubles.  For an arbitrary plane, a point
      // on the ortho image is planeCenter + groundRight*x + groundUp*y;
      // therefore the line coefficients are its tangent-space projections.
      normalizedAxisLineX = normalizedNdcLine(
          glm::dvec3(glm::dot(groundRight, tangentU),
                     glm::dot(groundUp, tangentU),
                     glm::dot(planeCenter, tangentU)));
      normalizedAxisLineZ = normalizedNdcLine(
          glm::dvec3(glm::dot(groundRight, tangentV),
                     glm::dot(groundUp, tangentV),
                     glm::dot(planeCenter, tangentV)));
      axisLineX = anchoredNdcLine(normalizedAxisLineX);
      axisLineZ = anchoredNdcLine(normalizedAxisLineZ);
      axisVisible = isSpecialGridPlane(gridPlane)
          ? glm::vec2(
                lineIntersectsNdcSquare(normalizedAxisLineX) ? 1.0f : 0.0f,
                lineIntersectsNdcSquare(normalizedAxisLineZ) ? 1.0f : 0.0f)
          : glm::vec2(0.0f);
    }
  }
  else
  {
    // Mirror the ortho path: snap the grid anchor to the camera's gaze hit
    // on the active plane.  Otherwise orbiting the camera makes the grid
    // slide across the world even when the scene is anchored to (0, 0, 0) --
    // the cube then no longer lines up with the major grid lines and axes.
    //
    // Reuse the hoisted planeCenter computed once at the top of render().
    axisVisible = isSpecialGridPlane(gridPlane) ? glm::vec2(1.0f)
                                                : glm::vec2(0.0f);
  }

  // Rebase-relative grid anchor (was originWorld - cameraPos).
  // The shader receives the rebased offset and never sees the absolute
  // world coordinate; magnitude is bounded by chunkSize/2 instead of
  // step/2.
  planeOriginRelative = glm::vec3(originWorld - rebase);
  // The shader's p is already relative to the snapped grid anchor, so this
  // value intentionally stays grid-relative.  Resolving it against the frame
  // rebase would shift the world axes by one rebase chunk.
  // A world axis is a single line, not a repeating pattern, so its anchored
  // constant must stay exact: reducing it modulo any period would repaint
  // the axis on the nearest period-multiple grid anchor instead of at the
  // true world origin.  Float32 is still safe here because the active LOD
  // keeps one pixel worth of plane space at or above ULP(|constant|).
  {
    const glm::dvec3 gridPlaneToAnchor = gridPlaneOrigin - originWorld;
    const double axisU = glm::dot(gridPlaneToAnchor, tangentU);
    const double axisV = glm::dot(gridPlaneToAnchor, tangentV);
    axisOriginGridRelative = glm::vec2(
        static_cast<float>(axisU),
        static_cast<float>(axisV));
  }

  planeTangentU = glm::vec3(tangentU);
  planeTangentV = glm::vec3(tangentV);
  if (gridPlane == GridPlaneType::XZ)
  {
      axisColorU = kAxisColorX;   // X red
      axisColorV = kAxisColorZ;   // Z blue
  }
  else if (gridPlane == GridPlaneType::XY)
  {
      axisColorU = kAxisColorX;   // X red
      axisColorV = kAxisColorY;   // Y green
  }
  else if (gridPlane == GridPlaneType::YZ)
  {
      axisColorU = kAxisColorY;   // Y green
      axisColorV = kAxisColorZ;   // Z blue
  }
  else
  {
      glm::dvec3 startAxisWorld = gridPlaneStartAxisOrigin -
          planeNormal * glm::dot(gridPlaneStartAxisOrigin - gridPlaneOrigin,
                                 planeNormal);
      startAxisOrigin = glm::vec3(startAxisWorld - originWorld);
      glm::dvec3 startAxisDirection =
          gridPlaneStartAxisDirection -
          planeNormal * glm::dot(gridPlaneStartAxisDirection, planeNormal);
      if (glm::length(startAxisDirection) < 1e-9)
          startAxisDirection = tangentU;
      startAxisDirection = glm::normalize(startAxisDirection);
      // Project a short segment around the point where the start axis is
      // closest to the camera's gaze.  This avoids endpoint/w-plane problems
      // while still producing the true infinite line's NDC coefficients.
      const glm::dvec3 axisCenterOnPlane =
          startAxisWorld +
          startAxisDirection *
              glm::dot(planeCenter - startAxisWorld, startAxisDirection);
      const double axisSegmentLength =
          0.1 * std::max(1.0, glm::length(planeCenter - cameraPos));
      const glm::dvec3 axisPoint0 =
          axisCenterOnPlane - startAxisDirection * axisSegmentLength;
      const glm::dvec3 axisPoint1 =
          axisCenterOnPlane + startAxisDirection * axisSegmentLength;
      const glm::vec4 clip0 = overlayProjection * view *
          glm::vec4(glm::vec3(axisPoint0 - rebase), 1.0f);
      const glm::vec4 clip1 = overlayProjection * view *
          glm::vec4(glm::vec3(axisPoint1 - rebase), 1.0f);
      if (clip0.w > 0.0f && clip1.w > 0.0f)
      {
          const glm::dvec2 ndc0(clip0.x / clip0.w, clip0.y / clip0.w);
          const glm::dvec2 ndc1(clip1.x / clip1.w, clip1.y / clip1.w);
          if (glm::length(ndc1 - ndc0) > 1e-12)
          {
              const glm::dvec3 normalizedLine = normalizedNdcLine(glm::dvec3(
                  ndc0.y - ndc1.y,
                  ndc1.x - ndc0.x,
                  ndc0.x * ndc1.y - ndc1.x * ndc0.y));
              if (lineIntersectsNdcSquare(normalizedLine))
              {
                  startAxisLine = anchoredNdcLine(normalizedLine);
                  startAxisVisible = 1.0f;
              }
          }
      }
  }

  const glm::mat4 objectViewProj = projection * view;
  const glm::mat4 overlayViewProj = overlayProjection * view;
  const glm::mat4 gridViewProj = gridProjection * view;

  bool gridPlaneVisible = false;
  if (useOrthoProjection())
  {
    gridPlaneVisible = orthoPlaneValid;
    if (gridPlaneVisible && std::abs(frontOnNormal) > kMinGridPlaneCos)
    {
      const double halfH = orbitCam.orthoSize();
      const double halfW = (double)halfH * (double)aspect;
      const glm::dvec3 right(orbitCam.Right);
      const glm::dvec3 up(orbitCam.Up);
      const double cameraPlaneDistance =
          glm::dot(cameraPos - gridPlaneOrigin, planeNormal);
      const double groundCenterDepth =
          -cameraPlaneDistance / frontOnNormal;
      const double groundDepthRadius =
          (std::abs(glm::dot(right, planeNormal)) * halfW +
           std::abs(glm::dot(up, planeNormal)) * halfH) /
          std::abs(frontOnNormal);
      gridPlaneVisible =
          (groundCenterDepth + groundDepthRadius >= activeNear) &&
          (groundCenterDepth - groundDepthRadius <= activeFar);
    }
  }
  else
  {
    const glm::dvec3 front = glm::normalize(orbitCam.Front);
    const double planeCos = std::abs(glm::dot(front, planeNormal));
    gridPlaneVisible = planeCos >= kMinGridPlaneCos;
    if (gridPlaneVisible)
    {
      const double denom = glm::dot(front, planeNormal);
      const double t =
          glm::dot(gridPlaneOrigin - cameraPos, planeNormal) / denom;
      gridPlaneVisible = t > 0.0;
    }
  }

  if (frustumCaptureRequested)
  {
    frustumCaptureRequested = false;
    frustumWireframeVisible = true;
    const glm::mat4 invVP = glm::inverse(overlayViewProj);
    for (int i = 0; i < 8; ++i)
    {
      const float ndcX = (i & 1) ? 1.0f : -1.0f;
      const float ndcY = (i & 2) ? 1.0f : -1.0f;
      const float ndcZ = (i & 4) ? 1.0f : -1.0f;
      glm::vec4 p = invVP * glm::vec4(ndcX, ndcY, ndcZ, 1.0f);
      if (std::abs(p.w) > 1e-10f)
        p /= p.w;
      frustumCorners[i] = glm::dvec3(p) + rebase;
    }

    // The grid plane's visible extent inside the frustum.  This is the
    // plane clipped by all four side planes *and* by near/far.  Without
    // the depth clip the debug polygon would show regions whose grid
    // fragments are discarded outside the active depth slab.
    std::array<glm::dvec3, 16> gridPolygon{};
    int gridPolygonCount = 0;
    if (useOrthoProjection() && orthoPlaneValid)
    {
      gridPolygon[0] = originWorld +
          glm::dvec3(orthoPlaneCenter - orthoRight - orthoUp);
      gridPolygon[1] = originWorld +
          glm::dvec3(orthoPlaneCenter + orthoRight - orthoUp);
      gridPolygon[2] = originWorld +
          glm::dvec3(orthoPlaneCenter + orthoRight + orthoUp);
      gridPolygon[3] = originWorld +
          glm::dvec3(orthoPlaneCenter - orthoRight + orthoUp);
      gridPolygonCount = 4;
      gridVisibleQuadValid = true;
    }
    else if (!useOrthoProjection())
    {
      const glm::dvec3 camPos = orbitCam.Position;
      const glm::dvec3 n = glm::normalize(gridPlaneNormal);
      const double planeDist = glm::dot(gridPlaneOrigin - camPos, n);
      gridVisibleQuadValid = true;
      for (int i = 0; i < 4; ++i)
      {
        const glm::dvec3 dir =
            glm::normalize(frustumCorners[4 + i] - camPos);
        const double denom = glm::dot(dir, n);
        if (std::abs(denom) < 1e-10 || planeDist / denom < 0.0)
        {
          gridVisibleQuadValid = false;
          break;
        }
        gridPolygon[i] = camPos + dir * (planeDist / denom);
      }
      gridPolygonCount = gridVisibleQuadValid ? 4 : 0;
    }
    else
    {
      gridVisibleQuadValid = false;
    }

    if (gridVisibleQuadValid)
    {
      std::array<glm::dvec3, 16> clippedGridPolygon{};
      int clippedGridPolygonCount = gridPolygonCount;
      clipPolygonAgainstDepth(
          gridPolygon.data(), clippedGridPolygonCount,
          clippedGridPolygon.data(), (int)clippedGridPolygon.size(),
          cameraPos, frontVec, true, overlayFar);
      if (clippedGridPolygonCount >= 3)
      {
        clipPolygonAgainstDepth(
            clippedGridPolygon.data(), clippedGridPolygonCount,
            clippedGridPolygon.data(), (int)clippedGridPolygon.size(),
            cameraPos, frontVec, false, overlayNear);
      }
      gridVisibleQuadValid = clippedGridPolygonCount >= 3 &&
                             clippedGridPolygonCount <= 8;
      if (gridVisibleQuadValid)
      {
        std::copy_n(clippedGridPolygon.begin(), clippedGridPolygonCount,
                    gridVisibleQuad);
        gridVisibleQuadCount = clippedGridPolygonCount;
      }
    }
    if (!gridVisibleQuadValid)
      gridVisibleQuadCount = 0;
  }

    const rendering::GridRenderData gridRenderData{
      .view = view,
        .projection = gridProjection,
        .invViewProj = glm::inverse(gridViewProj),
      .viewProj = gridViewProj,
      .camFront = glm::vec3(orbitCam.Front),
      .orthoPlaneCenter = orthoPlaneCenter,
      .orthoRight = orthoRight,
      .orthoUp = orthoUp,
      .planeOriginRelative = planeOriginRelative,
      .plane = (float)(gridPlane == GridPlaneType::XZ ? 0
                      : gridPlane == GridPlaneType::XY ? 1
                       : gridPlane == GridPlaneType::YZ ? 2
                                                        : 3),
      .planeNormal = glm::vec3(planeNormal),
      .planeTangentU = planeTangentU,
      .planeTangentV = planeTangentV,
      .axisColorU = axisColorU,
      .axisColorV = axisColorV,
      .startAxisOrigin = startAxisOrigin,
      .startAxisDirection = startAxisDirection,
      .startAxisVisible = startAxisVisible,
      .startAxisLine = startAxisLine,
      .axisOriginGridRelative = axisOriginGridRelative,
      .axisLineX = axisLineX,
      .axisLineZ = axisLineZ,
      .orthoPlaneValid = orthoPlaneValid ? 1.0f : 0.0f,
      .groundRelativeY = 0.0f, // unused in current shader; was (float)(-rebase.y) which lost precision
      .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
      .step = step,
      .axisVisible = axisVisible,
      .screenHeight = static_cast<float>(drawableHeight),
      .screenWidth = static_cast<float>(drawableWidth),
      .gridColorMajor = glm::vec3(0.5f, 0.5f, 0.5f),
      .gridColorMinor = glm::vec3(0.3f, 0.3f, 0.3f),
      .gridOpacity = 0.6f,
      .logDepth = logDepth,
  };
  static scene::SceneDrawList sceneOverlay;
  sceneOverlay.clear();
  if (gridPlaneVisible)
    sceneOverlay.setGrid(gridRenderData);

  // Opaque geometry first so transparent passes can depth-test against it.
  const MeshEntityRecord centerCube = getCenterCubeEntity();
  if (centerCubeInFrame)
  {
    appendMeshEntityToScene(centerCube, sceneOverlay);
    queueGpuMeshEntity(centerCube, kGpuPickCenterCubeId);
  }

  // The logical line still runs through the literal world origin.  Only its
  // frustum-clipped portion is submitted, so both GPU endpoints remain small
  // after rebase even when the full segment spans 1e7 world units.
  if (referenceLineVisible)
  {
    appendSceneLine(sceneOverlay, referenceLineStart, referenceLineEnd,
                    glm::vec3(0.15f, 1.0f, 0.25f), 0.9f);
  }
  submitAcGiDrawable(sceneOverlay, viewRte, projection, overlayProjection,
                      orbitCam.Position, cameraPos, cameraRight, cameraUp,
                      frontVec, logDepth, pixelSize);

  // Translucent meshes remain sorted far-to-near. They depth-test against
  // opaque geometry but must not overwrite the shared depth buffer.
  drawLargeCoordinateObjects(viewRte, projection, orbitCam.Position, drawOrder,
                             logDepth, pixelSize, 0.15f);

  // Draw line-like outlines before the original CAD overlays.  The overlay
  // view is sequential, so the source line strokes/curves composite on top of
  // their wider outline instead of the outline covering them.
  drawLineLikeOutline(viewRte, overlayProjection, cameraPos, cameraRight,
                      cameraUp, frontVec, pixelSize, logDepth);
  drawSolidFillOutline(viewRte, overlayProjection, cameraPos, frontVec,
                       pixelSize, logDepth);
  drawCadPointOutline(viewRte, overlayProjection, cameraPos, frontVec,
                      pixelSize, logDepth);

  drawVectorPrimitivesDemo(viewRte, projection, overlayProjection,
                           orbitCam.Position, logDepth,
                           cameraPos, frontVec, cameraRight, cameraUp,
                           pixelSize, visibleCadDraws, tinyCadDraws);

  // SDF text pass: the AcGi text engine expands each queued request into
  // per-glyph quads (one R8 distance-field texture per glyph, uploaded on
  // first use).  Sources: Text/MText entities and the standalone demo
  // string, both registered in the request queue (which also feeds the
  // content bounds so the depth slab covers the glyphs).
  if (gSdfFontReady && rendererBackend)
  {
    constexpr glm::mat4 identityView(1.0f);
    // Frustum culling: skip text whose oriented bounding rectangle lies
    // entirely outside the ortho viewport (perspective keeps drawing —
    // its frustum test hooks in at the same call when needed).
    const double orthoHalfHeight = useOrthoProjection()
                                       ? orbitCam.orthoSize()
                                       : std::numeric_limits<double>::max();
    const double orthoHalfWidth =
        orthoHalfHeight * double(currentDrawableWidth()) /
        std::max(1, currentDrawableHeight());
    for (const acgi::TextRequest &request : acgi::textRequests())
    {
      if (useOrthoProjection() &&
          !acgi::textEngine().intersectsOrthoViewport(
              request, cameraPos, cameraRight, cameraUp, frontVec,
              orthoHalfWidth, orthoHalfHeight))
      {
        continue;
      }
      acgi::textEngine().drawText(*rendererBackend, identityView,
                                  projection, cameraPos, cameraRight,
                                  cameraUp, frontVec, request);
    }
  }

  // Below the mesh LOD threshold, emit stable center-point impostors.  The
  // renderer projects and batches all points into one GPU submission per
  // transient-buffer chunk.
  sceneOverlay.clear();
  for (const LargeCoordinateObject *object : tinyDraws)
    appendScenePoint(sceneOverlay, object->worldPosition,
                     glm::vec3(meshEntityColor(*object)), 2.0);

  // Tiny mesh impostors are renderable entities too.  Their full-scene ID
  // representation is a small camera-facing quad, matching the visible
  // two-pixel point rather than silently omitting it from the ID pass.
  // Tiny mesh impostors must also join the one-pixel pick pass: the CPU
  // fallback raycast can hit a tiny mesh the visible-draw queue skipped, and
  // the fallback outline lookup can only resolve ids that were registered.
  if ((gpuPickSceneDebugQueueActive || gpuPickFocusWaiting()) &&
      !tinyDraws.empty())
  {
    static std::vector<rendering::FillVertex> tinyMeshPickVertices;
    const glm::vec3 pickRight(orbitCam.Right);
    const glm::vec3 pickUp(orbitCam.Up);
    const double referenceDistance =
        glm::length(orbitCam.Position - orbitCam.Target);
    for (const LargeCoordinateObject *object : tinyDraws)
    {
      if (!object || !meshEntityVisible(*object))
        continue;

      const glm::dvec3 center = object->worldPosition;
      const double depth = glm::dot(center - orbitCam.Position, orbitCam.Front);
      const double objectPixelSize =
          useOrthoProjection()
              ? double(pixelSize)
              : double(pixelSize) * std::max(0.05, depth) /
                    std::max(0.05, referenceDistance);
      // The visible impostor is a disc with a 2-pixel radius; the ID quad
      // must have the same projected radius, while retaining a small
      // screen-space minimum for extreme zoom-out.
      const float radius = std::max(1.5f, float(2.0 * objectPixelSize));
      const uint32_t objectId = registerGpuPickEntity(
          {VisibilityKind::MeshObject, object, nullptr});
      const glm::vec4 idColor = encodeGpuPickId(objectId);
      const glm::vec3 relative = glm::vec3(center - orbitCam.Position);
      const glm::vec3 right = pickRight * radius;
      const glm::vec3 up = pickUp * radius;
      tinyMeshPickVertices.clear();
      tinyMeshPickVertices.push_back({relative - right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right + up, idColor});
      tinyMeshPickVertices.push_back({relative - right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right + up, idColor});
      tinyMeshPickVertices.push_back({relative - right + up, idColor});
      rendererBackend->queueGpuTrianglePick(
          0, tinyMeshPickVertices.data(),
          uint32_t(tinyMeshPickVertices.size()), viewRte, projection,
          logDepth, objectId);
    }
  }

  // Small 5-pixel "sphere" (disc-shaded point) at the orbit target so the
  // camera's focus point is always visible.  Uses the same RTE rebase as
  // every other draw call.
  // The camera focus marker remains a camera overlay, not a CAD entity.
  appendScenePoint(sceneOverlay, orbitCam.Target,
                   glm::vec3(1.0f, 0.15f, 0.15f), 5.0);

  if (frustumWireframeVisible)
  {
    constexpr int nearEdges[4][2] = {{0,1},{1,2},{2,3},{3,0}};
    constexpr int farEdges[4][2] = {{4,5},{5,6},{6,7},{7,4}};
    constexpr int sideEdges[4][2] = {{0,4},{1,5},{2,6},{3,7}};
    const glm::vec3 nearColor(1.0f, 0.2f, 0.2f);
    const glm::vec3 farColor(0.2f, 0.4f, 1.0f);
    const glm::vec3 sideColor(1.0f, 1.0f, 1.0f);
    for (const auto &e : nearEdges)
      appendSceneLine(sceneOverlay, frustumCorners[e[0]],
                      frustumCorners[e[1]], nearColor, 0.9f);
    for (const auto &e : farEdges)
      appendSceneLine(sceneOverlay, frustumCorners[e[0]],
                      frustumCorners[e[1]], farColor, 0.9f);
    for (const auto &e : sideEdges)
      appendSceneLine(sceneOverlay, frustumCorners[e[0]],
                      frustumCorners[e[1]], sideColor, 0.9f);
    if (gridVisibleQuadValid)
    {
      const glm::vec3 gridQuadColor(1.0f, 0.85f, 0.1f);
      for (int i = 0; i < gridVisibleQuadCount; ++i)
        appendSceneLine(sceneOverlay, gridVisibleQuad[i],
                        gridVisibleQuad[(i + 1) % gridVisibleQuadCount],
                        gridQuadColor, 0.9f);
    }
  }

  submitAcGiDrawable(sceneOverlay, viewRte, projection, overlayProjection,
                      orbitCam.Position, cameraPos, cameraRight, cameraUp,
                      frontVec, logDepth, pixelSize);

  static bool screenshotRequested = false;
  if (!screenshotRequested && lineDebugEnabled())
  {
    if (const char *screenshot = std::getenv("GRID_SCREENSHOT");
        screenshot && *screenshot)
    {
      static uint32_t screenshotDelay = 30;
      if (screenshotDelay > 0)
      {
        --screenshotDelay;
      }
      else
      {
        rendererBackend->requestDebugScreenShot(screenshot);
        screenshotRequested = true;
        std::cout << "[LINE_DEBUG] requested screenshot: " << screenshot
                  << std::endl;
      }
    }
  }

  logCameraStateIfChanged(orbitCam.Target, activeNear, activeFar,
                          useOrthoProjection());

  if (rendererBackend)
  {
    uint32_t outlineId = 0;
    if (outlineLockTest)
    {
      outlineId = lockedOutlineId;
    }
    else if (outlineEntity)
    {
      if (gpuPickSceneDebugQueueActive)
        cachedOutlineObjectId =
            findGpuPickObjectIdForEntity(*outlineEntity);
      // Reuse the cached id on idle frames; the ID texture is unchanged.
      outlineId = cachedOutlineObjectId;
    }
    (*rendererBackend).setSelectionOutlineId(
        outlineEntity && !outlineUsesGeometry(*outlineEntity) ? outlineId : 0);
    if (pickDebugEnabled())
    {
      static uint32_t lastLoggedOutlineId = 0xffffffffu;
      if (outlineId != lastLoggedOutlineId)
      {
        lastLoggedOutlineId = outlineId;
        std::printf("[PICK_DEBUG] outlineId=%u sceneQueueActive=%d\n",
                    outlineId, gpuPickSceneDebugQueueActive ? 1 : 0);
      }
    }
  }
  // Present normalization does not need a full-frame readback; expose the
  // range assigned while this frame's entities were queued.
  if (rendererBackend)
  {
    const uint32_t maxId = gpuPickNextEntityId > 2
        ? gpuPickNextEntityId - 1
        : 2;
    rendererBackend->setGpuPickIdRange(2, maxId);
  }

  rendererBackend->endFrame();
}

int main(int argc, char *argv[])
{
  SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "1");

  if (!init())
  {
    std::cerr << "Failed to initialize" << std::endl;
    return -1;
  }
  SDL_PumpEvents();
  // Stress-test field location: printed once so it can be reached by
  // panning (or by pressing L) instead of generating on demand.
  printLargeCoordinateValidation();

  double requestedOrthoHalfHeight = -1.0;
  if (const char *testOrtho = std::getenv("GRID_CAMERA_TEST_ORTHO");
      testOrtho && std::strcmp(testOrtho, "0") != 0)
  {
    useOrthoProjection() = true;
    if (const char *halfH = std::getenv("GRID_CAMERA_TEST_ORTHO_HALFH"))
      requestedOrthoHalfHeight = std::max(0.01, std::atof(halfH));
    std::cout << "Projection: ORTHOGRAPHIC (test)" << std::endl;
  }

  fitCameraToRenderableObjects();

  // The demo always starts in the large-coordinate stress field so every
  // object group renders by default without extra environment setup.
  largeCoordinateCameraView = true;
  cubeWorldPosition =
      LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
  fitCameraToStressField();

  // Debug helper: jump the startup camera straight to a world point.
  // GRID_CAMERA_START_TARGET=x,y,z[,distance]
  if (const char *startTarget = std::getenv("GRID_CAMERA_START_TARGET"))
  {
    double tx = 0.0, ty = 0.0, tz = 0.0, dist = 1500.0;
    if (std::sscanf(startTarget, "%lf,%lf,%lf,%lf", &tx, &ty, &tz, &dist) >= 3)
    {
      orbitCam.setOrbit(glm::dvec3(tx, ty, tz), std::max(1.0, dist));
      std::cout << "Camera start target: (" << tx << ", " << ty << ", "
                << tz << ") distance=" << dist << std::endl;
    }
  }
  if (runCadPickAudit())
  {
    close();
    return 0;
  }
  // Keep the legacy automated zoom test working without a separate ortho
  // size state: convert the requested half-height into the authoritative
  // Distance after the OpenCAD-style fit has chosen its orientation.
  if (requestedOrthoHalfHeight > 0.0)
  {
    const double tanHalfFov = std::tan(glm::radians(orbitCam.Zoom) * 0.5);
    orbitCam.setTargetDistance(requestedOrthoHalfHeight / tanHalfFov);
  }
  resetSlabStabilizers();
  SDL_SetWindowTitle(
      window, "grid plane - large-coordinate stress field");

  SDL_Event evt;
  bool running = true;
  static uint32_t debugExitFrames = 0;
  if (debugExitFrames == 0)
  {
    if (const char *exitFrames = std::getenv("GRID_EXIT_FRAMES"))
      debugExitFrames = static_cast<uint32_t>(std::max(1, std::atoi(exitFrames)));
  }
  bool middleMouseDrag = false;
  bool gpuPickDebugVisible = false;
  bool testPanApplied = false;
  bool originOrthoScenarioApplied = false;
  bool testDoubleClickApplied = false;


  const Uint64 frameTimerFrequency = SDL_GetPerformanceFrequency();
  Uint64 frameTimerStart = SDL_GetPerformanceCounter();

  while (running)
  {
    float currentFrame = SDL_GetTicks() / 1000.0f;

    // Automated validation: inject the same SDL event path as a real
    // left-button double click. Set GRID_CAMERA_TEST_DOUBLE_CLICK_AT_SECONDS.
    if (!testDoubleClickApplied)
    {
      const char *testAtValue = std::getenv(
          "GRID_CAMERA_TEST_DOUBLE_CLICK_AT_SECONDS");
      if (testAtValue && currentFrame >= std::atof(testAtValue))
      {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        const char *testX = std::getenv("GRID_CAMERA_TEST_DOUBLE_CLICK_X");
        const char *testY = std::getenv("GRID_CAMERA_TEST_DOUBLE_CLICK_Y");
        const int x = testX ? std::atoi(testX) : winW / 2;
        const int y = testY ? std::atoi(testY) : winH / 2;
        SDL_Event synthetic{};
        synthetic.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        synthetic.button.windowID = SDL_GetWindowID(window);
        synthetic.button.timestamp = SDL_GetTicks();
        synthetic.button.clicks = 2;
        synthetic.button.button = SDL_BUTTON_LEFT;
        synthetic.button.down = true;
        synthetic.button.x = x;
        synthetic.button.y = y;
        const int pushed = SDL_PushEvent(&synthetic);
        testDoubleClickApplied = true;
        if (pushed == 0)
        {
          std::puts("[PICK_DEBUG] test event push failed");
        }
        std::printf("[PICK_DEBUG] test double-click x=%d y=%d", x, y);
        std::puts("");
      }
    }

    while (SDL_PollEvent(&evt))
    {
      if (evt.type == SDL_EVENT_QUIT)
      {
        running = false;
      }
      if (evt.type == SDL_EVENT_KEY_DOWN)
      {
        if (evt.key.key == SDLK_ESCAPE)
        {
          running = false;
        }
        if (evt.key.key == SDLK_P)
        {
          switchProjectionMode();
        }
        if (evt.key.key == SDLK_I)
        {
          gpuPickDebugVisible = !gpuPickDebugVisible;
          if (rendererBackend)
            rendererBackend->setGpuPickDebugVisible(gpuPickDebugVisible);
          std::cout << "GPU pick debug texture: "
                    << (gpuPickDebugVisible ? "visible" : "hidden")
                    << std::endl;
        }
        if (evt.key.key == SDLK_1)
          applyGridPlane(GridPlaneType::XY);
        if (evt.key.key == SDLK_2)
          applyGridPlane(GridPlaneType::XZ);
        if (evt.key.key == SDLK_3)
          applyGridPlane(GridPlaneType::YZ);
        if (evt.key.key == SDLK_4)
          applyGridPlane(GridPlaneType::Custom);
        if (evt.key.key == SDLK_R)
          resetWorldUpAndPlaneFromCamera();
        if (evt.key.key == SDLK_C)
          setTargetPlaneConstraint(!targetPlaneConstraintEnabled);
        if (evt.key.key == SDLK_F)
        {
          frustumCaptureRequested = true;
          std::cout << "Frustum wireframe: captured" << std::endl;
        }
        if (evt.key.key == SDLK_K)
        {
          gpuPickSceneDebug = !gpuPickSceneDebug;
          if (rendererBackend)
            (*rendererBackend).setGpuPickSceneDebug(gpuPickSceneDebug);
          if (gpuPickSceneDebug)
            std::puts("Full-scene GPU ID view: on");
          else
            std::puts("Full-scene GPU ID view: off");
        }
        if (evt.key.key == SDLK_O)
        {
          outlineLockTest = !outlineLockTest;
          if (outlineLockTest)
          {
            std::printf("Outline lock: on (id=%u)", lockedOutlineId);
            std::puts("");
          }
          else
          {
            std::puts("Outline lock: off");
          }
        }
        if (evt.key.key == SDLK_U)
        {
          outlineAllTest = !outlineAllTest;
          if (rendererBackend)
            (*rendererBackend).setSelectionOutlineAll(outlineAllTest);
          if (outlineAllTest)
            std::puts("Outline all: on");
          else
            std::puts("Outline all: off");
        }
        // V -- cycle the visual style through all render modes.
        if (evt.key.key == SDLK_V)
        {
          visualStyleManager.cycle();
          if (rendererBackend)
            rendererBackend->setRenderMode(visualStyleManager.mode());
          std::cout << "Visual style: "
                    << rendering::renderModeLabel(visualStyleManager.mode())
                    << std::endl;
        }
        if (evt.key.scancode == SDL_SCANCODE_L)
        {
          // L key -- move the camera between the origin scene and the
          // large-coordinate stress field.  The stress models are always
          // generated at startup; this key only teleports the camera.
          largeCoordinateCameraView = !largeCoordinateCameraView;
          if (largeCoordinateCameraView)
          {
            cubeWorldPosition =
                LARGE_COORDINATE_BASE_POINT +
                LARGE_COORDINATE_DETAIL_OFFSET;
            fitCameraToStressField();
            resetSlabStabilizers();
            SDL_SetWindowTitle(
                window, "grid plane - large-coordinate stress field");
          }
          else
          {
            cubeWorldPosition = glm::dvec3(0.0);
            orbitCam.clearDepthBounds();
            orbitCam.setOrbit(cubeWorldPosition, 15.0);
            resetSlabStabilizers();
            SDL_SetWindowTitle(window, "grid plane");
          }
          std::cout << "Camera: "
                    << (largeCoordinateCameraView ? "large-coordinate field"
                                                  : "origin")
                    << std::endl;
        }
      }

      if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
          evt.button.button == SDL_BUTTON_MIDDLE)
      {
        middleMouseDrag = true;
        orbitPivot = viewCenterObjectPivot().value_or(orbitCam.Target);
      }
      if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP &&
          evt.button.button == SDL_BUTTON_MIDDLE)
      {
        middleMouseDrag = false;
        // VSG persistent pivot: keep the pivot so the next orbit drag
        // continues around the same scene point.  It is re-captured on
        // the next middle-button down.
      }
      if (evt.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
      {
        middleMouseDrag = false;
      }

      // Double-click left button: autofocus at cursor position.
      if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
          evt.button.button == SDL_BUTTON_LEFT &&
          evt.button.clicks == 2)
      {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        if (pickDebugEnabled())
        {
          std::printf("[PICK_DEBUG] double-click x=%d y=%d gpu=%d",
                      static_cast<int>(evt.button.x), static_cast<int>(evt.button.y),
                      gpuPickEnabled() ? 1 : 0);
          std::puts("");
        }
        const double ndcX = cursorToNdcX(evt.button.x, winW);
        const double ndcY = cursorToNdcY(evt.button.y, winH);
        if (gpuPickEnabled())
        {
          gpuPickFocus.ndcX = ndcX;
          gpuPickFocus.ndcY = ndcY;
          gpuPickFocus.pendingNdc = true;
          gpuPickFocus.pendingFrames = 0;
          gpuPickFocus.waitingResult = false;
          gpuPickFocus.camera.reset();
        }
        else if (const std::optional<AutofocusResult> selectedEntity =
                     autofocusAtNdc(ndcX, ndcY))
        {
          reportAutofocus(*selectedEntity);
        }
      }

      handleOrbitMouseMovement(evt, middleMouseDrag);
      handleOrbitZoom(evt);
    }


    // Automated validation can use this when desktop input focus is denied.
    if (!testPanApplied)
    {
      const char *testPanValue = std::getenv("GRID_CAMERA_TEST_PAN_X");
      const char *testPanAtValue =
          std::getenv("GRID_CAMERA_TEST_PAN_AT_SECONDS");
      if (testPanValue && testPanAtValue &&
          currentFrame >= std::atof(testPanAtValue))
      {
        orbitCam.panScreen((float)std::atof(testPanValue), 0.0f,
                           (float)currentDrawableHeight());
        testPanApplied = true;
      }
    }

    // A single post-input correction covers every camera mutation, including
    // orbiting, dolly/zoom, teleport shortcuts, and future input paths.
    if (targetPlaneConstraintEnabled)
      enforceTargetPlaneConstraint();

    if (!originOrthoScenarioApplied)
    {
      const char *testAtValue = std::getenv(
          "GRID_CAMERA_TEST_ORIGIN_ORTHO_AT_SECONDS");
      if (testAtValue && currentFrame >= std::atof(testAtValue))
      {
        largeCoordinateCameraView = false;
        cubeWorldPosition = glm::dvec3(0.0);
        orbitCam.clearDepthBounds();
        orbitCam.setOrbit(cubeWorldPosition, 15.0);
        switchProjectionMode();
        resetSlabStabilizers();
        originOrthoScenarioApplied = true;
        std::cout << "Camera test: origin orthographic convergence"
                  << std::endl;
      }
    }

    render();

    if (debugExitFrames > 0)
    {
      --debugExitFrames;
      if (debugExitFrames == 0)
        running = false;
    }

    rendererBackend->present();

    if (frameLogEnabled())
    {
      static uint32_t frameLogFrames = 0;
      static double frameLogMilliseconds = 0.0;
      const Uint64 frameTimerEnd = SDL_GetPerformanceCounter();
      frameLogMilliseconds += 1000.0 *
          double(frameTimerEnd - frameTimerStart) /
          double(frameTimerFrequency);
      ++frameLogFrames;
      if (frameLogFrames >= 60)
      {
        const double averageMilliseconds =
            frameLogMilliseconds / double(frameLogFrames);
        std::printf("[FRAME_LOG] avg %.2f ms (%.1f fps)\n",
                    averageMilliseconds,
                    averageMilliseconds > 0.0 ? 1000.0 / averageMilliseconds
                                              : 0.0);
        frameLogFrames = 0;
        frameLogMilliseconds = 0.0;
      }
    }
    frameTimerStart = SDL_GetPerformanceCounter();
  }

  close();

  return 0;
}
