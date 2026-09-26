#include "main.h"
#include "entities/tessellate.h"
#include "rendering/ProceduralMesh.h"
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <array>
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

namespace
{

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
  else
  {
    std::cerr << "Unknown WINDOW_RENDERER value '" << backendName
              << "'. Supported: bgfx, dx11, dx12, gl, vk." << std::endl;
  }
  return requested;
}

std::unique_ptr<rendering::RendererBackend> rendererBackend;

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

FPSCamera fpsCam(glm::vec3(0.0f, 1.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), -90.0f, 0.0f);

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

  std::cout << "Renderer backend: " << rendererBackend->name() << std::endl;

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
  if (const char *textureEnv = std::getenv("GRID_MESH_TEXTURE"))
  {
    if (textureEnv[0] != '\0')
    {
      gMeshTextureIndex = rendererBackend->loadMeshTexture(textureEnv);
      std::cout << "Mesh texture index: " << gMeshTextureIndex << std::endl;
    }
  }
  std::cout << "Grid plane: " << gridPlaneName(gridPlane)
            << " (1=XY, 2=XZ, 3=YZ, 4=CUSTOM; XYZ=red/green/blue, "
               "custom start axis=yellow)"
            << std::endl;

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

void drawTargetPoint(const glm::mat4 &view, const glm::mat4 &projection,
                     const glm::dvec3 &rebaseOrigin,
                     const glm::dvec3 &targetWorldPosition,
                     const glm::vec4 &logDepth,
                     float pixelSizeWorld)
{
  if (!rendererBackend)
    return;

  const rendering::TargetPointRenderData renderData{
      .view = view,
      .projection = projection,
      .relativePosition = glm::vec3(targetWorldPosition - rebaseOrigin),
      .pointSize = 5.0f,
        .pixelSizeWorld = pixelSizeWorld,
      .color = glm::vec3(1.0f, 0.15f, 0.15f),
      .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
      .logDepth = logDepth,
  };
  rendererBackend->drawTargetPoint(renderData);
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

void drawAabbForCube(const rendering::CubeRenderData &renderData)
{
  if (!rendererBackend)
    return;

  // The renderer treats the model translation separately from the rebase
  // position, so the AABB must do the same.
  glm::mat4 modelNoTranslation = renderData.model;
  modelNoTranslation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  glm::dvec3 relativeMin(std::numeric_limits<double>::max());
  glm::dvec3 relativeMax(std::numeric_limits<double>::lowest());
  const glm::dvec3 objectEncoded =
      glm::dvec3(renderData.object.high) + glm::dvec3(renderData.object.low);
  const glm::dvec3 eyeEncoded =
      glm::dvec3(renderData.eye.high) + glm::dvec3(renderData.eye.low);
  for (const float x : {-0.5f, 0.5f})
  {
    for (const float y : {-0.5f, 0.5f})
    {
      for (const float z : {-0.5f, 0.5f})
      {
        const glm::dvec3 relative = glm::dvec3(
            modelNoTranslation * glm::vec4(x, y, z, 1.0f)) +
            (objectEncoded - eyeEncoded);
        relativeMin = glm::min(relativeMin, relative);
        relativeMax = glm::max(relativeMax, relative);
      }
    }
  }
  const glm::dvec3 relativeCenter = (relativeMin + relativeMax) * 0.5;

  const rendering::AabbRenderData aabb{
      .view = renderData.view,
      .projection = renderData.projection,
      .relativeMin = glm::vec3(relativeMin),
      .relativeMax = glm::vec3(relativeMax),
      .color = glm::vec3(1.0f, 0.90f, 0.15f),
      .opacity = 1.0f,
      .logDepth = renderData.logDepth,
      .eye = renderData.eye,
      .object = rendering::encodeDoubleSingle(relativeCenter),
  };
  rendererBackend->drawAabb(aabb);
}

void drawCube(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::dvec3 &rebaseOrigin,
              const glm::dvec3 &objectWorldPosition,
              const glm::vec3 &objectColor,
              float opacity,
              float size,
              const glm::vec4 &logDepth)
{
  if (!rendererBackend)
    return;

  const rendering::CubeRenderData renderData{
      .model = glm::scale(glm::mat4(1.0f), glm::vec3(size)),
      .view = view,
      .projection = projection,
      .modelRelativePosition = glm::vec3(objectWorldPosition - rebaseOrigin),
      .objectColor = objectColor,
      .opacity = opacity,
      .logDepth = logDepth,
      .eye = rendering::encodeDoubleSingle(orbitCam.Position),
      .object = rendering::encodeDoubleSingle(objectWorldPosition),
  };
  rendererBackend->drawCube(renderData);
  drawAabbForCube(renderData);
}

// View-space Z is negative in front of the eye, while clipping code uses
// positive distance along OrbitCamera::Front.  Evaluate in double here, then
// let the shader interpolate already-projected, camera-sized endpoints.
glm::dvec3 toViewSpace(const glm::dvec3 &worldPosition,
                       const glm::dvec3 &cameraPosition,
                       const glm::dvec3 &cameraRight,
                       const glm::dvec3 &cameraUp,
                       const glm::dvec3 &cameraFront)
{
  const glm::dvec3 delta = worldPosition - cameraPosition;
  return {glm::dot(delta, cameraRight),
          glm::dot(delta, cameraUp),
          -glm::dot(delta, cameraFront)};
}

void drawWorldLine(const glm::mat4 &projection,
                   const glm::dvec3 &cameraPosition,
                   const glm::dvec3 &cameraRight,
                   const glm::dvec3 &cameraUp,
                   const glm::dvec3 &cameraFront,
                   const glm::dvec3 &startWorldPosition,
                   const glm::dvec3 &endWorldPosition,
                   const glm::vec3 &color = glm::vec3(0.15f, 1.0f, 0.25f),
                   float opacity = 0.9f,
                   const glm::vec4 &logDepth = glm::vec4(0.0f))
{
  if (!rendererBackend)
    return;

  const rendering::WorldLineRenderData renderData{
      .projection = projection,
      .viewStart = glm::vec3(toViewSpace(startWorldPosition, cameraPosition,
                                         cameraRight, cameraUp,
                                         cameraFront)),
      .viewEnd = glm::vec3(toViewSpace(endWorldPosition, cameraPosition,
                                       cameraRight, cameraUp,
                                       cameraFront)),
      .lineWidth = 2.0f,
      .color = color,
      .opacity = opacity,
      .logDepth = logDepth,
  };
  rendererBackend->drawWorldLine(renderData);
}

void drawMesh(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::dvec3 &rebaseOrigin,
              const glm::dvec3 &objectWorldPosition,
              const glm::vec3 &objectColor, float opacity, float size,
              rendering::MeshType mesh, const glm::vec4 &logDepth)
{
  if (!rendererBackend)
    return;

  const rendering::CubeRenderData renderData{
      .model = glm::scale(glm::mat4(1.0f), glm::vec3(size)),
      .view = view,
      .projection = projection,
      .modelRelativePosition = glm::vec3(objectWorldPosition - rebaseOrigin),
      .objectColor = objectColor,
      .opacity = opacity,
      .mesh = mesh,
      .logDepth = logDepth,
      .eye = rendering::encodeDoubleSingle(rebaseOrigin),
      .object = rendering::encodeDoubleSingle(objectWorldPosition),
  };
  rendererBackend->drawCube(renderData);
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
  CadPoint
};

struct CadEntityRange;

struct GpuPickEntity
{
  VisibilityKind kind = VisibilityKind::MeshObject;
  const MeshEntityRecord *mesh = nullptr;
  const CadEntityRange *cadRange = nullptr;

  bool operator==(const GpuPickEntity &other) const
  {
    return kind == other.kind && mesh == other.mesh &&
           cadRange == other.cadRange;
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

struct GpuPickFocusState
{
  double ndcX = 0.0;
  double ndcY = 0.0;
  uint32_t requestToken = 0;
  uint32_t pendingFrames = 0;
  bool pendingNdc = false;
  bool waitingResult = false;
};

GpuPickFocusState gpuPickFocus;

static bool gpuPickFocusWaiting()
{
  return gpuPickEnabled() && gpuPickFocus.waitingResult;
}

glm::vec4 meshEntityColor(const MeshEntityRecord &entity)
{
  return entity.entity.common.color;
}

bool meshEntityVisible(const MeshEntityRecord &entity)
{
  return entity.entity.common.visible && entity.entity.common.color.a > 0.0f;
}

static void queueGpuMeshEntity(const MeshEntityRecord &entity,
                               uint32_t objectId)
{
  if (!rendererBackend || !gpuPickFocusWaiting() ||
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

struct CadEntityRange
{
  std::string name;
  size_t begin = 0;
  size_t count = 0;
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
  std::vector<MeshEntityRecord> meshes;
  std::vector<CadEntityRange> strokeRanges;
  std::vector<CadEntityRange> fillRanges;
  std::vector<CadEntityRange> pointRanges;
};

template <typename EntityType>
void appendVectorPrimitive(const EntityType &entity, const char *name,
                           const entities::TesselationOptions &options,
                           VectorPrimitivesTessellation &target,
                           bool fillIs3DFace = false)
{
  auto addRange = [name](std::vector<CadEntityRange> &ranges,
                         size_t begin, size_t end) {
    if (end != begin)
      ranges.push_back({name, begin, end - begin});
  };
  const size_t strokeBegin = target.geometry.strokes.size();
  const size_t fillBegin = target.geometry.fills.size();
  const size_t pointBegin = target.geometry.points.size();
  entities::tessellate(entity, target.geometry, options);
  for (size_t i = strokeBegin; i < target.geometry.strokes.size(); ++i)
  {
    entities::Stroke &stroke = target.geometry.strokes[i];
    stroke.common = entity.common;
    stroke.lineWeight = entity.common.lineWeight;
  }
  for (size_t i = fillBegin; i < target.geometry.fills.size(); ++i)
  {
    entities::Triangle &fill = target.geometry.fills[i];
    fill.common = entity.common;
    fill.is3DFace = fillIs3DFace;
  }
  for (size_t i = pointBegin; i < target.geometry.points.size(); ++i)
  {
    entities::TessellatedPoint &point = target.geometry.points[i];
    point.common = entity.common;
    point.pointSize = entity.common.lineWeight > 0.0
                          ? entity.common.lineWeight
                          : 7.0;
  }
  addRange(target.strokeRanges, strokeBegin,
           target.geometry.strokes.size());
  addRange(target.fillRanges, fillBegin, target.geometry.fills.size());
  addRange(target.pointRanges, pointBegin, target.geometry.points.size());
}

// The CAD vector demo is authored as formal entities.  One immutable
// tessellation is shared by drawing and CPU picking.
const VectorPrimitivesTessellation &getVectorPrimitivesTessellation()
{
  static const VectorPrimitivesTessellation tessellation = [] {
    VectorPrimitivesTessellation target;
    entities::TessellatedEntity &result = target.geometry;
    if (!cadEntityDemoEnabled())
      return target;

    const glm::dvec3 cadAnchor =
        vectorPrimitivesAnchor() + glm::dvec3(1536.0, -1280.0, 0.0);
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
    lwpolyline.elevation = 0.0;
    lwpolyline.closed = true;
    for (glm::dvec2 &vertex : lwpolyline.vertices)
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

    entities::MLine mline;
    mline.common.color = glm::vec4(0.85f, 0.35f, 0.35f, 0.95f);
    mline.vertices = {
        cadAnchor + glm::dvec3(-256.0, -768.0, 0.0),
        cadAnchor + glm::dvec3(256.0, -704.0, 0.0),
        cadAnchor + glm::dvec3(768.0, -832.0, 0.0)};
    mline.scale = glm::dvec3(24.0, 1.0, 1.0);
    appendVectorPrimitive(mline, "MLine", options, target);

    entities::Point cadPoint;
    cadPoint.common.color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    cadPoint.location = cadAnchor + glm::dvec3(512.0, 0.0, 0.0);
    appendVectorPrimitive(cadPoint, "Point", options, target);

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
      const glm::dvec3 dir =
          glm::normalize(dashedArrow.end - dashedArrow.start);
      const glm::dvec3 side =
          glm::normalize(glm::cross(dir, glm::dvec3(0.0, 0.0, 1.0))) * 24.0;
      const glm::dvec3 base = dashedArrow.end - dir * 48.0;
      entities::Solid arrowHead;
      arrowHead.common = dashedArrow.common;
      arrowHead.firstCorner = dashedArrow.end;
      arrowHead.secondCorner = base - side;
      arrowHead.thirdCorner = base + side;
      arrowHead.fourthCorner = base + side;
      appendVectorPrimitive(arrowHead, "ArrowHead", options, target);
    }

    {
      constexpr int surfaceSegs = 24;
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
      appendVectorPrimitive(surface, "ParamSurface", options, target, true);

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

    {
      // Demo meshes remain formal entities, but stay out of the default CAD
      // scene.  They are available only for explicit rendering/pick debugging.
      if (!demoMeshesEnabled())
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
  }();
  return tessellation;
}

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
  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  static std::vector<rendering::PrimVertex> polyVerts;
  static std::vector<rendering::FillVertex> fillVerts;
  static std::vector<rendering::FillVertex> surfaceFillVerts;
  static std::vector<rendering::TargetPointInstance> cadPoints;
  static std::vector<bool> strokeVisible;
  static std::vector<bool> fillVisible;
  static std::vector<bool> pointVisible;
  polyVerts.clear();
  fillVerts.clear();
  surfaceFillVerts.clear();
  cadPoints.clear();

  auto appendRibbon = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                          const glm::vec4 &color, float halfWidth,
                          float u0, float u1) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const glm::vec3 side =
        glm::normalize(glm::cross(direction, camFront)) * halfWidth;
    polyVerts.push_back({ra - side, color, {u0, 0.0f}});
    polyVerts.push_back({ra + side, color, {u0, 1.0f}});
    polyVerts.push_back({rb + side, color, {u1, 1.0f}});
    polyVerts.push_back({ra - side, color, {u0, 0.0f}});
    polyVerts.push_back({rb + side, color, {u1, 1.0f}});
    polyVerts.push_back({rb - side, color, {u1, 0.0f}});
  };

  auto appendCadStroke = [&](const entities::Stroke &stroke) {
    if (!stroke.common.visible)
      return;
    const glm::vec4 color = stroke.common.color;
    const float halfWidth = stroke.lineWeight > 0.0
                                ? float(stroke.lineWeight) * 0.5f
                                : 2.0f;
    const size_t count = stroke.points.size();
    if (count < 2)
      return;

    if (stroke.common.lineType == "DASHED")
    {
      const glm::dvec3 start = stroke.points.front();
      const glm::dvec3 end = stroke.points.back();
      const glm::dvec3 direction = glm::normalize(end - start);
      const double total = glm::length(end - start);
      for (double d = 0.0; d < total; d += 144.0)
      {
        const double e = std::min(d + 96.0, total);
        if (e - d < 1.0)
          break;
        appendRibbon(glm::vec3(start + direction * d - rebase),
                     glm::vec3(start + direction * e - rebase), color,
                     halfWidth, float(d / total), float(e / total));
      }
      return;
    }

    for (const glm::dvec3 &world : stroke.points)
    {
      const glm::vec3 center = glm::vec3(world - rebase);
      for (int side = 0; side < 8; ++side)
      {
        const float a0 = side * 0.7853982f;
        const float a1 = (side + 1) * 0.7853982f;
        fillVerts.push_back({center, color});
        fillVerts.push_back({center + camRight * halfWidth * std::cos(a0) +
                                 camUp * halfWidth * std::sin(a0), color});
        fillVerts.push_back({center + camRight * halfWidth * std::cos(a1) +
                                 camUp * halfWidth * std::sin(a1), color});
      }
    }

    const size_t segmentCount = stroke.closed ? count : count - 1;
    for (size_t i = 0; i < segmentCount; ++i)
    {
      appendRibbon(glm::vec3(stroke.points[i] - rebase),
                   glm::vec3(stroke.points[(i + 1) % count] - rebase),
                   color, halfWidth, 0.0f, 1.0f);
    }
  };

  const entities::TessellatedEntity &tess = tessellation.geometry;
  static std::vector<rendering::FillVertex> gpuPickVertices;
  const bool gpuPickQueueActive =
      rendererBackend && gpuPickEnabled() && gpuPickFocus.waitingResult;
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
          gpuPickVertices.data() + first, uint32_t(count), view,
          pickProjection, logDepth, objectId);
    }
  };
  auto appendPickRibbon = [&](const glm::vec3 &ra, const glm::vec3 &rb,
                              const glm::vec4 &color, float halfWidth) {
    const glm::vec3 direction = rb - ra;
    if (glm::length(direction) < 1.0e-5f)
      return;
    const glm::vec3 side =
        glm::normalize(glm::cross(direction, camFront)) * halfWidth;
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
    gpuPickVertices.clear();
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::Triangle &triangle = tess.fills[i];
      if (!triangle.common.visible)
        continue;
      gpuPickVertices.push_back({glm::vec3(triangle.a - rebase), idColor});
      gpuPickVertices.push_back({glm::vec3(triangle.b - rebase), idColor});
      gpuPickVertices.push_back({glm::vec3(triangle.c - rebase), idColor});
    }
    queueGpuSoup(projection, objectId);
  };
  auto queueCadStroke = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const CadEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadStroke, nullptr, &range});
    const glm::vec4 idColor = encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::Stroke &stroke = tess.strokes[i];
      const size_t count = stroke.points.size();
      if (!stroke.common.visible || count < 2)
        continue;
      const float halfWidth = std::max(
          stroke.lineWeight > 0.0 ? float(stroke.lineWeight) * 0.5f : 2.0f,
          pixelSizeWorld * 1.5f);
      const size_t segmentCount =
          stroke.closed ? count : count - 1;
      for (size_t segment = 0; segment < segmentCount; ++segment)
      {
        appendPickRibbon(
            glm::vec3(stroke.points[segment] - rebase),
            glm::vec3(stroke.points[(segment + 1) % count] - rebase),
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
          float(point.pointSize) * 0.5f, pixelSizeWorld * 3.0f);
      appendPickPoint(glm::vec3(point.location - rebase), radius, idColor);
    }
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
    appendPickPoint(glm::vec3(candidate.center - rebase),
                    std::max(candidate.overlayPointSize,
                             pixelSizeWorld > 0.0f
                                 ? 3.0f * pixelSizeWorld
                                 : 3.0f),
                    idColor);
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
      appendCadStroke(stroke);
  }
  for (const entities::Triangle &triangle : tess.fills)
  {
    if (!fillVisible.empty() && !fillVisible[&triangle - tess.fills.data()])
      continue;
    if (!triangle.common.visible)
      continue;
    std::vector<rendering::FillVertex> &target =
        triangle.is3DFace ? surfaceFillVerts : fillVerts;
    target.push_back({glm::vec3(triangle.a - rebase), triangle.common.color});
    target.push_back({glm::vec3(triangle.b - rebase), triangle.common.color});
    target.push_back({glm::vec3(triangle.c - rebase), triangle.common.color});
  }
  for (const entities::TessellatedPoint &point : tess.points)
  {
    if (!pointVisible.empty() && !pointVisible[&point - tess.points.data()])
      continue;
    if (!point.common.visible)
      continue;
    cadPoints.push_back({glm::vec3(point.location - rebase),
                         glm::vec3(point.common.color),
                         float(point.pointSize)});
  }

  // A whole stroke, face, point group, or mesh below the pixel threshold is
  // represented by one stable impostor instead of submitting invisible pixels.
  for (const VisibilityCandidate *candidate : tinyCad)
  {
    if (!candidate)
      continue;
    cadPoints.push_back({
        glm::vec3(candidate->center - rebase),
        glm::vec3(candidate->overlayColor),
        candidate->overlayPointSize});
  }

  if (gpuPickQueueActive)
  {
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadFill)
        queueCadFill(*candidate);
    }
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (candidate && candidate->kind == VisibilityKind::CadStroke)
        queueCadStroke(*candidate);
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

  if (rendererBackend && !cadPoints.empty())
  {
    const rendering::TargetPointInstancesRenderData cadPointData{
      .view = view, .projection = overlayProjection, .instances = cadPoints.data(),
      .instanceCount = static_cast<uint32_t>(cadPoints.size()),
      .pointSize = 7.0f, .pixelSizeWorld = pixelSizeWorld,
      .isOrtho = useOrthoProjection() ? 1.0f : 0.0f, .logDepth = logDepth};
    rendererBackend->drawTargetPointInstances(cadPointData);
  }

  if (!surfaceFillVerts.empty()) {
    rendering::FilledTrianglesRenderData surfaceFillData;
    surfaceFillData.view = view;
    surfaceFillData.projection = projection;
    surfaceFillData.vertices = surfaceFillVerts.data();
    surfaceFillData.vertexCount = static_cast<uint32_t>(surfaceFillVerts.size());
    surfaceFillData.is3DFace = true;
    surfaceFillData.layer = envLayer("GRID_FILL_LAYER");
    surfaceFillData.logDepth = logDepth;
    rendererBackend->drawFilledTriangles(surfaceFillData);
  }

  if (!fillVerts.empty()) {
    rendering::FilledTrianglesRenderData fillData;
    fillData.view = view;
    fillData.projection = projection;
    fillData.vertices = fillVerts.data();
    fillData.vertexCount = static_cast<uint32_t>(fillVerts.size());
    fillData.layer = envLayer("GRID_FILL_LAYER");
    fillData.logDepth = logDepth;
    rendererBackend->drawFilledTriangles(fillData);
  }
  if (!polyVerts.empty()) {
    rendering::PolylineRenderData polyData;
    polyData.view = view;
    polyData.projection = overlayProjection;
    polyData.vertices = polyVerts.data();
    polyData.vertexCount = static_cast<uint32_t>(polyVerts.size());
    polyData.logDepth = logDepth;
    polyData.edgeSoftness = 2.0f;
    rendererBackend->drawPolylines(polyData);
  }

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
    drawMesh(view, projection, rebase, mesh.worldPosition,
             glm::vec3(meshEntityColor(mesh)), mesh.entity.common.color.a,
             mesh.size, mesh.mesh, logDepth);
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
                                const glm::vec4 &logDepth)
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
  rendererBackend->setRealisticLights(realisticLights);

  if (cadAlgorithmDemoEnabled())
  {
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
      const rendering::DoubleSingleVec3 objectPosition =
          rendering::encodeDoubleSingle(object->worldPosition);
      const float scale = object->size;
      const glm::vec4 color = meshEntityColor(*object);
      cadGroups[meshIndex].push_back(makeMeshInstance(
          scale, glm::vec3(color), color.a, objectPosition));
      queueGpuMeshEntity(object);
    }

    const rendering::MeshType cadMeshTypes[kCadMeshCount] = {
        rendering::MeshType::Cube, rendering::MeshType::Sphere,
        rendering::MeshType::Cone, rendering::MeshType::Torus};
    const glm::vec3 relativeCameraPos(0.0f);
    for (size_t meshIndex = 0; meshIndex < kCadMeshCount; ++meshIndex)
    {
      auto &instances = cadGroups[meshIndex];
      if (instances.empty())
        continue;
      const rendering::CadAlgorithmDemoRenderData renderData{
          .view = view,
          .projection = projection,
          .instances = instances.data(),
          .instanceCount = static_cast<uint32_t>(instances.size()),
          .mesh = cadMeshTypes[meshIndex],
            .renderMode = visualStyleManager.mode(),
          .cameraPos = relativeCameraPos,
          .lightDir = glm::vec3(0.4f, 0.8f, 0.55f),
          .baseColor = glm::vec3(1.0f, 1.0f, 1.0f),
          .metallic = 0.0f,
          .roughness = 0.35f,
          .transparency = 0.5f,
          .strokeWidth = 1.0f,
          .strokeDensity = 1.0f,
          .layer = envLayer("GRID_MESH_LAYER"),
          .logDepth = logDepth,
          .eye = rendering::encodeDoubleSingle(orbitCam.Position),
      };
      rendererBackend->drawCadAlgorithmDemo(renderData);
    }
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
        const rendering::DoubleSingleVec3 objectPosition =
            rendering::encodeDoubleSingle(object->worldPosition);
        const float scale = object->size;

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
        groupIt->instances.push_back(
            makeMeshInstance(
                scale, glm::vec3(color), color.a, objectPosition));
        queueGpuMeshEntity(object);
    }

  for (auto &bucket : buckets)
    for (const InstanceGroup &group : bucket)
    {
      if (group.instances.empty())
        continue;
        const rendering::MeshInstancesRenderData renderData{
            .view = view,
            .projection = projection,
            .instances = group.instances.data(),
            .instanceCount = static_cast<uint32_t>(group.instances.size()),
            .mesh = group.mesh,
            .opaque = group.opacity >= 1.0f,
            .layer = envLayer("GRID_MESH_LAYER"),
            .logDepth = logDepth,
            .eye = rendering::encodeDoubleSingle(orbitCam.Position),
            .diffuseTextureIndex = gMeshTextureIndex,
            .headlight = meshHeadlight(),
            .triplanarUv = meshTriplanar(),
            .realistic = group.realistic,
            .material = group.material,
        };
        rendererBackend->drawMeshInstances(renderData);
    }
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

