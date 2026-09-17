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

bool largeCoordinateSceneEnabled = false;
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

// Edit these to define a non-axis-aligned infinite grid.  Key 4 activates it.
glm::dvec3 customGridPlaneOrigin(1.0, 0.5, -0.5);
glm::dvec3 customGridPlaneNormal =
    glm::normalize(glm::dvec3(0.25, 1.0, 0.15));
glm::dvec3 customGridPlaneStartAxisOrigin(1.0, 0.5, -0.5);
glm::dvec3 customGridPlaneStartAxisDirection =
    glm::normalize(glm::dvec3(1.0, 0.0, 0.25));

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
                     const glm::dvec3 &targetWorldPosition)
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
  };
  rendererBackend->drawAabb(aabb);
}

void drawCube(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::dvec3 &rebaseOrigin,
              const glm::dvec3 &objectWorldPosition,
              const glm::vec3 &objectColor,
              float opacity,
              float size)
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
  };
  rendererBackend->drawCube(renderData);
  drawAabbForCube(renderData);
}

void drawWorldLine(const glm::mat4 &view, const glm::mat4 &projection,
                   const glm::dvec3 &rebaseOrigin,
                   const glm::dvec3 &startWorldPosition,
                   const glm::dvec3 &endWorldPosition)
{
  if (!rendererBackend)
    return;

  const rendering::WorldLineRenderData renderData{
      .view = view,
      .projection = projection,
      .relativeStart = glm::vec3(startWorldPosition - rebaseOrigin),
      .relativeEnd = glm::vec3(endWorldPosition - rebaseOrigin),
      .lineWidth = 2.0f,
  };
  rendererBackend->drawWorldLine(renderData);
}

struct LargeCoordinateObject
{
  glm::dvec3 worldPosition;
  glm::vec3 color;
  float size;
};

