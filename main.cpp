#include "main.h"
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <limits>
#include <chrono>

namespace
{

rendering::BackendType resolveRequestedBackend()
{
  const char *backend = std::getenv("WINDOW_RENDERER");
  if (!backend)
    return rendering::BackendType::Bgfx;

  const std::string_view backendName(backend);
  if (backendName == "bgfx")
    return rendering::BackendType::Bgfx;

  std::cerr << "Unknown WINDOW_RENDERER value '" << backendName
            << "', falling back to bgfx." << std::endl;
  return rendering::BackendType::Bgfx;
}

std::unique_ptr<rendering::RendererBackend> rendererBackend;
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

GridPlaneType gridPlane = GridPlaneType::XZ;
glm::dvec3 gridPlaneOrigin(0.0);
glm::dvec3 gridPlaneNormal(0.0, 1.0, 0.0);
glm::dvec3 gridPlaneStartAxisOrigin(0.0);
glm::dvec3 gridPlaneStartAxisDirection(1.0, 0.0, 0.0);

// Keep the CPU visibility test, slab construction, and grid shader discard
// aligned.  A plane within five degrees of the view direction is hidden.
constexpr double kMinGridPlaneCos = 0.087155743; // sin(5 degrees)

// Edit these to define a non-axis-aligned infinite grid.  Key 4 activates it.
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
// Default magnitudes stay at 1e5 to keep the rebased coordinate frame well
// inside float32 precision (ULP ≈ 8 mm at this scale).  Bump both
// components to ~1e9 when you want a true precision-stress demonstration;
// the four "micro X" cubes will start to collapse onto 0 / 512-unit buckets
// if the rebase layer is ever bypassed, exactly as the reference project's
// README describes.
const glm::dvec3 LARGE_COORDINATE_BASE_POINT(1e7, 0.0, 1e7);
const glm::dvec3 LARGE_COORDINATE_DETAIL_OFFSET(1536.0, 0.0, -1024.0);

OrbitCamera orbitCam(
    glm::vec3(0.0f), // Target is origin
    15.0f,           // Radius distance from target
    -45.0f,          // Yaw
    20.0f            // Pitch
);

// Project the orbit focus onto the active grid plane. Translating the eye by
// the same plane correction preserves the view direction and orbit distance;
// ordinary orbiting may then still place the eye off the plane.
void enforceTargetPlaneConstraint()
{
    const glm::dvec3 planeNormal = glm::normalize(gridPlaneNormal);
    const double targetOnNormal =
        glm::dot(orbitCam.Target - gridPlaneOrigin, planeNormal);
    const glm::dvec3 correction = planeNormal * -targetOnNormal;
    orbitCam.Target += correction;
    orbitCam.Position += correction;
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

  rendererBackend = rendering::createRenderer(resolveRequestedBackend());

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
      window, "grid plane - XZ  [1]XY [2]XZ [3]YZ [4]CUSTOM");

  if (!rendererBackend->initialize(window))
  {
    std::cerr << "Failed to initialize render backend: "
              << rendererBackend->name() << std::endl;
    close();
    return false;
  }