// Immutable scene bounds are computed once; the per-frame pass only converts
// this one conservative box to camera space before doing exact object culling.
const WorldAabb &immutableObjectBounds()
{
  static const WorldAabb bounds = [] {
    WorldAabb result;
    for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
    {
      expandWorldAabb(result, object.worldPosition,
                      glm::dvec3(object.size * 0.5));
    }
    for (const LargeCoordinateObject &object : getStressObjects())
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
void fitCameraToRenderableObjects()
{
  const WorldAabb &bounds = immutableObjectBounds();
  if (!bounds.valid)
    return;

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  orbitCam.fitToBounds(bounds.min, bounds.max, aspect);

  std::cout << std::fixed << std::setprecision(3)
            << "Initial fit-all camera: center=(" << orbitCam.Target.x << ", "
            << orbitCam.Target.y << ", " << orbitCam.Target.z
            << ") distance=" << orbitCam.Distance
            << std::endl;
}

// Pressing L asks for the stress field itself, not the nearby validation
// cluster.  Fit that field's local bounds so a perspective camera does not
// inherit an orbit focus that places the requested geometry behind the eye.
void fitCameraToStressField()
{
  WorldAabb bounds;
  for (const LargeCoordinateObject &object : getStressObjects())
  {
    expandWorldAabb(bounds, object.worldPosition,
                    glm::dvec3(object.size * 0.5));
  }

  if (!bounds.valid)
  {
    const glm::dvec3 detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    orbitCam.setOrbit(detailCenter, 6000.0);
    orbitCam.fitDepthToBounds(detailCenter, detailCenter);
    return;
  }

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  orbitCam.fitToBounds(bounds.min, bounds.max, aspect);

  std::cout << "Stress-field camera: center=(" << orbitCam.Target.x << ", "
            << orbitCam.Target.y << ", " << orbitCam.Target.z
            << ") distance=" << orbitCam.Distance
            << std::endl;
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

void update()
{
  // FPS Update logic
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

// -------------------------------------------------------------------------
// FPS camera scaffolding -- built but not wired into render().
//
// fpsCam is constructed at startup, the keyboard/mouse handlers below mutate
// it, but the active scene is rendered from orbitCam via worldRebase() /
// RTE uniforms.  fpsCam.updatePhysics(deltaTime) is intentionally commented
// out in main() because nothing in the render path observes fpsCam yet.
// This is the path the README "FPS-style camera movement" line promises;
// completing it is a TODO.  Mouse handler is currently orphaned -- the
// only call site was the commented-out handleFPSMouseMovement(evt) line
// that this cleanup removed.
// -------------------------------------------------------------------------

void handleFPSMouseMovement(const SDL_Event &event)
{
  if (event.type == SDL_EVENT_MOUSE_MOTION)
  {
    float xoffset = event.motion.xrel;
    float yoffset = -event.motion.yrel; // Invert y for typical FPS
    fpsCam.processMouseMovement(xoffset, yoffset);
  }
}

void handleFPSKeyMovement(SDL_Scancode key, float deltaTime)
{
  // GLuint num;
  const bool *keystates = SDL_GetKeyboardState(NULL);

  if (keystates[SDL_SCANCODE_W])
    fpsCam.processKeyboard("FORWARD", deltaTime);
  if (keystates[SDL_SCANCODE_S])
    fpsCam.processKeyboard("BACKWARD", deltaTime);
  if (keystates[SDL_SCANCODE_A])
    fpsCam.processKeyboard("LEFT", deltaTime);
  if (keystates[SDL_SCANCODE_D])
    fpsCam.processKeyboard("RIGHT", deltaTime);
  if (keystates[SDL_SCANCODE_SPACE])
    fpsCam.jump();
}

void handleKeys(SDL_Scancode key, float deltaTime)
{
  handleFPSKeyMovement(key, deltaTime);
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

struct WorldAabb2
{
    glm::dvec3 min;
    glm::dvec3 max;
};

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

// Static median-split BVH over the immutable startup scene.  World positions
// remain double precision; the camera-space conversion happens only for a
// visited node.  This removes the per-frame O(objects) broad-phase transform
// pass while preserving exact leaf-side slab/LOD decisions.
class SceneObjectBvh
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

    void build(std::vector<const LargeCoordinateObject *> sceneObjects)
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
                 std::vector<const LargeCoordinateObject *> &output) const
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

private:
    static WorldAabb2 objectBounds(const LargeCoordinateObject &object)
    {
        const glm::dvec3 halfExtent(object.size * 0.5);
        return {object.worldPosition - halfExtent,
                object.worldPosition + halfExtent};
    }

    static WorldAabb2 mergeBounds(const WorldAabb2 &a, const WorldAabb2 &b)
    {
        return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
    }

    uint32_t buildRange(uint32_t begin, uint32_t end)
    {
        WorldAabb2 bounds = objectBounds(*objects[begin]);
        glm::dvec3 centroidSum = objectBounds(*objects[begin]).min +
                                 objectBounds(*objects[begin]).max;
        for (uint32_t i = begin + 1; i < end; ++i)
        {
            const WorldAabb2 itemBounds = objectBounds(*objects[i]);
            bounds = mergeBounds(bounds, itemBounds);
            centroidSum += itemBounds.min + itemBounds.max;
        }

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
                         [axis](const LargeCoordinateObject *lhs,
                                const LargeCoordinateObject *rhs) {
                             const glm::dvec3 lhsCentroid =
                                 lhs->worldPosition;
                             const glm::dvec3 rhsCentroid =
                                 rhs->worldPosition;
                             return lhsCentroid[axis] < rhsCentroid[axis];
                         });

        const uint32_t leftChild = buildRange(begin, middle);
        // buildRange can reallocate nodes, so refresh the parent reference.
        nodes[nodeIndex].leftChild = leftChild;
        const uint32_t rightChild = buildRange(middle, end);
        nodes[nodeIndex].rightChild = rightChild;
        nodes[nodeIndex].leaf = false;
        return nodeIndex;
    }

    std::vector<const LargeCoordinateObject *> objects;
    std::vector<Node> nodes;
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
PickRay pickRayFromNdc(double ndcX, double ndcY)
{
    PickRay ray;
    ray.origin = orbitCam.Position;

    if (useOrthoProjection())
    {
        const double halfH = orbitCam.orthoSize();
        const double aspect = (double)currentDrawableWidth() /
                               (double)currentDrawableHeight();
        const double halfW = halfH * aspect;
        ray.origin += orbitCam.Right * (ndcX * halfW) +
                       orbitCam.Up * (ndcY * halfH);
        ray.direction = glm::normalize(orbitCam.Front);
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
            orbitCam.Front +
            orbitCam.Right * (ndcX * tanHalfH) +
            orbitCam.Up * (ndcY * tanHalfV));
    }
    return ray;
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
    if (exit < std::max(enter, 0.0))
        return false;

    hitDepth = std::max(enter, 0.0);
    return true;
}

// Closest approach between a normalized picking ray and a finite segment.
// Keeping every intermediate value in double avoids false hits/misses in the
// large-coordinate CAD demo.
bool rayIntersectsSegment(const PickRay &ray,
                          const glm::dvec3 &start,
                          const glm::dvec3 &end,
                          double tolerance,
                          double &hitDepth)
{
    const glm::dvec3 segment = end - start;
    const double segmentLength2 = glm::dot(segment, segment);
    if (segmentLength2 < 1.0e-24)
        return false;

    const glm::dvec3 originToStart = ray.origin - start;
    const double uu = glm::dot(ray.direction, ray.direction);
    const double uv = glm::dot(ray.direction, segment);
    const double vv = segmentLength2;
    const double wd = glm::dot(originToStart, ray.direction);
    const double we = glm::dot(originToStart, segment);
    const double denominator = uu * vv - uv * uv;

    double segmentParameter;
    if (denominator > std::max(1.0e-24, vv * 1.0e-14))
        segmentParameter = (uu * we - uv * wd) / denominator;
    else
        segmentParameter = we / vv;
    segmentParameter = glm::clamp(segmentParameter, 0.0, 1.0);

    double rayParameter = (uv * segmentParameter - wd) / uu;
    rayParameter = std::max(rayParameter, 0.0);
    const glm::dvec3 rayPoint = ray.origin + ray.direction * rayParameter;
    const glm::dvec3 segmentPoint = start + segment * segmentParameter;
    const double distance = glm::distance(rayPoint, segmentPoint);
    if (distance > tolerance)
        return false;

    hitDepth = rayParameter;
    return true;
}

bool rayIntersectsTriangle(const PickRay &ray,
                           const glm::dvec3 &a,
                           const glm::dvec3 &b,
                           const glm::dvec3 &c,
                           double &hitDepth)
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
    if (depth <= 0.0)
        return false;

    hitDepth = depth;
    return true;
}

bool rayIntersectsPoint(const PickRay &ray,
                        const glm::dvec3 &location,
                        double tolerance,
                        double &hitDepth)
{
    const double depth = glm::dot(location - ray.origin, ray.direction);
    if (depth <= 0.0)
        return false;
    const glm::dvec3 rayPoint = ray.origin + ray.direction * depth;
    if (glm::distance(rayPoint, location) > tolerance)
        return false;

    hitDepth = depth;
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

    const glm::dvec3 origin = (ray.origin - object.worldPosition) / scale;
    const glm::dvec3 direction = ray.direction / directionLength;
    double best = std::numeric_limits<double>::infinity();
    const PickRay localRay{origin, direction};
    const std::vector<float> &soup =
        rendering::proceduralMeshVertices(object.mesh);
    constexpr size_t kVertexFloats =
        rendering::kProceduralMeshFloatStride;

    for (size_t vertex = 0; vertex + 2 < soup.size() / kVertexFloats;
         vertex += 3)
    {
        const auto position = [&](size_t index) {
            const size_t first = index * kVertexFloats;
            return glm::dvec3(soup[first], soup[first + 1], soup[first + 2]);
        };

        double depth = 0.0;
        if (rayIntersectsTriangle(localRay, position(vertex),
                                  position(vertex + 1),
                                  position(vertex + 2), depth) &&
            depth > 0.0 && depth < best)
        {
            best = depth;
            if (faceIndex)
                *faceIndex = vertex / 3;
        }
    }

    if (!std::isfinite(best) || best <= 0.0)
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
    return worldPerPixel * 3.0;
}

double cadStrokePickTolerance(const PickRay &ray,
                              const entities::Stroke &stroke,
                              const glm::dvec3 &worldPoint)
{
    const double renderedHalfWidth =
        stroke.lineWeight > 0.0 ? stroke.lineWeight * 0.5 : 2.0;
    return std::max(cadPickTolerance(ray, worldPoint), renderedHalfWidth);
}

double cadPointPickTolerance(const PickRay &ray,
                             const entities::TessellatedPoint &point)
{
    return std::max(cadPickTolerance(ray, point.location),
                    point.pointSize * 0.5);
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
    size_t faceIndex = 0;
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
                                     size_t faceIndex) {
        if (hitDepth < nearestMeshDepth)
        {
            nearestMeshDepth = hitDepth;
            trace.meshHit = true;
            trace.meshDepth = hitDepth;
            trace.meshName = name;
        }
        considerHit(hitDepth, name, faceIndex);
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
            considerMeshHit(hitDepth, object->displayName(), faceIndex);
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
            considerMeshHit(hitDepth, centerCube.displayName(), faceIndex);
        }
    }

    // CAD vector primitives are CPU-tessellated for drawing; test that same
    // geometry so lines, curves, fills, and points participate in autofocus.
    if (cadEntityDemoEnabled())
    {
        const VectorPrimitivesTessellation &cad =
            getVectorPrimitivesTessellation();
        const auto cadState = [&](VisibilityKind kind,
                                  const CadEntityRange &range) {
            return pickVisibility.classify(
                makeCadRangeCandidate(cad, range, kind));
        };
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
        for (const CadEntityRange &range : cad.strokeRanges)
        {
            if (!range.count || cadState(VisibilityKind::CadStroke, range) ==
                                    VisibilityState::Offscreen)
                continue;

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
                    const glm::dvec3 midpoint =
                        (stroke.points[i] + stroke.points[next]) * 0.5;
                    if (rayIntersectsSegment(ray, stroke.points[i],
                                             stroke.points[next],
                                             cadStrokePickTolerance(
                                                 ray, stroke, midpoint),
                                             hitDepth))
                    {
                        considerCadOverlayHit(
                            hitDepth, range.name.c_str(),
                            VisibilityKind::CadStroke);
                    }
                }
            }
        }

        for (const CadEntityRange &range : cad.fillRanges)
        {
            if (!range.count || cadState(VisibilityKind::CadFill, range) ==
                                    VisibilityState::Offscreen)
                continue;

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
                considerCadSurfaceHit(hitDepth, mesh.displayName().c_str(),
                                      faceIndex);
            }
        }

        for (const CadEntityRange &range : cad.pointRanges)
        {
            if (!range.count || cadState(VisibilityKind::CadPoint, range) ==
                                    VisibilityState::Offscreen)
                continue;

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
        const double overlayDepthTolerance =
            cadPickTolerance(ray, overlayPivot);

        // Screen-space overlays win ties and small depth differences, which
        // keeps curve strokes pickable where they lie on a surface.  Do not
        // let a tolerant stroke hit replace a visibly closer solid fill such
        // as an arrowhead sharing an endpoint with its leader line.
        if (!std::isfinite(nearestCadSurfaceDepth) ||
            trace.cadOverlayDepth <=
                nearestCadSurfaceDepth + overlayDepthTolerance)
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

// Double-click autofocus: focus at the view depth of the nearest object under
// the cursor while keeping the eye fixed.  The orbit target remains on the
// camera's center axis; orthographic zoom is compensated to preserve framing.
std::optional<AutofocusResult> autofocusAtNdc(double ndcX, double ndcY)
{
    const PickRay ray = pickRayFromNdc(ndcX, ndcY);
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
    orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
    return AutofocusResult{result.objectName, result.pivot, viewDepth};
}

// The GPU pass only identifies an entity.  Refine against its CPU geometry so
// every hit point still comes from the same exact primitive tests used by the
// fallback picker.
std::optional<AutofocusResult> autofocusGpuPick(uint32_t objectId,
                                                double ndcX, double ndcY)
{
    const PickRay ray = pickRayFromNdc(ndcX, ndcY);
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
        orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
        return AutofocusResult{meshEntity->displayName(), hitPivot,
                               viewDepth, faceIndex};
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
        const size_t end = std::min(cad.geometry.strokes.size(),
                                    range.begin + range.count);
        for (size_t strokeIndex = range.begin; strokeIndex < end;
             ++strokeIndex)
        {
            const entities::Stroke &stroke = cad.geometry.strokes[strokeIndex];
            const size_t pointCount = stroke.points.size();
            if (!stroke.common.visible || pointCount < 2)
                continue;

            const size_t segmentCount =
                stroke.closed ? pointCount : pointCount - 1;
            for (size_t segment = 0; segment < segmentCount; ++segment)
            {
                const size_t next = (segment + 1) % pointCount;
                const glm::dvec3 midpoint =
                    (stroke.points[segment] + stroke.points[next]) * 0.5;
                double depth = 0.0;
                if (rayIntersectsSegment(ray, stroke.points[segment],
                                         stroke.points[next],
                                         cadStrokePickTolerance(ray, stroke,
                                                                midpoint),
                                         depth))
                {
                    considerPrimitive(depth, strokeIndex);
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
              << " face=" << selected.faceIndex << std::endl;
}

static void reportGpuPickFallback(double ndcX, double ndcY)
{
  if (const std::optional<AutofocusResult> selectedEntity =
          autofocusAtNdc(ndcX, ndcY))
    reportAutofocus(*selectedEntity);
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

// Convert SDL window coordinates to NDC [-1, 1].
double cursorToNdcX(double windowX, int windowWidth)
{
    return windowWidth > 0 ? 2.0 * windowX / windowWidth - 1.0 : 0.0;
}
double cursorToNdcY(double windowY, int windowHeight)
{
    return windowHeight > 0 ? 1.0 - 2.0 * windowY / windowHeight : 0.0;
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
      candidate.overlayColor = stroke.common.color;
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
      candidate.overlayColor = fill.common.color;
    }
  }
  else
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const entities::TessellatedPoint &point =
          tessellation.geometry.points[i];
      include(point.location);
      candidate.overlayColor = point.common.color;
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

static const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates()
{
  static const std::vector<VisibilityCandidate> candidates = [] {
    const VectorPrimitivesTessellation &cad =
        getVectorPrimitivesTessellation();
    std::vector<VisibilityCandidate> result;
    result.reserve(cad.strokeRanges.size() + cad.fillRanges.size() +
                   cad.pointRanges.size());
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

void render()
{
  if (!rendererBackend)
    return;

  if (!rendererBackend->beginFrame(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)))
    return;

  if (gpuPickEnabled() && gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickResult gpuResult = rendererBackend->pollGpuPick();
    const bool resultMatches = gpuResult.ready &&
        gpuResult.requestToken == gpuPickFocus.requestToken;
    ++gpuPickFocus.pendingFrames;
    // bgfx readTexture() becomes readable two frames after submission.  Keep
    // one extra frame for driver latency, then always answer the double click.
    if (resultMatches || gpuPickFocus.pendingFrames >= 4)
    {
      gpuPickFocus.waitingResult = false;
      gpuPickFocus.pendingFrames = 0;
      std::optional<AutofocusResult> selectedEntity;
      if (resultMatches)
      {
        selectedEntity = autofocusGpuPick(gpuResult.objectId,
                                          gpuPickFocus.ndcX,
                                          gpuPickFocus.ndcY);
      }
      if (!selectedEntity)
      {
        reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
      }
      else
      {
        reportAutofocus(*selectedEntity);
      }
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

    auto includeObjectDepth = [&](const LargeCoordinateObject &object) {
      if (!meshEntityVisible(object))
        return;
      const double halfSize = (double)object.size * 0.5;
      const CameraSpaceAabb bounds = cameraAabbBounds(
          object.worldPosition, glm::dvec3(halfSize), cameraPos, right,
          up, front);
      if (aabbIntersectsOrthoViewport(bounds, halfW, halfH))
      {
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
        slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
        slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        centerCubeInFrame = true;
      }
      else if (candidate.kind == VisibilityKind::CadMesh ||
               candidate.kind == VisibilityKind::CadFill)
      {
        const CameraSpaceAabb bounds = orthoVisibility.cameraAabb(candidate);
        slabMinDepth = std::min(slabMinDepth, bounds.minDepth);
        slabMaxDepth = std::max(slabMaxDepth, bounds.maxDepth);
        visibleCadDraws.push_back(&candidate);
      }
      else
      {
        visibleCadDraws.push_back(&candidate);
      }
    }

    // OpenCADStudio centers the ortho slab on distance, not on a dynamic
    // content midpoint.  Like its ortho_depth_range(), CAD near/far follow the
    // model AABB plus a screen-rotation allowance.  The infinite grid and the
    // demo origin line are intentionally excluded: at grazing angles their
    // horizon depths are effectively unbounded and destroy depth precision.
    const double cameraDistance =
        glm::length(orbitCam.Position - orbitCam.Target);
    const double slabCenterDepth = std::max(0.001, cameraDistance);
    double visibleDepthRadius =
        (slabMaxDepth - slabMinDepth) * 0.5;
    constexpr double kMinDepthSpan = 1024.0;
    const double frameRadius = std::max(
        {visibleDepthRadius, imageRadius, kMinDepthSpan * 0.5,
         orbitCam.orthoSize() * 3.0});

    // The stored model bounds are a conservative fit-all fallback, not a
    // per-frame visibility request.  A previous scene can leave them millions
    // of units away from the active target; cap that historical contribution
    // by the currently visible slab so the stabilizer can actually converge.
    // Rotation changes still get headroom, and genuinely large visible content
    // raises frameRadius before this ceiling is applied.
    constexpr double kModelDepthRadiusHeadroom = 4.0;
    const double modelDepthRadiusCeiling =
        std::max(kMinDepthSpan, frameRadius * kModelDepthRadiusHeadroom);
    const double modelDepthRadius =
        std::min(orbitCam.orthoDepthRadius(), modelDepthRadiusCeiling);
    const double slabRadius = std::max(frameRadius, modelDepthRadius);
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

  logSlabIfChanged(useOrthoProjection(), activeNear, activeFar,
                   overlayNear, overlayFar);

  if (gpuPickEnabled() && gpuPickFocus.pendingNdc)
  {
    const rendering::GpuPickRequest request{
        .view = viewRte,
        .projection = projection,
        .eye = rendering::encodeDoubleSingle(orbitCam.Position),
        .ndcX = gpuPickFocus.ndcX,
        .ndcY = gpuPickFocus.ndcY,
        .nearDepth = activeNear,
        .farDepth = activeFar,
        .logDepth = logDepth,
    };
    const uint32_t requestToken = rendererBackend->requestGpuPick(request);
    if (requestToken != 0)
    {
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
  // Opaque geometry first so transparent passes can depth-test against it.
  const MeshEntityRecord centerCube = getCenterCubeEntity();
  if (centerCubeInFrame)
  {
    drawMesh(viewRte, projection, orbitCam.Position, centerCube.worldPosition,
             glm::vec3(meshEntityColor(centerCube)),
             centerCube.entity.common.color.a,
             centerCube.size, centerCube.mesh, logDepth);
    queueGpuMeshEntity(centerCube, kGpuPickCenterCubeId);
  }

  if (gridPlaneVisible)
    rendererBackend->drawGrid(gridRenderData);

  // The logical line still runs through the literal world origin.  Only its
  // frustum-clipped portion is submitted, so both GPU endpoints remain small
  // after rebase even when the full segment spans 1e7 world units.
  if (referenceLineVisible)
    drawWorldLine(overlayProjection, cameraPos, cameraRight, cameraUp, frontVec,
                  referenceLineStart, referenceLineEnd,
                  glm::vec3(0.15f, 1.0f, 0.25f), 0.9f, logDepth);

  // Translucent meshes remain sorted far-to-near. They depth-test against
  // opaque geometry but must not overwrite the shared depth buffer.
  drawLargeCoordinateObjects(viewRte, projection, orbitCam.Position, drawOrder, logDepth);
  drawVectorPrimitivesDemo(viewRte, projection, overlayProjection,
                           orbitCam.Position, logDepth,
                           cameraPos, frontVec, cameraRight, cameraUp,
                           pixelSize, visibleCadDraws, tinyCadDraws);

  // Below the mesh LOD threshold, emit stable center-point impostors.  The
  // renderer projects and batches all points into one GPU submission per
  // transient-buffer chunk.
  static std::vector<rendering::TargetPointInstance> pointInstances;
  pointInstances.clear();
  pointInstances.reserve(tinyDraws.size());
  for (const LargeCoordinateObject *object : tinyDraws)
  {
    pointInstances.push_back({
        glm::vec3(object->worldPosition - orbitCam.Position),
        glm::vec3(meshEntityColor(*object))});
  }
  if (!pointInstances.empty())
  {
    const rendering::TargetPointInstancesRenderData pointRenderData{
        .view = viewRte,
        .projection = overlayProjection,
        .instances = pointInstances.data(),
        .instanceCount = static_cast<uint32_t>(pointInstances.size()),
        .pointSize = 2.0f,
        .pixelSizeWorld = pixelSize,
        .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
        .logDepth = logDepth,
    };
    rendererBackend->drawTargetPointInstances(pointRenderData);
  }

  // Small 5-pixel "sphere" (disc-shaded point) at the orbit target so the
  // camera's focus point is always visible.  Uses the same RTE rebase as
  // every other draw call.
  // The camera focus marker remains a camera overlay, not a CAD entity.
  drawTargetPoint(viewRte, projection, orbitCam.Position, orbitCam.Target,
                  logDepth, pixelSize);

  if (frustumWireframeVisible)
  {
    constexpr int nearEdges[4][2] = {{0,1},{1,2},{2,3},{3,0}};
    constexpr int farEdges[4][2] = {{4,5},{5,6},{6,7},{7,4}};
    constexpr int sideEdges[4][2] = {{0,4},{1,5},{2,6},{3,7}};
    const glm::vec3 nearColor(1.0f, 0.2f, 0.2f);
    const glm::vec3 farColor(0.2f, 0.4f, 1.0f);
    const glm::vec3 sideColor(1.0f, 1.0f, 1.0f);
    for (const auto &e : nearEdges)
    drawWorldLine(overlayProjection, cameraPos, cameraRight, cameraUp, frontVec,
                    frustumCorners[e[0]], frustumCorners[e[1]],
                    nearColor, 0.9f, logDepth);
    for (const auto &e : farEdges)
      drawWorldLine(overlayProjection, cameraPos, cameraRight, cameraUp, frontVec,
                    frustumCorners[e[0]], frustumCorners[e[1]],
                    farColor, 0.9f, logDepth);
    for (const auto &e : sideEdges)
      drawWorldLine(overlayProjection, cameraPos, cameraRight, cameraUp, frontVec,
                    frustumCorners[e[0]], frustumCorners[e[1]],
                    sideColor, 0.9f, logDepth);
    if (gridVisibleQuadValid)
    {
      const glm::vec3 gridQuadColor(1.0f, 0.85f, 0.1f);
      for (int i = 0; i < gridVisibleQuadCount; ++i)
        drawWorldLine(overlayProjection, cameraPos, cameraRight, cameraUp,
                      frontVec,
                      gridVisibleQuad[i],
                      gridVisibleQuad[(i + 1) % gridVisibleQuadCount],
                      gridQuadColor, 0.9f, logDepth);
    }
  }

  logCameraStateIfChanged(orbitCam.Target, activeNear, activeFar,
                          useOrthoProjection());

  rendererBackend->endFrame();
}

int main(int argc, char *argv[])
{
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

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
  bool middleMouseDrag = false;
  bool testPanApplied = false;
  bool originOrthoScenarioApplied = false;


  while (running)
  {
    float currentFrame = SDL_GetTicks() / 1000.0f;
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    while (SDL_PollEvent(&evt))
    {
      if (evt.type == SDL_EVENT_QUIT)
      {
        running = false;
      }
      if (evt.type == SDL_EVENT_KEY_DOWN)
      {
        handleKeys(evt.key.scancode, deltaTime);
        if (evt.key.key == SDLK_ESCAPE)
        {
          running = false;
        }
        if (evt.key.key == SDLK_P)
        {
          switchProjectionMode();
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
          visualStyleManager.cycle();
          if (rendererBackend)
            rendererBackend->setRenderMode(visualStyleManager.mode());
          std::cout << "Visual style: "
                    << rendering::renderModeLabel(visualStyleManager.mode())
                    << " [" << rendering::renderModeCommand(visualStyleManager.mode())
                    << "]" << std::endl;
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
        const double ndcX = cursorToNdcX(evt.button.x, winW);
        const double ndcY = cursorToNdcY(evt.button.y, winH);
        if (gpuPickEnabled())
        {
          gpuPickFocus.ndcX = ndcX;
          gpuPickFocus.ndcY = ndcY;
          gpuPickFocus.pendingNdc = true;
          gpuPickFocus.pendingFrames = 0;
          gpuPickFocus.waitingResult = false;
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

    rendererBackend->present();
  }

  close();

  return 0;
}