std::vector<LargeCoordinateObject> getLargeCoordinateObjects()
{
  const glm::dvec3 detailCenter =
      LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
  return {
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
}

void drawLargeCoordinateObjects(const glm::mat4 &view,
                                const glm::mat4 &projection,
                                const glm::dvec3 &rebaseOrigin)
{
  const std::vector<LargeCoordinateObject> objects =
      getLargeCoordinateObjects();

  std::vector<const LargeCoordinateObject *> drawOrder;
  drawOrder.reserve(objects.size());
  for (const auto &object : objects)
    drawOrder.push_back(&object);

  std::sort(drawOrder.begin(), drawOrder.end(),
            [&](const LargeCoordinateObject *lhs,
                const LargeCoordinateObject *rhs) {
              // Distance to camera is the same as distance to rebase
              // (translation-invariant), so sorting against the rebase
              // origin preserves the original back-to-front order.
              const glm::dvec3 lhsDelta = lhs->worldPosition - rebaseOrigin;
              const glm::dvec3 rhsDelta = rhs->worldPosition - rebaseOrigin;
              return glm::dot(lhsDelta, lhsDelta) > glm::dot(rhsDelta, rhsDelta);
            });

  // All objects are translucent, so paint them far-to-near for stable alpha.
  for (const auto *object : drawOrder)
  {
    drawCube(view, projection, rebaseOrigin, object->worldPosition,
             object->color, 0.45f, object->size);
  }
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

void includeAabbCameraDepth(const glm::dvec3 &worldCenter,
                            const glm::dvec3 &halfExtent,
                            const glm::dvec3 &cameraPosition,
                            const glm::dvec3 &cameraRight,
                            const glm::dvec3 &cameraUp,
                            const glm::dvec3 &cameraFront,
                            double &minDepth,
                            double &maxDepth)
{
  // Transform all eight corners, as opposed to estimating the depth from
  // the center and a bounding sphere.  This is the same conservative
  // bounding-box approach used by scene-graph cullers such as OSG.
  for (int x = -1; x <= 1; x += 2)
  {
    for (int y = -1; y <= 1; y += 2)
    {
      for (int z = -1; z <= 1; z += 2)
      {
        const glm::dvec3 corner = worldCenter + glm::dvec3(
            x * halfExtent.x, y * halfExtent.y, z * halfExtent.z);
        const CameraSpacePoint cameraPoint = toCameraSpace(
            corner, cameraPosition, cameraRight, cameraUp, cameraFront);
        minDepth = std::min(minDepth, cameraPoint.depth);
        maxDepth = std::max(maxDepth, cameraPoint.depth);
      }
    }
  }
}

bool aabbIntersectsOrthoViewport(const glm::dvec3 &worldCenter,
                                 const glm::dvec3 &halfExtent,
                                 const glm::dvec3 &cameraPosition,
                                 const glm::dvec3 &cameraRight,
                                 const glm::dvec3 &cameraUp,
                                 const glm::dvec3 &cameraFront,
                                 double halfWidth,
                                 double halfHeight)
{
  double minX = std::numeric_limits<double>::max();
  double maxX = -std::numeric_limits<double>::max();
  double minY = std::numeric_limits<double>::max();
  double maxY = -std::numeric_limits<double>::max();

  for (int x = -1; x <= 1; x += 2)
  {
    for (int y = -1; y <= 1; y += 2)
    {
      for (int z = -1; z <= 1; z += 2)
      {
        const glm::dvec3 corner = worldCenter + glm::dvec3(
            x * halfExtent.x, y * halfExtent.y, z * halfExtent.z);
        const CameraSpacePoint cameraPoint = toCameraSpace(
            corner, cameraPosition, cameraRight, cameraUp, cameraFront);
        minX = std::min(minX, cameraPoint.x);
        maxX = std::max(maxX, cameraPoint.x);
        minY = std::min(minY, cameraPoint.y);
        maxY = std::max(maxY, cameraPoint.y);
      }
    }
  }

  return maxX >= -halfWidth && minX <= halfWidth &&
         maxY >= -halfHeight && minY <= halfHeight;
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
  float pixelSize = 0.0f;
  double activeNear = 0.0;
  double activeFar  = 0.0;
  static bool hasStableOrthoDepthSlab = false;
  static double stableOrthoNear = 0.0;
  static double stableOrthoFar  = 0.0;
  static double pendingOrthoNear = 0.0;
  static double pendingOrthoFar = 0.0;
  static int stableOrthoDepthSlabFrames = 0;
  // Perspective hysteresis: same shape as the ortho slab, but only the far
  // plane is dynamic (near is fixed at 0.1 by glm::perspective's constraint).
  // Prevents per-frame far-plane wobble from turning depth precision into a
  // flickering step during pan/orbit.
  static bool hasStablePerspectiveFar = false;
  static double stablePerspectiveFar = 0.0;
  static double pendingPerspectiveFar = 0.0;
  static int stablePerspectiveFarFrames = 0;
  // The reference line belongs to the world, not to either validation scene.
  const glm::dvec3 worldLineEnd = LARGE_COORDINATE_BASE_POINT;
  const double farSceneDistance = std::max(
      glm::length(orbitCam.Position),
      glm::length(orbitCam.Position - worldLineEnd));

  // The grid and the world reference line both want an anchor expressed on
  // the active plane (XZ, XY or YZ).  Compute it once in double precision so
  // both consumers (ortho and perspective) share the exact same value
  // rather than each branch rerunning the same arithmetic with its own
  // local copy.
  //
  // When the camera looks parallel to the plane, the gaze/plane intersection
  // is degenerate; fall back to the camera's own in-plane coordinates so the
  // grid still has a meaningful anchor that follows the camera.
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

    // Convert the target and the active cube center to camera space before
    // calculating the depth interval.  The camera-space convention used
    // here is +Z forward, so `depth` is the positive distance in front of
    // the eye.  This is the double-precision equivalent of inverse-view
    // transformation, without passing large absolute coordinates through
    // a float matrix.
    const CameraSpacePoint targetCamera =
        toCameraSpace(orbitCam.Target, cameraPos, right, up, front);
    const double targetDepth = targetCamera.depth;

    // Start with the orthographic image extent.  The grid is infinite, so
    // its depth interval is handled analytically below rather than through
    // a finite scene bound.
    const double imageRadius = std::sqrt(halfW * halfW + halfH * halfH);
    double minDepth = targetDepth - imageRadius;
    double maxDepth = targetDepth + imageRadius;

    // OSG-style scene bounds: transform every AABB corner to camera space
    // and accumulate the actual minimum and maximum depth.  This avoids
    // using a center-depth estimate plus an unnecessarily large sphere.
    if (aabbIntersectsOrthoViewport(
            cubeWorldPosition, glm::dvec3(0.5), cameraPos, right, up, front,
            halfW, halfH))
    {
      includeAabbCameraDepth(
          cubeWorldPosition, glm::dvec3(0.5), cameraPos, right, up, front,
          minDepth, maxDepth);
    }
    // Include the large-coordinate reference cubes.  These are drawn
    // every frame (unconditionally, in the render pass below), so they
    // must always participate in the depth-slab computation as well.
    // The per-object viewport intersection test still excludes any cube
    // that is entirely off-screen, so panning far away does not inflate
    // far by geometry the user cannot see.
    for (const LargeCoordinateObject &object : getLargeCoordinateObjects())
    {
      const double halfSize = (double)object.size * 0.5;
      if (aabbIntersectsOrthoViewport(
              object.worldPosition, glm::dvec3(halfSize), cameraPos, right,
              up, front, halfW, halfH))
      {
        includeAabbCameraDepth(
            object.worldPosition, glm::dvec3(halfSize), cameraPos, right,
            up, front, minDepth, maxDepth);
      }
    }

    // The world reference line is always drawn.  Rather than only testing
    // its two endpoints (which misses cases where both endpoints lie
    // outside the viewport but the middle of the segment crosses it, and
    // conversely can pull far towards the far endpoint even when it is
    // off-screen), clip the segment against the ortho viewport in camera
    // space and accumulate only the visible portion's depth interval.
    includeSegmentCameraDepth(
        glm::dvec3(0.0, 0.0, 0.0), worldLineEnd,
        cameraPos, right, up, front, halfW, halfH,
        minDepth, maxDepth);

    // The infinite grid lies on the active plane.  For an orthographic
    // camera its plane-intersection depth varies across the image when the
    // view is pitched.  Compute that variation analytically instead of
    // using a grazing-angle multiplier or the distance to unrelated world
    // geometry.
    if (std::abs(frontOnNormal) > 1e-8)
    {
      const double cameraPlaneDistance =
          glm::dot(cameraPos - gridPlaneOrigin, planeNormal);
      const double groundCenterDepth =
          -cameraPlaneDistance / frontOnNormal;
      const double groundDepthRadius =
          (std::abs(glm::dot(right, planeNormal)) * halfW +
           std::abs(glm::dot(up, planeNormal)) * halfH) /
          std::abs(frontOnNormal);
      minDepth = std::min(minDepth,
                          groundCenterDepth - groundDepthRadius);
      maxDepth = std::max(maxDepth,
                          groundCenterDepth + groundDepthRadius);
    }

    // Add a small symmetric safety margin around the accumulated bounds.
    // Orthographic projection allows negative near values (glm::ortho does
    // not have the near > 0 constraint that perspective imposes), so do
    // NOT clamp near to a small positive floor.  Clamping would skew the
    // depth slab asymmetrically whenever geometry straddles the camera
    // plane (e.g. when the orbit radius is small enough that parts of the
    // visible cubes fall behind the eye), which is exactly the case that
    // made pan-computed near/far diverge from the large-orbit values even
    // when the target position was the same.
    const double kDepthMargin = 1.0;
    double nearD = minDepth - kDepthMargin;
    double farD  = maxDepth + kDepthMargin;
    // Guarantee a strictly positive slab thickness for glm::ortho.
    if (farD <= nearD)
      farD = nearD + kDepthMargin;

    // Depth-buffer precision floor.  Only kicks in under extreme zoom-in
    // (halfH << 1) or when the viewport contains no finite geometry at
    // all, so the depth slab does not collapse below the resolution of a
    // 24-bit depth buffer.  In normal operation minDepth/maxDepth already
    // dominate and this clamp is a no-op.  The floor grows with halfH so
    // it scales with the current view size instead of imposing a fixed
    // world-unit lower bound at all zoom levels.
    const double kMinDepthSpan = std::max(2.0, (double)halfH * 0.5);
    if (farD - nearD < kMinDepthSpan)
    {
      const double mid = 0.5 * (nearD + farD);
      nearD = mid - kMinDepthSpan * 0.5;
      farD  = mid + kMinDepthSpan * 0.5;
    }

    // Keep a conservative depth slab while the camera is being panned.
    // Expanding is immediate (never clip visible geometry); shrinking only
    // happens after the raw candidate has stayed within candidateEpsilon
    // of the pending value for twenty frames.
    //
    // Anti-jitter invariant: when the raw candidate is judged stable (i.e.
    // its float noise is under candidateEpsilon), NEITHER `stableOrtho*`
    // NOR `pendingOrtho*` may change. Otherwise the ~1 ULP noise on the
    // candidate leaks into the stable slab and produces the two-value
    // ping-pong observed in the log (far oscillating between .207/.595).
    if (!hasStableOrthoDepthSlab)
    {
      stableOrthoNear = nearD;
      stableOrthoFar = farD;
      pendingOrthoNear = nearD;
      pendingOrthoFar = farD;
      stableOrthoDepthSlabFrames = 0;
      hasStableOrthoDepthSlab = true;
    }
    else
    {
      // A somewhat generous epsilon (1e-4) keeps sub-millimeter float
      // noise from ever counting as motion. A pure 1e-6 relative epsilon
      // is smaller than the noise floor of the depth-slab arithmetic
      // itself (Liang-Barsky clipping + several dot products in double).
      const double candidateEpsilon = std::max(
          1e-4, std::max(std::abs(pendingOrthoNear),
                         std::abs(pendingOrthoFar)) * 1e-4);
      const bool candidateIsStable =
          std::abs(nearD - pendingOrthoNear) <= candidateEpsilon &&
          std::abs(farD - pendingOrthoFar) <= candidateEpsilon;

      if (!candidateIsStable)
      {
        // Real motion: extend the slab immediately (never clip), refresh
        // the pending target, and restart the stability countdown.
        stableOrthoNear = std::min(stableOrthoNear, nearD);
        stableOrthoFar = std::max(stableOrthoFar, farD);
        pendingOrthoNear = nearD;
        pendingOrthoFar = farD;
        stableOrthoDepthSlabFrames = 0;
      }
      else
      {
        // Stable: do NOT touch stable/pending -- freezing them is what
        // stops the two-value oscillation. Just count frames until we
        // are allowed to shrink to the pending target.
        if (stableOrthoDepthSlabFrames < 20)
          ++stableOrthoDepthSlabFrames;
        if (stableOrthoDepthSlabFrames == 20)
        {
          stableOrthoNear = pendingOrthoNear;
          stableOrthoFar = pendingOrthoFar;
          stableOrthoDepthSlabFrames = 0;
        }
      }
    }

    nearD = stableOrthoNear;
    farD = stableOrthoFar;

    projection = glm::ortho(-halfH * aspect, halfH * aspect,
                             -halfH,            halfH,
                              (float)nearD, (float)farD);
    activeNear = nearD;
    activeFar  = farD;
    // Reset perspective hysteresis so re-entering perspective picks up
    // the current camera state from scratch instead of a stale slab.
    hasStablePerspectiveFar = false;
    stablePerspectiveFarFrames = 0;
    // Analytic ortho pixel size (doc section 4.2): the vertical frustum
    // extent 2 * halfH maps onto the drawable viewport height.
    pixelSize = (2.0f * halfH) / static_cast<float>(drawableHeight);
  }
  else
  {
    const double candidateFar = std::max(
        100.0,
        farSceneDistance
            + glm::length(orbitCam.Position - orbitCam.Target) * 8.0
            + 1000.0);

    // Same hysteresis policy as the ortho slab. Anti-jitter invariant:
    // when the candidate is judged stable, NEITHER `stablePerspectiveFar`
    // NOR `pendingPerspectiveFar` may change; otherwise per-frame float
    // noise leaks into the stable slab and produces a visible depth
    // ping-pong during pan / orbit.
    if (!hasStablePerspectiveFar)
    {
      stablePerspectiveFar = candidateFar;
      pendingPerspectiveFar = candidateFar;
      stablePerspectiveFarFrames = 0;
      hasStablePerspectiveFar = true;
    }
    else
    {
      const double candidateEpsilon =
          std::max(1e-4, std::abs(pendingPerspectiveFar) * 1e-4);
      const bool candidateIsStable =
          std::abs(candidateFar - pendingPerspectiveFar) <= candidateEpsilon;

      if (!candidateIsStable)
      {
        stablePerspectiveFar = std::max(stablePerspectiveFar, candidateFar);
        pendingPerspectiveFar = candidateFar;
        stablePerspectiveFarFrames = 0;
      }
      else
      {
        if (stablePerspectiveFarFrames < 20)
          ++stablePerspectiveFarFrames;
        if (stablePerspectiveFarFrames == 20)
        {
          stablePerspectiveFar = pendingPerspectiveFar;
          stablePerspectiveFarFrames = 0;
        }
      }
    }

    const float far = (float)stablePerspectiveFar;
    projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, far);
    activeNear = 0.1;
    activeFar  = (double)far;
    hasStableOrthoDepthSlab = false;
    stableOrthoDepthSlabFrames = 0;
    // Rough estimate around the orbit target, only used to seed the LOD
    // step; the shader computes exact per-fragment sizes for perspective.
    pixelSize = (2.0f * (float)glm::length(orbitCam.Position - orbitCam.Target)
                  * tan(glm::radians(22.5f)))
               / static_cast<float>(drawableHeight);
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
    const double kMinOrthoPlaneCos = 0.087155743; // sin(5 degrees)
    orthoPlaneValid = std::abs(frontOnNormal) > kMinOrthoPlaneCos;
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
  // -originWorld on the two in-plane axes in float32, used by the
  // perspective axis pass to draw the world axes through the origin.
  // originWorld itself can be 1e5+, so we cannot upload it directly
  // without losing precision when step is fractional.  Instead we
  // exploit the fact that originWorld == uOriginRelative + rebase
  // (uOriginRelative is bounded to chunkSize/2, rebase is a chunk
  // multiple of 1e4; both are exact in float32) and compute the
  // negation in float32.
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
      const glm::vec4 clip0 = projection * view *
          glm::vec4(glm::vec3(axisPoint0 - rebase), 1.0f);
      const glm::vec4 clip1 = projection * view *
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

  const glm::mat4 viewProj = projection * view;
    const rendering::GridRenderData gridRenderData{
        .view = view,
        .projection = projection,
        .invViewProj = glm::inverse(viewProj),
      .viewProj = viewProj,
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
  };
  rendererBackend->drawGrid(gridRenderData);

  // The green reference line is anchored to the literal world origin
  // (0, 0, 0) and the rebased grid anchor (planeCenter) is a separate
  // point that follows the camera.  On purpose these two world points
  // are independent -- after a camera pan they will not coincide on
  // screen, by design.  This visually demonstrates the rebased grid
  // staying attached to the camera while the world frame of reference
  // stays put.
  drawWorldLine(view, projection, rebase,
                glm::dvec3(0.0, 0.0, 0.0),
                worldLineEnd);

  // Translucent geometry goes last: the grid remains visible through the
  // cube, while the cube tints everything already rendered behind its front
  // faces.  Keep depth writes off so later transparent geometry can still be
  // depth-sorted independently.
  drawLargeCoordinateObjects(view, projection, rebase);
  drawCube(view, projection, rebase, cubeWorldPosition,
      glm::vec3(1.0f, 0.58f, 0.25f), 1.0f, 1.0f);

  // Small 5-pixel "sphere" (disc-shaded point) at the orbit target so the
  // camera's focus point is always visible.  Uses the same RTE rebase as
  // every other draw call.
  drawTargetPoint(view, projection, rebase, orbitCam.Target);
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

  if (const char *testOrtho = std::getenv("GRID_CAMERA_TEST_ORTHO");
      testOrtho && std::strcmp(testOrtho, "0") != 0)
  {
    useOrthoProjection() = true;
    if (const char *halfH = std::getenv("GRID_CAMERA_TEST_ORTHO_HALFH"))
      orthoHalfHeight() = std::max(0.01f, (float)std::atof(halfH));
    std::cout << "Projection: ORTHOGRAPHIC (test)" << std::endl;
  }

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
          useOrthoProjection() = !useOrthoProjection();
          std::cout << "Projection: " << (useOrthoProjection() ? "ORTHOGRAPHIC" : "PERSPECTIVE") << std::endl;
        }
        if (evt.key.key == SDLK_1)
          applyGridPlane(GridPlaneType::XY);
        if (evt.key.key == SDLK_2)
          applyGridPlane(GridPlaneType::XZ);
        if (evt.key.key == SDLK_3)
          applyGridPlane(GridPlaneType::YZ);
        if (evt.key.key == SDLK_4)
          applyGridPlane(GridPlaneType::Custom);
        if (evt.key.scancode == SDL_SCANCODE_L)
        {
          // L key -- swap the camera-position parameters only.  Projection
          // mode (perspective vs orthographic) and ortho half-height are
          // intentionally untouched; toggle projection with P and zoom
          // with the mouse wheel.
          largeCoordinateSceneEnabled = !largeCoordinateSceneEnabled;
          if (largeCoordinateSceneEnabled)
          {
            cubeWorldPosition =
                LARGE_COORDINATE_BASE_POINT +
                LARGE_COORDINATE_DETAIL_OFFSET;
            orbitCam.setOrbit(cubeWorldPosition, 6000.0);
            SDL_SetWindowTitle(
                window, "grid plane - large-coordinate validation");
            printLargeCoordinateValidation();
          }
          else
          {
            cubeWorldPosition = glm::dvec3(0.0);
            orbitCam.setOrbit(cubeWorldPosition, 15.0);
            SDL_SetWindowTitle(window, "grid plane");
          }
          std::cout << "Large-coordinate scene: "
                    << (largeCoordinateSceneEnabled ? "enabled" : "disabled")
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

    render();

    rendererBackend->present();
  }

  close();

  return 0;
}