  std::cout << "Renderer backend: " << rendererBackend->name() << std::endl;
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
                     const glm::vec4 &logDepth)
{
  if (!rendererBackend)
    return;

  const rendering::TargetPointRenderData renderData{
      .view = view,
      .projection = projection,
      .relativePosition = glm::vec3(targetWorldPosition - rebaseOrigin),
      .pointSize = 5.0f,
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

void drawAabbForCube(const rendering::CubeRenderData &renderData)
{
  if (!rendererBackend)
    return;

  // The renderer treats the model translation separately from the rebase
  // position, so the AABB must do the same.
  glm::mat4 modelNoTranslation = renderData.model;
  modelNoTranslation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  glm::vec3 relativeMin(std::numeric_limits<float>::max());
  glm::vec3 relativeMax(std::numeric_limits<float>::lowest());
  for (const float x : {-0.5f, 0.5f})
  {
    for (const float y : {-0.5f, 0.5f})
    {
      for (const float z : {-0.5f, 0.5f})
      {
        const glm::vec3 relative = glm::vec3(
            modelNoTranslation * glm::vec4(x, y, z, 1.0f)) +
            renderData.modelRelativePosition;
        relativeMin = glm::min(relativeMin, relative);
        relativeMax = glm::max(relativeMax, relative);
      }
    }
  }

  const rendering::AabbRenderData aabb{
      .view = renderData.view,
      .projection = renderData.projection,
      .relativeMin = relativeMin,
      .relativeMax = relativeMax,
      .color = glm::vec3(1.0f, 0.90f, 0.15f),
      .opacity = 1.0f,
      .logDepth = renderData.logDepth,
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
  };
  rendererBackend->drawCube(renderData);
}

struct LargeCoordinateObject
{
  glm::dvec3 worldPosition;
  glm::vec3 color;
  float size;
  rendering::MeshType mesh = rendering::MeshType::Cube;
};

const std::vector<LargeCoordinateObject> &getLargeCoordinateObjects()
{
  static const std::vector<LargeCoordinateObject> objects = [] {
    const glm::dvec3 detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    std::vector<LargeCoordinateObject> objects{
        {LARGE_COORDINATE_BASE_POINT + glm::dvec3(0.0, 256.0, 0.0),
         glm::vec3(0.43f, 0.91f, 0.98f), 512.0f},
        {detailCenter + glm::dvec3(0.0, 224.0, 0.0),
         glm::vec3(1.0f, 0.58f, 0.25f), 448.0f},
        {detailCenter + glm::dvec3(1920.0, 64.0, 0.0),
         glm::vec3(0.95f, 0.95f, 0.95f), 128.0f},
        {detailCenter + glm::dvec3(0.0, 64.0, -1920.0),
         glm::vec3(0.95f, 0.95f, 0.95f), 128.0f},
        {detailCenter + glm::dvec3(-288.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f},
        {detailCenter + glm::dvec3(-96.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f},
        {detailCenter + glm::dvec3(96.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f},
        {detailCenter + glm::dvec3(288.0, 32.0, -224.0),
         glm::vec3(1.0f, 0.55f, 0.41f), 64.0f},
    };
    return objects;
  }();
  return objects;
}

int stressObjectCount();
const std::vector<LargeCoordinateObject> &getStressObjects();

void drawLargeCoordinateObjects(const glm::mat4 &view,
                                const glm::mat4 &projection,
                                const glm::dvec3 &rebaseOrigin,
                                std::vector<const LargeCoordinateObject *> &drawOrder,
                                const glm::vec4 &logDepth)
{
  const glm::dvec3 cameraPos(orbitCam.Position);
  const glm::dvec3 cameraFront(orbitCam.Front);
  std::sort(drawOrder.begin(), drawOrder.end(),
      [&](const LargeCoordinateObject *lhs,
          const LargeCoordinateObject *rhs) {
        const double lhsDepth =
            glm::dot(lhs->worldPosition - cameraPos, cameraFront);
        const double rhsDepth =
            glm::dot(rhs->worldPosition - cameraPos, cameraFront);
        return lhsDepth > rhsDepth;
      });

  for (const LargeCoordinateObject *object : drawOrder)
  {
    drawMesh(view, projection, rebaseOrigin, object->worldPosition,
             object->color, 0.45f, object->size, object->mesh, logDepth);
  }
}


int stressObjectCount()
{
  static const int count = [] {
    int value = 1000;
    if (const char *env = std::getenv("GRID_STRESS_COUNT"))
    {
      const long parsed = std::strtol(env, nullptr, 10);
      if (parsed >= 0)
        value = static_cast<int>(std::min<long>(parsed, 50000));
    }
    return value;
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
      object.color = kPalette[i % 6];
      object.size = static_cast<float>(kObjectSize);
      object.mesh = kMeshCycle[i % 4];
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

  const glm::dvec3 center = (bounds.min + bounds.max) * 0.5;
  const glm::dvec3 halfExtent = (bounds.max - bounds.min) * 0.5;
  const double boundingRadius = std::max(1.0, glm::length(halfExtent));

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  // glm::perspective() uses a 45-degree vertical FOV.  Fit the bounding
  // sphere against the narrower of the vertical/horizontal FOVs.
  const double tanHalfVertical = std::tan(glm::radians(45.0) * 0.5);
  const double halfFov = std::min(
      glm::radians(45.0) * 0.5,
      std::atan(tanHalfVertical * aspect));
  const double distance =
      std::nextafter(boundingRadius / std::sin(halfFov),
                     std::numeric_limits<double>::infinity()) * 1.04;

  orbitCam.setOrbit(center, distance);
  if (useOrthoProjection())
    orthoHalfHeight() = static_cast<float>(boundingRadius * 1.04);

  std::cout << std::fixed << std::setprecision(3)
            << "Initial fit-all camera: center=(" << center.x << ", "
            << center.y << ", " << center.z << ") distance=" << distance
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
    return;
  }

  const glm::dvec3 center = (bounds.min + bounds.max) * 0.5;
  const glm::dvec3 halfExtent = (bounds.max - bounds.min) * 0.5;
  const double boundingRadius = std::max(1.0, glm::length(halfExtent));

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  const double tanHalfVertical = std::tan(glm::radians(45.0) * 0.5);
  const double halfFov = std::min(
      glm::radians(45.0) * 0.5,
      std::atan(tanHalfVertical * aspect));
  const double distance =
      std::nextafter(boundingRadius / std::sin(halfFov),
                     std::numeric_limits<double>::infinity()) * 1.04;

  orbitCam.setOrbit(center, distance);
  if (useOrthoProjection())
    orthoHalfHeight() = static_cast<float>(boundingRadius * 1.04);

  std::cout << "Stress-field camera: center=(" << center.x << ", "
            << center.y << ", " << center.z << ") distance=" << distance
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

  std::cout
            << "  stress objects: " << stressObjectCount()
            << " (GRID_STRESS_COUNT, 0 disables)" << std::endl;

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

void handleOrbitMouseMovement(SDL_Event event, bool middleMouseDrag)
{
  if (event.type == SDL_EVENT_MOUSE_MOTION)
  {
    if (!middleMouseDrag)
      return;

    if (shiftKeyDown())
    {
      // Shift + middle-drag orbits the camera.
      orbitCam.processMouseMovement(event.motion.xrel,
                                    -event.motion.yrel);
      return;
    }

    // Middle-drag alone pans the view at the cursor's world scale.
    // Ortho uses its exact frustum scale; perspective uses the target-plane FOV.
    const float worldPerPixel = useOrthoProjection()
        ? (2.0f * orthoHalfHeight()) / (float)SCREEN_HEIGHT
        : (2.0f * (float)glm::length(orbitCam.Position - orbitCam.Target)
              * std::tan(glm::radians(orbitCam.Zoom * 0.5f)))
              / (float)SCREEN_HEIGHT;

    // Plane-constrained pan in both ortho and perspective: for XZ this keeps
    // target/eye altitude fixed; for XY/YZ/custom it moves both points within
    // the active plane so the camera stays consistent with the displayed grid.
    glm::dvec3 panTangentU;
    glm::dvec3 panTangentV;
    activePlaneTangents(panTangentU, panTangentV);
    orbitCam.processMousePan(event.motion.xrel, event.motion.yrel,
                             worldPerPixel, true, panTangentU, panTangentV);
  }
}

void handleOrbitZoom(SDL_Event event)
{
  if (event.type != SDL_EVENT_MOUSE_WHEEL)
    return;

  const float delta = static_cast<float>(event.wheel.y);

  if (useOrthoProjection())
  {
    // In ortho mode the wheel zooms the orthographic frustum, not the
    // camera distance.  Dividing by 10.0f gives a comfortable rate so a
    // typical notch (1.0) shrinks the view by ~10%% per click.
    float &halfH = orthoHalfHeight();
    // halfH is the ortho half-height; the wheel scales it directly.
    halfH *= (1.0f - 0.1f * delta);
    if (halfH < halfHMin)
      halfH = halfHMin;
    if (halfH > halfHMax)
      halfH = halfHMax;
    if (cameraDebugEnabled())
    {
      std::cout << "Ortho half-height: " << std::scientific
                << std::setprecision(4) << halfH << std::endl;
    }
  }
  else
  {
    orbitCam.processMouseScroll(delta);
  }
}

// Keep the world size visible at OrbitCamera::Target when switching modes.
// Ortho scale is halfHeight; perspective scale at the target plane is
// distance * tan(FOV / 2).  Do not derive this from dynamic near/far slabs.
void switchProjectionMode()
{
  constexpr double kFovDegrees = 45.0;
  const double tanHalfVertical = std::tan(glm::radians(kFovDegrees) * 0.5);
  bool &isOrtho = useOrthoProjection();

  if (isOrtho)
  {
    const double distance = orthoHalfHeight() / tanHalfVertical;
    orbitCam.setTargetDistance(distance);
    isOrtho = false;
  }
  else
  {
    const double distance = glm::length(orbitCam.Position - orbitCam.Target);
    float &halfHeight = orthoHalfHeight();
    halfHeight = glm::clamp(
        static_cast<float>(distance * tanHalfVertical), halfHMin, halfHMax);
    isOrtho = true;
  }

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

  rendererBackend->beginFrame(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));

  // ── Rebase layer ──────────────────────────────────────────────────────────
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
  glm::mat4 projection;
  glm::mat4 overlayProjection;
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
  const glm::dvec3 worldLineEnd = LARGE_COORDINATE_BASE_POINT;
  glm::dvec3 referenceLineStart = glm::dvec3(0.0);
  glm::dvec3 referenceLineEnd = worldLineEnd;
  bool referenceLineVisible = false;

  const glm::dvec3 cameraPos(orbitCam.Position);
  const glm::dvec3 frontVec(orbitCam.Front);
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
    const float halfH = orthoHalfHeight();
    const double halfW = (double)halfH * (double)aspect;
    const glm::dvec3 front(glm::dvec3(orbitCam.Front));
    const glm::dvec3 right(glm::dvec3(orbitCam.Right));
    const glm::dvec3 up(glm::dvec3(orbitCam.Up));

    // Start with the depth interval covered by the visible ortho image.
    // The scene bounds below can make near negative; that is intentional
    // because orthographic geometry can straddle the camera plane.
    // Clamping near to zero would hide the lower half of the center cube.
    const CameraSpacePoint targetCamera =
        toCameraSpace(orbitCam.Target, cameraPos, right, up, front);
    const double targetDepth = targetCamera.depth;

    const double imageRadius = std::sqrt(halfW * halfW + halfH * halfH);
    double slabRadius = imageRadius;

    auto includeObjectDepth = [&](const LargeCoordinateObject &object) {
      const double halfSize = (double)object.size * 0.5;
      const CameraSpaceAabb bounds = cameraAabbBounds(
          object.worldPosition, glm::dvec3(halfSize), cameraPos, right,
          up, front);
      if (aabbIntersectsOrthoViewport(bounds, halfW, halfH))
      {
        slabRadius = std::max(
            {slabRadius,
             std::abs(bounds.minDepth - targetDepth),
             std::abs(bounds.maxDepth - targetDepth)});
        drawOrder.push_back(&object);
      }
    };

    {
      const CameraSpaceAabb bounds = cameraAabbBounds(
          cubeWorldPosition, glm::dvec3(0.5), cameraPos, right, up, front);
      if (aabbIntersectsOrthoViewport(bounds, halfW, halfH))
      {
        slabRadius = std::max(
            {slabRadius,
             std::abs(bounds.minDepth - targetDepth),
             std::abs(bounds.maxDepth - targetDepth)});
      }
    }
    for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
      includeObjectDepth(object);
    for (const LargeCoordinateObject &object : getStressObjects())
      includeObjectDepth(object);

    // The reference line can be 1e7 units long.  Clip it to the ortho
    // viewport first so only its visible part can extend the slab.
    double segmentMinDepth = targetDepth;
    double segmentMaxDepth = targetDepth;
    includeSegmentCameraDepth(
        glm::dvec3(0.0, 0.0, 0.0), worldLineEnd,
        cameraPos, right, up, front, halfW, halfH,
        segmentMinDepth, segmentMaxDepth);
    slabRadius = std::max(
        {slabRadius,
         std::abs(segmentMinDepth - targetDepth),
         std::abs(segmentMaxDepth - targetDepth)});

    // The infinite ground plane can extend beyond the target-centered
    // viewport interval, especially at grazing angles.  Use the same
    // analytic interval that decides whether the grid plane is visible.
    if (std::abs(frontOnNormal) > kMinGridPlaneCos)
    {
      const double cameraPlaneDistance =
          glm::dot(cameraPos - gridPlaneOrigin, planeNormal);
      const double groundCenterDepth =
          -cameraPlaneDistance / frontOnNormal;
      const double groundDepthRadius =
          (std::abs(glm::dot(right, planeNormal)) * halfW +
           std::abs(glm::dot(up, planeNormal)) * halfH) /
          std::abs(frontOnNormal);
      slabRadius = std::max(
          {slabRadius,
           std::abs(groundCenterDepth - groundDepthRadius - targetDepth),
           std::abs(groundCenterDepth + groundDepthRadius - targetDepth)});
    }

    // Keep a stable minimum slab as the ortho image zooms in.  A full 128
    // world-unit span comfortably covers close-up geometry without pushing
    // ortho depth precision far enough to matter even at 1e7 coordinates.
    constexpr double kMinDepthSpan = 1024.0;
    slabRadius = std::max(slabRadius, kMinDepthSpan * 0.5);

    const double orthoNearDepth = targetDepth - slabRadius;
    const double orthoFarDepth = targetDepth + slabRadius;
    const float near = floatExpandOutward(orthoNearDepth, true);
    const float far = floatExpandOutward(orthoFarDepth, false);

    projection = glm::ortho(-halfH * aspect, halfH * aspect,
                             -halfH,            halfH, near, far);
    activeNear = near;
    activeFar  = far;

    // Analytic ortho pixel size (doc section 4.2): the vertical frustum
    // extent 2 * halfH maps onto the drawable viewport height.
    pixelSize = (2.0f * halfH) / static_cast<float>(drawableHeight);
  }
  else
  {
    const glm::dvec3 right(glm::dvec3(orbitCam.Right));
    const glm::dvec3 up(glm::dvec3(orbitCam.Up));
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

    {
      const CameraSpaceAabb cubeBounds = cameraAabbBounds(
          cubeWorldPosition, glm::dvec3(0.5), cameraPos, right, up,
          frontVec);
      provisionalNear = std::min(provisionalNear, cubeBounds.minDepth);
      provisionalFar = std::min(
          kMaxPerspectiveFar,
          std::max(provisionalFar, cubeBounds.maxDepth));
      if (aabbIntersectsPerspectiveFrustum(
              cubeBounds, provisionalNear, provisionalFar,
              tanHalfVertical, tanHalfHorizontal))
      {
        overlayMinDepth = std::min(overlayMinDepth, cubeBounds.minDepth);
        overlayMaxDepth = std::max(overlayMaxDepth, cubeBounds.maxDepth);
        objectMinDepth = std::min(objectMinDepth, cubeBounds.minDepth);
        objectMaxDepth = std::max(objectMaxDepth, cubeBounds.maxDepth);
      }
    }

    auto addVisibleObjects =
        [&](const std::vector<LargeCoordinateObject> &objects) {
          for (const LargeCoordinateObject &object : objects)
          {
            const double halfSize = (double)object.size * 0.5;
            const CameraSpaceAabb bounds = cameraAabbBounds(
                object.worldPosition, glm::dvec3(halfSize), cameraPos,
                right, up, frontVec);
            if (aabbIntersectsPerspectiveFrustum(
                    bounds, provisionalNear, provisionalFar,
                    tanHalfVertical,
                    tanHalfHorizontal))
            {
              const double nearestDepth =
                  std::max(kNearDepthFloor, bounds.minDepth);
              const double screenExtent =
                  (double)object.size * drawableHeight /
                  (2.0 * nearestDepth * tanHalfVertical);
              if (screenExtent < kMinObjectPixelExtent)
              {
                tinyDraws.push_back(&object);
                continue;
              }

              overlayMinDepth = std::min(overlayMinDepth, bounds.minDepth);
              overlayMaxDepth = std::max(overlayMaxDepth, bounds.maxDepth);
              objectMinDepth = std::min(objectMinDepth, bounds.minDepth);
              objectMaxDepth = std::max(objectMaxDepth, bounds.maxDepth);
              drawOrder.push_back(&object);
            }
          }
        };
    addVisibleObjects(getLargeCoordinateObjects());
    addVisibleObjects(getStressObjects());

    // The reference line is drawn without an AABB, so clip the segment to the
    // exact perspective side frusta.  It is a non-depth-writing overlay and
    // must not consume the depth precision reserved for solid geometry.
    includeSegmentPerspectiveDepth(
        toCameraSpace(glm::dvec3(0.0), cameraPos, right, up, frontVec),
        toCameraSpace(worldLineEnd, cameraPos, right, up, frontVec),
        kNearDepthFloor, tanHalfVertical, tanHalfHorizontal,
        overlayMinDepth, overlayMaxDepth);

    // Four corner rays bound the finite part of the visible plane.  At
    // grazing angles one ray can meet the plane behind the eye; then the
    // grid runs to the horizon and needs the conservative far cap.
    if (std::abs(frontOnNormal) >= kMinGridPlaneCos &&
        glm::dot(gridPlaneOrigin - cameraPos, planeNormal) /
            frontOnNormal > 0.0)
    {
      bool planeExtentIsFinite = true;
      for (int y = 0; y < 2; ++y)
      {
        for (int x = 0; x < 2; ++x)
        {
          const glm::dvec3 cornerDirection =
              frontVec +
              right * ((x ? tanHalfHorizontal : -tanHalfHorizontal)) +
              up * ((y ? tanHalfVertical : -tanHalfVertical));
          const double denominator =
              glm::dot(cornerDirection, planeNormal);
          if (std::abs(denominator) < 1.0e-12)
          {
            planeExtentIsFinite = false;
            continue;
          }

          const double cornerDepth =
              glm::dot(gridPlaneOrigin - cameraPos, planeNormal) /
              denominator;
          if (cornerDepth <= 0.0)
          {
            planeExtentIsFinite = false;
            continue;
          }

          overlayMinDepth = std::min(overlayMinDepth, cornerDepth);
          overlayMaxDepth = std::max(overlayMaxDepth, cornerDepth);
        }
      }

      if (!planeExtentIsFinite)
        overlayMaxDepth = std::max(overlayMaxDepth, kMaxPerspectiveFar);
    }

    // Solid geometry receives a compact depth slab.  Infinite ground and the
    // 1e7 reference line are transparent overlays; if they share this slab,
    // a close cube maps to float32 NDC depth 1.0 and is clipped by the GPU.
    const double depthMagnitude = std::max(
        {kNearDepthFloor, std::abs(objectMinDepth),
         std::abs(objectMaxDepth)});
    // A few float32 ULPs are the real precision floor because projection is
    // float; the extra world-unit keeps matrix conversion from being marginal.
    const double depthMargin =
        std::max(1.0, 8.0 * std::numeric_limits<float>::epsilon() *
                           depthMagnitude);

    const float near = floatExpandOutward(
        std::max(kNearDepthFloor, objectMinDepth - depthMargin), true);
    const float far = floatExpandOutward(std::min(
        kMaxPerspectiveFar,
        std::max(objectMaxDepth + depthMargin,
                 (double)near + kMinPerspectiveSpan)), false);

    projection = glm::perspective(glm::radians(45.0f), aspect,
                                  near, far);
    activeNear = near;
    activeFar  = far;

    const double overlayDepthMagnitude = std::max(
        {kNearDepthFloor, std::abs(overlayMinDepth),
         std::abs(overlayMaxDepth)});
    const double overlayDepthMargin =
        std::max(1.0, 8.0 * std::numeric_limits<float>::epsilon() *
                           overlayDepthMagnitude);
    overlayNear = floatExpandOutward(kNearDepthFloor, true);
    overlayFar = floatExpandOutward(std::min(
        kMaxPerspectiveFar,
        std::max(overlayMaxDepth + overlayDepthMargin,
                 (double)overlayNear + kMinPerspectiveSpan)), false);
    overlayProjection = glm::perspective(glm::radians(45.0f), aspect,
                                         overlayNear, overlayFar);

    // Rough estimate around the orbit target, only used to seed the LOD
    // step; the shader computes exact per-fragment sizes for perspective.
    pixelSize = (2.0f * (float)glm::length(orbitCam.Position - orbitCam.Target)
                  * tan(glm::radians(22.5f)))
               / static_cast<float>(drawableHeight);
  }

  if (useOrthoProjection())
  {
    overlayProjection = projection;
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

  const glm::dvec3 cameraRight(orbitCam.Right);
  const glm::dvec3 cameraUp(orbitCam.Up);
  if (useOrthoProjection())
  {
    referenceLineVisible = clipReferenceSegmentToOrtho(
        glm::dvec3(0.0), worldLineEnd, cameraPos, cameraRight, cameraUp,
        frontVec, activeNear, activeFar,
        (double)orthoHalfHeight() * (double)aspect, orthoHalfHeight(),
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

  // LOD step with hysteresis (doc section 4.5).  In orthographic mode the
  // step is frozen below an OrthoSize of 1 so extreme close-up zoom keeps a
  // stable grid size instead of continuously introducing finer cells.
  static float step = 1.0f;
  const float baseStep = 1.0f;
  const bool freezeStep = useOrthoProjection() && orthoHalfHeight() < 1.0f;
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
    const double halfH = (double)orthoHalfHeight();
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
  axisOriginGridRelative = glm::vec2(
      (float)glm::dot(-originWorld, tangentU),
      (float)glm::dot(-originWorld, tangentV));

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

  bool gridPlaneVisible = false;
  if (useOrthoProjection())
  {
    gridPlaneVisible = orthoPlaneValid;
    if (gridPlaneVisible && std::abs(frontOnNormal) > kMinGridPlaneCos)
    {
      const float halfH = orthoHalfHeight();
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
    const glm::dvec3 front = glm::normalize(glm::dvec3(orbitCam.Front));
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
        .projection = overlayProjection,
        .invViewProj = glm::inverse(overlayViewProj),
      .viewProj = overlayViewProj,
      .camFront = orbitCam.Front,
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
      .groundRelativeY = (float)(-rebase.y),
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
  drawCube(view, projection, rebase, cubeWorldPosition,
      glm::vec3(1.0f, 0.58f, 0.25f), 1.0f, 1.0f, logDepth);

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
  drawLargeCoordinateObjects(view, projection, rebase, drawOrder, logDepth);

  // Below the mesh LOD threshold, emit a stable center-point impostor so a
  // distant object remains addressable without expanding the solid depth slab.
  for (const LargeCoordinateObject *object : tinyDraws)
  {
    const rendering::TargetPointRenderData renderData{
        .view = view,
        .projection = overlayProjection,
        .relativePosition = glm::vec3(object->worldPosition - rebase),
        .pointSize = 2.0f,
        .color = object->color,
        .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
        .logDepth = logDepth,
    };
    rendererBackend->drawTargetPoint(renderData);
  }

  // Small 5-pixel "sphere" (disc-shaded point) at the orbit target so the
  // camera's focus point is always visible.  Uses the same RTE rebase as
  // every other draw call.
  drawTargetPoint(view, projection, rebase, orbitCam.Target, logDepth);

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

  if (const char *testOrtho = std::getenv("GRID_CAMERA_TEST_ORTHO");
      testOrtho && std::strcmp(testOrtho, "0") != 0)
  {
    useOrthoProjection() = true;
    if (const char *halfH = std::getenv("GRID_CAMERA_TEST_ORTHO_HALFH"))
      orthoHalfHeight() = std::max(0.01f, (float)std::atof(halfH));
    std::cout << "Projection: ORTHOGRAPHIC (test)" << std::endl;
  }

  fitCameraToRenderableObjects();

  SDL_Event evt;
  bool running = true;
  bool middleMouseDrag = false;
  bool testPanApplied = false;


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
        if (evt.key.key == SDLK_C)
          setTargetPlaneConstraint(!targetPlaneConstraintEnabled);
        if (evt.key.key == SDLK_F)
        {
          frustumCaptureRequested = true;
          std::cout << "Frustum wireframe: captured" << std::endl;
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
            SDL_SetWindowTitle(
                window, "grid plane - large-coordinate stress field");
          }
          else
          {
            cubeWorldPosition = glm::dvec3(0.0);
            orbitCam.setOrbit(cubeWorldPosition, 15.0);
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
      }
      if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP &&
          evt.button.button == SDL_BUTTON_MIDDLE)
      {
        middleMouseDrag = false;
      }
      if (evt.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
      {
        middleMouseDrag = false;
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
        const float worldPerPixel = useOrthoProjection()
            ? (2.0f * orthoHalfHeight()) / (float)SCREEN_HEIGHT
            : (2.0f * (float)glm::length(orbitCam.Position - orbitCam.Target) *
               std::tan(glm::radians(orbitCam.Zoom * 0.5f))) /
              (float)SCREEN_HEIGHT;
        glm::dvec3 panTangentU;
        glm::dvec3 panTangentV;
        activePlaneTangents(panTangentU, panTangentV);
        orbitCam.processMousePan((float)std::atof(testPanValue), 0.0f,
                                 worldPerPixel, true, panTangentU,
                                 panTangentV);
        testPanApplied = true;
      }
    }

    // A single post-input correction covers every camera mutation, including
    // orbiting, dolly/zoom, teleport shortcuts, and future input paths.
    if (targetPlaneConstraintEnabled)
      enforceTargetPlaneConstraint();

    render();

    rendererBackend->present();
  }

  close();

  return 0;
}
