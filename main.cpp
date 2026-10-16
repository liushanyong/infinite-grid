#include <SDL.h>

#include <cadui/ViewCubeOverlay.hpp>
#include <cadui/UcsIconOverlay.hpp>
#include <cadui/GripOverlay.hpp>
#include <imgui.h>
#include <imgui_bgfx.h>

#include "application/ApplicationPlatform.hpp"
#include "coordinate/WorldRebase.h"
#include "arx/ArxTypes.h"
#include "acdb/AcDbTessellate.h"
#include "acdb/AcDbEntityWorldDraw.h"
#include "acdb/AcDbFileIo.h"
#include "acdb/AcDbGltf.h"
#include "acdb/AcDbTransform.h"
#include "brep/KernelSelfTest.h"
#include "acdb/StoreSelfTest.h"
#include "acdb/FileIoSelfTest.h"
#include "acgs/AcGsView.h"
#include "acgs/AcGsMath.h"
#include "acgs/AcGsSelectionManager.h"
#include "acgs/AcGsPicking.h"
#include "acgs/DocumentSceneBridge.h"
#include "util/resource_path.h"
#include "acgi/AcGiLineType.h"
#include "acgs/model/DrawContext.h"
#include "acgs/model/AcGsModel.h"
#include "acgs/model/AcGsDocumentReplayer.h"
#include "acge/AcGeBvh.h"
#include "acge/AcGePoint.h"
#include "rendering/RenderTypes.h"
#include "rendering/ProceduralMesh.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <array>
#include <cctype>
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
#include <numbers>

namespace
{

constexpr int SCREEN_WIDTH = 1200;
constexpr int SCREEN_HEIGHT = 768;

cadui::ViewCubeOverlay viewCubeOverlay;
cadui::UcsIconOverlay ucsIconOverlay;
application::Platform appPlatform;

// ---- acgs ImGui panels: per-viewport presentation state ----
// GRID_MULTI_VIEW=1 enables the two-panel round-robin (each panel owns
// an AcGsView); the default is one fullscreen panel.  The panel block
// publishes g_panelPx (device pixels) for the frame head and routes
// input through the AcGsView interaction facade.
static bool multiViewEnabled()
{
  static const bool requested = []() {
    const char *value = std::getenv("GRID_MULTI_VIEW");
    return value != nullptr && std::strcmp(value, "0") != 0;
  }();
  return requested;
}
static ImVec2 g_panelPx[2] = {ImVec2(0, 0), ImVec2(0, 0)};
static int g_activeSceneSlot = 0;   // this frame's rendered viewport
static int g_pickedSlot = -1;       // slot pinned by a panel click
static int g_imguiHoveredPanel = -1;

// The AcGs view owns the camera, projection mode, and every renderer
// submission.  ObjectARX: AcGsView is a first-class citizen owned by
// AcGsManager — this accessor rebinds to the active viewport, so the
// legacy acgsView call sites below always act on the CVPORT view.
static acgs::AcGsView &acgsView()
{
  return *acgs::acgsGetManager()->activeView();
}


bool &useOrthoProjection()
{
    return acgsView().orthoMode();
}

WorldRebase &worldRebase()
{
    static WorldRebase instance;
    return instance;
}



// Keep CAD content readable even when the authored color is close to the
// clear color.  A squared RGB distance of 0.04 corresponds to a 0.2 channel
// delta, which is enough to catch near-black/near-background overlays while
// avoiding needless color changes for clearly distinct hues.
// The single render gateway every submission goes through.
// Legacy alias kept for the submission call sites below.
// Camera-space projection now lives in the acgi render gateway.
using CameraSpacePoint = acgs::CameraSpacePoint;


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
    float scale, const acgs::math::Vec3 &color, float opacity,
    const rendering::DoubleSingleVec3 &position,
    const acgs::math::Vec4 &material = acgs::math::Vec4(0.0f, 0.0f, 0.0f, 0.0f))
{
  rendering::MeshInstance instance;
  instance.transformColumn0 = acgs::math::Vec4(scale, 0.0f, 0.0f, color.r);
  instance.transformColumn1 = acgs::math::Vec4(0.0f, scale, 0.0f, color.g);
  instance.transformColumn2 = acgs::math::Vec4(0.0f, 0.0f, scale, color.b);
  instance.positionHigh = acgs::math::Vec4(position.high, opacity);
  instance.positionLow = acgs::math::Vec4(position.low, material.r);
  return instance;
}
// Every demo path funnels renderer submissions through this helper.  The
// protocol keeps instancing compact; only the submitter knows backend details.
// All world-space object positions stay double precision on the CPU.
// The GPU never sees these; only (objWorld - worldRebase().origin()) does.
AcGePoint3d cubeWorldPosition;

bool largeCoordinateCameraView = false;
bool frustumCaptureRequested = false;
bool frustumWireframeVisible = false;
bool targetPlaneConstraintEnabled = false;
AcGePoint3d frustumCorners[8];
bool gridVisibleQuadValid = false;
AcGePoint3d gridVisibleQuad[8];
int gridVisibleQuadCount = 0;
enum class GridPlaneType
{
    XZ,
    XY,
    YZ,
    Custom,
};

GridPlaneType gridPlane = GridPlaneType::XY;
AcGePoint3d gridPlaneOrigin;
AcGeVector3d gridPlaneNormal{0.0, 0.0, 1.0};
AcGePoint3d gridPlaneStartAxisOrigin;
AcGeVector3d gridPlaneStartAxisDirection{1.0, 0.0, 0.0};

// Keep the CPU visibility test, slab construction, and grid shader discard
// aligned.  A plane within five degrees of the view direction is hidden.
constexpr double kMinGridPlaneCos = 0.087155743; // sin(5 degrees)

// Edit these to define a non-axis-aligned infinite grid.  Key 4 activates it;
// R rebuilds them from the current camera frame.
AcGePoint3d customGridPlaneOrigin{1.0, 0.5, -0.5};
AcGeVector3d customGridPlaneNormal =
    AcGeVector3d(0.25, 1.0, 0.15).normal();
AcGePoint3d customGridPlaneStartAxisOrigin{1.0, 0.5, -0.5};
AcGeVector3d customGridPlaneStartAxisDirection =
    AcGeVector3d(1.0, 0.0, 0.25).normal();

void enforceTargetPlaneConstraint();

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

constexpr acgs::math::Vec3 kAxisColorX(0.8f, 0.2f, 0.2f);
constexpr acgs::math::Vec3 kAxisColorY(0.2f, 0.8f, 0.2f);
constexpr acgs::math::Vec3 kAxisColorZ(0.2f, 0.2f, 0.8f);

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
        gridPlaneOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneNormal = AcGeVector3d(0.0, 0.0, 1.0);
        gridPlaneStartAxisOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneStartAxisDirection = AcGeVector3d(1.0, 0.0, 0.0);
        break;
    case GridPlaneType::YZ:
        gridPlaneOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneNormal = AcGeVector3d(1.0, 0.0, 0.0);
        gridPlaneStartAxisOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneStartAxisDirection = AcGeVector3d(0.0, 1.0, 0.0);
        break;
    case GridPlaneType::Custom:
        gridPlaneOrigin = customGridPlaneOrigin;
        gridPlaneNormal = customGridPlaneNormal;
        gridPlaneStartAxisOrigin = customGridPlaneStartAxisOrigin;
        gridPlaneStartAxisDirection = customGridPlaneStartAxisDirection;
        break;
    case GridPlaneType::XZ:
    default:
        gridPlaneOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneNormal = AcGeVector3d(0.0, 1.0, 0.0);
        gridPlaneStartAxisOrigin = AcGePoint3d(0.0, 0.0, 0.0);
        gridPlaneStartAxisDirection = AcGeVector3d(1.0, 0.0, 0.0);
        break;
    }
    if (appPlatform.window())
        SDL_SetWindowTitle(
            appPlatform.window(),
            (std::string("grid plane - ") + gridPlaneName(plane) +
             "  [1]XY [2]XZ [3]YZ [4]CUSTOM")
                .c_str());
    std::cout << "Grid plane: " << gridPlaneName(plane)
              << " (1=XY, 2=XZ, 3=YZ, 4=CUSTOM; origin=("
              << gridPlaneOrigin.x << ", " << gridPlaneOrigin.y << ", "
              << gridPlaneOrigin.z << "))" << std::endl;
    acgsView().resetDepthSlabs();
    if (targetPlaneConstraintEnabled)
        enforceTargetPlaneConstraint();
}

void activePlaneTangents(AcGeVector3d &tangentU, AcGeVector3d &tangentV)
{
    const AcGeVector3d planeNormal = gridPlaneNormal.normal();
    if (gridPlane == GridPlaneType::XY)
    {
        tangentU = AcGeVector3d(1.0, 0.0, 0.0);
        tangentV = AcGeVector3d(0.0, 1.0, 0.0);
    }
    else if (gridPlane == GridPlaneType::XZ)
    {
        tangentU = AcGeVector3d(1.0, 0.0, 0.0);
        tangentV = AcGeVector3d(0.0, 0.0, 1.0);
    }
    else if (gridPlane == GridPlaneType::YZ)
    {
        tangentU = AcGeVector3d(0.0, 1.0, 0.0);
        tangentV = AcGeVector3d(0.0, 0.0, 1.0);
    }
    else
    {
        tangentU = gridPlaneStartAxisDirection -
                   planeNormal *
                       gridPlaneStartAxisDirection.dotProduct(planeNormal);
        if (tangentU.length() < 1e-9)
        {
            tangentU = AcGeVector3d(1.0, 0.0, 0.0) -
                       planeNormal * planeNormal.x;
            if (tangentU.length() < 1e-9)
                tangentU = AcGeVector3d(0.0, 1.0, 0.0) -
                           planeNormal * planeNormal.y;
        }
        tangentU.normalize();
        tangentV = planeNormal.crossProduct(tangentU).normal();
    }
}
// Validation scene adapted from external/large-coordinate-rendering.
//
  // The base point is 1e7 so the far cluster exercises large coordinates.
  // The rebased camera/target stay bounded by the 1e4 chunk; distant
  // objects sit far from the rebase origin, but at those distances they
  // render below the impostor threshold, so float32 quantization stays
  // sub-pixel.  Bump to ~1e9 for a true precision-stress demonstration:
const AcGePoint3d LARGE_COORDINATE_BASE_POINT(1e7, 0.0, 1e7);
const AcGeVector3d LARGE_COORDINATE_DETAIL_OFFSET(1536.0, 0.0, -1024.0);

// The view owns the orbit camera; demo code keeps the orbitCam alias.
AcGsOrbitCamera &orbitCam = acgsView().orbitCamera();

// An orbit drag locks its pivot when it starts.  Until this demo has a
// selection system, use the same fallback as OpenCADStudio: the camera target.
std::optional<AcGePoint3d> orbitPivot;

// Project the orbit focus onto the active grid plane. Translating the eye by
// the same plane correction preserves the view direction and orbit distance;
// ordinary orbiting may then still place the eye off the plane.
void enforceTargetPlaneConstraint()
{
    const AcGeVector3d planeNormal = gridPlaneNormal.normal();
    const AcGePoint3d target(orbitCam.Target);
    const double targetOnNormal =
        (target - gridPlaneOrigin).dotProduct(planeNormal);
    const AcGeVector3d correction = planeNormal * -targetOnNormal;
    orbitCam.setTarget(acgs::math::DVec3(target + correction));
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

    const AcGeVector3d horizontalAxis =
        AcGeVector3d(orbitCam.Right).normal();
    customGridPlaneOrigin = AcGePoint3d(orbitCam.Target);
    customGridPlaneNormal = AcGeVector3d(orbitCam.WorldUp).normal();
    customGridPlaneStartAxisOrigin = AcGePoint3d(orbitCam.Target);
    customGridPlaneStartAxisDirection = horizontalAxis;

    applyGridPlane(GridPlaneType::Custom);
    std::cout << "World up reset from camera: ("
              << orbitCam.WorldUp.x << ", " << orbitCam.WorldUp.y << ", "
              << orbitCam.WorldUp.z << ")" << std::endl;
}

} // namespace

static void initializeTextResources();

void close();

bool init()
{
  if (!appPlatform.initializeSdl())
    return false;

  acgs::AcGsManager *gsManager = acgs::acgsGetManager();
  std::string deviceError;
  if (!gsManager->prepareDevice(&deviceError))
  {
    std::cerr << deviceError << std::endl;
    close();
    return false;
  }

  SDL_Window *window = appPlatform.createWindow(
      "grid plane", SCREEN_WIDTH, SCREEN_HEIGHT,
      gsManager->deviceWindowFlags());
  if (!window)
  {
    close();
    return false;
  }
  SDL_SetWindowTitle(
      window, "grid plane - XY  [1]XY [2]XZ [3]YZ [4]CUSTOM");

  if (!gsManager->initializeDevice(window, &deviceError))
  {
    std::cerr << deviceError << std::endl;
    close();
    return false;
  }

  if (std::strncmp(gsManager->deviceName(), "bgfx", 4) == 0)
  {
    std::string cjkFontPath;
    if (util::resourceExists("fonts/WenQuanWeiMiHei-1.ttf"))
      cjkFontPath =
          util::resourcePath("fonts/WenQuanWeiMiHei-1.ttf").string();
    // The panels own the window: the device skips the full-window
    // present resolve (compositeFrame blits the viewport finals and
    // kicks), and the UI view clears the swapchain so any pixel the
    // panels do not paint is a deterministic color.  Text/outline live
    // in the scene target now, so nothing needs a backbuffer underlay.
    const acgs::math::Vec4 &uiClear = acgs::kClearColor;
    const std::uint32_t uiClearColor =
        (std::uint32_t(uiClear.r * 255.0f) << 24) |
        (std::uint32_t(uiClear.g * 255.0f) << 16) |
        (std::uint32_t(uiClear.b * 255.0f) << 8) |
        std::uint32_t(uiClear.a * 255.0f);
    if (appPlatform.initializeImGui(
            window, 18.0f,
            cjkFontPath.empty() ? nullptr : cjkFontPath.c_str(),
            uiClearColor))
    {
      gsManager->setImGuiActive(true);
      if (!viewCubeOverlay.create(imguiBgfxGetFontTexture()))
        std::cerr << "ViewCube BGFX renderer unavailable; using ImGui fallback"
                  << std::endl;
    }
  }

  std::cout << "Renderer backend: " << gsManager->deviceName()
            << " (" << gsManager->deviceApiName() << ")"
            << std::endl;

  // Optional startup visual style (0=Wireframe2D .. 5=DepthBuffer), used by
  // tooling/screenshots to render specific styles without key input.
  if (const char *styleEnv = std::getenv("GRID_START_STYLE"))
  {
    const int styleIndex = std::atoi(styleEnv);
    if (styleIndex > 0 && styleIndex < 6)
    {
      gsManager->setActiveVisualStyle(
          static_cast<rendering::RenderMode>(styleIndex));
      std::cout << "Visual style: "
              << rendering::renderModeLabel(acgsView().visualStyle().mode())
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
    gMeshTextureIndex = gsManager->loadMeshTexture(meshTexturePath);
    std::cout << "Mesh texture: " << meshTexturePath
              << " (index=" << gMeshTextureIndex << ")" << std::endl;
  }

  initializeTextResources();
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
  viewCubeOverlay.destroy();
  appPlatform.shutdownImGui();
  acgs::acgsGetManager()->shutdownDevice();
  appPlatform.shutdownSdl();
}

// The ViewCube overlay (window chrome, bgfx renderer, navigation actions and
// their camera application) lives in cadui::ViewCubeOverlay; these two
// wrappers only bridge what the class must not know: SDL window queries and
// the ImGui-enabled gate.
bool isViewCubeScreenPoint(float x, float y)
{
  if (!appPlatform.imguiEnabled())
    return false;
  int displayWidth = 0;
  int displayHeight = 0;
  SDL_GetWindowSize(appPlatform.window(), &displayWidth, &displayHeight);
  return viewCubeOverlay.capturesScreenPoint(x, y, (float)displayWidth,
                                             (float)displayHeight);
}

void drawViewCubeOverlay()
{
  if (!appPlatform.imguiEnabled())
    return;
  if (viewCubeOverlay.render(orbitCam))
  {
    acgsView().resetDepthSlabs();
  }
}

// The UCS icon overlay (window chrome, tripod widget, grip-drag state, camera
// chase) lives in cadui::UcsIconOverlay; these two wrappers only bridge what
// the class must not know: SDL window queries and the ImGui-enabled gate.
bool isUcsIconScreenPoint(float x, float y)
{
  if (!appPlatform.imguiEnabled())
    return false;
  int displayWidth = 0;
  int displayHeight = 0;
  SDL_GetWindowSize(appPlatform.window(), &displayWidth, &displayHeight);
  return ucsIconOverlay.capturesScreenPoint(x, y, (float)displayHeight);
}

int currentDrawableWidth();
int currentDrawableHeight();

void drawUcsIconOverlay()
{
  if (!appPlatform.imguiEnabled())
    return;
  if (ucsIconOverlay.render(orbitCam, acgs::kClearColor,
                            currentDrawableHeight()))
  {
    acgsView().resetDepthSlabs();
  }
}

void logCameraTargetIfChanged(const AcGePoint3d &target)
{
  static AcGePoint3d lastTarget(std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN());
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

void logCameraStateIfChanged(const AcGePoint3d &target,
                             double nearPlane, double farPlane,
                             bool isOrtho)
{
  static AcGePoint3d lastTarget(std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN());
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

void appendSceneLine(acgs::AcGsModel &drawList,
                     const acgs::math::DVec3 &start, const acgs::math::DVec3 &end,
                     const acgs::math::Vec3 &color, float opacity,
                     double lineWeight = 2.0)
{
  acgs::ViewportDraw draw(drawList.geometry());
  AcGiWorldDraw graphics(draw);
  graphics.setColor(acgsView().contrastColor(acgs::math::Vec4(color, opacity)));
  graphics.setLineWeight(lineWeight);
  graphics.worldLine(AcGePoint3d(start), AcGePoint3d(end));
}

bool lineDebugEnabled()
{
  static const bool enabled = [] {
    const char *value = std::getenv("GRID_LINE_DEBUG");
    return value && *value && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

void appendScenePoint(acgs::AcGsModel &drawList,
                      const acgs::math::DVec3 &location, const acgs::math::Vec3 &color,
                      double pointSize)
{
  acgs::ViewportDraw draw(drawList.geometry());
  AcGiWorldDraw graphics(draw);
  graphics.setColor(acgsView().contrastColor(acgs::math::Vec4(color, 1.0f)));
  graphics.setLineWeight(pointSize);
  graphics.worldPoint(AcGePoint3d(location));
}

// Dynamic overlays stay in the AcGi-lite protocol but are never placed in the
// immutable CAD draw-list cache.  The submitter preserves their cheap
// view-space line and point pipelines.
int currentDrawableWidth();
int currentDrawableHeight();

// Imported glTF geometry shared between tessellation rebuilds: the CPU
// buffers parse once per model file and the GPU upload happens lazily on
// the first draw with a live device.  The cache is keyed by file path (not
// by tessellation-stored pointers) and its shared_ptrs are owned by the
// records themselves, so rebuilds cannot dangle it -- it deliberately sits
// outside invalidateTessellationKeyedCaches().
struct GltfStaticMesh
{
  rendering::StaticMeshVertices cpu;
  std::uint32_t gpuId = 0;
};

struct MeshEntityRecord
{
  acdb::AcDbPolyFaceMesh entity;
  AcGePoint3d worldPosition;
  float size;
  rendering::MeshType mesh = rendering::MeshType::Cube;
  // Non-null: draw/pick upload and rasterize this static geometry instead
  // of the procedural prototype selected by |mesh|.
  std::shared_ptr<GltfStaticMesh> staticMesh;
  // Per-record albedo texture slot (0 = scene default texture).
  std::uint32_t diffuseTextureIndex = 0;
  // Instance scale for records whose geometry is authored at final world
  // size (imported models bake their normalization in).  Prototype records
  // keep 0 and scale by |size|, whose vertex radius is 0.5.
  float renderScale = 0.0f;

  float instanceScale() const { return renderScale > 0.0f ? renderScale : size; }

  bool realistic() const
  {
    return entity.style == acdb::MeshStyle::Realistic;
  }

  const std::string &displayName() const
  {
    return entity.name;
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
  CadCurve,
  CadText
};


struct GpuPickEntity
{
  VisibilityKind kind = VisibilityKind::MeshObject;
  const MeshEntityRecord *mesh = nullptr;
  const acgs::AcGsEntityRange *cadRange = nullptr;
  const acgs::CurveBatchCommand *curve = nullptr;
  const acgi::TextRequest *text = nullptr;

  bool operator==(const GpuPickEntity &other) const
  {
    return kind == other.kind && mesh == other.mesh &&
           cadRange == other.cadRange &&
           curve == other.curve &&
           text == other.text;
  }
};

static acgs::AcGsSelectionManager &gpuPickManager()
{
  return acgsView().selectionManager();
}

// The demo entity type bridges to the type-erased pick record: the
// manager stores a kind tag plus opaque pointers and never sees the
// demo-level scene types.
static acgs::AcGsPickEntity toPickEntity(const GpuPickEntity &entity)
{
  return {uint32_t(entity.kind), entity.mesh, entity.cadRange,
          entity.curve, entity.text};
}

static bool gpuPickEnabled()
{
  return acgs::AcGsSelectionManager::pickEnabled();
}

static uint32_t registerGpuPickEntity(GpuPickEntity entity)
{
  return gpuPickManager().registerEntity(toPickEntity(entity));
}

constexpr uint32_t kGpuPickCenterCubeId = 1;

static const GpuPickEntity *findGpuPickEntity(uint32_t id)
{
  thread_local GpuPickEntity converted;
  if (id == kGpuPickCenterCubeId)
  {
    converted = {};
    converted.kind = VisibilityKind::CenterCube;
    return &converted;
  }
  const acgs::AcGsPickEntity *found = gpuPickManager().find(id);
  if (!found)
    return nullptr;
  converted.kind = VisibilityKind(found->kind);
  converted.mesh = static_cast<const MeshEntityRecord *>(found->mesh);
  converted.cadRange =
      static_cast<const acgs::AcGsEntityRange *>(found->range);
  converted.curve =
      static_cast<const acgs::CurveBatchCommand *>(found->curve);
  converted.text = static_cast<const acgi::TextRequest *>(found->text);
  return &converted;
}

using GpuPickCameraBasis = acgs::AcGsSelectionManager::CameraBasis;
using GpuPickFocusState = acgs::AcGsSelectionManager::FocusState;

GpuPickFocusState &gpuPickFocus = gpuPickManager().focus();
bool gpuPickSceneDebug = false;
bool gpuPickSceneDebugQueueActive = false;
std::optional<GpuPickEntity> outlineEntity;
bool outlineLockTest = false;
bool outlineAllTest = false;
// Startup override for automated validation: GRID_OUTLINE_ALL=1 enables
// the full-scene selection compositor without a key press.
const bool outlineAllAtStartup = [] {
  const char *value = std::getenv("GRID_OUTLINE_ALL");
  return value != nullptr && std::strcmp(value, "0") != 0;
}();
// Startup override for automated validation: GRID_ID_VISIBLE=1 presents
// the full-scene GPU ID debug square without a key press.
const bool idVisibleAtStartup = [] {
  const char *value = std::getenv("GRID_ID_VISIBLE");
  return value != nullptr && std::strcmp(value, "0") != 0;
}();
// Startup override for automated validation: GRID_SELECT_NAME=<entity name>
// selects (outlines + tints) the named entity once the registry sees it, so
// captures can exercise the single-selection path without a scripted click.
// A later Esc deselects normally; the hook does not re-select.
const std::string selectNameAtStartup = [] {
  const char *value = std::getenv("GRID_SELECT_NAME");
  return value ? std::string(value) : std::string();
}();
uint32_t lockedOutlineId = 0;

static uint64_t hashGpuPickSceneBytes(uint64_t hash, const void *data,
                                      size_t size)
{
  return acgs::AcGsSelectionManager::hashSceneBytes(hash, data, size);
}

uint32_t findGpuPickObjectIdForEntity(const GpuPickEntity &entity)
{
  if (entity.kind == VisibilityKind::CenterCube)
    return kGpuPickCenterCubeId;
  return gpuPickManager().findIdFor(toPickEntity(entity));
}

static bool gpuPickFocusWaiting()
{
  return gpuPickEnabled() && gpuPickFocus.waitingResult;
}

acgs::math::Vec4 meshEntityColor(const MeshEntityRecord &entity)
{
  return acgsView().contrastColor(
      entity.entity.color.renderColor());
}

bool meshEntityVisible(const MeshEntityRecord &entity)
{
  return entity.entity.visible &&
         entity.entity.color.alpha() > 0.0f;
}

// In-scene selection recolor: the selected entity's primitives draw with
// this color IN their own passes (unified depth buffer — same geometry,
// same depth, same blending; only the color changes).
static constexpr acgs::math::Vec3 kSelectionHighlightRGB(0.35f, 0.65f, 1.0f);

struct MeshInstanceCacheEntry
{
  rendering::MeshInstance instance;
  AcGePoint3d position;
  float size = 0.0f;
  acgs::math::Vec4 color;
  acgs::math::Vec4 material;
};

// Keyed on tessellation-stored record pointers; cleared wholesale by
// invalidateTessellationKeyedCaches() when the tessellation rebuilds.
static std::unordered_map<const MeshEntityRecord *, MeshInstanceCacheEntry>
    g_meshInstanceCache;

// Wireframe chains for imported models: the mesh's unique edges chained
// into greedy polylines, cached in world space per record pointer (cleared
// by the same umbrella).  Wireframe styles draw these strokes because
// imported models carry no procedural feature-edge buffer.
static std::unordered_map<const MeshEntityRecord *,
                          std::vector<std::vector<AcGePoint3d>>>
    g_gltfWireframeCache;

static const std::vector<std::vector<AcGePoint3d>> &gltfWireframeChains(
    const MeshEntityRecord &entity)
{
  auto found = g_gltfWireframeCache.find(&entity);
  if (found != g_gltfWireframeCache.end())
    return found->second;

  std::vector<std::vector<AcGePoint3d>> chains;
  const auto &positions = entity.staticMesh->cpu.positionU;
  const auto &indices = entity.staticMesh->cpu.indices;

  // Unique undirected edges of the index buffer.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
  edges.reserve(indices.size());
  for (size_t t = 0; t + 2 < indices.size(); t += 3)
  {
    for (int e = 0; e < 3; ++e)
    {
      const std::uint32_t a = indices[t + e];
      const std::uint32_t b = indices[t + (e + 1) % 3];
      if (a == b)
        continue;
      edges.emplace_back(std::min(a, b), std::max(a, b));
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

  // Greedy chaining: walk from an unused edge through shared endpoints so
  // the thousands of mesh edges collapse into a few hundred polylines.
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> atVertex;
  for (std::uint32_t e = 0; e < edges.size(); ++e)
  {
    atVertex[edges[e].first].push_back(e);
    atVertex[edges[e].second].push_back(e);
  }
  std::vector<std::uint8_t> used(edges.size(), 0);
  auto toWorld = [&entity, &positions](std::uint32_t index) {
    const acgs::math::Vec3 &local = acgs::math::Vec3(positions[index]);
    return entity.worldPosition + AcGeVector3d(local);
  };
  for (std::uint32_t start = 0; start < edges.size(); ++start)
  {
    if (used[start])
      continue;
    used[start] = 1;
    std::vector<AcGePoint3d> chain{toWorld(edges[start].first),
                                   toWorld(edges[start].second)};
    for (int direction = 0; direction < 2; ++direction)
    {
      std::uint32_t cursor = direction == 0 ? edges[start].second
                                            : edges[start].first;
      for (;;)
      {
        std::uint32_t nextEdge = 0xffffffffu;
        for (const std::uint32_t candidate : atVertex[cursor])
        {
          if (!used[candidate])
          {
            nextEdge = candidate;
            break;
          }
        }
        if (nextEdge == 0xffffffffu)
          break;
        used[nextEdge] = 1;
        const std::uint32_t other =
            edges[nextEdge].first == cursor ? edges[nextEdge].second
                                            : edges[nextEdge].first;
        if (direction == 0)
          chain.push_back(toWorld(other));
        else
          chain.insert(chain.begin(), toWorld(other));
        cursor = other;
      }
    }
    if (chain.size() >= 2)
      chains.push_back(std::move(chain));
  }
  return g_gltfWireframeCache.emplace(&entity, std::move(chains))
      .first->second;
}

static rendering::MeshInstance &cachedMeshInstance(
    const MeshEntityRecord &entity,
    const acgs::math::Vec4 &material = acgs::math::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    const acgs::math::Vec3 *colorOverride = nullptr)
{
  const acgs::math::Vec4 color =
      colorOverride
          ? acgs::math::Vec4(*colorOverride, meshEntityColor(entity).a)
          : meshEntityColor(entity);
  const float scale = entity.instanceScale();
  auto found = g_meshInstanceCache.find(&entity);
  if (found != g_meshInstanceCache.end() &&
      found->second.position == entity.worldPosition &&
      found->second.size == scale && found->second.color == color &&
      found->second.material == material)
  {
    return found->second.instance;
  }

  MeshInstanceCacheEntry entry;
  entry.position = entity.worldPosition;
  entry.size = scale;
  entry.color = color;
  entry.material = material;
  entry.instance = makeMeshInstance(
      scale, acgs::math::Vec3(color), color.a,
      rendering::encodeDoubleSingle(entity.worldPosition), material);
  if (found != g_meshInstanceCache.end())
  {
    found->second = std::move(entry);
    return found->second.instance;
  }
  return g_meshInstanceCache.emplace(&entity, std::move(entry))
      .first->second.instance;
}

static void queueGpuMeshEntity(const MeshEntityRecord &entity,
                               uint32_t objectId)
{
  if (!acgs::acgsGetManager()->deviceReady() ||
      !(gpuPickFocusWaiting() || gpuPickSceneDebugQueueActive) ||
      !meshEntityVisible(entity) || objectId == 0)
    return;

  // Imported models queue their own geometry; without a live upload the ID
  // pass would rasterize a prototype cube, so skip rather than mis-pick.
  if (entity.staticMesh && entity.staticMesh->gpuId == 0)
    return;

  const rendering::DoubleSingleVec3 objectPosition =
      rendering::encodeDoubleSingle(entity.worldPosition);
  const acgs::math::Vec4 color = meshEntityColor(entity);
  const rendering::MeshInstance instance = makeMeshInstance(
      entity.instanceScale(), acgs::math::Vec3(color), color.a, objectPosition);
  acgs::acgsGetManager()->queueGpuMeshPick(
      instance, entity.mesh, objectId,
      entity.staticMesh ? entity.staticMesh->gpuId : 0);
}

static void queueGpuMeshEntity(const MeshEntityRecord *entity)
{
  if (!entity)
    return;
  queueGpuMeshEntity(*entity,
                     registerGpuPickEntity({VisibilityKind::MeshObject,
                                            entity, nullptr}));
}

acgs::math::Vec4 meshEntityRenderMaterial(const MeshEntityRecord &entity);

void appendMeshEntityToScene(const MeshEntityRecord &entity,
                             acgs::AcGsModel &drawList,
                             bool cadAlgorithm = false,
                             const acgs::math::Vec3 *colorOverride = nullptr)
{
  if (!meshEntityVisible(entity))
    return;

  // Imported glTF meshes keep the static-mesh batch: the albedo texture and
  // PBR factors live in the mesh pipeline, which the AcGi geometry toolkit
  // does not carry.
  if (entity.staticMesh)
  {
    if (entity.staticMesh->gpuId == 0)
    {
      if (acgs::acgsGetManager()->deviceReady())
        entity.staticMesh->gpuId =
            acgs::acgsGetManager()->createStaticMesh(entity.staticMesh->cpu);
      if (entity.staticMesh->gpuId == 0)
      {
        static bool warnedStaticUpload = false;
        if (!warnedStaticUpload)
        {
          warnedStaticUpload = true;
          std::cerr << "glTF static mesh upload failed; model skipped"
                    << std::endl;
        }
        return;
      }
    }

    const acgs::math::Vec4 color =
        colorOverride
            ? acgs::math::Vec4(*colorOverride,
                               entity.entity.color.alpha())
            : entity.entity.color.renderColor();
    const acgs::math::Vec4 material = meshEntityRenderMaterial(entity);
    acgs::MeshBatchCommand &batch = drawList.addMeshBatch(
        entity.mesh, color.a >= 1.0f, true /* imported: realistic */, material,
        cadAlgorithm);
    batch.acgiMaterial.algorithm = acgs::AcGiShaderAlgorithm::Realistic;
    batch.acgiMaterial.baseColor = color;
    batch.acgiMaterial.accentColor = entity.entity.material.specularFactor;
    batch.acgiMaterial.metallic = entity.entity.material.metallicFactor;
    batch.acgiMaterial.roughness = entity.entity.material.roughnessFactor;
    batch.acgiMaterial.transparency = 1.0f - color.a;
    batch.acgiMaterial.lineWidth = float(entity.entity.lineWeight);
    // Imported models keep their authored albedo colors: the contrast pass
    // exists for flat demo colors against the CAD background and would tint
    // dark PBR base colors.
    batch.staticMeshId = entity.staticMesh->gpuId;
    batch.diffuseTextureIndex = entity.diffuseTextureIndex;
    batch.instances.push_back(cachedMeshInstance(entity, material, colorOverride));
    return;
  }

  // Prototype meshes draw through the AcGiGeometry mesh toolkit: fill
  // styles emit the prototype soup as a shell face list, wireframe styles
  // emit the feature-edge list as worldLine strokes.  Placement rides the
  // model transform stack — the same uniform scale + translation the former
  // GPU instance carried.  GPU picking keeps rasterizing the record itself
  // (queueGpuMeshPick), so the visible channel is free to move.
  const rendering::RenderModeFlags renderFlags =
      acgs::acgsGetManager()->deviceReady()
          ? acgs::acgsGetManager()->deviceRenderModeFlags()
          : rendering::RenderModeFlags{};
  const acgs::math::Vec4 color =
      colorOverride
          ? acgs::math::Vec4(*colorOverride, meshEntityColor(entity).a)
          : meshEntityColor(entity);

  acgs::ViewportDraw draw(drawList.geometry());
  AcGiWorldDraw graphics(draw);
  acdb::AcDbEntity meshTraits;
  meshTraits.name = entity.entity.name;
  graphics.setFrom(meshTraits);
  graphics.setColor(color);
  graphics.pushModelTransform(entity.worldPosition.asVector());
  graphics.pushModelTransform(AcGeMatrix3d::setToScaling(entity.instanceScale()));

  if (renderFlags.meshFill)
  {
    // The prototype soup is the exact triangle data CPU picking uses, so
    // the shell face list walks it verbatim (three consecutive soup
    // vertices per face) instead of regenerating grid geometry.
    const std::vector<float> &soup =
        rendering::proceduralMeshVertices(entity.mesh);
    const size_t vertexCount =
        soup.size() / rendering::kProceduralMeshFloatStride;
    std::vector<AcGePoint3d> vertices;
    vertices.reserve(vertexCount);
    for (size_t index = 0; index < vertexCount; ++index)
      vertices.emplace_back(soup[index * 8 + 0], soup[index * 8 + 1],
                            soup[index * 8 + 2]);
    std::vector<Adesk::Int32> faceList;
    faceList.reserve(vertexCount / 3 * 4);
    for (size_t vertex = 0; vertex < vertexCount; vertex += 3)
    {
      faceList.push_back(3);
      faceList.push_back(static_cast<Adesk::Int32>(vertex));
      faceList.push_back(static_cast<Adesk::Int32>(vertex + 1));
      faceList.push_back(static_cast<Adesk::Int32>(vertex + 2));
    }
    graphics.shell(static_cast<Adesk::UInt32>(vertexCount), vertices.data(),
                   static_cast<Adesk::UInt32>(faceList.size()),
                   faceList.data());
  }
  else
  {
    // Wireframe styles: the mesh channel used to rasterize the prototype's
    // feature edges from this same line list.
    const std::vector<float> &edges =
        rendering::proceduralMeshFeatureEdges(entity.mesh);
    const size_t edgeCount =
        edges.size() / rendering::kProceduralMeshFloatStride / 2;
    for (size_t index = 0; index < edgeCount; ++index)
    {
      const size_t base = index * 2 * rendering::kProceduralMeshFloatStride;
      graphics.worldLine(
          AcGePoint3d(edges[base + 0], edges[base + 1], edges[base + 2]),
          AcGePoint3d(edges[base + 8], edges[base + 9], edges[base + 10]));
    }
  }
}

acgs::math::Vec4 meshEntityRenderMaterial(const MeshEntityRecord &entity)
{
  // fs_pbr_mesh uniform: x=metallic y=roughness z=emissive w=alphaCutoff.
  // Imported glTF materials map 1:1 (alphaMask=1 marks a MASK material).
  if (entity.staticMesh)
  {
    const acdb::AcDbMaterial &material = entity.entity.material;
    return acgs::math::Vec4(
        material.metallicFactor, material.roughnessFactor,
        material.emissiveFactor.r,
        material.alphaMask > 0.5f ? material.alphaMaskCutoff : 0.0f);
  }
  return entity.realistic()
      ? acgs::math::Vec4(entity.entity.material.metallicFactor,
                  entity.entity.material.roughnessFactor, 0.0f, 0.0f)
      : acgs::math::Vec4(0.0f, 0.35f, 0.0f, 0.0f);
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
    const AcGePoint3d detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    struct ValidationPoint
    {
      AcGeVector3d offset;
      acgs::math::Vec3 color;
      float size;
      rendering::MeshType mesh;
    };
    const ValidationPoint validationPoints[] = {
        {AcGeVector3d(0.0, 256.0, 0.0), acgs::math::Vec3(0.43f, 0.91f, 0.98f),
         512.0f, rendering::MeshType::Cube},
        {AcGeVector3d(0.0, 224.0, 0.0), acgs::math::Vec3(1.0f, 0.58f, 0.25f),
         448.0f, rendering::MeshType::Cube},
        {AcGeVector3d(1920.0, 64.0, 0.0), acgs::math::Vec3(0.95f, 0.95f, 0.95f),
         128.0f, rendering::MeshType::Cube},
        {AcGeVector3d(0.0, 64.0, -1920.0), acgs::math::Vec3(0.95f, 0.95f, 0.95f),
         128.0f, rendering::MeshType::Cube},
        {AcGeVector3d(-288.0, 32.0, -224.0),
         acgs::math::Vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {AcGeVector3d(-96.0, 32.0, -224.0),
         acgs::math::Vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {AcGeVector3d(96.0, 32.0, -224.0),
         acgs::math::Vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
        {AcGeVector3d(288.0, 32.0, -224.0),
         acgs::math::Vec3(1.0f, 0.55f, 0.41f), 64.0f, rendering::MeshType::Cube},
    };
    std::vector<LargeCoordinateObject> objects;
    if (debugValidationMeshesEnabled())
    {
      objects.reserve(std::size(validationPoints));
      for (const ValidationPoint &point : validationPoints)
      {
        MeshEntityRecord object;
        object.worldPosition = detailCenter + point.offset;
        object.entity.color = acgs::math::Vec4(point.color, 0.45f);
        object.size = point.size;
        object.mesh = point.mesh;
        object.entity.name =
            indexedDisplayName(meshDisplayName(object.mesh), objects.size());
        objects.push_back(std::move(object));
      }
    }
    if (realisticMeshEnabled())
    {
      for (size_t i = 0; i < objects.size(); ++i)
      {
        objects[i].entity.style = (i % 2) != 0
                                      ? acdb::MeshStyle::Realistic
                                      : acdb::MeshStyle::Cad;
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
  entity.entity.name = "CenterCube";
  entity.entity.visible =
      !largeCoordinateCameraView || centerCubeForced();
  entity.entity.color = acgs::math::Vec4(1.0f, 0.58f, 0.25f, 1.0f);
  entity.worldPosition = cubeWorldPosition;
  entity.size = 1.0f;
  entity.mesh = rendering::MeshType::Cube;
  return entity;
}

static AcGePoint3d vectorPrimitivesAnchor()
{
  return LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
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
  const acgs::AcGsEntityRange *cadRange = nullptr;
  size_t rangeBegin = 0;
  size_t rangeCount = 0;
  const MeshEntityRecord *mesh = nullptr;
  const acgs::CurveBatchCommand *curve = nullptr;
  acgs::math::DVec3 min{0.0};
  acgs::math::DVec3 max{0.0};
  acgs::math::DVec3 center{0.0};
  double lodSize = 0.0;
  acgs::math::Vec4 overlayColor{1.0f};
  float overlayPointSize = 2.0f;
};

// The scene metafile cache: acgs replays the document into an
// AcGsMetafile; the mesh-instance demo records ride alongside.
struct VectorPrimitivesTessellation : acgs::AcGsMetafile
{
  std::vector<MeshEntityRecord> meshes;
};



// The drawing document backing the demo (AcDbDatabase); every authored
// entity and every imported DWG entity is resident here.  ObjectARX: the
// host initializes the working database
// (acdbHostApplicationServices()->setWorkingDatabase(new AcDbDatabase(true)))
// before application code touches drawing data — main is the host here,
// so initWorkingDatabase() constructs it explicitly at startup.  The
// constructor runs createDefaults(), the ARX createDefault parity
// (layer "0", the linetype family, the default text styles, MODEL/PAPER
// space blocks, the active viewport, the named-object dictionary).
static acdb::AcDbDatabase &initWorkingDatabase()
{
  static acdb::AcDbDatabase workingDatabase;
  return workingDatabase;
}

static acdb::AcDbDatabase &acdbDocument()
{
  return initWorkingDatabase();
}

static void initializeTextResources()
{
  // The host coordinates document text styles with AcGi's font runtime;
  // AcDbDatabase owns the style records but not the renderer-facing engine.
  const std::string sdfFontPath =
      acgi::textEngine().loadSdfFontWithFallback(
          util::resourcePath("fonts/WenQuanWeiMiHei-1.ttf").string(),
          util::resourcePath("fonts/NotoSansLatin.ttf").string());
  const bool sdfFontReady = !sdfFontPath.empty();
  std::cout << "SDF font"
            << (sdfFontReady
                    ? " (" + std::filesystem::path(sdfFontPath).filename().string() +
                          "): loaded"
                    : ": failed")
            << std::endl;

  const bool shxFontLoaded =
      acdbDocument().loadTextStyleShxFonts("SHX", acgi::textEngine());
  std::cout << "SHX fonts (acdb text-style loader): "
            << (shxFontLoaded ? "loaded" : "not loaded") << std::endl;
}

// The in-memory ECS mirror of the demo document ("SQLite 管理 AcDbHandle
// 管文件，ECS 管理 AcDbObjectId 管内存"): one SceneStore +
// DocumentSceneBridge pair bound to acdbDocument() for the process
// lifetime.  First access attaches the reactor; open() bulk-mirrors
// whatever is already resident.  Every later document mutation lands in
// the mirror through the reactor on its own.
static acgs::SceneStore &demoSceneStore()
{
  static acgs::SceneStore store;
  return store;
}

static acgs::DocumentSceneBridge &demoSceneBridge()
{
  static acgs::DocumentSceneBridge bridge(acdbDocument(),
                                          demoSceneStore());
  return bridge;
}

// Attaches the mirror and absorbs pre-existing residents (idempotent).
static void openDemoScene()
{
  demoSceneBridge().open();
}

// Per-frame mirror watcher: one stdout line whenever the mirror revision
// moves (a document mutation landed).  Doubles as the low-cost consumer
// proving the bridge is live in the demo loop.
static void reportDemoSceneMirror()
{
  static std::uint64_t reportedRevision = 0;
  const acgs::SceneStore &store = demoSceneStore();
  const std::uint64_t revision = store.revision();
  if (revision == reportedRevision)
    return;
  reportedRevision = revision;
  std::cout << "scene mirror: revision=" << revision
            << " entities=" << store.count()
            << " dirty=" << store.dirtyCount() << std::endl;
}

static bool g_dwgDialogRequested = false;
static std::filesystem::path g_dwgDialogDirectory;
static std::filesystem::path g_dwgDialogSelection;
static std::vector<std::filesystem::directory_entry> g_dwgDialogEntries;
static std::string g_dwgDialogMessage;
static std::vector<acdb::AcDbHandle> g_dwgImportedHandles;
static void fitCameraToImportedEntities(
    const std::vector<acdb::AcDbHandle> &handles);

static bool replaceDemoDocumentFromDwg(
    const char *filename, acdb::AcDbFileIoReport *report)
{
  acdb::AcDbDatabase replacement;
  if (!replacement.readDwgFile(filename, report))
    return false;

  g_dwgImportedHandles = replacement.modelSpace().entityHandles();
  // Replacement notifies the scene bridge, which mirrors the new document and
  // advances the revision used to rebuild the CAD draw and visibility caches.
  acdbDocument().replaceContents(std::move(replacement));
  return true;
}

static std::string pathToUtf8(const std::filesystem::path &path)
{
  const std::u8string encoded = path.u8string();
  return std::string(reinterpret_cast<const char *>(encoded.data()),
                     encoded.size());
}

static void refreshDwgDialogEntries()
{
  g_dwgDialogEntries.clear();
  std::error_code error;
  std::filesystem::directory_iterator entry(g_dwgDialogDirectory, error);
  const std::filesystem::directory_iterator end;
  for (; !error && entry != end; entry.increment(error))
  {
    std::error_code statusError;
    if (entry->is_directory(statusError))
      g_dwgDialogEntries.push_back(*entry);
    else if (!statusError && entry->is_regular_file(statusError))
    {
      std::string extension = entry->path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                     });
      if (!statusError && extension == ".dwg")
        g_dwgDialogEntries.push_back(*entry);
    }
  }
  std::sort(g_dwgDialogEntries.begin(), g_dwgDialogEntries.end(),
            [](const auto &left, const auto &right) {
              std::error_code leftError;
              std::error_code rightError;
              const bool leftDirectory = left.is_directory(leftError);
              const bool rightDirectory = right.is_directory(rightError);
              if (leftDirectory != rightDirectory)
                return leftDirectory;
              return pathToUtf8(left.path().filename()) <
                     pathToUtf8(right.path().filename());
            });
}

static void drawDwgFileDialog()
{
  if (!appPlatform.imguiEnabled())
  {
    g_dwgDialogRequested = false;
    return;
  }

  constexpr const char *popupName = "Open DWG Drawing";
  if (g_dwgDialogRequested)
  {
    std::error_code error;
    g_dwgDialogDirectory = std::filesystem::current_path(error);
    if (error)
      g_dwgDialogDirectory = std::filesystem::path(".");
    g_dwgDialogSelection.clear();
    g_dwgDialogMessage.clear();
    refreshDwgDialogEntries();
    ImGui::OpenPopup(popupName);
    g_dwgDialogRequested = false;
  }

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
  bool open = true;
  if (!ImGui::BeginPopupModal(popupName, &open,
                              ImGuiWindowFlags_NoSavedSettings))
    return;

  ImGui::TextUnformatted("DWG files");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", pathToUtf8(g_dwgDialogDirectory).c_str());
  if (ImGui::Button("Up") && g_dwgDialogDirectory.has_parent_path())
  {
    g_dwgDialogDirectory = g_dwgDialogDirectory.parent_path();
    g_dwgDialogSelection.clear();
    g_dwgDialogMessage.clear();
    refreshDwgDialogEntries();
  }

  ImGui::Separator();
  if (ImGui::BeginChild("##dwg-file-list", ImVec2(0.0f, -72.0f),
                        ImGuiChildFlags_Border))
  {
    if (g_dwgDialogEntries.empty())
      ImGui::TextDisabled("No folders or DWG files in this directory.");
    for (const auto &entry : g_dwgDialogEntries)
    {
      const std::filesystem::path path = entry.path();
      std::error_code statusError;
      const bool isDirectory = entry.is_directory(statusError);
      const std::string name = pathToUtf8(path.filename());
      const bool selected = !isDirectory && path == g_dwgDialogSelection;
      const std::string label =
          std::string(isDirectory ? "[Folder] " : "[DWG] ") + name;
      if (ImGui::Selectable(label.c_str(), selected))
      {
        if (isDirectory)
        {
          g_dwgDialogDirectory = path;
          g_dwgDialogSelection.clear();
          g_dwgDialogMessage.clear();
          refreshDwgDialogEntries();
          ImGui::SetScrollHereY(0.0f);
          break;
        }
        g_dwgDialogSelection = path;
      }
    }
  }
  ImGui::EndChild();

  if (!g_dwgDialogMessage.empty())
    ImGui::TextWrapped("%s", g_dwgDialogMessage.c_str());
  const bool canOpen = !g_dwgDialogSelection.empty();
  ImGui::BeginDisabled(!canOpen);
  if (ImGui::Button("Open"))
  {
    acdb::AcDbFileIoReport report;
    const std::string filename = g_dwgDialogSelection.string();
    if (replaceDemoDocumentFromDwg(filename.c_str(), &report))
    {
      fitCameraToImportedEntities(g_dwgImportedHandles);
      std::cout << "DWG: inserted " << report.entitiesAdded << " entities ("
                << report.unsupportedSkipped << " unsupported) from "
                << filename << std::endl;
      ImGui::CloseCurrentPopup();
    }
    else
    {
      g_dwgDialogMessage =
          "Could not decode this DWG (error " +
          std::to_string(report.errorCode) + ").";
      std::cerr << "DWG: failed to decode " << filename << " (error "
                << report.errorCode << ")" << std::endl;
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Cancel"))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}



// Renders one block record's model-space instances through the shared
// tessellation: the nested walk composes each member's world transform,
// the payload copy is translated (ObjectARX transformBy), and the result
// feeds appendVectorPrimitive like any authored entity.  Rotations and
// non-uniform scales of curved geometry degrade through translation-only
// placement for now.

// Demo block: a two-stroke bracket block inserted at three positions
// (GRID_BLOCKS=1).  Exercises createBlockDefinition / addBlockReference /
// nested-instance rendering end to end.
static void appendDemoBlocks()
{
  acdb::AcDbDatabase &document = acdbDocument();
  if (document.blockTable().contains("DEMO_BRACKET"))
    return; // builder cache may rerun; keep the definition unique

  const AcGePoint3d blockAnchor = vectorPrimitivesAnchor();
  const double base = 240.0;
  acdb::AcDbLine left;
  left.color = acgs::math::Vec4(0.30f, 0.85f, 0.55f, 1.0f);
  left.start = blockAnchor;
  left.end = blockAnchor + AcGeVector3d(0.0, base, 0.0);
  acdb::AcDbLine bottom;
  bottom.color = acgs::math::Vec4(0.30f, 0.85f, 0.55f, 1.0f);
  bottom.start = blockAnchor;
  bottom.end = blockAnchor + AcGeVector3d(base, 0.0, 0.0);
  const acdb::AcDbHandle leftHandle = document.addEntity(left);
  const acdb::AcDbHandle bottomHandle = document.addEntity(bottom);

  const acdb::AcDbHandle record = document.createBlockDefinition(
      "DEMO_BRACKET", blockAnchor, {leftHandle, bottomHandle});
  if (!record.isValid())
    return;
  // The members left model space with the definition; put the two
  // instances where the demo can see them.
  for (const AcGePoint3d position :
       {blockAnchor + AcGeVector3d(512.0, 96.0, 0.0),
        blockAnchor + AcGeVector3d(512.0, 416.0, 0.0),
        blockAnchor + AcGeVector3d(512.0, 736.0, 0.0)})
  {
    document.addBlockReference("DEMO_BRACKET", position);
  }
}

// The CAD vector demo is authored as formal entities.  The cached draw list is
// shared by drawing and CPU picking; dynamic documents replace this builder's
// revision with a dirty-document notification.
// ---- demo document (AcDb): the drawing is the single source of truth --





static AcGePoint3d demoAnchorPoint()
{
  return vectorPrimitivesAnchor();
}

static AcGePoint3d demoCadAnchor()
{
  return vectorPrimitivesAnchor() + AcGeVector3d(1536.0, -1280.0, 0.0);
}

// Authors one demo entity into the document under a stable label; the
// label doubles as the acgs::AcGsEntityRange key for picking and selection.
// fillIs3DFace lands on the entity itself (entity data drives drawing —
// ARX style); only payloads carrying the field accept it.
template <typename EntityType>
static void addDemo(EntityType entity, const std::string &name,
                    bool fillIs3DFace = false,
                    acgs::AcGsPickShape pickShape =
                        acgs::AcGsPickShape::Primitives,
                    bool excludeFromFit = false)
{
  entity.name = name;
  if constexpr (requires(EntityType &e) { e.fillIs3DFace; })
    entity.fillIs3DFace = fillIs3DFace;
  acgs::AcGsReplayHints hints;
  hints.pickShape = pickShape;
  hints.excludeFromFit = excludeFromFit;
  acgs::replayHints()[name] = hints;
  acdbDocument().addEntity(std::move(entity));
}

// ---- glTF demo (GRID_GLTF / G key) ----------------------------------------
//
// A glTF model imports as one AcDbPolyFaceMesh per primitive (PBR factors +
// base-color texture), normalizes around a demo placement, and renders
// through the realistic mesh pipeline (albedo texture + metallic/roughness +
// headlight/point lights).  GRID_GLTF=<file> loads at startup; G cycles the
// models found in resources/models; Ctrl+G (or GRID_GLTF_EXPORT=1) writes
// the loaded model back out as .glb and .gltf.

constexpr double kGltfDemoTargetRadius = 240.0;

static std::filesystem::path gGltfActivePath;
static std::uint64_t gGltfModelSerial = 0;

// GRID_GLTF=<path|file.glb> queues a model at startup: an absolute path, or
// a bare file name resolved under resources/models.  GRID_GLTF=0 disables
// imported models entirely; with no GRID_GLTF at all the demo loads
// duck_textured.glb (falling back to duck.glb when the generated asset is
// absent) at the large-coordinate placement, so the default startup scene
// carries a textured imported model.
static const bool gGltfEnvResolved = [] {
  const char *value = std::getenv("GRID_GLTF");
  if (value && std::strcmp(value, "0") == 0)
    return false;
  std::filesystem::path candidate;
  if (value && *value != '\0')
  {
    candidate = value;
    if (!std::filesystem::exists(candidate))
      candidate = util::resourcePath("models") / candidate;
    if (!std::filesystem::exists(candidate))
    {
      std::cout << "GRID_GLTF: model not found: " << value << std::endl;
      return true;
    }
  }
  else
  {
    const std::filesystem::path models = util::resourcePath("models");
    candidate = models / "duck_textured.glb";
    if (!std::filesystem::exists(candidate))
      candidate = models / "duck.glb";
    if (!std::filesystem::exists(candidate))
      return false;
  }
  gGltfActivePath = candidate;
  gGltfModelSerial = 1;
  return true;
}();

static AcGePoint3d gltfDemoPlacement()
{
  // Inside the large-coordinate stress field, off to the side of the demo
  // cube cluster so the startup fit-all camera frames the model.
  return demoAnchorPoint() + AcGeVector3d(768.0, -1024.0, 0.0);
}

static std::vector<std::filesystem::path> listGltfDemoModels()
{
  std::vector<std::filesystem::path> models;
  std::error_code ec;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::directory_iterator(util::resourcePath("models"), ec))
  {
    const std::string extension = entry.path().extension().string();
    const bool gltfFile = extension == ".glb" || extension == ".gltf";
    if (gltfFile)
      models.push_back(entry.path());
  }
  std::sort(models.begin(), models.end(),
            [](const std::filesystem::path &a, const std::filesystem::path &b) {
              return a.filename().string() < b.filename().string();
            });
  return models;
}

struct GltfDemoModelAsset
{
  acdb::AcDbGltfImport import;
  std::vector<std::shared_ptr<GltfStaticMesh>> meshes; // per import.meshes
  std::string name;                                    // file stem
  double scale = 1.0; // instance scale: target radius / model radius
  bool valid = false;
};

// Parses once per model file (path-keyed); rebuilds copy records out of it.
static const GltfDemoModelAsset &gltfDemoAsset(
    const std::filesystem::path &path)
{
  static std::map<std::string, GltfDemoModelAsset> cache;
  const std::string key = path.filename().string();
  auto [entry, inserted] = cache.try_emplace(key);
  if (!inserted)
    return entry->second;

  GltfDemoModelAsset &asset = entry->second;
  std::string error;
  if (!acdb::importGltf(path, asset.import, &error))
  {
    std::cerr << "GRID_GLTF: " << error << std::endl;
    return asset;
  }
  asset.name = path.stem().string();

  const acgs::math::DVec3 extent = asset.import.maximum - asset.import.minimum;
  const double radius =
      0.5 * std::max(extent.x, std::max(extent.y, extent.z));
  asset.scale = radius > 0.0 ? kGltfDemoTargetRadius / radius : 1.0;

  asset.meshes.resize(asset.import.meshes.size());
  for (size_t i = 0; i < asset.import.meshes.size(); ++i)
  {
    const acdb::AcDbPolyFaceMeshGeometry &geometry =
        asset.import.meshes[i].geometry;
    auto staticMesh = std::make_shared<GltfStaticMesh>();
    const size_t vertexCount = geometry.positions.size();
    staticMesh->cpu.positionU.resize(vertexCount);
    staticMesh->cpu.normalV.resize(vertexCount);
    const bool hasUvs = geometry.uvs.size() == vertexCount;
    const bool hasNormals = geometry.normals.size() == vertexCount;
    for (size_t v = 0; v < vertexCount; ++v)
    {
      const AcGePoint3d &position = geometry.positions[v];
      const acgs::math::Vec2 uv = hasUvs ? geometry.uvs[v] : acgs::math::Vec2(0.0f);
      // The normalization scale bakes into the vertices so the rendered
      // geometry is final-size and the instance only translates.
      staticMesh->cpu.positionU[v] = acgs::math::Vec4(
          float(position.x) * float(asset.scale),
          float(position.y) * float(asset.scale),
          float(position.z) * float(asset.scale), uv.x);
      const acgs::math::Vec3 normal = hasNormals ? geometry.normals[v]
                                          : acgs::math::Vec3(0.0f, 0.0f, 1.0f);
      staticMesh->cpu.normalV[v] = acgs::math::Vec4(normal, uv.y);
    }
    staticMesh->cpu.indices = geometry.indices;
    asset.meshes[i] = std::move(staticMesh);
  }

  asset.valid = true;
  std::cout << "GRID_GLTF: imported " << asset.import.meshes.size()
            << " primitives from " << path.filename().string();
  if (!asset.import.textures.empty())
    std::cout << ", " << asset.import.textures.size() << " textures";
  if (!asset.import.skipped.empty())
    std::cout << ", " << asset.import.skipped.size() << " skipped";
  std::cout << std::endl;
  return asset;
}

// Renderer texture slot per glTF texture key; the device owns the GPU
// handles, the resolution cache is keyed by string and survives rebuilds.
static std::uint32_t gltfDemoTextureIndex(const GltfDemoModelAsset &asset,
                                          const std::string &key)
{
  static std::unordered_map<std::string, std::uint32_t> resolved;
  if (key.empty())
    return 0;
  const auto found = resolved.find(key);
  if (found != resolved.end())
    return found->second;
  if (!acgs::acgsGetManager()->deviceReady())
    return 0;
  for (const acdb::AcDbGltfTexture &texture : asset.import.textures)
  {
    if (texture.key != key)
      continue;
    const std::uint32_t index = acgs::acgsGetManager()->loadMeshTextureFromMemory(
        key, texture.bytes.data(), texture.bytes.size());
    if (index != 0)
      resolved.emplace(key, index);
    return index;
  }
  return 0;
}

// External naming-convention materials (the CADplatformer model set ships
// textures next to the glb, not inside it): <name>_bcr carries RGB=baseColor
// + A=roughness, <name>_nmao packs octahedral normal xy + metal + AO, and
// parameter-sweep assets (sphere) live under a same-named folder indexed by
// <i>_<j>.  Models without any matching file fall back to sphere/sphere_0_0
// -- exactly how CADplatformer's levels dress duck.glb -- so every import
// renders with the glossy red stand-in material.  Only the bcr slot feeds
// the current PBR shader (albedo); the nmao channels wait for a normal-map
// sampling path.
static std::uint32_t gltfExternalTextureIndex(const std::string &name,
                                              bool *isFallback = nullptr)
{
  static std::unordered_map<std::string,
                            std::pair<std::uint32_t, bool>>
      resolved; // name -> {texture index, came from the stand-in material}
  if (isFallback)
    *isFallback = false;
  if (name.empty())
    return 0;
  const auto found = resolved.find(name);
  if (found != resolved.end())
  {
    if (isFallback)
      *isFallback = found->second.second;
    return found->second.first;
  }

  const std::filesystem::path textures = util::resourcePath("textures");
  const std::filesystem::path fallback = textures / "sphere" /
                                         "sphere_0_0_bcr.png";
  const std::filesystem::path candidates[] = {
      textures / (name + "_bcr.png"),
      textures / (name + "_bcr.dds"),
      textures / name / (name + "_0_0_bcr.png"),
      textures / name / (name + "_0_0_bcr.dds"),
      fallback,
      textures / "sphere" / "sphere_0_0_bcr.dds",
  };
  for (const std::filesystem::path &candidate : candidates)
  {
    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec))
      continue;
    const std::uint32_t index =
        acgs::acgsGetManager()->loadMeshTexture(candidate.string());
    if (index != 0)
    {
      const bool standIn =
          candidate == fallback ||
          candidate == textures / "sphere" / "sphere_0_0_bcr.dds";
      resolved.emplace(name, std::make_pair(index, standIn));
      if (isFallback)
        *isFallback = standIn;
      std::cout << "GRID_GLTF: external material texture for " << name
                << " -> " << candidate.filename().string()
                << " (index=" << index << ")" << std::endl;
      return index;
    }
    std::cerr << "GRID_GLTF: failed to load external material texture "
              << candidate.string() << std::endl;
  }
  resolved.emplace(name, std::make_pair(0, false));
  return 0;
}

// Exports glTF-imported records back to .glb and .gltf under
// resources/models/export/.  Takes the records directly: the startup export
// runs while the tessellation is still being built, so it must not re-query
// the (not yet cached) draw list.
static void exportGltfDemoRecords(
    const std::vector<MeshEntityRecord> &records)
{
  std::vector<const acdb::AcDbPolyFaceMesh *> meshes;
  for (const MeshEntityRecord &record : records)
  {
    if (record.staticMesh)
      meshes.push_back(&record.entity);
  }
  if (meshes.empty())
  {
    std::cout << "GRID_GLTF: nothing to export (load a model with G first)"
              << std::endl;
    return;
  }

  const std::filesystem::path exportDir =
      util::resourcePath("models") / "export";
  std::error_code ec;
  std::filesystem::create_directories(exportDir, ec);
  const std::string stem = gGltfActivePath.empty()
                               ? std::string("gltf_export")
                               : gGltfActivePath.stem().string();

  acdb::AcDbGltfExportOptions options;
  std::string error;
  const std::filesystem::path glbPath = exportDir / (stem + "_export.glb");
  options.binary = true;
  if (acdb::exportGltf(meshes, glbPath, options, &error))
    std::cout << "GRID_GLTF: exported " << meshes.size() << " meshes -> "
              << glbPath.string() << std::endl;
  else
    std::cerr << "GRID_GLTF: glb export failed: " << error << std::endl;

  const std::filesystem::path gltfPath = exportDir / (stem + "_export.gltf");
  options.binary = false;
  if (acdb::exportGltf(meshes, gltfPath, options, &error))
    std::cout << "GRID_GLTF: exported " << meshes.size() << " meshes -> "
              << gltfPath.string() << std::endl;
  else
    std::cerr << "GRID_GLTF: gltf export failed: " << error << std::endl;
}

// The active glTF model lives in the document as AcDbPolyFaceMesh
// entities — the document is the source of truth (GRID_SAVE_DWG captures
// them, the revision system owns invalidation).  The static-mesh GPU cache
// stays rendering-side, keyed by entity handle, and the Gs replay skips
// these handles: the records below feed the textured mesh channel instead.
static std::vector<acdb::AcDbHandle> gGltfDocumentHandles;
static std::filesystem::path gGltfLoadedPath;

static std::map<acdb::AcDbHandle, std::shared_ptr<GltfStaticMesh>>
    &gltfStaticMeshByHandle()
{
  static std::map<acdb::AcDbHandle, std::shared_ptr<GltfStaticMesh>> cache;
  return cache;
}

static void loadGltfModelIntoDocument()
{
  if (gGltfActivePath.empty() || gGltfLoadedPath == gGltfActivePath)
    return;
  acdb::AcDbDatabase &document = acdbDocument();
  for (const acdb::AcDbHandle handle : gGltfDocumentHandles)
  {
    document.removeEntity(handle);
    gltfStaticMeshByHandle().erase(handle);
  }
  gGltfDocumentHandles.clear();
  gGltfLoadedPath = gGltfActivePath;

  const GltfDemoModelAsset &asset = gltfDemoAsset(gGltfActivePath);
  if (!asset.valid)
    return;
  for (size_t i = 0; i < asset.import.meshes.size(); ++i)
  {
    acdb::AcDbPolyFaceMesh entity = asset.import.meshes[i];
    if (entity.name.empty())
      entity.name = asset.name + "/" + std::to_string(i);
    const acdb::AcDbHandle handle = document.addEntity(std::move(entity));
    gGltfDocumentHandles.push_back(handle);
    gltfStaticMeshByHandle()[handle] = asset.meshes[i];
  }
}

static void appendGltfDemoMeshes(VectorPrimitivesTessellation &target)
{
  loadGltfModelIntoDocument();
  if (gGltfDocumentHandles.empty())
    return;
  const GltfDemoModelAsset &asset = gltfDemoAsset(gGltfActivePath);
  if (!asset.valid)
    return;
  const acdb::AcDbDatabase &document = acdbDocument();
  const AcGePoint3d placement = gltfDemoPlacement();
  for (const acdb::AcDbHandle handle : gGltfDocumentHandles)
  {
    const acdb::AcDbEntityVariant *payload = document.getEntity(handle);
    if (payload == nullptr)
      continue;
    const auto *entity = std::get_if<acdb::AcDbPolyFaceMesh>(payload);
    if (entity == nullptr)
      continue;
    const auto staticIt = gltfStaticMeshByHandle().find(handle);
    if (staticIt == gltfStaticMeshByHandle().end())
      continue;
    MeshEntityRecord record;
    record.entity = *entity;
    record.worldPosition = placement;
    // |size| is the visibility-candidate extent (makeMeshCandidate uses
    // size/2 as the half extent); the normalization already lives in the
    // vertices, so the instance draws at scale 1.
    record.size = float(2.0 * kGltfDemoTargetRadius);
    record.renderScale = 1.0f;
    record.staticMesh = staticIt->second;
    const std::string &textureKey = record.entity.material.diffuseTexture;
    if (!textureKey.empty())
    {
      record.diffuseTextureIndex = gltfDemoTextureIndex(asset, textureKey);
    }
    else
    {
      // No embedded texture: fall back to the external naming-convention
      // material, then to CADplatformer's stand-in (sphere/sphere_0_0, the
      // glossy red).  The convention packs roughness into the texture's
      // alpha, which the current shader ignores, so the factors are chosen
      // per hit: authored assets read matte, the stand-in reads glossy.
      bool standIn = false;
      record.diffuseTextureIndex =
          gltfExternalTextureIndex(asset.name, &standIn);
      if (record.diffuseTextureIndex != 0)
      {
        record.entity.material.metallicFactor = 0.05f;
        record.entity.material.roughnessFactor = standIn ? 0.25f : 0.6f;
      }
    }
    target.meshes.push_back(std::move(record));
  }

  static const bool exportAtStartup = [] {
    const char *value = std::getenv("GRID_GLTF_EXPORT");
    return value != nullptr && *value != '\0' &&
           std::strcmp(value, "0") != 0;
  }();
  static const bool exportedAtStartup = [&] {
    if (exportAtStartup)
      exportGltfDemoRecords(target.meshes);
    return true;
  }();
  (void)exportedAtStartup;
}

static void cycleGltfDemoModel()
{
  const std::vector<std::filesystem::path> models = listGltfDemoModels();
  if (models.empty())
  {
    std::cout << "GRID_GLTF: no .glb/.gltf files under resources/models"
              << std::endl;
    return;
  }
  std::filesystem::path next = models.front();
  if (!gGltfActivePath.empty())
  {
    for (size_t i = 0; i < models.size(); ++i)
    {
      if (std::filesystem::equivalent(models[i], gGltfActivePath))
      {
        next = models[(i + 1) % models.size()];
        break;
      }
    }
  }
  gGltfActivePath = next;
  ++gGltfModelSerial; // bumps the draw-list key; rebuild imports the model
  const AcGePoint3d placement = gltfDemoPlacement();
  std::cout << "GRID_GLTF: queued " << next.filename().string()
            << " (place at " << placement.x << ", " << placement.y << ", "
            << placement.z << "; G cycles, Ctrl+G exports)" << std::endl;
}



// Authors the demo content once per process: guard entities, DWG
// import, and block definitions all land in the document here.
static void buildDemoDocument()
{
  // Author exactly once; the tessellation cache may rebuild many
  // times but the document only ever receives one copy.
  static const bool authored = []() -> bool {
    // Attach the ECS mirror before authoring: the reactor observes every
    // add below, and open() absorbs anything resident already (DWG
    // import, builder reruns).
    openDemoScene();
    acdb::AcDbDatabase &document = acdbDocument();
    const AcGePoint3d cadAnchor = demoCadAnchor();
    const AcGePoint3d demoAnchor = demoAnchorPoint();

    acdb::AcDbLine line;
    line.color = acgs::math::Vec4(1.0f, 0.24f, 0.20f, 1.0f);
    line.start = cadAnchor;
    line.end = cadAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    addDemo(line, "Line");

    acdb::AcDbArc arc;
    arc.color = acgs::math::Vec4(1.0f, 0.52f, 0.10f, 1.0f);
    arc.center = cadAnchor + AcGeVector3d(1024.0, 256.0, 0.0);
    arc.radius = 192.0;
    arc.startAngle = 0.0;
    arc.endAngle = acgs::math::radians(270.0);
    addDemo(arc, "Arc");

    acdb::AcDbCircle circle;
    circle.color = acgs::math::Vec4(0.20f, 0.60f, 0.90f, 1.0f);
    circle.center = cadAnchor + AcGeVector3d(256.0, 512.0, 0.0);
    circle.radius = 160.0;
    addDemo(circle, "Circle");

    acdb::AcDbEllipse ellipse;
    ellipse.color = acgs::math::Vec4(0.65f, 0.30f, 0.85f, 1.0f);
    ellipse.center = cadAnchor + AcGeVector3d(768.0, 640.0, 0.0);
    ellipse.majorAxis = AcGeVector3d(224.0, 0.0, 0.0);
    ellipse.radiusRatio = 0.55;
    addDemo(ellipse, "Ellipse");

    // Partial ellipse: the parameter range draws an elliptical arc instead
    // of the closed curve.
    acdb::AcDbEllipse ellipseArc;
    ellipseArc.color = acgs::math::Vec4(0.75f, 0.40f, 0.95f, 1.0f);
    ellipseArc.center = cadAnchor + AcGeVector3d(1536.0, -640.0, 0.0);
    ellipseArc.majorAxis = AcGeVector3d(160.0, 0.0, 0.0);
    ellipseArc.radiusRatio = 0.55;
    ellipseArc.startParameter = acgs::math::pi<double>() * 0.25;
    ellipseArc.endParameter = acgs::math::pi<double>() * 1.75;
    addDemo(ellipseArc, "EllipseArc");

    acdb::AcDb3dPolyline polyline;
    polyline.color = acgs::math::Vec4(0.60f, 0.20f, 1.00f, 1.0f);
    polyline.vertices = {
        cadAnchor + AcGeVector3d(-384.0, 128.0, 0.0),
        cadAnchor + AcGeVector3d(-128.0, 384.0, 0.0),
        cadAnchor + AcGeVector3d(128.0, 256.0, 0.0),
        cadAnchor + AcGeVector3d(384.0, 512.0, 0.0)};
    polyline.bulges = {0.25, 0.0, -0.35};
    addDemo(polyline, "Polyline");

    acdb::AcDbPolyline lwpolyline;
    lwpolyline.color = acgs::math::Vec4(0.25f, 0.80f, 0.45f, 1.0f);
    lwpolyline.vertices = {AcGePoint2d(-256.0, -384.0),
                           AcGePoint2d(0.0, -192.0),
                           AcGePoint2d(256.0, -448.0)};
    lwpolyline.elevation = cadAnchor.z;
    lwpolyline.closed = true;
    for (AcGePoint2d &vertex : lwpolyline.vertices)
      vertex += AcGeVector2d(cadAnchor.x, cadAnchor.y);
    addDemo(lwpolyline, "LwPolyline");

    acdb::AcDbSpline spline;
    spline.color = acgs::math::Vec4(0.90f, 0.70f, 0.20f, 1.0f);
    spline.degree = 3;
    spline.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    spline.controlPoints = {
        cadAnchor + AcGeVector3d(-768.0, 512.0, 0.0),
        cadAnchor + AcGeVector3d(-512.0, 896.0, 128.0),
        cadAnchor + AcGeVector3d(-256.0, 384.0, -128.0),
        cadAnchor + AcGeVector3d(0.0, 768.0, 0.0)};
    addDemo(spline, "Spline");

    acdb::AcDbHatch hatch;
    hatch.color = acgs::math::Vec4(0.30f, 0.80f, 0.50f, 0.75f);
    hatch.outerLoop = {
        cadAnchor + AcGeVector3d(1024.0, -384.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -384.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -128.0, 0.0),
        cadAnchor + AcGeVector3d(1024.0, -128.0, 0.0)};
    addDemo(hatch, "Hatch");

    // ANSI31 line pattern at unit scale; the inner loop punches a hole via
    // the even-odd rule.
    acdb::AcDbHatch patternHatch;
    patternHatch.color = acgs::math::Vec4(0.30f, 0.80f, 0.50f, 0.90f);
    patternHatch.solidFill = false;
    patternHatch.patternName = "ANSI31";
    patternHatch.patternScale = 1.0;
    patternHatch.patternAngle = 0.0;
    patternHatch.outerLoop = {
        cadAnchor + AcGeVector3d(1024.0, -768.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -768.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -512.0, 0.0),
        cadAnchor + AcGeVector3d(1024.0, -512.0, 0.0)};
    patternHatch.innerLoops = {{
        cadAnchor + AcGeVector3d(1152.0, -704.0, 0.0),
        cadAnchor + AcGeVector3d(1280.0, -704.0, 0.0),
        cadAnchor + AcGeVector3d(1280.0, -576.0, 0.0),
        cadAnchor + AcGeVector3d(1152.0, -576.0, 0.0)}};
    addDemo(patternHatch, "PatternHatch");

    // Same pattern family rotated 45 degrees and widened by patternScale.
    acdb::AcDbHatch angledHatch;
    angledHatch.color = acgs::math::Vec4(0.35f, 0.55f, 0.95f, 0.90f);
    angledHatch.solidFill = false;
    angledHatch.patternName = "ANSI31";
    angledHatch.patternScale = 2.0;
    angledHatch.patternAngle = acgs::math::pi<double>() * 0.25;
    angledHatch.outerLoop = {
        cadAnchor + AcGeVector3d(1024.0, -1024.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -1024.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -832.0, 0.0),
        cadAnchor + AcGeVector3d(1024.0, -832.0, 0.0)};
    addDemo(angledHatch, "AngledHatch");

    acdb::AcDbSolid solid;
    solid.color = acgs::math::Vec4(0.50f, 0.50f, 0.90f, 0.85f);
    solid.firstCorner = cadAnchor + AcGeVector3d(0.0, -128.0, 256.0);
    solid.secondCorner = solid.firstCorner + AcGeVector3d(512.0, 0.0, 0.0);
    solid.thirdCorner = solid.firstCorner + AcGeVector3d(0.0, 384.0, 0.0);
    solid.fourthCorner = solid.firstCorner + AcGeVector3d(512.0, 384.0, 0.0);
    addDemo(solid, "Solid");

    acdb::AcDbRay ray;
    ray.color = acgs::math::Vec4(0.10f, 0.85f, 0.75f, 1.0f);
    ray.start = cadAnchor + AcGeVector3d(-640.0, -768.0, 0.0);
    ray.direction = AcGeVector3d(1.0, 0.25, 0.0);
    addDemo(ray, "Ray");

    acdb::AcDbXline xline;
    xline.color = acgs::math::Vec4(0.65f, 0.35f, 0.95f, 1.0f);
    xline.point = cadAnchor + AcGeVector3d(256.0, -1152.0, 0.0);
    xline.direction = AcGeVector3d(2.0, -1.0, 0.0);
    addDemo(xline, "XLine");

    // SHX-styled text: an ordinary AcDbText entity — like every text
    // entity it renders through its own subWorldDraw into the AcGi text
    // engine.  The style name is entity data: it selects the SHX vector
    // fonts the acdb system loaded (stroke glyphs) when bound, and falls
    // back to the SDF engine otherwise.
    {
      acdb::AcDbText shxText;
      shxText.color = acgs::math::Vec4(0.95f, 0.85f, 0.30f, 1.0f);
      shxText.styleName = "SHX";
      // SHX glyphs hang below their anchor (cap line at the anchor,
      // baseline one em down): the insertion is the cap line.
      shxText.insertion = cadAnchor + AcGeVector3d(256.0, 128.0, 96.0);
      shxText.height = 96.0; // world units per em
      const AcString shxMessage = L"\u4E2D\u6587 INFINITE - GRID 123";
      shxText.text = shxMessage;
      addDemo(std::move(shxText), "ShxText");
    }

    acdb::AcDbMline mline;
    mline.color = acgs::math::Vec4(0.85f, 0.35f, 0.35f, 0.95f);
    mline.vertices = {
        cadAnchor + AcGeVector3d(-256.0, -768.0, 0.0),
        cadAnchor + AcGeVector3d(256.0, -704.0, 0.0),
        cadAnchor + AcGeVector3d(768.0, -832.0, 0.0)};
    mline.scale = AcGeVector3d(24.0, 1.0, 1.0);
    addDemo(mline, "MLine", false, acgs::AcGsPickShape::PairedStrokeBand);

    // Closed multi-line: the offset band wraps around and the enclosed
    // strip is filled between the two boundary strokes.
    acdb::AcDbMline closedMLine;
    closedMLine.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 0.95f);
    closedMLine.vertices = {
        cadAnchor + AcGeVector3d(1472.0, -880.0, 0.0),
        cadAnchor + AcGeVector3d(1728.0, -960.0, 0.0),
        cadAnchor + AcGeVector3d(1600.0, -1088.0, 0.0)};
    closedMLine.scale = AcGeVector3d(40.0, 1.0, 1.0);
    closedMLine.closed = true;
    addDemo(closedMLine, "ClosedMLine", false, acgs::AcGsPickShape::PairedStrokeBand);

    // Closed polyline with non-zero thickness: the outline extrudes into
    // wall quads along the normal.
    acdb::AcDb3dPolyline borderedPolyline;
    borderedPolyline.color = acgs::math::Vec4(0.95f, 0.45f, 0.15f, 1.0f);
    borderedPolyline.vertices = {
        cadAnchor + AcGeVector3d(1792.0, -576.0, 0.0),
        cadAnchor + AcGeVector3d(1984.0, -576.0, 0.0),
        cadAnchor + AcGeVector3d(1984.0, -736.0, 0.0),
        cadAnchor + AcGeVector3d(1792.0, -736.0, 0.0)};
    borderedPolyline.closed = true;
    borderedPolyline.thickness = 48.0;
    addDemo(borderedPolyline, "BorderedPolyline");

    // Fit-point splines: the C1 fallback interpolates the fit points, open
    // and closed forms.
    acdb::AcDbSpline fitSpline;
    fitSpline.color = acgs::math::Vec4(0.90f, 0.70f, 0.20f, 1.0f);
    fitSpline.degree = 3;
    fitSpline.fitPoints = {
        cadAnchor + AcGeVector3d(1024.0, -1152.0, 0.0),
        cadAnchor + AcGeVector3d(1152.0, -1024.0, 0.0),
        cadAnchor + AcGeVector3d(1280.0, -1216.0, 0.0),
        cadAnchor + AcGeVector3d(1408.0, -1088.0, 0.0)};
    addDemo(fitSpline, "FitSpline");

    acdb::AcDbSpline closedFitSpline;
    closedFitSpline.color = acgs::math::Vec4(0.55f, 0.85f, 0.25f, 1.0f);
    closedFitSpline.degree = 3;
    closedFitSpline.closed = true;
    closedFitSpline.fitPoints = {
        cadAnchor + AcGeVector3d(1024.0, -1408.0, 160.0),
        cadAnchor + AcGeVector3d(1152.0, -1296.0, -160.0),
        cadAnchor + AcGeVector3d(1312.0, -1424.0, 288.0),
        cadAnchor + AcGeVector3d(1152.0, -1520.0, -96.0)};
    addDemo(closedFitSpline, "ClosedFitSpline");

    // The closed ring through the same points: the interpolation visibly
    // smooths the corners of this reference polygon.
    acdb::AcDb3dPolyline closedFitRing;
    closedFitRing.color = acgs::math::Vec4(0.55f, 0.55f, 0.55f, 0.9f);
    closedFitRing.vertices = closedFitSpline.fitPoints;
    closedFitRing.closed = true;
    addDemo(closedFitRing, "ClosedFitRing");

    // Non-planar fit spline: the fit points span all three dimensions, so
    // the curve bends out of the ground plane (the fit-point demos above
    // are flat for comparison).
    acdb::AcDbSpline spaceSpline;
    spaceSpline.color = acgs::math::Vec4(0.30f, 0.65f, 0.95f, 1.0f);
    spaceSpline.degree = 3;
    spaceSpline.fitPoints = {
        cadAnchor + AcGeVector3d(1024.0, -1152.0, 480.0),
        cadAnchor + AcGeVector3d(1152.0, -1024.0, -480.0),
        cadAnchor + AcGeVector3d(1280.0, -1216.0, 768.0),
        cadAnchor + AcGeVector3d(1408.0, -1088.0, -320.0),
        cadAnchor + AcGeVector3d(1536.0, -1216.0, 640.0)};
    addDemo(spaceSpline, "SpaceSpline");

    acdb::AcDbPoint cadPoint;
    cadPoint.color = acgs::math::Vec4(0.95f, 0.95f, 0.95f, 1.0f);
    cadPoint.location = cadAnchor + AcGeVector3d(512.0, 0.0, 0.0);
    addDemo(cadPoint, "Point");

    acdb::AcDbText text;
    text.color = acgs::math::Vec4(0.95f, 0.95f, 0.30f, 1.0f);
    text.insertion = cadAnchor + AcGeVector3d(1152.0, 128.0, 384.0);
    text.height = 96.0;
    const AcString demoText = L"\u4E2D\u6587 TEXT";
    text.text = demoText;
    addDemo(text, "Text");

    acdb::AcDbMText mtext;
    mtext.color = acgs::math::Vec4(0.35f, 0.90f, 0.95f, 1.0f);
    mtext.insertion = cadAnchor + AcGeVector3d(1152.0, 320.0, 384.0);
    mtext.direction = AcGeVector3d(1.0, 0.0, 0.0);
    mtext.height = 64.0;
    const AcString demoMText = L"\u4E2D\u6587 MTEXT\nDEMO";
    mtext.text = demoMText;
    addDemo(mtext, "MText");

    // Text locators: bright vertical lines pointing at each text insertion
    // point, so the glyph position can be found from any distance while
    // debugging the SDF path.
    acdb::AcDbLine textLocator;
    textLocator.color = acgs::math::Vec4(1.0f, 0.20f, 0.90f, 1.0f);
    textLocator.lineWeight = 3.0;
    textLocator.start = text.insertion + AcGeVector3d(0.0, 0.0, -320.0);
    textLocator.end = text.insertion;
    addDemo(textLocator, "TextLocator");
    acdb::AcDbLine mtextLocator;
    mtextLocator.color = acgs::math::Vec4(1.0f, 0.20f, 0.90f, 1.0f);
    mtextLocator.lineWeight = 3.0;
    mtextLocator.start = mtext.insertion + AcGeVector3d(0.0, 0.0, -320.0);
    mtextLocator.end = mtext.insertion;
    addDemo(mtextLocator, "MTextLocator");

    // ---- annotation family demos (dimension / leader / tolerance) ----
    // The cluster sits right of the 3d-solid demo: a small plate outline
    // with the dimension types AutoCAD drawings actually carry.
    const AcGePoint3d dimAnchor = demoAnchor + AcGeVector3d(2304.0, 0.0, 0.0);
    const double dimTextHeight = 56.0;

    acdb::AcDbLine plateA;
    plateA.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 1.0f);
    plateA.start = dimAnchor;
    plateA.end = dimAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    addDemo(plateA, "PlateBottom");
    acdb::AcDbLine plateB;
    plateB.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 1.0f);
    plateB.start = dimAnchor + AcGeVector3d(0.0, 512.0, 0.0);
    plateB.end = dimAnchor + AcGeVector3d(768.0, 512.0, 0.0);
    addDemo(plateB, "PlateTop");
    acdb::AcDbLine plateC;
    plateC.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 1.0f);
    plateC.start = dimAnchor;
    plateC.end = dimAnchor + AcGeVector3d(0.0, 512.0, 0.0);
    addDemo(plateC, "PlateLeft");
    acdb::AcDbLine plateD;
    plateD.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 1.0f);
    plateD.start = dimAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    plateD.end = dimAnchor + AcGeVector3d(768.0, 512.0, 0.0);
    addDemo(plateD, "PlateRight");

    acdb::AcDbRotatedDimension dimRotated;
    dimRotated.color = acgs::math::Vec4(0.95f, 0.80f, 0.25f, 1.0f);
    dimRotated.dimension.textHeight = dimTextHeight;
    dimRotated.xLine1Point = dimAnchor;
    dimRotated.xLine2Point = dimAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    dimRotated.dimensionLinePoint =
        dimAnchor + AcGeVector3d(384.0, -160.0, 0.0);
    dimRotated.dimensionLineAngle = 0.0;
    addDemo(dimRotated, "DimRotated");

    acdb::AcDbAlignedDimension dimAligned;
    dimAligned.color = acgs::math::Vec4(0.45f, 0.90f, 0.45f, 1.0f);
    dimAligned.dimension.textHeight = dimTextHeight;
    dimAligned.xLine1Point = dimAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    dimAligned.xLine2Point = dimAnchor + AcGeVector3d(768.0, 512.0, 0.0);
    dimAligned.dimensionLinePoint =
        dimAnchor + AcGeVector3d(928.0, 256.0, 0.0);
    addDemo(dimAligned, "DimAligned");

    acdb::AcDbCircle dimCircle;
    dimCircle.color = acgs::math::Vec4(0.55f, 0.75f, 0.95f, 1.0f);
    dimCircle.center = dimAnchor + AcGeVector3d(192.0, 256.0, 0.0);
    dimCircle.radius = 96.0;
    addDemo(dimCircle, "DimBore");

    acdb::AcDbRadialDimension dimRadial;
    dimRadial.color = acgs::math::Vec4(0.95f, 0.55f, 0.30f, 1.0f);
    dimRadial.dimension.textHeight = dimTextHeight;
    dimRadial.centerPoint = dimCircle.center;
    dimRadial.radiusPoint =
        dimCircle.center +
        AcGeVector3d(96.0 * 0.7071, 96.0 * 0.7071, 0.0);
    dimRadial.leaderLength = 96.0;
    addDemo(dimRadial, "DimRadial");

    acdb::AcDbDiametricDimension dimDiameter;
    dimDiameter.color = acgs::math::Vec4(0.95f, 0.45f, 0.55f, 1.0f);
    dimDiameter.dimension.textHeight = dimTextHeight;
    dimDiameter.chordPoint =
        dimCircle.center - AcGeVector3d(96.0, 0.0, 0.0);
    dimDiameter.farChordPoint =
        dimCircle.center + AcGeVector3d(96.0, 0.0, 0.0);
    dimDiameter.leaderLength = 128.0;
    addDemo(dimDiameter, "DimDiameter");

    acdb::AcDb3PointAngularDimension dimAngular;
    dimAngular.color = acgs::math::Vec4(0.75f, 0.55f, 0.95f, 1.0f);
    dimAngular.dimension.textHeight = dimTextHeight;
    dimAngular.centerPoint = dimAnchor + AcGeVector3d(768.0, 0.0, 0.0);
    dimAngular.xLine1Point = dimAnchor + AcGeVector3d(384.0, 0.0, 0.0);
    dimAngular.xLine2Point = dimAnchor + AcGeVector3d(768.0, 384.0, 0.0);
    dimAngular.dimension.textPosition =
        dimAnchor + AcGeVector3d(560.0, 120.0, 0.0);
    addDemo(dimAngular, "DimAngular");

    acdb::AcDbOrdinateDimension dimOrdinate;
    dimOrdinate.color = acgs::math::Vec4(0.85f, 0.85f, 0.35f, 1.0f);
    dimOrdinate.dimension.textHeight = dimTextHeight;
    dimOrdinate.featureLocationPoint = dimAnchor;
    dimOrdinate.leaderEndPoint =
        dimAnchor + AcGeVector3d(-96.0, -224.0, 0.0);
    dimOrdinate.useXAxis = true;
    addDemo(dimOrdinate, "DimOrdinate");

    acdb::AcDbLeader leader;
    leader.color = acgs::math::Vec4(0.90f, 0.90f, 0.90f, 1.0f);
    leader.points = {
        dimAnchor + AcGeVector3d(192.0, 256.0, 0.0) +
            AcGeVector3d(96.0 * 0.7071, 96.0 * 0.7071, 0.0),
        dimAnchor + AcGeVector3d(480.0, 704.0, 0.0),
        dimAnchor + AcGeVector3d(704.0, 704.0, 0.0)};
    leader.hasArrowhead = true;
    addDemo(leader, "Leader");

    acdb::AcDbFcf fcf;
    fcf.color = acgs::math::Vec4(0.95f, 0.95f, 0.95f, 1.0f);
    fcf.insertion = dimAnchor + AcGeVector3d(716.0, 640.0, 0.0);
    fcf.direction = AcGeVector3d(1.0, 0.0, 0.0);
    fcf.text = "{|0.05|A}";
    fcf.height = dimTextHeight;
    addDemo(fcf, "Fcf");

    acdb::AcDb3dSolid solid3d;
    solid3d.color = acgs::math::Vec4(0.75f, 0.65f, 0.25f, 1.0f);
    solid3d.renderClass = acdb::RenderClass::Cad;
    const AcGePoint3d boxMin = cadAnchor + AcGeVector3d(1536.0, 128.0, 384.0);
    const AcGePoint3d boxMax = boxMin + AcGeVector3d(256.0, 256.0, 256.0);
    solid3d.vertices = {
        AcGePoint3d(boxMin.x, boxMin.y, boxMin.z),
        AcGePoint3d(boxMax.x, boxMin.y, boxMin.z),
        AcGePoint3d(boxMax.x, boxMax.y, boxMin.z),
        AcGePoint3d(boxMin.x, boxMax.y, boxMin.z),
        AcGePoint3d(boxMin.x, boxMin.y, boxMax.z),
        AcGePoint3d(boxMax.x, boxMin.y, boxMax.z),
        AcGePoint3d(boxMax.x, boxMax.y, boxMax.z),
        AcGePoint3d(boxMin.x, boxMax.y, boxMax.z)
    };
    solid3d.indices = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6,
        3, 0, 4, 3, 4, 7
    };
    addDemo(solid3d, "Solid3d", true);

    acdb::AcDbLight light;
    // Entity display color via the ARX accessor — AcDbLight::color (the
    // photometric rgb) shadows the AcDbEntity base field.
    light.setColor(acgs::math::Vec4(1.0f, 0.90f, 0.55f, 1.0f));
    light.type = acdb::LightType::Spot;
    light.position = cadAnchor + AcGeVector3d(-1152.0, 256.0, 512.0);
    light.target = AcGeVector3d(cadAnchor.x - 768.0, cadAnchor.y, cadAnchor.z);
    light.range = 1024.0f;
    addDemo(light, "Light");

    // Former in-function demo geometry.  These are now ordinary CAD entities,
    // so the same tessellation drives rendering, names, and autofocus picking.

    for (int i = 0; i < 24; ++i)
    {
      acdb::AcDbPoint gridPoint;
      gridPoint.color = acgs::math::Vec4(
          0.2f + 0.03f * i, 0.9f - 0.025f * i, 0.3f + 0.02f * i, 1.0f);
      gridPoint.lineWeight = 6.0;
      gridPoint.location =
          demoAnchor + AcGeVector3d((i % 8) * 96.0, (i / 8) * 96.0 - 640.0, 0.0) +
          AcGeVector3d(1024.0, 0.0, 0.0);
      addDemo(gridPoint, "PointGrid" + std::to_string(i));
    }

    acdb::AcDbLine dashedArrow;
    dashedArrow.color = acgs::math::Vec4(0.95f, 0.25f, 0.75f, 1.0f);
    dashedArrow.lineType = "DASHED";
    dashedArrow.lineWeight = 2.5;
    dashedArrow.start = demoAnchor + AcGeVector3d(-1024.0, -640.0, -512.0);
    dashedArrow.end = dashedArrow.start + AcGeVector3d(1024.0, 256.0, 0.0);
    addDemo(dashedArrow, "DashedArrow");

    {
      const AcGeVector3d dir = (dashedArrow.end - dashedArrow.start).normal();
      const AcGeVector3d side =
          dir.crossProduct(AcGeVector3d(0.0, 0.0, 1.0)).normal() * 24.0;
      const AcGePoint3d base = dashedArrow.end - dir * 48.0;
      acdb::AcDbSolid arrowHead;
      static_cast<acdb::AcDbEntity &>(arrowHead) = dashedArrow;
      arrowHead.firstCorner = dashedArrow.end;
      arrowHead.secondCorner = base - side;
      arrowHead.thirdCorner = base + side;
      arrowHead.fourthCorner = base + side;
      addDemo(arrowHead, "ArrowHead");
    }

    if (paramSurfaceDemoEnabled())
    {
      const int surfaceSegs = paramSurfaceSegmentCount();
      const AcGePoint3d surfaceOrigin =
          demoAnchor + AcGeVector3d(512.0, 768.0, -512.0);
      auto surfacePoint = [&](double u, double v) {
        const double height = acgs::math::sin(acgs::math::pi<double>() * u) *
                              acgs::math::sin(acgs::math::pi<double>() * v);
        return surfaceOrigin + AcGeVector3d(u * 768.0, height * 224.0,
                                            v * 512.0);
      };

      acdb::AcDbPolyFaceMesh surface;
      surface.color = acgs::math::Vec4(0.20f, 0.45f, 0.85f, 0.80f);
      surface.layer = "GRID_FILL_LAYER";
      surface.style = acdb::MeshStyle::Cad;
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
      addDemo(surface, "ParamSurface", false);

      if (paramSurfaceIsolinesEnabled())
      for (int k = 0; k <= surfaceSegs; k += 4)
      {
        acdb::AcDb3dPolyline isoU;
        acdb::AcDb3dPolyline isoV;
        isoU.color = acgs::math::Vec4(0.05f, 0.10f, 0.25f, 0.85f);
        isoU.lineWeight = 2.0;
        isoV = isoU;
        const double g = double(k) / surfaceSegs;
        for (int i = 0; i <= surfaceSegs; ++i)
        {
          const double t = double(i) / surfaceSegs;
          isoU.vertices.push_back(surfacePoint(t, g));
          isoV.vertices.push_back(surfacePoint(g, t));
        }
        addDemo(isoU, "ParamSurfaceIsoU" + std::to_string(k));
        addDemo(isoV, "ParamSurfaceIsoV" + std::to_string(k));
      }
    }

    for (int i = 0; i < 5; ++i)
    {
      acdb::AcDbPoint widthDot;
      widthDot.color = acgs::math::Vec4(
          0.2f + i * 0.15f, 0.9f - i * 0.1f, 0.5f + i * 0.05f, 1.0f);
      widthDot.lineWeight = 12.0;
      widthDot.location =
          demoAnchor + AcGeVector3d(i * 256.0, -768.0, 0.0);
      addDemo(widthDot, "WidthDot" + std::to_string(i));
    }

    {
      const float widths[] = {1.0f, 2.0f, 4.0f, 8.0f};
      const acgs::math::Vec4 colors[] = {
          {1.0f, 0.2f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.2f, 1.0f},
          {0.2f, 0.4f, 1.0f, 1.0f}, {1.0f, 0.8f, 0.2f, 1.0f}};
      for (int i = 0; i < 4; ++i)
      {
        acdb::AcDbLine widthLine;
        widthLine.color = colors[i];
        widthLine.lineWeight = widths[i];
        widthLine.start =
            demoAnchor + AcGeVector3d(-1024.0, -384.0 + i * 192.0, -512.0);
        widthLine.end = widthLine.start + AcGeVector3d(1024.0, 0.0, 0.0);
        addDemo(widthLine, "WidthLine" + std::to_string(i));
      }
    }

    {
      const char *lineTypes[] = {"CONTINUOUS", "DASHED", "HIDDEN", "CENTER",
                                 "DOT", "PHANTOM"};
      const acgs::math::Vec4 lineColors[] = {
          {0.85f, 0.85f, 0.85f, 1.0f}, {0.95f, 0.25f, 0.75f, 1.0f},
          {0.20f, 0.80f, 0.80f, 1.0f}, {1.00f, 0.70f, 0.20f, 1.0f},
          {0.60f, 0.60f, 1.00f, 1.0f}, {0.80f, 0.20f, 0.20f, 1.0f}};
      for (int i = 0; i < 6; ++i)
      {
        acdb::AcDbLine specimen;
        specimen.color = lineColors[i];
        specimen.lineType = lineTypes[i];
        specimen.lineWeight = 24.0;
        specimen.start =
            demoAnchor + AcGeVector3d(-1024.0, 1536.0 + i * 256.0, -3072.0);
        specimen.end = specimen.start + AcGeVector3d(2048.0, 0.0, 0.0);
        addDemo(specimen, std::string(lineTypes[i]) + "Line");
      }
    }

    acdb::AcDb3dPolyline demoPolyline;
    demoPolyline.color = acgs::math::Vec4(0.60f, 0.20f, 1.00f, 1.0f);
    demoPolyline.lineWeight = 4.0;
    const AcGePoint3d polylineBase =
        demoAnchor + AcGeVector3d(-512.0, 0.0, -512.0);
    demoPolyline.vertices = {
        polylineBase,
        polylineBase + AcGeVector3d(256.0, 256.0, 0.0),
        polylineBase + AcGeVector3d(512.0, 128.0, 256.0),
        polylineBase + AcGeVector3d(768.0, 384.0, 0.0)};
    addDemo(demoPolyline, "DemoPolyline");

    acdb::AcDbHatch hexagon;
    hexagon.color = acgs::math::Vec4(0.30f, 0.80f, 0.50f, 1.0f);
    hexagon.outerLoop.reserve(6);
    for (int i = 0; i < 6; ++i)
    {
      const double angle = acgs::math::two_pi<double>() * i / 6.0;
      hexagon.outerLoop.push_back(
          demoAnchor + AcGeVector3d(0.0, 256.0, -512.0) +
          AcGeVector3d(128.0 * std::cos(angle), 128.0 * std::sin(angle), 0.0));
    }
    addDemo(hexagon, "Hexagon");

    {
      acdb::AcDbHatch circleFill;
      circleFill.color = acgs::math::Vec4(0.20f, 0.60f, 0.90f, 1.0f);
      constexpr int circleSegments = 48;
      circleFill.outerLoop.reserve(circleSegments);
      for (int i = 0; i < circleSegments; ++i)
      {
        const double angle = acgs::math::two_pi<double>() * i / circleSegments;
        circleFill.outerLoop.push_back(
            demoAnchor + AcGeVector3d(256.0, 512.0, 256.0) +
            AcGeVector3d(160.0 * std::cos(angle),
                         160.0 * std::sin(angle), 0.0));
      }
      addDemo(circleFill, "CircleFill");
    }

    acdb::AcDbSolid rectangle;
    rectangle.color = acgs::math::Vec4(0.50f, 0.50f, 0.90f, 1.0f);
    rectangle.firstCorner =
        demoAnchor + AcGeVector3d(-256.0, -128.0, 256.0);
    rectangle.secondCorner =
        rectangle.firstCorner + AcGeVector3d(512.0, 0.0, 0.0);
    rectangle.thirdCorner =
        rectangle.firstCorner + AcGeVector3d(0.0, 384.0, 0.0);
    rectangle.fourthCorner =
        rectangle.firstCorner + AcGeVector3d(512.0, 384.0, 0.0);
    addDemo(rectangle, "Rectangle");

    acdb::AcDbSpline bezier;
    bezier.color = acgs::math::Vec4(0.90f, 0.70f, 0.20f, 1.0f);
    bezier.lineWeight = 6.0;
    bezier.degree = 3;
    bezier.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    const AcGePoint3d bezierBase =
        demoAnchor + AcGeVector3d(-768.0, 512.0, 0.0);
    bezier.controlPoints = {
        bezierBase,
        bezierBase + AcGeVector3d(256.0, 384.0, 128.0),
        bezierBase + AcGeVector3d(512.0, -128.0, -128.0),
        bezierBase + AcGeVector3d(768.0, 256.0, 0.0)};
    addDemo(bezier, "Bezier");

    // A GRID_DWG file replaces the demo document with its decoded contents.
    if (const char *dwgPath = std::getenv("GRID_DWG"))
    {
      acdb::AcDbFileIoReport report;
      if (replaceDemoDocumentFromDwg(dwgPath, &report))
      {
        std::cout << "GRID_DWG: inserted " << report.entitiesAdded
                  << " entities (" << report.unsupportedSkipped
                  << " unsupported) from " << dwgPath << std::endl;
      }
      else
        std::cout << "GRID_DWG: failed to decode " << dwgPath << std::endl;
    }
    if (const char *dxfPath = std::getenv("GRID_DXF"))
    {
      acdb::AcDbFileIoReport report;
      if (document.dxfIn(dxfPath, &report))
        std::cout << "GRID_DXF: inserted " << report.entitiesAdded
                  << " entities (" << report.unsupportedSkipped
                  << " unsupported) from " << dxfPath << std::endl;
      else
        std::cout << "GRID_DXF: failed to decode " << dxfPath << std::endl;
    }

    // File-IO write hooks: save the authored document through the
    // ObjectARX writeDwgFile / dxfOut entry points.
    if (const char *saveDwg = std::getenv("GRID_SAVE_DWG");
        saveDwg && *saveDwg)
      std::cout << "GRID_SAVE_DWG: "
                << (document.writeDwgFile(saveDwg) ? "wrote " : "FAILED ")
                << saveDwg << std::endl;
    if (const char *saveDxf = std::getenv("GRID_SAVE_DXF");
        saveDxf && *saveDxf)
      std::cout << "GRID_SAVE_DXF: "
                << (document.dxfOut(saveDxf) ? "wrote " : "FAILED ")
                << saveDxf << std::endl;

    // Block references (GRID_BLOCKS=1): definition + three inserts.
    if (const char *blocks = std::getenv("GRID_BLOCKS");
        blocks && *blocks && std::strcmp(blocks, "0") != 0)
      appendDemoBlocks();


    // ---- rendered-object collection (ARX: the document is the single
    // source of truth) ----
    //
    // The former main.cpp-side demo bodies — the prototype meshes and the
    // GPU demo curves — are ordinary entities now: prototype soups bake
    // into AcDbPolyFaceMesh geometry at authoring time, and the GPU curves
    // are control-point AcDbSplines that the Gs replay converts back into
    // curve batches.  What deliberately stays outside the document:
    // the orbit center cube (a view gizmo: its position follows the orbit
    // target, its visibility tracks the camera mode, and its fixed pick id
    // carries the click-to-pivot UX — view state, not drawing data), the
    // synthetic stress field (instanced batches, up to a million
    // instances — materializing them as entities would explode memory and
    // the replay), the infinite grid (environment shader), and the
    // per-frame camera diagnostics (frustum wireframe, grid-visible quad,
    // focus marker, orbit-target point — re-authored every frame from
    // camera state).  The glTF models ARE document entities now (see
    // loadGltfModelIntoDocument); only their static-mesh GPU cache lives
    // rendering-side, because the albedo texture / PBR pipeline is not an
    // entity-channel capability.  The world-origin reference line moved
    // into the document below (see ReferenceLine).

    // Prototype meshes as AcDbPolyFaceMesh entities: the procedural soup
    // (pos.xyz normal.xyz uv, 8 floats per vertex) bakes into real mesh
    // data with placement and scale applied once at authoring time.  Same
    // GRID_DEMO_MESHES gate the main.cpp-side records used (default off,
    // so the default scene stays unchanged).
    if (demoMeshesEnabled())
    {
    const auto bakePrototype = [](rendering::MeshType type,
                                  const char *name,
                                  const AcGePoint3d &origin, double size,
                                  const acgs::math::Vec4 &color) {
      acdb::AcDbPolyFaceMesh mesh;
      mesh.name = name;
      mesh.setColor(color);
      const std::vector<float> &soup =
          rendering::proceduralMeshVertices(type);
      const size_t vertexCount =
          soup.size() / rendering::kProceduralMeshFloatStride;
      mesh.geometry.positions.reserve(vertexCount);
      mesh.geometry.normals.reserve(vertexCount);
      mesh.geometry.uvs.reserve(vertexCount);
      for (size_t index = 0; index < vertexCount; ++index)
      {
        const float *vertex = soup.data() + index * 8;
        mesh.geometry.positions.push_back(
            origin + AcGeVector3d(vertex[0] * size, vertex[1] * size,
                                  vertex[2] * size));
        mesh.geometry.normals.push_back(
            glm::vec3(vertex[3], vertex[4], vertex[5]));
        mesh.geometry.uvs.push_back(glm::vec2(vertex[6], vertex[7]));
        mesh.geometry.indices.push_back(static_cast<std::uint32_t>(index));
      }
      addDemo(std::move(mesh), name);
    };

    bakePrototype(rendering::MeshType::Cube, "RedDebugCube", demoAnchor,
                  200.0, acgs::math::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    const AcGePoint3d solidBase =
        demoAnchor + AcGeVector3d(0.0, 768.0, 512.0);
    const rendering::MeshType demoMeshTypes[] = {
        rendering::MeshType::Cube, rendering::MeshType::Sphere,
        rendering::MeshType::Cone, rendering::MeshType::Torus};
    const char *demoMeshNames[] = {
        "DemoCube", "DemoSphere", "DemoCone", "DemoTorus"};
    for (size_t index = 0; index < std::size(demoMeshTypes); ++index)
    {
      bakePrototype(demoMeshTypes[index], demoMeshNames[index],
                    solidBase + AcGeVector3d((index % 2) * 512.0,
                                             (index / 2) * 512.0, 0.0),
                    192.0, acgs::math::Vec4(0.8f, 0.7f, 0.9f, 0.65f));
    }
    }

    // GPU demo curves as control-point AcDbSplines; the Gs replay converts
    // them back into curve batches (degree/weights/color ride the entity).
    auto addCurveSpline = [](const char *name, const acgs::math::Vec4 &color,
                             std::vector<AcGePoint3d> controlPoints,
                             std::vector<double> weights = {}) {
      acdb::AcDbSpline spline;
      spline.name = name;
      spline.setColor(color);
      spline.degree = 3;
      spline.controlPoints = std::move(controlPoints);
      spline.weights = std::move(weights);
      addDemo(std::move(spline), name);
    };

    const AcGePoint3d curveCenter = demoAnchor;
    addCurveSpline("BezierCurve",
                   acgs::math::Vec4(0.15f, 1.0f, 0.55f, 1.0f),
                   {curveCenter + AcGeVector3d(-240.0, -120.0, 0.0),
                    curveCenter + AcGeVector3d(-80.0, 220.0, 0.0),
                    curveCenter + AcGeVector3d(80.0, -220.0, 0.0),
                    curveCenter + AcGeVector3d(240.0, 120.0, 0.0)});
    addCurveSpline("BSplineCurve",
                   acgs::math::Vec4(0.20f, 0.62f, 1.00f, 1.0f),
                   {curveCenter + AcGeVector3d(-320.0, -180.0, 0.0),
                    curveCenter + AcGeVector3d(-140.0, 180.0, 0.0),
                    curveCenter + AcGeVector3d(0.0, -140.0, 0.0),
                    curveCenter + AcGeVector3d(140.0, 180.0, 0.0),
                    curveCenter + AcGeVector3d(320.0, -180.0, 0.0)});
    addCurveSpline("NurbsCurve",
                   acgs::math::Vec4(0.95f, 0.82f, 0.25f, 1.0f),
                   {curveCenter + AcGeVector3d(-280.0, 260.0, 0.0),
                    curveCenter + AcGeVector3d(-110.0, -260.0, 0.0),
                    curveCenter + AcGeVector3d(110.0, 260.0, 0.0),
                    curveCenter + AcGeVector3d(280.0, -260.0, 0.0)},
                   {1.0, 2.0, 2.0, 1.0});

    // The world-origin reference line: the logical line from the literal
    // world origin through the large-coordinate base point — formerly a
    // per-frame frustum-clipped overlay, now an ordinary AcDbLine entity
    // rendered by the replay like any other line.
    acdb::AcDbLine referenceLine;
    referenceLine.name = "ReferenceLine";
    referenceLine.setColor(acgs::math::Vec4(0.15f, 1.0f, 0.25f, 0.9f));
    referenceLine.start = AcGePoint3d(0.0, 0.0, 0.0);
    referenceLine.end = LARGE_COORDINATE_BASE_POINT;
    // excludeFromFit: the line spans origin -> 1e7, so it must never
    // contribute bounds to the depth-slab fit (it would re-frame the
    // startup view); it renders inside whatever slab the real content
    // produces, with the depth shader discarding outside it.
    addDemo(std::move(referenceLine), "ReferenceLine", false,
            acgs::AcGsPickShape::Primitives, true);

    // The former GPU arc demo: an ordinary AcDbArc entity (stroke channel).
    acdb::AcDbArc curveArc;
    curveArc.name = "CurveArc";
    curveArc.setColor(acgs::math::Vec4(0.92f, 0.35f, 0.72f, 1.0f));
    curveArc.center = curveCenter;
    curveArc.normal = AcGeVector3d(0.0, 0.0, 1.0);
    curveArc.radius = 360.0;
    curveArc.startAngle = -0.35;
    curveArc.endAngle = -0.35 + 1.60;
    addDemo(std::move(curveArc), "CurveArc");

    return true;
  }();
  (void)authored;
}

// Drop every cache/identity keyed on tessellation-stored pointers; called
// from buildVectorPrimitivesTessellation before the rebuild repopulates.
static void invalidateTessellationKeyedCaches();
bool pickDebugEnabled();

VectorPrimitivesTessellation buildVectorPrimitivesTessellation()
{  VectorPrimitivesTessellation target;
  acdb::TessellatedEntity &result = target.geometry;
  if (!cadEntityDemoEnabled())
    return target;

  buildDemoDocument();
  acdb::AcDbDatabase &document = acdbDocument();
  // The rebuild invalidates every tessellation-stored pointer: pick
  // registry entries, text identities, pointer-keyed caches, and the live
  // selection. Clear them together BEFORE the replayer repopulates the
  // text queue, so no stale copy ever accumulates (the queue used to grow
  // by a full duplicate set per revision).
  invalidateTessellationKeyedCaches();
  acgi::textRequests().clear();

  target.anchor = demoCadAnchor();
  const AcGePoint3d demoAnchor = demoAnchorPoint();
  const acdb::TesselationOptions options;

  // The Gs replay: the document model space records into the
  // metafile (entities, SHX-styled text, block instances).
  acgs::AcGsDocumentReplayer replayer;
  replayer.skipHandles = gGltfDocumentHandles;
  replayer.record(document, options, target);

  // Imported glTF demo model (GRID_GLTF env / G key): one mesh record per
  // primitive, rendered with its PBR material and albedo texture.  Placed
  // ahead of the GRID_DEMO_MESHES gate so the glTF demo does not depend on
  // the legacy prototype-mesh opt-in.
  appendGltfDemoMeshes(target);

  return target;
}

const VectorPrimitivesTessellation &getVectorPrimitivesTessellation()
{
  // The revision rides the ECS mirror (demoSceneStore): every document
  // mutation lands in the mirror through the DocumentSceneBridge and
  // bumps its generation, so the draw list rebuilds exactly when the
  // document changed.  This is the dirty-document notification the
  // former constant cadDemoRevision always promised.
  // The glTF serial folds into traitsVersion: G-key model switches change
  // the mesh records without touching the document, so the cache key must
  // move with them.
  static constexpr std::uint64_t cadDemoTraitsVersion = 1;
  static constexpr std::uint32_t cadDemoToleranceBucket = 0;
  const acgs::DrawListKey key{
      demoSceneStore().revision(),
      cadDemoTraitsVersion + gGltfModelSerial,
      vectorPrimitivesAnchor(), cadDemoToleranceBucket};
  static acgs::DrawListCache cache;
  return cache.get(key, buildVectorPrimitivesTessellation);
}



int currentDrawableHeight();


struct CadPairedBandPoints
{
    const acdb::Stroke *left = nullptr;
    const acdb::Stroke *right = nullptr;
    size_t segmentCount = 0;
};

bool cadPairedBandPoints(const acgs::AcGsEntityRange &range,
                         const acdb::TessellatedEntity &tess,
                         CadPairedBandPoints &band);

// The anchor-relative frame is camera-independent, so the cached buffers stay
// valid; cadAnchorView supplies the camera-dependent translation.
// Visible CAD fills are immutable once tessellated.  Cache the renderer-side
// camera-relative vertices per entity range so normal drawing does not repeat
// the per-triangle double-to-float conversion and color contrast pass every
// frame.  The anchor-relative frame is camera-independent.
// Both caches below key on tessellation-stored pointers and are cleared
// wholesale by invalidateTessellationKeyedCaches() when the tessellation
// rebuilds (document revision bump) — a stale pointer must never resolve.
static std::map<const acgs::AcGsEntityRange *, std::vector<rendering::FillVertex>>
    g_cadFillVertexCache;
static std::unordered_map<const acgs::CurveBatchCommand *,
                          std::vector<acgs::math::DVec3>>
    g_curveSampleCache;
static std::unordered_map<const MeshEntityRecord *, acgs::math::DVec3>
    g_curveCenterCache;
static void invalidateTessellationKeyedCaches()
{
  g_cadFillVertexCache.clear();
  g_curveSampleCache.clear();
  g_curveCenterCache.clear();
  g_meshInstanceCache.clear();
  g_gltfWireframeCache.clear();
  // Registry entries and text identities alias the old tessellation too;
  // ids re-register (and stay stable) from the next queue refresh.
  gpuPickManager().clearRegistry();
  outlineEntity.reset();
}

static const std::vector<rendering::FillVertex> &
cadVisibleFillVertices(const acgs::AcGsEntityRange &range)
{
  auto [it, inserted] = g_cadFillVertexCache.try_emplace(&range);
  if (!inserted)
    return it->second;

  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  std::vector<rendering::FillVertex> &vertices = it->second;
  vertices.reserve(range.count * 3);
  for (size_t i = range.begin; i < range.begin + range.count; ++i)
  {
    const acdb::Triangle &triangle = tessellation.geometry.fills[i];
    if (!triangle.common.visible)
      continue;
    const acgs::math::Vec4 fillColor =
        acgsView().contrastColor(triangle.common.color.renderColor());
    vertices.push_back(
        {acgs::math::Vec3(triangle.a - tessellation.anchor), fillColor});
    vertices.push_back(
        {acgs::math::Vec3(triangle.b - tessellation.anchor), fillColor});
    vertices.push_back(
        {acgs::math::Vec3(triangle.c - tessellation.anchor), fillColor});
  }
  return vertices;
}

static void drawVectorPrimitivesDemo(const acgs::math::Mat4 &view,
                                     const acgs::math::Mat4 &projection,
                                     const acgs::math::Mat4 &overlayProjection,
                                     const acgs::math::DVec3 &rebase,
                                     const acgs::math::Vec4 &logDepth,
                                     const acgs::math::DVec3 &cameraPos,
                                     const acgs::math::DVec3 &cameraFront,
                                     const acgs::math::DVec3 &cameraRightD,
                                     const acgs::math::DVec3 &cameraUpD,
                                     float pixelSizeWorld,
                                     const std::vector<const VisibilityCandidate *> &visibleCad,
                                     const std::vector<const VisibilityCandidate *> &tinyCad) {
  if (visibleCad.empty() && tinyCad.empty()) return;

  const acgs::math::Vec3 camRight = acgs::math::normalize(acgs::math::Vec3(cameraRightD));
  const acgs::math::Vec3 camUp = acgs::math::normalize(acgs::math::Vec3(cameraUpD));
  const acgs::math::Vec3 camFront = acgs::math::normalize(acgs::math::Vec3(cameraFront));
  // Visible point impostors are exact screen-space discs.  Use the
  // world-per-pixel value at the point itself so the ID quad has the same
  // projected pixel radius in perspective views, not merely at the camera
  // target.
  auto pointWorldPerPixel = [&](const acgs::math::DVec3 &worldPoint) {
    if (useOrthoProjection())
      return 2.0 * orbitCam.orthoSize() / currentDrawableHeight();
    const double viewDepth = std::max(1.0e-9,
        acgs::math::dot(worldPoint - cameraPos, cameraFront));
    return 2.0 * viewDepth * std::tan(acgs::math::radians(45.0) * 0.5) /
           currentDrawableHeight();
  };

  const VectorPrimitivesTessellation &tessellation =
      getVectorPrimitivesTessellation();
  const acgs::math::Mat4 cadAnchorView = view * acgs::math::translate(
      acgs::math::Mat4(1.0f), acgs::math::Vec3(tessellation.anchor - cameraPos));
  static acgs::AcGsModel cadDrawList;
  // Keep the geometry vectors' capacity across frames: clear() freed and
  // re-allocated them every frame.
  cadDrawList.clearKeepCapacity();
  static std::vector<bool> strokeVisible;
  static std::vector<bool> pointVisible;

  const acdb::TessellatedEntity &tess = tessellation.geometry;
  static std::vector<rendering::FillVertex> gpuPickVertices;
  const bool gpuPickQueueActive =
      acgs::acgsGetManager()->deviceReady() && gpuPickEnabled() &&
      (gpuPickFocus.waitingResult || gpuPickSceneDebugQueueActive);
  // Process-constant env override; cached (this runs every frame).
  static const uint8_t linePickOcclusionRank = [] {
    const char *lineLayerValue = std::getenv("GRID_LINE_LAYER");
    return lineLayerValue && std::atof(lineLayerValue) > 0.0 ? 3 : 2;
  }();
  const rendering::RenderModeFlags renderFlags = acgs::acgsGetManager()->deviceReady()
      ? acgs::acgsGetManager()->deviceRenderModeFlags()
      : rendering::RenderModeFlags{};
  auto queueGpuSoup = [&](const acgs::math::Mat4 &pickProjection, uint32_t objectId,
                          uint8_t occlusionRank,
                          const acgs::math::Vec4 &slabLogDepth = acgs::math::Vec4(0.0f)) {
    gpuPickManager().queueSoupChunks(gpuPickVertices,
                                     view, pickProjection, logDepth,
                                     objectId, gpuPickQueueActive,
                                     occlusionRank, slabLogDepth);
  };
  // Match the centered visible AcGi ribbon and its symmetric edge shader.
  // A one-sided pick quad would offset the ID buffer from the rendered pixels.
  auto appendPickRibbon = [&](const acgs::math::Vec3 &ra, const acgs::math::Vec3 &rb,
                              const acgs::math::Vec4 &color, float halfWidth) {
    const acgs::math::Vec3 direction = rb - ra;
    if (acgs::math::length(direction) < 1.0e-5f)
      return;
    const float minimumHalfWidth =
        pixelSizeWorld > 0.0f ? pixelSizeWorld * 1.0f : 1.0f;
    halfWidth = std::max(halfWidth, minimumHalfWidth);
    const acgs::math::Vec3 side = acgsView().ribbonSide(direction, camFront, halfWidth);
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
  auto appendCenteredPickRibbon = [&](const acgs::math::Vec3 &ra,
                                      const acgs::math::Vec3 &rb,
                                      const acgs::math::Vec4 &color) {
    const acgs::math::Vec3 direction = rb - ra;
    if (acgs::math::length(direction) < 1.0e-5f)
      return;
    const acgs::math::DVec3 worldMidpoint =
        cameraPos + acgs::math::DVec3((ra + rb) * 0.5f);
    // Hardware AA line strips rasterize wider than their nominal 1-pixel
    // centerline, so the ID ribbon uses the same 2-pixel floor as AcGi
    // stroke ribbons; otherwise the visible curve is easier to see than to
    // pick.
    const float halfWidth = float(pointWorldPerPixel(worldMidpoint));
    const acgs::math::Vec3 side = acgsView().ribbonSide(direction, camFront, halfWidth);
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({ra + side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({ra - side, color});
    gpuPickVertices.push_back({rb + side, color});
    gpuPickVertices.push_back({rb - side, color});
  };
  auto appendPickPoint = [&](const acgs::math::Vec3 &center, float radius,
                             const acgs::math::Vec4 &color) {
    const acgs::math::Vec3 right = camRight * radius;
    const acgs::math::Vec3 up = camUp * radius;
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
    const acgs::AcGsEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadFill, nullptr, &range});
    const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
    // Fully transient (the strokes/points convention): the pick id is
    // re-assigned on every registry rebuild, so any id-keyed cache
    // (CPU soup or GPU vertex buffer) grows without bound under the
    // continuous full-scene pass and eventually exhausts bgfx buffers
    // -- fills silently vanished from the ID view when that happened.
    // CAD fills that render write depth even when translucent; style-omitted
    // fills remain pickable without acting as depth occluders.
    const acdb::Triangle &firstFill = tess.fills[range.begin];
    const bool fillWritesDepth = firstFill.is3DFace
        ? renderFlags.face3dFill || renderFlags.hiddenLine
        : renderFlags.show2dSolidFills;
    // Visible CAD fills always write depth (including translucent fills).
    // A fill omitted by the active visual style remains selectable, but must
    // not occlude IDs for geometry that is actually rendered.
    const uint8_t fillOcclusionRank = fillWritesDepth ? 0 : 1;
    constexpr size_t kMaxPickChunkVertices = 3 * 21000;
    for (size_t consumed = 0; consumed < range.count;)
    {
      const size_t triangleCount =
          std::min(range.count - consumed, kMaxPickChunkVertices / 3);
      gpuPickVertices.clear();
      for (size_t i = range.begin + consumed;
           i < range.begin + consumed + triangleCount; ++i)
      {
        const acdb::Triangle &triangle = tess.fills[i];
        if (!triangle.common.visible)
          continue;
        gpuPickVertices.push_back(
            {acgs::math::Vec3(triangle.a - tessellation.anchor), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(triangle.b - tessellation.anchor), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(triangle.c - tessellation.anchor), idColor});
      }
      if (!gpuPickVertices.empty())
      {
        acgs::acgsGetManager()->queueGpuTrianglePick(
            0, gpuPickVertices.data(), uint32_t(gpuPickVertices.size()),
            cadAnchorView, projection, logDepth, objectId,
            fillOcclusionRank);
      }
      consumed += triangleCount;
    }
  };
  auto queueCadStroke = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const acgs::AcGsEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadStroke, nullptr, &range});
    const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
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
            {acgs::math::Vec3(band.left->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(band.right->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(band.right->points[next] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(band.left->points[segment] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(band.right->points[next] - cameraPos), idColor});
        gpuPickVertices.push_back(
            {acgs::math::Vec3(band.left->points[next] - cameraPos), idColor});
      }
    }
    else
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const acdb::Stroke &stroke = tess.strokes[i];
      const size_t count = stroke.points.size();
      if (!stroke.common.visible || count < 2)
        continue;
      const float halfWidth = acgsView().strokeHalfWidth(stroke);
      const size_t segmentCount =
          stroke.closed ? count : count - 1;
      for (size_t segment = 0; segment < segmentCount; ++segment)
      {
        // The scene ID pass and the 1x1 pick pass both hardware-clip at
        // the overlay slab, so clipping the soup there is pixel-exact.
        acgs::math::DVec3 clippedStart, clippedEnd;
        if (!acgsView().clipStrokeSegment(
                stroke.points[segment],
                stroke.points[(segment + 1) % count],
                clippedStart, clippedEnd, stroke.semiInfinite))
          continue;
        appendPickRibbon(
            acgs::math::Vec3(clippedStart - cameraPos),
            acgs::math::Vec3(clippedEnd - cameraPos),
            idColor, halfWidth);
      }
    }
    queueGpuSoup(overlayProjection, objectId, linePickOcclusionRank);
  };
  auto queueCadPoint = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const acgs::AcGsEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadPoint, nullptr, &range});
    const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    double pickDepthMin = 0.0;
    double pickDepthMax = 0.0;
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const acdb::TessellatedPoint &point = tess.points[i];
      if (!point.common.visible)
        continue;
      const float radius = std::max(
          float(point.pointSize) * float(pointWorldPerPixel(point.location)),
          float(3.0 * pointWorldPerPixel(point.location)));
      // The visible point draws with a half-pixel camera bias (see
      // drawTargetPointInstances, perspective branch only) so it stays
      // visible on the surface it marks. Bias the pick disc identically,
      // or its depth z-fights with that surface and the point's ID loses —
      // the ID texture's depth relationship would differ from the visible.
      const float pickBias = useOrthoProjection()
                                 ? 0.0f
                                 : std::max(0.5f * float(pointWorldPerPixel(
                                                 point.location)),
                                            0.01f);
      const acgs::math::Vec3 discCenter =
          acgs::math::Vec3(point.location - cameraPos) - camFront * pickBias;
      appendPickPoint(discCenter, radius, idColor);
      const double discDepth = acgs::math::dot(acgs::math::DVec3(discCenter),
                                        acgs::math::DVec3(camFront));
      pickDepthMin = pickDepthMin == 0.0
                         ? discDepth
                         : std::min(pickDepthMin, discDepth);
      pickDepthMax = std::max(pickDepthMax, discDepth);
      if (pickDebugEnabled() && outlineEntity.has_value() &&
          outlineEntity->cadRange == candidate.cadRange)
      {
        std::printf("[POINT_PICK] selected point queued: index=%zu verts=%zu "
                    "range=[%zu,%zu]\n",
                    i, gpuPickVertices.size(), range.begin, range.count);
        std::fflush(stdout);
      }
    }
    // Point impostors are screen-overlay pickables (the visible point
    // draws effectively always-on-top through the overlay projection):
    // give the discs their own slab around their depths (rank 3 = no
    // content-slab discard, no depth-test surprise).
    const acgs::math::Vec4 pointSlab(
        1.0f, float(std::max(1.0, pickDepthMin * 0.5)),
        float(std::max(2.0, pickDepthMax * 2.0)), 0.0f);
    queueGpuSoup(overlayProjection, objectId, 3, pointSlab);
  };
  auto queueCadCurve = [&](const VisibilityCandidate &candidate) {
    if (!candidate.curve)
      return;
    const uint32_t objectId = registerGpuPickEntity(
        {VisibilityKind::CadCurve, nullptr, nullptr, candidate.curve});
    const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    // The sampled polyline is camera-independent and the curve is immutable
    // once tessellated: cache it per curve pointer (cleared when the
    // tessellation rebuilds) instead of re-running de Casteljau/de Boor on
    // every pick-refresh frame.
    auto [sampleIt, inserted] =
        g_curveSampleCache.try_emplace(candidate.curve);
    if (inserted)
      sampleIt->second = acgs::AcGsView::sampleCurveBatch(*candidate.curve);
    const std::vector<acgs::math::DVec3> &points = sampleIt->second;
    for (size_t i = 0; i + 1 < points.size(); ++i)
      appendCenteredPickRibbon(
          acgs::math::Vec3(points[i] - cameraPos),
          acgs::math::Vec3(points[i + 1] - cameraPos), idColor);
    queueGpuSoup(overlayProjection, objectId, linePickOcclusionRank);
  };
  auto queueTinyCadPoint = [&](const VisibilityCandidate &candidate) {
    if (!candidate.cadRange || !candidate.cadRange->count)
      return;
    const acgs::AcGsEntityRange &range = *candidate.cadRange;
    const uint32_t objectId = registerGpuPickEntity(
        {candidate.kind, nullptr, &range});
    const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
    gpuPickVertices.clear();
    const float minimumRadius =
        float(3.0 * pointWorldPerPixel(candidate.center));
    const float pointRadius = candidate.overlayPointSize > 0.0f
        ? candidate.overlayPointSize *
              float(pointWorldPerPixel(candidate.center))
        : minimumRadius;
    // Tiny impostors render through the point path's half-pixel camera bias
    // as well — without it the disc's depth z-fights with whatever the
    // point sits on and the ID loses to the surface.
    const float pickBias = useOrthoProjection()
                               ? 0.0f
                               : std::max(0.5f * float(pointWorldPerPixel(
                                               candidate.center)),
                                          0.01f);
    if (pickDebugEnabled() && outlineEntity.has_value() &&
        outlineEntity->cadRange == candidate.cadRange)
    {
      const double pointDepth =
          acgs::math::dot(candidate.center - cameraPos, cameraFront);
      std::printf("[POINT_PICK] tiny selected point queued: verts=%zu "
                  "viewDepth=%.1f slab=[%.1f, %.1f]\n",
                  gpuPickVertices.size(), pointDepth, logDepth.y,
                  logDepth.z);
      std::fflush(stdout);
    }
    // Screen-overlay pickable: own slab + rank 3 (mirrors the visible
    // impostor's effective always-on-top semantics).
    const double tinyDepth =
        acgs::math::dot(acgs::math::DVec3(candidate.center - cameraPos),
                 acgs::math::DVec3(camFront));
    const acgs::math::Vec4 tinySlab(
        1.0f, float(std::max(1.0, tinyDepth * 0.5)),
        float(std::max(2.0, tinyDepth * 2.0)), 0.0f);
    const float tinyRadius = std::max(pointRadius, minimumRadius);
    const bool giantProbe =
        pickDebugEnabled() && outlineEntity.has_value() &&
        outlineEntity->cadRange == candidate.cadRange;
    appendPickPoint(acgs::math::Vec3(candidate.center - cameraPos) -
                        camFront * pickBias,
                    giantProbe ? 200.0f * tinyRadius : tinyRadius, idColor);
    queueGpuSoup(overlayProjection, objectId, 3, tinySlab);
  };

  strokeVisible.assign(tess.strokes.size(), false);
  pointVisible.assign(tess.points.size(), false);

  auto markRange = [&](VisibilityKind kind, size_t begin, size_t count) {
    std::vector<bool> &flags = kind == VisibilityKind::CadStroke
                                   ? strokeVisible
                                   : pointVisible;
    const size_t limit = kind == VisibilityKind::CadStroke
                             ? tess.strokes.size()
                             : tess.points.size();
    const size_t first = std::min(begin, limit);
    const size_t last = std::min(begin + count, limit);
    for (size_t i = first; i < last; ++i)
      flags[i] = true;
  };

  // In-scene selection recolor (unified depth buffer): the selected
  // entity's primitives draw in their OWN passes with the highlight color —
  // same geometry, same depth, same blending — so the highlight's depth
  // relationship is identical to the original by construction.
  acgs::math::DVec2 selectedStrokeRange{SIZE_MAX, 0}; // x=begin, y=count
  acgs::math::DVec2 selectedPointRange{SIZE_MAX, 0};
  const acgs::CurveBatchCommand *selectedCurve = nullptr;
  const acgs::AcGsEntityRange *selectedFillRange = nullptr;
  auto entitySelected = [&](const VisibilityCandidate *candidate) {
    if (!outlineEntity.has_value() || !candidate)
      return false;
    switch (outlineEntity->kind)
    {
    case VisibilityKind::CadStroke:
    case VisibilityKind::CadFill:
    case VisibilityKind::CadPoint:
      return candidate->cadRange != nullptr &&
             candidate->cadRange == outlineEntity->cadRange;
    case VisibilityKind::CadCurve:
      return candidate->curve != nullptr &&
             candidate->curve == outlineEntity->curve;
    default:
      return false;
    }
  };

  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (!candidate || !entitySelected(candidate))
      continue;
    if (candidate->kind == VisibilityKind::CadStroke)
      selectedStrokeRange = {candidate->rangeBegin, candidate->rangeCount};
    else if (candidate->kind == VisibilityKind::CadPoint)
      selectedPointRange = {candidate->rangeBegin, candidate->rangeCount};
    else if (candidate->kind == VisibilityKind::CadFill)
      selectedFillRange = candidate->cadRange;
    else if (candidate->kind == VisibilityKind::CadCurve)
      selectedCurve = candidate->curve;
  }

  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (!candidate)
      continue;
    if (candidate->kind == VisibilityKind::CadStroke ||
        candidate->kind == VisibilityKind::CadPoint)
    {
      markRange(candidate->kind, candidate->rangeBegin, candidate->rangeCount);
    }
  }

  const size_t selStrokeBegin = size_t(selectedStrokeRange.x);
  const size_t selStrokeLast =
      std::min(selStrokeBegin + size_t(selectedStrokeRange.y),
               tess.strokes.size());
  const size_t selPointBegin = size_t(selectedPointRange.x);
  const size_t selPointLast =
      std::min(selPointBegin + size_t(selectedPointRange.y),
               tess.points.size());
  for (const acdb::Stroke &stroke : tess.strokes)
  {
    const size_t strokeIndex = &stroke - tess.strokes.data();
    if (strokeVisible.empty() || !strokeVisible[strokeIndex])
      continue;
    if (strokeIndex >= selStrokeBegin && strokeIndex < selStrokeLast)
    {
      // Selected: recolor in place (keep the stroke's own alpha so the
      // highlight blends exactly like the original).
      acdb::Stroke recolored = stroke;
      recolored.common.color =
          acgs::math::Vec4(kSelectionHighlightRGB, stroke.common.color.alpha());
      cadDrawList.geometry().strokes.push_back(std::move(recolored));
      continue;
    }
    cadDrawList.geometry().strokes.push_back(stroke);
  }
  // CAD fills bypass the generic AcGsModel copy.  Visible ranges reuse
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
    if (candidate->cadRange == selectedFillRange)
    {
      // Selected fill: recolor the inserted vertices (keep per-vertex
      // alpha so translucency — and the depth relationship — survives).
      for (const rendering::FillVertex &vertex : rangeVertices)
      {
        visibleFillVertices.push_back(
            {vertex.position,
             acgs::math::Vec4(kSelectionHighlightRGB, vertex.color.a)});
      }
      continue;
    }
    visibleFillVertices.insert(visibleFillVertices.end(),
                               rangeVertices.begin(), rangeVertices.end());
  }
  if (!visibleFillVertices.empty())
  {
    acgsView().drawFillTriangles(visibleFillVertices, cadAnchorView,
                               projection, false,
                               acgs::envLayer("GRID_FILL_LAYER"));
  }

  // Wireframe modes suppress solid fills, which would make fill-only
  // entities (Solid, Rectangle, Hatch, CircleFill) vanish.  Draw their
  // per-range boundary edges (edges used by a single triangle, the same
  // shared-edge rule as the selection outline) as colored ribbons instead.
  auto fillRangeColor = [&](const acgs::AcGsEntityRange &range) {
    const size_t last =
        std::min(range.begin + range.count, tess.fills.size());
    for (size_t i = range.begin; i < last; ++i)
    {
      const acdb::Triangle &triangle = tess.fills[i];
      if (!triangle.common.visible)
        continue;
      return acgsView().contrastColor(
          triangle.common.color.renderColor());
    }
    return acgs::math::Vec4(0.0f, 0.0f, 0.0f, -1.0f); // alpha<0: no visible color
  };
  const rendering::RenderModeFlags fillRenderFlags = acgs::acgsGetManager()->deviceReady()
      ? acgs::acgsGetManager()->deviceRenderModeFlags()
      : rendering::RenderModeFlags{};
  if (!fillRenderFlags.show2dSolidFills && !fillRenderFlags.face3dFill &&
      !fillRenderFlags.hiddenLine)
  {
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      if (!candidate || candidate->kind != VisibilityKind::CadFill ||
          !candidate->cadRange || !candidate->cadRange->count)
        continue;
      const acgs::AcGsEntityRange &range = *candidate->cadRange;
      // The selected fill's boundary ribbons recolor too.
      const acgs::math::Vec4 boundaryColor =
          candidate->cadRange == selectedFillRange
              ? acgs::math::Vec4(kSelectionHighlightRGB, 1.0f)
              : fillRangeColor(range);
      if (boundaryColor.a < 0.0f)
        continue;
      acgsView().drawFillBoundary(
          tess, range.begin, range.count,
          1.5f * pixelSizeWorld, boundaryColor);
    }
  }
  for (const VisibilityCandidate *candidate : visibleCad)
  {
    if (candidate && candidate->kind == VisibilityKind::CadCurve &&
        candidate->curve)
    {
      if (candidate->curve == selectedCurve)
      {
        // Selected curve: recolor the batch copy (keep alpha).
        acgs::CurveBatchCommand &recolored = cadDrawList.addCurveBatch();
        recolored = *candidate->curve;
        recolored.acgiMaterial.baseColor = acgs::math::Vec4(
            kSelectionHighlightRGB, candidate->curve->acgiMaterial.baseColor.a);
      }
      else
      {
        cadDrawList.addCurveBatch() = *candidate->curve;
      }
    }
  }

  for (const acdb::TessellatedPoint &point : tess.points)
  {
    if (!pointVisible.empty() && !pointVisible[&point - tess.points.data()])
      continue;
    if (!point.common.visible)
      continue;
    const size_t pointIndex = &point - tess.points.data();
    if (pointIndex >= selPointBegin && pointIndex < selPointLast)
    {
      acdb::TessellatedPoint recolored = point;
      recolored.common.color =
          acgs::math::Vec4(kSelectionHighlightRGB, point.common.color.alpha());
      cadDrawList.geometry().points.push_back(std::move(recolored));
      continue;
    }
    cadDrawList.geometry().points.push_back(point);
  }

  // A whole stroke, face, point group, or mesh below the pixel threshold is
  // represented by one stable impostor instead of submitting invisible pixels.
  for (const VisibilityCandidate *candidate : tinyCad)
  {
    if (!candidate)
      continue;
    const bool selected = entitySelected(candidate);
    appendScenePoint(cadDrawList, candidate->center,
                     selected ? acgs::math::Vec3(kSelectionHighlightRGB)
                              : acgs::math::Vec3(candidate->overlayColor),
                     double(candidate->overlayPointSize));
  }

  if (gpuPickQueueActive)
  {
    for (const VisibilityCandidate *candidate : visibleCad)
    {
      // Pickability is style-independent (AutoCAD semantics): a fill is
      // selectable even in styles that do not draw it (Wireframe3D), so
      // no show2dSolidFills gate here.
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
          acgs::acgsGetManager()->gpuPickQueueStats();
      std::cout << "[PICK_QUEUE] style="
                << rendering::renderModeLabel(
                       acgsView().visualStyle().mode())
                << " meshes=" << stats.queuedMeshes
                << " edges=" << stats.queuedEdges
                << " triangles=" << stats.queuedTriangles
                << " droppedMeshes=" << stats.droppedMeshes
                << " droppedTriangles=" << stats.droppedTriangles
                << std::endl;
    }
  }

  acgsView().submit(cadDrawList, {nullptr, pixelSizeWorld, 2.0f, 7.0f});

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
    // In-scene selection recolor: the selected mesh draws in its own pass
    // with the highlight color (unified depth buffer).
    const bool meshSelected =
        outlineEntity.has_value() &&
        outlineEntity->kind == VisibilityKind::CadMesh &&
        outlineEntity->mesh == &mesh;
    // One reusable model for all CadMesh submits (was constructed and
    // destroyed per candidate per frame).
    static acgs::AcGsModel meshDrawList;
    meshDrawList.clearKeepCapacity();

    // Wireframe styles carry no mesh faces and imported models have no
    // procedural feature-edge buffer, so their wireframe is drawn here as
    // chained unique-edge strokes (selected recolor included).
    if (mesh.staticMesh && !fillRenderFlags.meshFill)
    {
      const acgs::math::Vec4 wireColor =
          meshSelected
              ? acgs::math::Vec4(kSelectionHighlightRGB,
                          meshEntityColor(mesh).a)
              : meshEntityColor(mesh);
      acgs::ViewportDraw wireDraw(meshDrawList.geometry());
      AcGiWorldDraw wireGraphics(wireDraw);
      acdb::AcDbEntity wireTraits;
      wireTraits.name = mesh.entity.name;
      wireGraphics.setFrom(wireTraits);
      wireGraphics.setColor(wireColor);
      for (const std::vector<AcGePoint3d> &chain : gltfWireframeChains(mesh))
        wireGraphics.worldPolyline(chain.data(),
                                   static_cast<int>(chain.size()), false);
    }
    else
    {
      appendMeshEntityToScene(mesh, meshDrawList, false,
                              meshSelected ? &kSelectionHighlightRGB
                                           : nullptr);
    }
    const rendering::DoubleSingleVec3 meshEye =
        rendering::encodeDoubleSingle(rebase);
    acgsView().submit(meshDrawList,
                    {nullptr, pixelSizeWorld, 0.0f, 0.0f, &meshEye});
    queueGpuMeshEntity(&mesh);
  }
}

int stressObjectCount();
const std::vector<LargeCoordinateObject> &getStressObjects();

static bool cadAlgorithmDemoEnabled()
{
  // The CAD instancing path is the design default. Keep the older stylized
  // shader experiment opt-in. Process-constant: cached.
  static const bool enabled = [] {
    const char *value = std::getenv("GRID_CAD_SHADER_DEMO");
    return value != nullptr && *value != '\0' &&
           std::strcmp(value, "0") != 0;
  }();
  return enabled;
}



void drawLargeCoordinateObjects(const acgs::math::Mat4 &view,
                                const acgs::math::Mat4 &projection,
                                const acgs::math::DVec3 &rebaseOrigin,
                                const std::vector<const LargeCoordinateObject *> &drawOrder,
                                const acgs::math::Vec4 &logDepth,
                                float pixelSizeWorld = 0.0f,
                                float edgeSoftness = 0.15f)
{
  const acgs::math::DVec3 cameraPos(orbitCam.Position);
  const acgs::math::DVec3 &cameraFront = orbitCam.Front;

  // Depth values feed both sort comparisons, so cache them once per frame.
  static std::vector<std::pair<double, const LargeCoordinateObject *>>
      sortedDraws;
  sortedDraws.clear();
  sortedDraws.reserve(drawOrder.size());
  for (const LargeCoordinateObject *object : drawOrder)
  {
    sortedDraws.emplace_back(
        acgs::math::dot(acgs::math::DVec3(object->worldPosition - AcGePoint3d(cameraPos)),
                 cameraFront),
        object);
  }
  std::sort(sortedDraws.begin(), sortedDraws.end(),
            [](const auto &lhs, const auto &rhs) {
              return lhs.first > rhs.first;
            });

  rendering::RealisticLightsRenderData realisticLights;
  realisticLights.pointLights[0].position =
      acgs::math::Vec3(acgs::math::DVec3(LARGE_COORDINATE_BASE_POINT +
                           LARGE_COORDINATE_DETAIL_OFFSET +
                           AcGeVector3d(-1024.0, 640.0, 512.0)) -
                cameraPos);
  realisticLights.pointLights[0].radius = 1024.0f;
  realisticLights.pointLights[0].color = acgs::math::Vec3(1.0f, 0.88f, 0.72f);
  realisticLights.pointLights[1].position =
      acgs::math::Vec3(acgs::math::DVec3(LARGE_COORDINATE_BASE_POINT +
                           LARGE_COORDINATE_DETAIL_OFFSET +
                           AcGeVector3d(1024.0, 384.0, -512.0)) -
                cameraPos);
  realisticLights.pointLights[1].radius = 1024.0f;
  realisticLights.pointLights[1].color = acgs::math::Vec3(0.52f, 0.74f, 1.0f);
  realisticLights.pointLightCount = 2;
  if (cadAlgorithmDemoEnabled())
  {
    static acgs::AcGsModel cadDrawList;
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
      // In-scene selection recolor: the selected mesh's instance draws
      // with the highlight color in its own instanced batch.
      const bool meshSelected =
          outlineEntity.has_value() &&
          outlineEntity->kind == VisibilityKind::MeshObject &&
          outlineEntity->mesh == object;
      const size_t meshIndex = static_cast<size_t>(object->mesh);
      cadGroups[meshIndex].push_back(
          cachedMeshInstance(*object, meshEntityRenderMaterial(*object),
                             meshSelected ? &kSelectionHighlightRGB
                                          : nullptr));
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
      acgs::MeshBatchCommand batch;
      batch.prototype = cadMeshTypes[meshIndex];
      batch.cadAlgorithm = true;
      batch.instances = std::move(instances);
      cadDrawList.meshBatches().push_back(std::move(batch));
    }
    acgsView().submit(cadDrawList, {&projection, pixelSizeWorld});
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
    acgs::math::Vec4 material;
    float opacity;
    std::vector<rendering::MeshInstance> instances;
  };
  static std::array<std::vector<InstanceGroup>, kDepthBucketCount> buckets;
  for (auto &bucket : buckets)
    bucket.clear();
  static acgs::AcGsModel meshDrawList;
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
        // In-scene selection recolor: the selected mesh's instance draws
        // with the highlight color (its own alpha and depth preserved).
        const bool meshSelected =
            outlineEntity.has_value() &&
            outlineEntity->kind == VisibilityKind::MeshObject &&
            outlineEntity->mesh == object;

        const bool realistic = object->realistic();
        const acgs::math::Vec4 material = meshEntityRenderMaterial(*object);
        const acgs::math::Vec4 color =
            meshSelected
                ? acgs::math::Vec4(kSelectionHighlightRGB, meshEntityColor(*object).a)
                : meshEntityColor(*object);

        {
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
              cachedMeshInstance(*object, material,
                                 meshSelected ? &kSelectionHighlightRGB
                                              : nullptr));
        }
        queueGpuMeshEntity(object);
    }

  for (auto &bucket : buckets)
    for (InstanceGroup &group : bucket)
    {
      if (group.instances.empty())
        continue;
        acgs::MeshBatchCommand batch;
        batch.prototype = group.mesh;
        batch.opaque = group.opacity >= 1.0f;
        batch.realistic = group.realistic;
        batch.material = group.material;
        batch.acgiMaterial.algorithm = group.realistic
            ? acgs::AcGiShaderAlgorithm::Realistic
            : acgs::AcGiShaderAlgorithm::Shaded;
        batch.acgiMaterial.metallic = group.material.x;
        batch.acgiMaterial.roughness = group.material.y;
        batch.acgiMaterial.transparency = 1.0f - group.opacity;
        batch.instances = std::move(group.instances);
        meshDrawList.meshBatches().push_back(std::move(batch));
    }
  acgsView().submit(meshDrawList, {&projection, pixelSizeWorld});
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
    static constexpr acgs::math::Vec3 kPalette[] = {
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
          acgs::math::DVec3(col * kSpacing - centerOffset, kObjectSize * 0.5,
                     row * kSpacing - centerOffset - 4096.0);
      object.entity.color = acgs::math::Vec4(kPalette[i % 6], 0.45f);
      object.size = static_cast<float>(kObjectSize);
      object.mesh = kMeshCycle[i % 4];
      object.entity.style = realisticMeshEnabled() && (i % 2) != 0
                                ? acdb::MeshStyle::Realistic
                                : acdb::MeshStyle::Cad;
      object.entity.material.metallicFactor = 0.78f;
      object.entity.material.roughnessFactor = 0.28f;
      object.entity.name =
          indexedDisplayName("StressMesh", size_t(i));
      result.push_back(object);
    }
    return result;
  }();
  return objects;
}

struct WorldAabb
{
  acgs::math::DVec3 min;
  acgs::math::DVec3 max;
  bool valid = false;
};

void expandWorldAabb(WorldAabb &bounds, const acgs::math::DVec3 &center,
                     const acgs::math::DVec3 &halfExtent)
{
  const acgs::math::DVec3 minimum = center - halfExtent;
  const acgs::math::DVec3 maximum = center + halfExtent;
  if (!bounds.valid)
  {
    bounds.min = minimum;
    bounds.max = maximum;
    bounds.valid = true;
    return;
  }

  bounds.min = acgs::math::min(bounds.min, minimum);
  bounds.max = acgs::math::max(bounds.max, maximum);
}

void expandCadTessellationBounds(WorldAabb &bounds,
                                 const VectorPrimitivesTessellation &tess)
{
  for (const acgs::AcGsEntityRange &range : tess.strokeRanges)
  {
    // Infinite entities (Ray/XLine) carry only a tessellation proxy
    // endpoint, and excludeFromFit ranges (construction/diagnostic
    // geometry such as the reference line) render inside the depth slab
    // by design — neither may inflate scene bounds: a huge-extent
    // reference line would re-frame every camera fit and the slab.
    if (range.excludeFromFit)
      continue;
    for (size_t index = range.begin; index < range.begin + range.count; ++index)
    {
      const acdb::Stroke &stroke = tess.geometry.strokes[index];
      if (stroke.semiInfinite)
        continue;
      for (const acgs::math::DVec3 &point : stroke.points)
        expandWorldAabb(bounds, point, acgs::math::DVec3(0.0));
    }
  }
  for (const acdb::Triangle &triangle : tess.geometry.fills)
  {
    expandWorldAabb(bounds, triangle.a, acgs::math::DVec3(0.0));
    expandWorldAabb(bounds, triangle.b, acgs::math::DVec3(0.0));
    expandWorldAabb(bounds, triangle.c, acgs::math::DVec3(0.0));
  }
  for (const acdb::TessellatedPoint &point : tess.geometry.points)
    expandWorldAabb(bounds, point.location, acgs::math::DVec3(0.0));
  for (const acgs::CurveBatchCommand &curve : tess.curves)
  {
    if (curve.algorithm == rendering::CurveAlgorithm::Arc)
    {
      const double radius = acgs::math::max(curve.radius, 0.0);
      expandWorldAabb(bounds, curve.center, acgs::math::DVec3(radius));
      continue;
    }
    for (const acgs::math::DVec3 &point : curve.controlPoints)
      expandWorldAabb(bounds, point, acgs::math::DVec3(0.0));
  }
  for (const MeshEntityRecord &mesh : tess.meshes)
    expandWorldAabb(bounds, mesh.worldPosition,
                    acgs::math::DVec3(mesh.size * 0.5));
}

// Immutable scene bounds are computed once; the per-frame pass only converts
// this one conservative box to camera space before doing exact object culling.
static WorldAabb stressFieldBounds()
{
  WorldAabb bounds;
  expandCadTessellationBounds(bounds, getVectorPrimitivesTessellation());
  for (const LargeCoordinateObject &object : getStressObjects())
    expandWorldAabb(bounds, object.worldPosition, acgs::math::DVec3(object.size * 0.5));

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
                    acgs::math::DVec3(extent, extent, request.height * 2.0));
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
                      acgs::math::DVec3(object.size * 0.5));
    }
    return result;
  }();
  return bounds;
}


const WorldAabb &immutableSceneBounds()
{
  static const WorldAabb bounds = [] {
    WorldAabb result = immutableObjectBounds();

    // The reference line (excludeFromFit) still joins this conservative
    // culling box via its endpoints: it connects the world origin to the
    // large-coordinate base point and is always drawn.  Camera fits never
    // see it (they read immutableObjectBounds / stressFieldBounds), so the
    // huge span cannot re-frame the view.
    expandWorldAabb(result, acgs::math::DVec3(0.0), acgs::math::DVec3(0.0));
    expandWorldAabb(result, LARGE_COORDINATE_BASE_POINT, acgs::math::DVec3(0.0));
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
  SDL_GetWindowSizeInPixels(appPlatform.window(), &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  return static_cast<double>(drawableWidth) /
         static_cast<double>(drawableHeight);
}

// Shared fit-to-bounds-and-report helper.
static void fitCameraToBounds(const WorldAabb &bounds, const char *label)
{
  acgsView().zoomExtents(bounds.min, bounds.max);
  acgsView().resetDepthSlabs();
  std::cout << label << " center=(" << orbitCam.Target.x << ", "
            << orbitCam.Target.y << ", " << orbitCam.Target.z
            << ") distance=" << orbitCam.Distance
            << std::endl;
}

static void fitCameraToImportedEntities(
    const std::vector<acdb::AcDbHandle> &handles)
{
  if (handles.empty())
    return;

  VectorPrimitivesTessellation imported;
  acgs::AcGsDocumentReplayer replayer;
  const std::size_t textRequestStart = acgi::textRequests().size();
  replayer.recordHandles(acdbDocument(), handles,
                         acdb::TesselationOptions{}, imported);

  WorldAabb bounds;
  expandCadTessellationBounds(bounds, imported);
  std::vector<acgi::TextRequest> &requests = acgi::textRequests();
  for (std::size_t index = textRequestStart; index < requests.size(); ++index)
  {
    const acgi::TextRequest &request = requests[index];
    const double extent =
        double(request.message.size() + 1) * request.height;
    expandWorldAabb(bounds, request.position,
                    acgs::math::DVec3(extent, extent, request.height * 2.0));
  }
  requests.resize(textRequestStart);

  if (bounds.valid)
    fitCameraToBounds(bounds, "DWG fit-to-view:");
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
    const acgs::math::DVec3 detailCenter =
        LARGE_COORDINATE_BASE_POINT + LARGE_COORDINATE_DETAIL_OFFSET;
    acgsView().focusOn(detailCenter, 6000.0);
    return;
  }
  fitCameraToBounds(bounds, "Stress-field camera:");
}

void printLargeCoordinateValidation()
{
  const acgs::math::DVec3 detailCenter =
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
    if (appPlatform.window())
        SDL_GetWindowSizeInPixels(appPlatform.window(), &width, &height);
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
  SDL_GetWindowSizeInPixels(appPlatform.window(), &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const double aspect = static_cast<double>(drawableWidth) /
                        static_cast<double>(drawableHeight);

  orbitCam.setProjectionPreservingFrame(isOrtho, !isOrtho, aspect);
  isOrtho = !isOrtho;
  acgsView().resetDepthSlabs();

  std::cout << "Projection: "
            << (isOrtho ? "ORTHOGRAPHIC" : "PERSPECTIVE")
            << std::endl;
}

AcGeVector3d normalizedNdcLine(const AcGeVector3d &line)
{
  const double scale = std::max(std::abs(line.x), std::abs(line.y));
  if (!(scale > 0.0) || !std::isfinite(scale))
    return AcGeVector3d(0.0, 0.0, 0.0);
  return line / scale;
}

bool lineIntersectsNdcSquare(const AcGeVector3d &line)
{
  const double f00 = -line.x - line.y + line.z;
  const double f10 = line.x - line.y + line.z;
  const double f01 = -line.x + line.y + line.z;
  const double f11 = line.x + line.y + line.z;
  const double minValue = std::min(std::min(f00, f10), std::min(f01, f11));
  const double maxValue = std::max(std::max(f00, f10), std::max(f01, f11));
  return minValue <= 0.0f && maxValue >= 0.0f;
}

acgs::math::Vec3 anchoredNdcLine(const AcGeVector3d &line)
{
  if (!lineIntersectsNdcSquare(line))
    return acgs::math::Vec3(float(line.x), float(line.y), float(line.z));

  // Re-anchor the infinite line at the midpoint of its segment inside the
  // NDC square.  The original constant can still contain a large world
  // coordinate; evaluating that constant in float32 causes cancellation and
  // flicker while panning.
  AcGePoint2d anchors[4];
  int anchorCount = 0;
  const double edgeEpsilon = 1e-12;
  const auto addAnchor = [&](double x, double y) {
    if (anchorCount < 4 &&
        x >= -1.0 - edgeEpsilon && x <= 1.0 + edgeEpsilon &&
        y >= -1.0 - edgeEpsilon && y <= 1.0 + edgeEpsilon)
    {
      anchors[anchorCount++] = AcGePoint2d(std::clamp(x, -1.0, 1.0),
                                           std::clamp(y, -1.0, 1.0));
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
    return acgs::math::Vec3(float(line.x), float(line.y), float(line.z));

  AcGeVector2d anchor(0.0, 0.0);
  for (int index = 0; index < anchorCount; ++index)
    anchor += anchors[index].asVector() / static_cast<double>(anchorCount);

  return acgs::math::Vec3(float(line.x), float(line.y),
                   float(-(line.x * anchor.x + line.y * anchor.y)));
}

struct CameraSpaceAabb
{
  double minX;
  double maxX;
  double minY;
  double maxY;
  double minDepth;
  double maxDepth;
};

// The exact camera-space bounds of an axis-aligned box.  The support radius
// along each camera basis vector is dot(abs(basis), halfExtent); summing all
// three world-axis contributions is required for a rotated camera basis.
CameraSpaceAabb cameraAabbBounds(const acgs::math::DVec3 &worldCenter,
                                 const acgs::math::DVec3 &halfExtent,
                                 const acgs::math::DVec3 &cameraPosition,
                                 const acgs::math::DVec3 &cameraRight,
                                 const acgs::math::DVec3 &cameraUp,
                                 const acgs::math::DVec3 &cameraFront)
{
  const acgs::math::DVec3 delta = worldCenter - cameraPosition;
  const double rightRadius = acgs::math::dot(acgs::math::abs(cameraRight), halfExtent);
  const double upRadius = acgs::math::dot(acgs::math::abs(cameraUp), halfExtent);
  const double frontRadius = acgs::math::dot(acgs::math::abs(cameraFront), halfExtent);
  const double centerX = acgs::math::dot(delta, cameraRight);
  const double centerY = acgs::math::dot(delta, cameraUp);
  const double centerDepth = acgs::math::dot(delta, cameraFront);

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
                                 const acgs::math::DVec3 &cameraPosition,
                                 const acgs::math::DVec3 &cameraRight,
                                 const acgs::math::DVec3 &cameraUp,
                                 const acgs::math::DVec3 &cameraFront)
{
    const acgs::math::DVec3 center = (worldBounds.min + worldBounds.max) * 0.5;
    const acgs::math::DVec3 halfExtent = (worldBounds.max - worldBounds.min) * 0.5;
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
        bvh_ = ge::AcGeBoundBvh{};
        if (objects.empty())
            return;

        // Binned-SAH construction (ge::AcGeBoundBvh, Embree-style): the
        // BVH instance is retained for ordered ray traversal; the node
        // copies keep the legacy collect() path working.
        std::vector<ge::AcGeBoundBox3d> bounds;
        bounds.reserve(objects.size());
        for (const ObjectT *object : objects)
        {
            const WorldAabb2 b = objectBounds(*object);
            bounds.push_back({AcGePoint3d(b.min.x, b.min.y, b.min.z),
                              AcGePoint3d(b.max.x, b.max.y, b.max.z)});
        }
        bvh_.build(std::move(bounds));

        std::vector<const ObjectT *> ordered;
        ordered.reserve(objects.size());
        for (std::uint32_t index : bvh_.order())
            ordered.push_back(objects[index]);
        objects = std::move(ordered);

        nodes.reserve(bvh_.nodes().size());
        for (const ge::AcGeBvhNode &node : bvh_.nodes())
        {
            nodes.push_back({WorldAabb2{acgs::math::DVec3(node.bounds.min.x,
                                                    node.bounds.min.y,
                                                    node.bounds.min.z),
                                        acgs::math::DVec3(node.bounds.max.x,
                                                   node.bounds.max.y,
                                                   node.bounds.max.z)},
                             node.leftChild, node.rightChild, node.begin,
                             node.end, node.leaf});
        }
    }

    // Near-ordered ray traversal over the primitive boxes; the callback
    // receives (object index, box entry depth) and returns false to stop
    // the walk.  Used by CPU picking so exact tests run nearest-first and
    // prune with the current best depth.
    template <typename Callback>
    void rayTraverse(const AcGePoint3d &origin, const AcGeVector3d &direction,
                     double tMin, double tMax, Callback &&callback) const
    {
        bvh_.rayTraverse(origin, direction, tMin, tMax, callback);
    }

    const ObjectT *object(std::uint32_t index) const
    {
        return objects[index];
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
    virtual acgs::math::DVec3 objectCentroid(const ObjectT &object) const = 0;

private:
    static WorldAabb2 mergeBounds(const WorldAabb2 &a, const WorldAabb2 &b)
    {
        return {acgs::math::min(a.min, b.min), acgs::math::max(a.max, b.max)};
    }



    std::vector<const ObjectT *> objects;
    std::vector<Node> nodes;
    ge::AcGeBoundBvh bvh_; // retained for ordered ray traversal
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
        const acgs::math::DVec3 halfExtent(object.size * 0.5);
        return {object.worldPosition - halfExtent,
                object.worldPosition + halfExtent};
    }

    acgs::math::DVec3 objectCentroid(const LargeCoordinateObject &object) const override
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
    if (appPlatform.window())
        SDL_GetWindowSizeInPixels(appPlatform.window(), &width, &height);
    return std::max(1, width);
}

static double g_pickDepthNear = 0.0;
static double g_pickDepthFar = std::numeric_limits<double>::infinity();

// VSG "double all the way": all ray construction and intersection math
// uses double precision.  The direction is normalized in double.
struct PickRay
{
    AcGePoint3d origin;
    AcGeVector3d direction;
    acgs::AcGsPickView view;
};

// Construct a picking ray from cursor NDC coordinates.
// Perspective: origin = eye, direction = normalize(front + right*ndcX*tanH + up*ndcY*tanV).
// Ortho:       origin shifts on the image plane, direction = front.
PickRay pickRayFromNdc(double ndcX, double ndcY,
                       const GpuPickCameraBasis &basis)
{
    PickRay ray;
    ray.view.position = AcGePoint3d(basis.position);
    ray.view.front = AcGeVector3d(basis.front);
    ray.view.right = AcGeVector3d(basis.right);
    ray.view.up = AcGeVector3d(basis.up);
    ray.view.orthographic = basis.ortho;
    ray.view.orthoHalfHeight = basis.orthoHalfHeight;
    ray.view.viewportWidth = basis.viewportWidth;
    ray.view.viewportHeight = basis.viewportHeight;
    ray.view.verticalFovRadians = basis.verticalFovRadians;
    ray.view.nearDepth = basis.nearDepth;
    ray.view.farDepth = basis.farDepth;
    const acgs::AcGsPickRay geometryRay =
        acgs::makePickRay(ndcX, ndcY, ray.view);
    ray.origin = geometryRay.origin;
    ray.direction = geometryRay.direction;
    return ray;
}

acgs::AcGsPickView currentPickView()
{
    const GpuPickCameraBasis liveCamera{
        orbitCam.Position, orbitCam.Front, orbitCam.Right, orbitCam.Up,
        useOrthoProjection(), orbitCam.orthoSize()};
    GpuPickCameraBasis snapshot = liveCamera;
    snapshot.viewportWidth = currentDrawableWidth();
    snapshot.viewportHeight = currentDrawableHeight();
    snapshot.nearDepth = g_pickDepthNear;
    snapshot.farDepth = g_pickDepthFar;
    const auto &frame = acgsView().frame();
    if (frame.viewportWidth > 1 && frame.viewportHeight > 1)
    {
        snapshot.viewportWidth = frame.viewportWidth;
        snapshot.viewportHeight = frame.viewportHeight;
        snapshot.nearDepth = frame.slabNear;
        snapshot.farDepth = frame.slabFar;
    }
    acgs::AcGsPickView view;
    view.position = AcGePoint3d(snapshot.position);
    view.front = AcGeVector3d(snapshot.front);
    view.right = AcGeVector3d(snapshot.right);
    view.up = AcGeVector3d(snapshot.up);
    view.orthographic = snapshot.ortho;
    view.orthoHalfHeight = snapshot.orthoHalfHeight;
    view.viewportWidth = snapshot.viewportWidth;
    view.viewportHeight = snapshot.viewportHeight;
    view.verticalFovRadians = snapshot.verticalFovRadians;
    view.nearDepth = snapshot.nearDepth;
    view.farDepth = snapshot.farDepth;
    return view;
}

PickRay pickRayFromNdc(double ndcX, double ndcY)
{
    const acgs::AcGsPickView view = currentPickView();
    const acgs::AcGsPickRay geometryRay = acgs::makePickRay(ndcX, ndcY, view);
    PickRay ray;
    ray.origin = geometryRay.origin;
    ray.direction = geometryRay.direction;
    ray.view = view;
    return ray;
}

// Ray-AABB slab test in double precision.
bool rayIntersectsAabb(const PickRay &ray,
                       const WorldAabb2 &bounds,
                       double &hitDepth)
{
    return acgs::rayIntersectsAabb(
        {ray.origin, ray.direction}, bounds.min, bounds.max,
        ray.view.orthographic ? ray.view.nearDepth : 0.0,
        std::numeric_limits<double>::infinity(), hitDepth);
}


VisibilityCandidate makeCurveCandidate(const acgs::CurveBatchCommand &curve)
{
    VisibilityCandidate candidate;
    candidate.kind = VisibilityKind::CadCurve;
    candidate.curve = &curve;
    candidate.overlayColor = acgsView().contrastColor(curve.acgiMaterial.baseColor);
    candidate.overlayPointSize = 2.0f;

    const std::vector<acgs::math::DVec3> points = acgs::AcGsView::sampleCurveBatch(curve);
    if (points.empty())
        return candidate;

    acgs::math::DVec3 minimum = points.front();
    acgs::math::DVec3 maximum = points.front();
    for (const acgs::math::DVec3 &point : points)
    {
        minimum = acgs::math::min(minimum, point);
        maximum = acgs::math::max(maximum, point);
    }

    candidate.min = minimum;
    candidate.max = maximum;
    candidate.center = (minimum + maximum) * 0.5;
    candidate.lodSize = acgs::math::length(maximum - minimum);
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

    acgs::math::DVec3 objectCentroid(const VisibilityCandidate &candidate) const override
    {
        return candidate.center;
    }
};

const CadRangeBvh &getCadRangeBvh()
{
    static std::uint64_t cachedRevision =
        std::numeric_limits<std::uint64_t>::max();
    static CadRangeBvh bvh;
    const std::uint64_t revision = demoSceneStore().revision();
    if (cachedRevision != revision)
    {
        const std::vector<VisibilityCandidate> &candidates =
            cadRangeVisibilityCandidates();
        std::vector<const VisibilityCandidate *> ranges;
        ranges.reserve(candidates.size());
        for (const VisibilityCandidate &candidate : candidates)
        {
            if (candidate.lodSize > 0.0)
                ranges.push_back(&candidate);
        }

        bvh.build(std::move(ranges));
        cachedRevision = revision;
    }
    return bvh;
}

double cadPickTolerance(const PickRay &ray, const AcGePoint3d &worldPoint);

// Closest approach between a normalized picking ray and a finite segment.
// Keeping every intermediate value in double avoids false hits/misses in the
// large-coordinate CAD demo.
// Closest approach between the pick ray and the segment, honoring the depth
// window.  Returns the clamped ray parameter (NaN for degenerate segments)
// and reports the matching segment parameter so callers can measure the
// true miss distance.
// The cursor tolerance must be measured where the pick ray actually
// approaches the geometry.  Evaluating it at a fixed reference point (for
// example a long stroke midpoint, which sits 5e5 units away for the
// semi-infinite Ray) inflates it to thousands of world units and lets that
// stroke steal every pick in the view.
bool rayIntersectsSegmentWithTolerance(
    const PickRay &ray,
    const AcGePoint3d &start,
    const AcGePoint3d &end,
    const std::function<double(double rayDepth)> &toleranceAtDepth,
    double &hitDepth,
    double depthNear = std::numeric_limits<double>::quiet_NaN(),
    double depthFar = std::numeric_limits<double>::quiet_NaN(),
    bool semiInfiniteRay = false)
{
    if (std::isnan(depthNear))
        depthNear = ray.view.orthographic ? ray.view.nearDepth : 0.0;
    if (std::isnan(depthFar))
        depthFar = ray.view.farDepth;
    return acgs::rayIntersectsSegmentWithTolerance(
        {ray.origin, ray.direction}, start, end, toleranceAtDepth, hitDepth,
        depthNear, depthFar, semiInfiniteRay, ray.view.orthographic);
}

bool rayIntersectsSegment(const PickRay &ray,
                          const AcGePoint3d &start,
                          const AcGePoint3d &end,
                          double tolerance,
                          double &hitDepth,
                          double depthNear = std::numeric_limits<double>::quiet_NaN(),
                          double depthFar = std::numeric_limits<double>::quiet_NaN())
{
    if (std::isnan(depthNear))
        depthNear = ray.view.orthographic ? ray.view.nearDepth : 0.0;
    if (std::isnan(depthFar))
        depthFar = ray.view.farDepth;
    return rayIntersectsSegmentWithTolerance(
        ray, start, end,
        [tolerance](double) { return tolerance; },
        hitDepth, depthNear, depthFar);
}

bool rayIntersectsTriangle(const PickRay &ray,
                           const AcGePoint3d &a,
                           const AcGePoint3d &b,
                           const AcGePoint3d &c,
                           double &hitDepth,
                           double depthNear = std::numeric_limits<double>::quiet_NaN(),
                           double depthFar = std::numeric_limits<double>::quiet_NaN())
{
    if (std::isnan(depthNear))
        depthNear = ray.view.orthographic ? ray.view.nearDepth : 0.0;
    if (std::isnan(depthFar))
        depthFar = ray.view.farDepth;
    return acgs::rayIntersectsTriangle(
        {ray.origin, ray.direction}, a, b, c, depthNear, depthFar, hitDepth);
}

bool cadPairedBandPoints(const acgs::AcGsEntityRange &range,
                         const acdb::TessellatedEntity &tess,
                         CadPairedBandPoints &band)
{
    if (range.pickShape != acgs::AcGsPickShape::PairedStrokeBand ||
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
                                const acdb::TessellatedEntity &tess,
                                const acgs::AcGsEntityRange &range,
                                double &hitDepth)
{
    CadPairedBandPoints band;
    if (!cadPairedBandPoints(range, tess, band))
        return false;

    bool hit = false;
    double best = std::numeric_limits<double>::infinity();
    auto consider = [&](const AcGePoint3d &a, const AcGePoint3d &b,
                        const AcGePoint3d &c) {
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
        const AcGePoint3d li(band.left->points[i]);
        const AcGePoint3d ln(band.left->points[next]);
        const AcGePoint3d ri(band.right->points[i]);
        const AcGePoint3d rn(band.right->points[next]);
        consider(li, ri, rn);
        consider(li, rn, ln);
    }

    hitDepth = best;
    return hit;
}

bool rayIntersectsPoint(const PickRay &ray,
                        const AcGePoint3d &location,
                        double tolerance,
                        double &hitDepth)
{
    return acgs::rayIntersectsPoint(
        {ray.origin, ray.direction}, location, tolerance,
        ray.view.orthographic ? ray.view.nearDepth : 0.0,
        ray.view.farDepth, hitDepth);
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
    bool hit = false;
    double best = std::numeric_limits<double>::infinity();

    for (size_t vertex = 0; vertex + 1 < edges.size() / 8; vertex += 2)
    {
        auto position = [&](size_t index) {
            const size_t first = index * 8;
            return AcGePoint3d(edges[first], edges[first + 1],
                               edges[first + 2]);
        };
        const AcGePoint3d a = position(vertex);
        const AcGePoint3d b = position(vertex + 1);
        const AcGePoint3d worldPoint =
            object.worldPosition + (a.asVector() + b.asVector()) * (0.5 * scale);
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
    const double directionLength = ray.direction.length();
    if (directionLength < 1.0e-20)
        return false;

    const bool ortho = ray.view.orthographic;
    const double localDepthNear = ortho
    ? ray.view.nearDepth * directionLength / scale : 0.0;
    const double localDepthFar = ortho
    ? ray.view.farDepth * directionLength / scale
        : std::numeric_limits<double>::infinity();
    const AcGeVector3d origin =
        (ray.origin - object.worldPosition) / scale;
    const AcGeVector3d direction = ray.direction / directionLength;
    double best = std::numeric_limits<double>::infinity();
    const PickRay localRay{AcGePoint3d(origin.x, origin.y, origin.z),
                           direction, ray.view};

    const rendering::RenderModeFlags renderFlags = acgs::acgsGetManager()->deviceReady()
        ? acgs::acgsGetManager()->deviceRenderModeFlags()
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
                return AcGePoint3d(soup[first], soup[first + 1],
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
double cadPickTolerance(const PickRay &ray, const AcGePoint3d &worldPoint)
{
    return acgs::pickCursorTolerance(
        {ray.origin, ray.direction}, worldPoint, ray.view, 5.0);
}

double cadCurvePickTolerance(const PickRay &ray,
                             const AcGePoint3d &worldPoint)
{
    // CurveBatchCommand is projected like every other cursor-sized overlay.
    // A fixed world-space radius is wrong as soon as ortho zoom changes.
    return cadPickTolerance(ray, worldPoint);
}

double cadStrokePickTolerance(const PickRay &ray,
                              const acdb::Stroke &stroke,
                              const AcGePoint3d &worldPoint)
{
    const double renderedHalfWidth = acgsView().strokeHalfWidth(stroke);
    return std::max(cadPickTolerance(ray, worldPoint), renderedHalfWidth);
}

double cadPointPickTolerance(const PickRay &ray,
                             const acdb::TessellatedPoint &point)
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
  acgs::math::DVec3 cameraPos{0.0};
  acgs::math::DVec3 right{1.0, 0.0, 0.0};
  acgs::math::DVec3 up{0.0, 1.0, 0.0};
  acgs::math::DVec3 front{0.0, 0.0, -1.0};
  double halfWidth = 1.0;
  double halfHeight = 1.0;
  double nearDepth = 0.05;
  double farDepth = 1.0e9;
  double tanHalfVertical = 0.4142;
  double tanHalfHorizontal = 0.4142;
  double drawableHeight = 1.0;
  double minPixelExtent = 0.1;

  static UnifiedVisibilityQuery makeOrtho(
      const acgs::math::DVec3 &cameraPosition, const acgs::math::DVec3 &cameraRight,
      const acgs::math::DVec3 &cameraUp, const acgs::math::DVec3 &cameraFront,
      double halfWidthValue, double halfHeightValue, double pixelsHigh)
  {
    return UnifiedVisibilityQuery{
        true, cameraPosition, cameraRight, cameraUp, cameraFront,
        halfWidthValue, halfHeightValue, 0.0, 0.0, 0.0, 0.0,
        pixelsHigh, 0.1};
  }

  static UnifiedVisibilityQuery makePerspective(
      const acgs::math::DVec3 &cameraPosition, const acgs::math::DVec3 &cameraRight,
      const acgs::math::DVec3 &cameraUp, const acgs::math::DVec3 &cameraFront,
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
    const acgs::math::DVec3 center = (candidate.min + candidate.max) * 0.5;
    const acgs::math::DVec3 halfExtent =
        acgs::math::max((candidate.max - candidate.min) * 0.5,
                 acgs::math::DVec3(0.5e-3));
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
                                 : acgs::math::length(candidate.max - candidate.min);
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
    const acgs::AcGsEntityRange &range, VisibilityKind kind);

const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates();

UnifiedVisibilityQuery makePickingVisibilityQuery()
{
  const double aspect = (double)currentDrawableWidth() /
                        (double)currentDrawableHeight();
  const acgs::math::DVec3 cameraPosition(orbitCam.Position);
  if (useOrthoProjection())
  {
    return UnifiedVisibilityQuery::makeOrtho(
        cameraPosition, orbitCam.Right, orbitCam.Up, orbitCam.Front,
        orbitCam.orthoSize() * aspect, orbitCam.orthoSize(),
        (double)currentDrawableHeight());
  }

  const double tanHalfVertical = std::tan(acgs::math::radians(45.0) * 0.5);
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
    AcGePoint3d pivot;
    double hitDepth = 0.0;
    bool hit = false;
    std::string objectName = "Scene";
    // Zero-based index into the selected procedural mesh's triangle list.
    std::optional<size_t> faceIndex;
};

struct AutofocusResult
{
    std::string entityName;
    AcGePoint3d hitPivot;
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
        const acgs::math::DVec3 halfExtent(object->size * 0.5);
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
                            object->entity.color.alpha() >= 0.999f);
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
                            centerCube.entity.color.alpha() >= 0.999f);
        }
    }

    // CAD vector primitives are CPU-tessellated for drawing; test that same
    // geometry so lines, curves, fills, and points participate in autofocus.
    // The pick is a near-ordered BVH traversal: candidate boxes are visited
    // closest-first, and the exact per-kind tests prune with nearestDepth,
    // so the walk stops once no remaining candidate can win (previously a
    // flat scan over all visible candidates).
    if (cadEntityDemoEnabled())
    {
        const VectorPrimitivesTessellation &cad =
            getVectorPrimitivesTessellation();
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
        getCadRangeBvh().rayTraverse(
            ray.origin, ray.direction,
            ray.view.orthographic ? ray.view.nearDepth : 0.0,
            ray.view.farDepth,
            [&](std::uint32_t objectIndex, double entryDepth) {
                if (entryDepth > nearestDepth)
                    return false; // nothing later can beat the best hit
                const VisibilityCandidate &candidate =
                    *getCadRangeBvh().object(objectIndex);
                const acgs::AcGsEntityRange &range = *candidate.cadRange;
                switch (candidate.kind)
                {
                case VisibilityKind::CadStroke:
                {
                    double bandDepth = 0.0;
                    if (rayIntersectsCadPairedBand(ray, cad.geometry, range,
                                                   bandDepth))
                    {
                        considerCadOverlayHit(bandDepth, range.name.c_str(),
                                              VisibilityKind::CadStroke);
                        break;
                    }
                    for (size_t strokeIndex = range.begin;
                         strokeIndex < range.begin + range.count;
                         ++strokeIndex)
                    {
                        const acdb::Stroke &stroke =
                            cad.geometry.strokes[strokeIndex];
                        if (!stroke.common.visible ||
                            stroke.points.size() < 2)
                            continue;
                        const size_t segmentCount =
                            stroke.closed ? stroke.points.size()
                                          : stroke.points.size() - 1;
                        for (size_t i = 0; i < segmentCount; ++i)
                        {
                            double hitDepth = 0.0;
                            const size_t next =
                                (i + 1) % stroke.points.size();
                            if (rayIntersectsSegmentWithTolerance(
                                    ray, stroke.points[i],
                                    stroke.points[next],
                                    [&](double depth) {
                                        return cadStrokePickTolerance(
                                            ray, stroke,
                                            ray.origin +
                                                ray.direction * depth);
                                    },
                                    hitDepth,
                                    ray.view.orthographic
                                        ? ray.view.nearDepth
                                        : 0.0,
                                    ray.view.farDepth,
                                    stroke.semiInfinite))
                            {
                                considerCadOverlayHit(
                                    hitDepth, range.name.c_str(),
                                    VisibilityKind::CadStroke);
                            }
                        }
                    }
                    break;
                }
                case VisibilityKind::CadFill:
                {
                    for (size_t fillIndex = range.begin;
                         fillIndex < range.begin + range.count; ++fillIndex)
                    {
                        const acdb::Triangle &triangle =
                            cad.geometry.fills[fillIndex];
                        if (!triangle.common.visible)
                            continue;
                        double hitDepth = 0.0;
                        if (rayIntersectsTriangle(ray, triangle.a,
                                                  triangle.b, triangle.c,
                                                  hitDepth))
                        {
                            considerCadSurfaceHit(hitDepth,
                                                  range.name.c_str());
                        }
                    }
                    break;
                }
                case VisibilityKind::CadCurve:
                {
                    if (!candidate.curve)
                        break;
                    const std::vector<acgs::math::DVec3> sampledPoints =
                        acgs::AcGsView::sampleCurveBatch(*candidate.curve);
                    std::vector<AcGePoint3d> points;
                    points.reserve(sampledPoints.size());
                    for (const acgs::math::DVec3 &point : sampledPoints)
                        points.emplace_back(point);
                    for (size_t i = 0; i + 1 < points.size(); ++i)
                    {
                        double hitDepth = 0.0;
                        if (rayIntersectsSegment(
                                ray, points[i], points[i + 1],
                                cadCurvePickTolerance(ray, points[i + 1]),
                                hitDepth))
                        {
                            considerCadOverlayHit(
                                hitDepth, candidate.curve->name.c_str(),
                                VisibilityKind::CadCurve);
                        }
                    }
                    break;
                }
                case VisibilityKind::CadPoint:
                {
                    for (size_t pointIndex = range.begin;
                         pointIndex < range.begin + range.count;
                         ++pointIndex)
                    {
                        const acdb::TessellatedPoint &point =
                            cad.geometry.points[pointIndex];
                        if (!point.common.visible)
                            continue;
                        double hitDepth = 0.0;
                        if (rayIntersectsPoint(
                                ray, point.location,
                                cadPointPickTolerance(ray, point),
                                hitDepth))
                        {
                            considerCadOverlayHit(
                                hitDepth, range.name.c_str(),
                                VisibilityKind::CadPoint);
                        }
                    }
                    break;
                }
                default:
                    break;
                }
                return true;
            });
    }

    if (trace.cadOverlayHit)
    {
        const AcGePoint3d overlayPivot =
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
std::optional<AcGePoint3d> viewCenterObjectPivot()
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
        (result.pivot - AcGePoint3d(orbitCam.Position))
            .dotProduct(AcGeVector3d(orbitCam.Front));
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

        const AcGePoint3d hitPivot = ray.origin + ray.direction * hitDepth;
        const double viewDepth =
            (hitPivot - AcGePoint3d(orbitCam.Position))
                .dotProduct(AcGeVector3d(orbitCam.Front));
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
        const std::vector<acgs::math::DVec3> sampledPoints =
            acgs::AcGsView::sampleCurveBatch(*pickEntity->curve);
        std::vector<AcGePoint3d> points;
        points.reserve(sampledPoints.size());
        for (const acgs::math::DVec3 &point : sampledPoints)
            points.emplace_back(point);
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
        const AcGePoint3d hitPivot = ray.origin + ray.direction * bestDepth;
        const double viewDepth =
            (hitPivot - AcGePoint3d(orbitCam.Position))
                .dotProduct(AcGeVector3d(orbitCam.Front));
        if (!entityNameIsInfinite(pickEntity->curve->name))
            orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
        return AutofocusResult{pickEntity->curve->name, hitPivot, viewDepth};
    }

    if (pickEntity && pickEntity->kind == VisibilityKind::CadText &&
        pickEntity->text)
    {
      // Glyphs pick as one block: refine the GPU hit against the text's
      // plane and block rectangle (the same frame the ID quads used).
      const acgi::TextRequest &request = *pickEntity->text;
      const acgi::TextEngine::TextBlockFrame frame =
          acgi::textEngine().textBlockFrame(request);
      const AcGeVector3d frameRight(frame.right);
      const AcGeVector3d frameUp(frame.up);
      const AcGeVector3d frameNormal(frame.normal);
      const double denom = ray.direction.dotProduct(frameNormal);
      double hitDepth = 0.0;
      bool hit = false;
      if (std::abs(denom) > 1.0e-12)
      {
        const double t =
            (AcGePoint3d(request.position) - ray.origin)
                    .dotProduct(frameNormal) /
                denom;
        if (t > 0.0)
        {
          const AcGeVector3d local =
              ray.origin + ray.direction * t - AcGePoint3d(request.position);
          const double u = local.dotProduct(frameRight);
          const double v = local.dotProduct(frameUp);
          const double margin = request.height * 0.25;
          if (-margin <= u && u <= frame.width + margin &&
              frame.minV - margin <= v && v <= frame.maxV + margin)
          {
            hit = true;
            hitDepth = t;
          }
        }
      }
      if (!hit)
        return std::nullopt;
      const AcGePoint3d hitPivot = ray.origin + ray.direction * hitDepth;
      const double viewDepth =
          (hitPivot - AcGePoint3d(orbitCam.Position))
              .dotProduct(AcGeVector3d(orbitCam.Front));
      if (!entityNameIsInfinite(request.message))
        orbitCam.setTargetDepth(viewDepth, useOrthoProjection());
      return AutofocusResult{request.message, hitPivot, viewDepth};
    }

    if (!pickEntity || !pickEntity->cadRange)
        return std::nullopt;

    const VectorPrimitivesTessellation &cad =
        getVectorPrimitivesTessellation();
    const acgs::AcGsEntityRange &range = *pickEntity->cadRange;
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
                const acdb::Stroke &stroke =
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
                            depth,
                            ray.view.orthographic ? ray.view.nearDepth : 0.0,
                            ray.view.farDepth,
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
            const acdb::Triangle &triangle = cad.geometry.fills[fillIndex];
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
            const acdb::TessellatedPoint &point =
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

    const AcGePoint3d hitPivot = ray.origin + ray.direction * bestDepth;
    const double viewDepth =
        (hitPivot - AcGePoint3d(orbitCam.Position))
            .dotProduct(AcGeVector3d(orbitCam.Front));
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
  thread_local GpuPickEntity converted;
  converted = {};
  const MeshEntityRecord centerCube = getCenterCubeEntity();
  if (centerCube.displayName() == name)
  {
    converted.kind = VisibilityKind::CenterCube;
    return &converted;
  }
  const GpuPickEntity *result = nullptr;
  gpuPickManager().forEach(
      [&](std::uint32_t, const acgs::AcGsPickEntity &registered) {
        if (result)
          return;
        if (registered.mesh &&
            static_cast<const MeshEntityRecord *>(registered.mesh)
                ->displayName() == name)
        {
          converted.kind = VisibilityKind(registered.kind);
          converted.mesh =
              static_cast<const MeshEntityRecord *>(registered.mesh);
          converted.cadRange = nullptr;
          converted.curve = nullptr;
          result = &converted;
        }
        else if (registered.range &&
                 static_cast<const acgs::AcGsEntityRange *>(registered.range)
                     ->name == name)
        {
          converted.kind = VisibilityKind(registered.kind);
          converted.mesh = nullptr;
          converted.cadRange =
              static_cast<const acgs::AcGsEntityRange *>(registered.range);
          converted.curve = nullptr;
          result = &converted;
        }
        else if (registered.curve &&
                 static_cast<const acgs::CurveBatchCommand *>(
                     registered.curve)
                     ->name == name)
        {
          converted.kind = VisibilityKind(registered.kind);
          converted.mesh = nullptr;
          converted.cadRange = nullptr;
          converted.curve =
              static_cast<const acgs::CurveBatchCommand *>(
                  registered.curve);
          result = &converted;
        }
        else if (registered.text &&
                 static_cast<const acgi::TextRequest *>(registered.text)
                         ->message == name)
        {
          converted.kind = VisibilityKind(registered.kind);
          converted.mesh = nullptr;
          converted.cadRange = nullptr;
          converted.curve = nullptr;
          converted.text =
              static_cast<const acgi::TextRequest *>(registered.text);
          result = &converted;
        }
      });
  if (result)
    return result;

  // The explicit CPU-picking mode does not populate the GPU ID registry.
  // Resolve the same entity identity from the current scene candidates so
  // CPU picks still drive the viewport's native selection highlights.
  static std::vector<VisibilityCandidate> candidates;
  candidates.clear();
  buildVisibilityCandidates(candidates);
  for (const VisibilityCandidate &candidate : candidates)
  {
    if (candidate.mesh && candidate.mesh->displayName() == name)
    {
      converted.kind = candidate.kind;
      converted.mesh = candidate.mesh;
      return &converted;
    }
    if (candidate.cadRange && candidate.cadRange->name == name)
    {
      converted.kind = candidate.kind;
      converted.cadRange = candidate.cadRange;
      return &converted;
    }
    if (candidate.curve && candidate.curve->name == name)
    {
      converted.kind = VisibilityKind::CadCurve;
      converted.curve = candidate.curve;
      return &converted;
    }
  }
  for (const acgi::TextRequest &request : acgi::textRequests())
  {
    if (request.message == name)
    {
      converted.kind = VisibilityKind::CadText;
      converted.text = &request;
      return &converted;
    }
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
    acgs::math::DVec3 location;
    double extent = 0.0;
    VisibilityCandidate candidate;
    const MeshEntityRecord *mesh = nullptr;
  };
  std::vector<AuditSample> samples;
  samples.reserve(cad.strokeRanges.size() + cad.fillRanges.size() +
                  cad.pointRanges.size() + cad.fillRanges.size() +
                  cad.meshes.size() + 4);

  for (const acgs::AcGsEntityRange &range : cad.strokeRanges)
  {
    if (!range.count)
      continue;
    const acdb::Stroke &stroke =
        cad.geometry.strokes[range.begin];
    if (!stroke.common.visible || stroke.points.size() < 2)
      continue;
    const double extent = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadStroke).lodSize;
    const acgs::math::DVec3 location =
        (stroke.points[0] + stroke.points[1]) * 0.5;
    AuditSample sample{range.name, VisibilityKind::CadStroke,
                       location, extent};
    sample.candidate = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadStroke);
    samples.push_back(std::move(sample));
  }
  for (const acgs::AcGsEntityRange &range : cad.fillRanges)
  {
    if (!range.count)
      continue;
    const acdb::Triangle &fill = cad.geometry.fills[range.begin];
    if (!fill.common.visible)
      continue;
    const double extent = makeCadRangeCandidate(
        cad, range, VisibilityKind::CadFill).lodSize;
    const double localExtent = std::max(
        {acgs::math::distance(fill.a, fill.b),
         acgs::math::distance(fill.b, fill.c),
         acgs::math::distance(fill.c, fill.a)});
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
  for (const acgs::AcGsEntityRange &range : cad.pointRanges)
  {
    if (!range.count)
      continue;
    const acdb::TessellatedPoint &point =
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
      sample.location += acgs::math::DVec3(0.325, 0.175, 0.0) *
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
  const acgs::math::DQuat previousRotation = orbitCam.Rotation;
  const acgs::math::DVec3 previousTarget = orbitCam.Target;
  const double previousDistance = orbitCam.Distance;
  const acgs::math::DVec3 previousWorldUp = orbitCam.WorldUp;
  const float previousZoom = orbitCam.Zoom;
  useOrthoProjection() = true;
  orbitCam.Zoom = 45.0f;

  constexpr int kAzimuthCount = 8;
  constexpr double kElevations[] = {
      acgs::math::radians(35.0), acgs::math::radians(70.0)};
  using AuditKey = std::pair<VisibilityKind, std::string>;
  std::map<AuditKey, AuditStats> results;
  for (const AuditSample &sample : samples)
  {
    AuditStats stats;
    stats.kind = sample.kind;
    results.emplace(AuditKey{sample.kind, sample.name}, stats);
  }

  auto setAuditCamera = [&](const acgs::math::DVec3 &target,
                            const acgs::math::DVec3 &viewDirection, double distance) {
    const acgs::math::DVec3 front = acgs::math::normalize(viewDirection);
    acgs::math::DVec3 right(1.0, 0.0, 0.0);
    if (std::abs(front.z) < 0.999)
      right = acgs::math::normalize(
          acgs::math::cross(front, acgs::math::DVec3(0.0, 0.0, 1.0)));
    const acgs::math::DVec3 up = acgs::math::normalize(acgs::math::cross(right, front));
    orbitCam.Rotation = acgs::math::DQuat(
        acgs::math::DMat3(right, up, -front));
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
    const PickRay ray{
        AcGePoint3d(orbitCam.Position),
        AcGeVector3d(sample.location - orbitCam.Position).normal(),
        currentPickView()};
    bool directHit = false;
    if (sample.kind == VisibilityKind::CadStroke)
    {
      for (size_t i = sample.candidate.rangeBegin;
           i < sample.candidate.rangeBegin + sample.candidate.rangeCount; ++i)
      {
        const acdb::Stroke &stroke = cad.geometry.strokes[i];
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
        const acdb::Triangle &fill = cad.geometry.fills[i];
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
      const double angle =
          2.0 * std::numbers::pi_v<double> * azimuth / kAzimuthCount;
      for (const double elevation : kElevations)
      {
        const acgs::math::DVec3 viewDirection = acgs::math::normalize(acgs::math::DVec3(
            std::cos(elevation) * std::cos(angle),
            std::cos(elevation) * std::sin(angle),
            std::sin(elevation)));
        setAuditCamera(sample.location, viewDirection, distance);
        const PickRay ray{
            AcGePoint3d(orbitCam.Position),
            AcGeVector3d(sample.location - orbitCam.Position).normal(),
            currentPickView()};
        recordSample(sample, pickObjectAlongRay(ray));
      }
    }

    setAuditCamera(sample.location, acgs::math::DVec3(0.0, 0.0, -1.0), distance);
    recordSample(sample, pickObjectAlongRay(
        {AcGePoint3d(orbitCam.Position), AcGeVector3d(0.0, 0.0, -1.0),
         currentPickView()}));
    setAuditCamera(sample.location, acgs::math::DVec3(0.0, 0.0, 1.0), distance);
    recordSample(sample, pickObjectAlongRay(
        {AcGePoint3d(orbitCam.Position), AcGeVector3d(0.0, 0.0, 1.0),
         currentPickView()}));

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

void expandWorldAabb(acgs::math::DVec3 &minimum, acgs::math::DVec3 &maximum,
                     const acgs::math::DVec3 &point)
{
  minimum = acgs::math::min(minimum, point);
  maximum = acgs::math::max(maximum, point);
}

VisibilityCandidate makeMeshCandidate(const MeshEntityRecord &mesh,
                                      VisibilityKind kind,
                                      size_t entityIndex)
{
  const acgs::math::DVec3 halfExtent(mesh.size * 0.5);
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
    const acgs::AcGsEntityRange &range, VisibilityKind kind)
{
  VisibilityCandidate candidate;
  candidate.kind = kind;
  candidate.entityIndex = 0;
  candidate.cadRange = &range;
  candidate.rangeBegin = range.begin;
  candidate.rangeCount = range.count;
  candidate.overlayPointSize = 2.0f;

  bool hasPoint = false;
  acgs::math::DVec3 minimum(0.0);
  acgs::math::DVec3 maximum(0.0);
  auto include = [&](const acgs::math::DVec3 &point) {
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
      const acdb::Stroke &stroke = tessellation.geometry.strokes[i];
      for (const acgs::math::DVec3 &point : stroke.points)
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
        const acgs::math::DVec3 direction =
            stroke.points[1] - stroke.points.front();
        const double length = acgs::math::length(direction);
        if (length > 1.0e-18)
          include(stroke.points.front() + direction / length *
                      kSemiInfiniteClassificationLength);
      }
      candidate.overlayColor = acgsView().contrastColor(
          stroke.common.color.renderColor());
      candidate.overlayPointSize = float(std::max(stroke.common.lineWeight, 2.0));
    }
  }
  else if (kind == VisibilityKind::CadFill)
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const acdb::Triangle &fill = tessellation.geometry.fills[i];
      include(fill.a);
      include(fill.b);
      include(fill.c);
      candidate.overlayColor = acgsView().contrastColor(
          fill.common.color.renderColor());
    }
  }
  else
  {
    for (size_t i = range.begin; i < range.begin + range.count; ++i)
    {
      const acdb::TessellatedPoint &point =
          tessellation.geometry.points[i];
      include(point.location);
      candidate.overlayColor = acgsView().contrastColor(
          point.common.color.renderColor());
      candidate.overlayPointSize = float(point.pointSize);
    }
  }

  if (!hasPoint)
    return candidate;

  const double padding = 1.0e-3;
  minimum -= acgs::math::DVec3(padding);
  maximum += acgs::math::DVec3(padding);
  candidate.min = minimum;
  candidate.max = maximum;
  candidate.center = (minimum + maximum) * 0.5;
  const acgs::math::DVec3 extent = maximum - minimum;
  candidate.lodSize = acgs::math::max(extent.x, acgs::math::max(extent.y, extent.z));
  return candidate;
}

const std::vector<VisibilityCandidate> &cadRangeVisibilityCandidates()
{
  static std::uint64_t cachedRevision =
      std::numeric_limits<std::uint64_t>::max();
  static std::vector<VisibilityCandidate> candidates;
  const std::uint64_t revision = demoSceneStore().revision();
  if (cachedRevision != revision)
  {
    candidates.clear();
    const VectorPrimitivesTessellation &cad =
        getVectorPrimitivesTessellation();
    std::vector<VisibilityCandidate> result;
    result.reserve(cad.strokeRanges.size() + cad.fillRanges.size() +
                   cad.pointRanges.size() + cad.curves.size());
    for (const acgs::AcGsEntityRange &range : cad.strokeRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadStroke));
    }
    for (const acgs::AcGsEntityRange &range : cad.fillRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadFill));
    }
    for (const acgs::AcGsEntityRange &range : cad.pointRanges)
    {
      if (range.count)
        result.push_back(makeCadRangeCandidate(
            cad, range, VisibilityKind::CadPoint));
    }
    for (const acgs::CurveBatchCommand &curve : cad.curves)
      result.push_back(makeCurveCandidate(curve));
    candidates = std::move(result);
    cachedRevision = revision;
  }
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
void includeSegmentCameraDepth(const acgs::math::DVec3 &startWorldPosition,
                               const acgs::math::DVec3 &endWorldPosition,
                               const acgs::math::DVec3 &cameraPosition,
                               const acgs::math::DVec3 &cameraRight,
                               const acgs::math::DVec3 &cameraUp,
                               const acgs::math::DVec3 &cameraFront,
                               double halfWidth,
                               double halfHeight,
                               double &minDepth,
                               double &maxDepth)
{
  const CameraSpacePoint p0 = acgs::toCameraSpace(
      startWorldPosition, cameraPosition, cameraRight, cameraUp, cameraFront);
  const CameraSpacePoint p1 = acgs::toCameraSpace(
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
// Clip a world-space polygon against one camera-depth plane.  Camera depth
// uses the same +Z-forward convention as the depth slab.  This keeps the
// debug wireframe aligned with what the grid shader actually accepts:
// fragments outside the [near, far] slab are discarded by depth test.
void clipPolygonAgainstDepth(const acgs::math::DVec3 *polygon, int &count,
                             acgs::math::DVec3 *clipped, int maxClipped,
                             const acgs::math::DVec3 &cameraPos,
                             const acgs::math::DVec3 &front,
                             bool keepAtMost, double depthLimit)
{
    if (count <= 0)
    {
        count = 0;
        return;
    }

    std::array<acgs::math::DVec3, 16> output{};
    int outputCount = 0;

    auto cameraDepth = [&](const acgs::math::DVec3 &p) {
        return acgs::math::dot(p - cameraPos, front);
    };
    auto inside = [&](double depth) {
        return keepAtMost ? depth <= depthLimit
                          : depth >= depthLimit;
    };
    auto addPoint = [&](const acgs::math::DVec3 &p) {
        if (outputCount >= (int)output.size())
            return;
        output[outputCount++] = p;
    };
    auto addIntersection = [&](const acgs::math::DVec3 &a,
                               const acgs::math::DVec3 &b) {
        const double da = cameraDepth(a) - depthLimit;
        const double db = cameraDepth(b) - depthLimit;
        const double denom = da - db;
        const double t = std::abs(denom) > 1e-18 ? da / denom : 0.0;
        addPoint(a + (b - a) * acgs::math::clamp(t, 0.0, 1.0));
    };

    bool previousInside = inside(cameraDepth(polygon[count - 1]));
    for (int i = 0; i < count; ++i)
    {
        const acgs::math::DVec3 &current = polygon[i];
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
  if (!acgs::acgsGetManager()->deviceReady())
    return;
  if (outlineAllAtStartup && !outlineAllTest)
  {
    outlineAllTest = true;
    acgs::acgsGetManager()->setSelectionOutlineAll(true);
  }
  if (!selectNameAtStartup.empty())
  {
    static bool selectNameApplied = false;
    if (!selectNameApplied && !outlineEntity.has_value())
    {
      if (pickDebugEnabled())
      {
        // One-shot registry name dump: what can GRID_SELECT_NAME match?
        static bool namesDumped = false;
        if (!namesDumped && gpuPickManager().registrySize() > 0)
        {
          namesDumped = true;
          int printed = 0;
          gpuPickManager().forEach(
              [&](std::uint32_t id, const acgs::AcGsPickEntity &registered) {
                if (printed >= 12)
                  return;
                if (registered.mesh)
                  std::printf("[SELECT_DEBUG] mesh id=%u name=%s\n", id,
                              static_cast<const MeshEntityRecord *>(
                                  registered.mesh)
                                  ->displayName()
                                  .c_str());
                else if (registered.range)
                  std::printf("[SELECT_DEBUG] range id=%u name=%s\n", id,
                              static_cast<const acgs::AcGsEntityRange *>(
                                  registered.range)
                                  ->name.c_str());
                else if (registered.curve)
                  std::printf("[SELECT_DEBUG] curve id=%u name=%s\n", id,
                              static_cast<const acgs::CurveBatchCommand *>(
                                  registered.curve)
                                  ->name.c_str());
                ++printed;
              });
          std::fflush(stdout);
        }
      }
      if (const GpuPickEntity *entity =
              findGpuPickEntityForAutofocusName(selectNameAtStartup))
      {
        outlineEntity = *entity;
        selectNameApplied = true;
        std::cout << "Startup selection: " << selectNameAtStartup
                  << std::endl;
        if (std::getenv("GRID_SELECT_FOCUS") != nullptr)
        {
          // Validation aid: aim the camera at the selected entity so the
          // capture can inspect its highlight. Meshes aim at the record's
          // world position (sized by the record); curves at the batch
          // center; text is handled in the message branch below; range
          // entities keep the startup camera (their centers live in the
          // tessellation).
          if (entity->mesh != nullptr)
          {
            const MeshEntityRecord *mesh = entity->mesh;
            orbitCam.setTarget(mesh->worldPosition);
            orbitCam.setTargetDistance(
                std::max(200.0, double(mesh->size) * 6.0));
            // Tilt down ~35 degrees so the mesh's filled faces (not just
            // its edge-on silhouette) are visible for the capture.
            orbitCam.setViewDirection(
                acgs::math::normalize(acgs::math::DVec3(0.25, 0.25, -0.9)));
          }
          else if (entity->curve != nullptr)
          {
            orbitCam.setTarget(entity->curve->center);
            orbitCam.setTargetDistance(
                std::max(200.0, entity->curve->radius * 6.0));
          }
          else if (entity->cadRange != nullptr)
          {
            // Range entities (stroke/point/fill) carry no center: average
            // their tessellated vertices.
            const auto &tess = getVectorPrimitivesTessellation();
            const acgs::AcGsEntityRange &range = *entity->cadRange;
            acgs::math::DVec3 center(0.0);
            size_t count = 0;
            const size_t last =
                std::min(range.begin + range.count, size_t(1) << 30);
            if (entity->kind == VisibilityKind::CadStroke)
            {
              const size_t last2 = std::min(last, tess.geometry.strokes.size());
              for (size_t i = range.begin; i < last2; ++i)
                for (const acgs::math::DVec3 &pt : tess.geometry.strokes[i].points)
                {
                  center += pt;
                  ++count;
                }
            }
            else if (entity->kind == VisibilityKind::CadFill)
            {
              const size_t last2 = std::min(last, tess.geometry.fills.size());
              for (size_t i = range.begin; i < last2; ++i)
              {
                center += tess.geometry.fills[i].a +
                          tess.geometry.fills[i].b +
                          tess.geometry.fills[i].c;
                count += 3;
              }
            }
            else
            {
              const size_t last2 = std::min(last, tess.geometry.points.size());
              for (size_t i = range.begin; i < last2; ++i)
              {
                center += tess.geometry.points[i].location;
                ++count;
              }
            }
            if (count > 0)
            {
              orbitCam.setTarget(center / double(count));
              orbitCam.setTargetDistance(std::max(300.0, orbitCam.Distance));
            }
          }
        }
      }
      else
      {
        // Text fallback: substring match on the message. Exact matching
        // needs the source encoding (CJK demo messages are UTF-8) while
        // getenv hands over ANSI bytes; an ASCII substring sidesteps that.
        // LAST match wins: re-tessellation pushes a fresh copy of each
        // request, and earlier copies keep stale pointers that never reach
        // the ID pass.
        const acgi::TextRequest *matched = nullptr;
        for (const acgi::TextRequest &request : acgi::textRequests())
        {
          if (request.message.find(selectNameAtStartup) ==
              std::string::npos)
            continue;
          matched = &request;
        }
        if (matched != nullptr)
        {
          outlineEntity = GpuPickEntity{VisibilityKind::CadText, nullptr,
                                        nullptr, nullptr, matched};
          selectNameApplied = true;
          std::cout << "Startup selection (text message): "
                    << selectNameAtStartup << std::endl;
          if (std::getenv("GRID_SELECT_FOCUS") != nullptr)
          {
            // Validation aid: top-down view of the text so the capture can
            // inspect the highlight without scripted input (a horizontal
            // startup camera sees ground text edge-on, with zero area).
            const double focusDistance =
                std::max(300.0, matched->height * 10.0);
            orbitCam.setTarget(matched->position);
            orbitCam.setTargetDistance(focusDistance);
            orbitCam.setViewDirection(acgs::math::DVec3(0.0, 0.0, -1.0));
          }
        }
      }
    }
  }
  if (idVisibleAtStartup)
  {
    static bool s_idVisibleApplied = false;
    if (!s_idVisibleApplied)
    {
      s_idVisibleApplied = true;
      acgs::acgsGetManager()->setGpuPickDebugVisible(true);
    }
  }

  // ECS mirror watcher: surface document mutations that landed since the
  // last frame (reactor -> Dirty -> revision).
  reportDemoSceneMirror();

  // ARX zoom/pan write-back: the live camera lands in its viewport
  // record every frame (a few doubles; the record is drawing data).
  {
    acdb::AcDbDatabase &document = acdbDocument();
    const std::string &recordName = acgsView().viewportRecordName();
    const char *bound = !recordName.empty()
                            ? recordName.c_str()
                            : acdb::kActiveViewportName;
    if (document.viewportTable().contains(bound))
      acgsView().writeToViewportRecord(
          *document.viewportTable().getMutable(bound));
  }

  // SYCAD round-robin: each frame renders ONE viewport into the shared
  // scene target, then blits into that viewport's persistent final;
  // the other panel keeps displaying its last blit.  A panel click
  // pins the slot until its pick resolves so the pixel read samples
  // the clicked view's camera.
  static std::uint64_t s_frameIndex = 0;
  int renderSlot =
      multiViewEnabled() && acgs::acgsGetManager()->viewCount() > 1
          ? int(s_frameIndex % 2)
          : 0;
  if (g_pickedSlot >= 0 &&
      (gpuPickFocus.pendingNdc || gpuPickFocus.waitingResult))
    renderSlot = g_pickedSlot;
  ++s_frameIndex;
  g_activeSceneSlot = renderSlot;
  if (g_panelPx[renderSlot].x > 0.0f && g_panelPx[renderSlot].y > 0.0f)
  {
    acgs::acgsGetManager()->setSceneRenderSize(
        std::uint32_t(g_panelPx[renderSlot].x),
        std::uint32_t(g_panelPx[renderSlot].y));
  }
  acgs::acgsGetManager()->setActiveSceneSlot(renderSlot);

  if (!acgs::acgsGetManager()->beginFrame())
    return;

  // The second viewport materializes lazily once the document bounds
  // exist (the tessellation builds during the first frames).
  static bool s_secondViewTried = false;
  if (!s_secondViewTried && s_frameIndex > 2 && multiViewEnabled())
  {
    s_secondViewTried = true;
    if (acgs::acgsGetManager()->viewCount() < 2)
    {
      acgs::AcGsView *top = acgs::acgsGetManager()->createView();
      top->setViewportRecordName("Top");
      acdb::AcDbDatabase &document = acdbDocument();
      acdb::AcDbViewportTableRecord &record =
          document.viewportTable().contains("Top")
              ? *document.viewportTable().getMutable("Top")
              : document.viewportTable().add(
                    "Top", document.allocateHandle());
      top->orbitCamera().setViewDirection(
          acgs::math::DVec3(0.0, 0.0, 1.0));
      top->writeToViewportRecord(record);
      std::cout << "Top viewport created" << std::endl;
    }
  }

  // The scene pipeline reads the ACTIVE view; round-robin aims it at
  // this frame's viewport.
  acgs::acgsGetManager()->setActiveView(renderSlot);

  if (gpuPickEnabled() && gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickQueueStats queueStats =
        acgs::acgsGetManager()->gpuPickQueueStats();
    if (queueStats.capacityExceeded())
    {
      gpuPickManager().finishPickReadback();
      std::cout << "GPU pick queue full meshes="
                << queueStats.queuedMeshes << "/"
                << queueStats.meshCapacity
                << " triangles=" << queueStats.queuedTriangles
                << "/" << queueStats.triangleCapacity
                << " dropped="
                << (queueStats.droppedMeshes + queueStats.droppedTriangles)
                << "; using CPU fallback" << std::endl;
      if (queueStats.resourceFailures != 0)
        std::cout << "GPU pick buffer creation failed on "
                  << queueStats.resourceFailures
                  << " geometries" << std::endl;
      reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
      gpuPickFocus.camera.reset();
    }
  }

  if (gpuPickEnabled() && gpuPickFocus.waitingResult)
  {
    const rendering::GpuPickResult gpuResult = acgs::acgsGetManager()->pollGpuPick();
    const bool resultMatches = gpuResult.ready &&
        gpuResult.requestToken == gpuPickFocus.requestToken;
    const std::uint32_t pendingFrames = gpuPickManager().advancePickFrames();
    // Vulkan readbacks can take several frames to become CPU visible. Keep
    // a bounded retry window, then fall back to the CPU raycast.
    if (resultMatches ||
        pendingFrames >= acgs::AcGsSelectionManager::kReadbackTimeoutFrames)
    {
      if (pickDebugEnabled() && !resultMatches)
      {
        std::printf("[PICK_DEBUG] timeout pendingFrames=%u last=(ready=%d id=%u token=%u)",
                    pendingFrames, gpuResult.ready ? 1 : 0,
                    gpuResult.objectId, gpuResult.requestToken);
        std::puts("");
      }
      gpuPickManager().finishPickReadback();
      if (!resultMatches)
      {
        // Stop the renderer from re-submitting the stalled pick every frame;
        // a stuck Vulkan readback otherwise loops until the next request.
        acgs::acgsGetManager()->cancelGpuPick();
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
          std::printf("[PICK_DEBUG] registry size=%zu nextId=%u found=%d",
                      gpuPickManager().registrySize(),
                      gpuPickManager().nextIdCounter(),
                      gpuPickManager().find(gpuResult.objectId) ? 1 : 0);
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
            case VisibilityKind::CadText: kindName = "CadText"; break;
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
  const acgs::math::DVec3 rebase = worldRebase().origin();

  int drawableWidth = SCREEN_WIDTH;
  int drawableHeight = SCREEN_HEIGHT;
  SDL_GetWindowSizeInPixels(appPlatform.window(), &drawableWidth, &drawableHeight);
  drawableWidth = std::max(drawableWidth, 1);
  drawableHeight = std::max(drawableHeight, 1);
  const float aspect = static_cast<float>(drawableWidth) /
                       static_cast<float>(drawableHeight);
  // RTE step: pass the rebase origin so the view matrix is built from
  // (camera - rebase, target - rebase).  Rotation is preserved; only the
  // translation column shifts, and the same shift is applied to every
  // object uniform downstream.
  acgs::math::Mat4 view = orbitCam.getViewMatrix(rebase);
  // Object pipelines use the strict RTE view: rotation only, zero translation.
  // Grid/overlay keeps the old rebase view because its ray shaders reconstruct
  // world-space rays from the inverse view-projection.
  const acgs::math::Mat4 viewRte = orbitCam.getViewRotationMatrix();
  acgs::math::Mat4 projection;
  acgs::math::Mat4 overlayProjection;
  acgs::math::Mat4 gridProjection;
  float pixelSize = 0.0f;
  double activeNear = 0.0;
  double activeFar  = 0.0;
  float overlayNear = 0.0f;
  float overlayFar = 0.0f;
  acgs::math::Vec4 logDepth(0.0f);
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
  const AcGePoint3d worldLineEnd = LARGE_COORDINATE_BASE_POINT;

  const acgs::math::DVec3 cameraPos(orbitCam.Position);
  const acgs::math::DVec3 &frontVec = orbitCam.Front;
  // The grid shader consumes rendering types, while its persistent plane
  // geometry uses the AcGe point/vector interfaces.
  const AcGeVector3d cameraFrontGe(orbitCam.Front);
  const AcGeVector3d cameraRightGe(orbitCam.Right);
  const AcGeVector3d cameraUpGe(orbitCam.Up);
  const AcGeVector3d planeNormal = gridPlaneNormal.normal();
  AcGeVector3d tangentUGe;
  AcGeVector3d tangentVGe;
  activePlaneTangents(tangentUGe, tangentVGe);

  const double frontOnNormal = cameraFrontGe.dotProduct(planeNormal);
  AcGePoint3d planeCenter;
  if (std::abs(frontOnNormal) > 1e-6)
  {
      const double planeDistance =
          (gridPlaneOrigin - AcGePoint3d(cameraPos)).dotProduct(planeNormal);
      planeCenter = AcGePoint3d(cameraPos) +
                    cameraFrontGe * (planeDistance / frontOnNormal);
  }
  else
  {
      planeCenter = AcGePoint3d(cameraPos) -
                    planeNormal *
                        (AcGePoint3d(cameraPos) - gridPlaneOrigin)
                            .dotProduct(planeNormal);
  }

  if (useOrthoProjection())
  {
    const double halfH = orbitCam.orthoSize();
    const double halfW = (double)halfH * (double)aspect;
    const acgs::math::DVec3 &front = orbitCam.Front;
    const acgs::math::DVec3 &right = orbitCam.Right;
    const acgs::math::DVec3 &up = orbitCam.Up;

    // Start with the depth interval covered by the visible ortho image.
    // The scene bounds below can make near negative; that is intentional
    // because orthographic geometry can straddle the camera plane.
    // Clamping near to zero would hide the lower half of the center cube.
    const CameraSpacePoint targetCamera =
        acgs::toCameraSpace(orbitCam.Target, cameraPos, right, up, front);
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
          object.worldPosition, acgs::math::DVec3(halfSize), cameraPos, right,
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

        // Construction/diagnostic ranges (excludeFromFit) render inside
        // the slab — the depth shader discards outside it, which is
        // exactly the old overlay's per-frame clip — but contribute no
        // bounds, so a huge-extent reference line can never re-frame the
        // view.
        const bool excludeFromFit =
            candidate.cadRange != nullptr && candidate.cadRange->excludeFromFit;

        if (!semiInfiniteRange && !excludeFromFit)
        {
          unionBounds(orthoVisibility.cameraAabb(candidate));
        }
        else if (!excludeFromFit)
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
            const acdb::Stroke &stroke = tess.geometry.strokes[i];
            if (stroke.points.size() < 2)
              continue;
            if (stroke.semiInfinite)
            {
              acgs::math::DVec3 clippedStart, clippedEnd;
              if (!acgsView().clipSemiInfiniteRay(
                      stroke.points.front(), stroke.points.back(), 0.0, 0.0,
                      clippedStart, clippedEnd, false))
                continue;
              const CameraSpacePoint a = acgs::toCameraSpace(
                  clippedStart, cameraPos, right, up, front);
              const CameraSpacePoint b = acgs::toCameraSpace(
                  clippedEnd, cameraPos, right, up, front);
              unionBounds({std::min(a.x, b.x), std::max(a.x, b.x),
                           std::min(a.y, b.y), std::max(a.y, b.y),
                           std::min(a.depth, b.depth),
                           std::max(a.depth, b.depth)});
            }
            else
            {
              for (const acgs::math::DVec3 &point : stroke.points)
              {
                const CameraSpacePoint c = acgs::toCameraSpace(
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
        acgs::math::length(orbitCam.Position - orbitCam.Target);
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
          acgs::toCameraSpace(request.position, cameraPos, right, up, front);
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
    double near = 0.0;
    double far = 0.0;
    acgsView().stabilizeDepthSlab(acgs::AcGsView::DepthSlab::Ortho,
                                slabCenterDepth - slabRadius,
                                slabCenterDepth + slabRadius, near, far);

    const acgs::math::DMat4 orthoDouble = acgs::math::ortho(
        -halfH * (double)aspect, halfH * (double)aspect,
        -halfH,                  halfH, near, far);
    projection = acgs::math::Mat4(orthoDouble);
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
    const acgs::math::DVec3 &right = orbitCam.Right;
    const acgs::math::DVec3 &up = orbitCam.Up;
    const double tanHalfVertical =
        std::tan(acgs::math::radians(45.0) * 0.5);
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
      const acgs::math::DVec3 center =
          (sceneBounds.min + sceneBounds.max) * 0.5;
      const acgs::math::DVec3 halfExtent =
          (sceneBounds.max - sceneBounds.min) * 0.5;
      const CameraSpaceAabb sceneCamera =
          cameraAabbBounds(center, halfExtent, cameraPos, right, up,
                           frontVec);
      provisionalNear = std::min(provisionalNear, sceneCamera.minDepth);
      provisionalFar = std::max(sceneCamera.maxDepth,
                                provisionalNear + kMinPerspectiveSpan);
    }

    const CameraSpacePoint targetCamera =
        acgs::toCameraSpace(orbitCam.Target, cameraPos, right, up, frontVec);
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
          const double centerDepth = acgs::math::dot(
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
          const double centerDepth = acgs::math::dot(
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
                                    : acgs::math::dot(candidate.center - cameraPos,
                                               frontVec);
        const double maxUseDepth = state == VisibilityState::Visible
                                       ? bounds.maxDepth
                                       : acgs::math::dot(candidate.center - cameraPos,
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
        acgs::toCameraSpace(acgs::math::DVec3(0.0), cameraPos, right, up, frontVec),
        acgs::toCameraSpace(worldLineEnd, cameraPos, right, up, frontVec),
        kNearDepthFloor, tanHalfVertical, tanHalfHorizontal,
        overlayMinDepth, overlayMaxDepth);

    // The infinite grid does not take part in camera depth-slab selection.
    // It still writes depth for sorting, so give it a stable dedicated frustum
    // and let its shader put distant background lines at depth one.
    gridProjection = acgs::math::Mat4(acgs::math::perspective(
        acgs::math::radians(45.0), (double)aspect,
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
    double near = 0.0;
    double far = 0.0;
    acgsView().stabilizeDepthSlab(acgs::AcGsView::DepthSlab::Perspective,
                                      candidateObjectNear,
                                      candidateObjectFar, near, far);

    const acgs::math::DMat4 perspectiveDouble = acgs::math::perspective(
        acgs::math::radians(45.0), (double)aspect, near, far);
    projection = acgs::math::Mat4(perspectiveDouble);
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
    acgsView().stabilizeDepthSlab(acgs::AcGsView::DepthSlab::Overlay,
                                (double)kNearDepthFloor, candidateOverlayFar,
                                stableOverlayNear, stableOverlayFar);
    overlayNear = stableOverlayNear;
    overlayFar = stableOverlayFar;
    const acgs::math::DMat4 overlayDouble = acgs::math::perspective(
        acgs::math::radians(45.0), (double)aspect, (double)overlayNear, (double)overlayFar);
    overlayProjection = acgs::math::Mat4(overlayDouble);

    // Rough estimate around the orbit target, only used to seed the LOD
    // step; the shader computes exact per-fragment sizes for perspective.
    pixelSize = (2.0f * (float)acgs::math::length(orbitCam.Position - orbitCam.Target)
                  * tan(acgs::math::radians(22.5f)))
               / static_cast<float>(drawableHeight);
  }

  if (useOrthoProjection())
  {
    overlayProjection = projection;
    gridProjection = projection;
    overlayNear = static_cast<float>(activeNear);
    overlayFar = static_cast<float>(activeFar);
    logDepth = acgs::math::Vec4(0.0f, overlayNear, overlayFar, 0.0f);
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
    logDepth = acgs::math::Vec4(1.0f, depthNear, depthFar, 0.0f);
  }

  // Publish this frame's overlay slab for stroke frustum clipping; the
  // full viewport context is pushed once the frame's locals exist.
  acgsView().mutableFrame().slabNear = overlayNear;
  acgsView().mutableFrame().slabFar = overlayFar;

  logSlabIfChanged(useOrthoProjection(), activeNear, activeFar,
                   overlayNear, overlayFar);

  gpuPickSceneDebugQueueActive = false;

  // The full-scene ID pass re-renders every visible entity a second time,
  // so only refresh it when something that can change the ID buffer
  // changes: view/projection, depth slab, viewport size, scene revision,
  // visual style, outlined entity, or debug view modes. While the camera is
  // idle the outline keeps sampling the last rendered ID texture and skips
  // the extra pass.
  static uint64_t lastSceneIdSignature = 0;
  static bool lastSceneIdSignatureValid = false;
  static uint32_t cachedOutlineObjectId = 0;
  uint64_t sceneIdSignature = 0xcbf29ce484222325ull;
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, acgs::math::value_ptr(viewRte), 16 * sizeof(float));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, acgs::math::value_ptr(projection), 16 * sizeof(float));
  // Strict-RTE viewRte intentionally has no translation.  Pan/rebase can
  // therefore leave viewRte and projection unchanged while every RTE vertex
  // (object - camera/rebase) changes.  Hash the camera world pose/rebase too,
  // or the selection outline keeps sampling the previous frame ID texture.
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, acgs::math::value_ptr(rebase), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, acgs::math::value_ptr(orbitCam.Target), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, acgs::math::value_ptr(orbitCam.Position), 3 * sizeof(double));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &activeNear, sizeof(activeNear));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &activeFar, sizeof(activeFar));
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &logDepth, sizeof(logDepth));
  const std::uint64_t sceneRevision = demoSceneStore().revision();
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &sceneRevision, sizeof(sceneRevision));
  const rendering::RenderMode activeRenderMode =
      acgsView().visualStyle().mode();
  sceneIdSignature = hashGpuPickSceneBytes(
      sceneIdSignature, &activeRenderMode, sizeof(activeRenderMode));
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
                             : outlineEntity->curve
                                   ? static_cast<const void *>(outlineEntity->curve)
                                   : static_cast<const void *>(outlineEntity->text);
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
  // Unified picking: the full-scene ID pass is the SINGLE ID source for the
  // K/I views, selection shading, and cursor picking.  It re-renders the
  // whole scene, so it runs ONLY when its output can change: a pick is in
  // flight, a debug ID view needs live content, or the scene signature
  // changed (camera/slab/viewport/scene/style/selection). The freshly rendered ID image
  // is copied into a persistent sampled texture (m_gpuPickIdSticky), which
  // keeps the highlight stable on gated frames at zero pass cost.
  bool gpuSceneIdPassRequested =
      gpuPickEnabled() &&
      (gpuPickFocus.pendingNdc || gpuPickFocus.waitingResult ||
       gpuPickSceneDebug || sceneIdSignatureChanged);
  if (acgs::acgsGetManager()->deviceReady())
    acgs::acgsGetManager()->setGpuPickScenePassEnabled(gpuSceneIdPassRequested);
  if (gpuPickEnabled() && gpuPickFocus.pendingNdc)
  {
    // Freeze the pose the CPU fallback refines with; the GPU path samples
    // the always-on full-scene ID texture (no per-click re-render).
    GpuPickCameraBasis pickCamera{
        orbitCam.Position, orbitCam.Front, orbitCam.Right, orbitCam.Up,
        useOrthoProjection(), orbitCam.orthoSize()};
    const acgs::ViewFrameContext &pickFrame = acgsView().frame();
    pickCamera.viewportWidth = pickFrame.viewportWidth;
    pickCamera.viewportHeight = pickFrame.viewportHeight;
    pickCamera.nearDepth = pickFrame.slabNear;
    pickCamera.farDepth = pickFrame.slabFar;
    gpuPickManager().setPickCamera(pickCamera);
    const uint32_t requestToken =
        acgs::acgsGetManager()->requestGpuPickPixel(gpuPickFocus.ndcX,
                                                    gpuPickFocus.ndcY);
    if (requestToken != 0)
    {
      if (pickDebugEnabled())
      {
        std::printf("[PICK_DEBUG] request token=%u ndc=(%f,%f)",
                    requestToken, gpuPickFocus.ndcX, gpuPickFocus.ndcY);
        std::puts("");
      }
      // No clearRegistry: the pixel read samples the standing
      // full-scene texture, whose ids live in the standing registry.
      gpuPickManager().beginPickReadback(requestToken);
    }
    else if (gpuPickManager().advancePickFrames() >=
             acgs::AcGsSelectionManager::kRequestSubmitTimeoutFrames)
    {
      gpuPickManager().finishPickSubmission();
      reportGpuPickFallback(gpuPickFocus.ndcX, gpuPickFocus.ndcY);
    }
  }

  // The queue refresh re-arms the pass AND rebuilds the CPU soup; non-
  // refresh frames sample the sticky ID copy at zero cost.
  if (gpuSceneIdPassRequested && acgs::acgsGetManager()->deviceReady() &&
      (gpuPickEnabled() || outlineEntity.has_value() || outlineAllTest) &&
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
    // prioritize the selected entity if the queue ever hits capacity.
    // Ids are stable across frames for the same scene state, and the
    // post-queue lookup below corrects the value before endFrame.
    if (outlineEntity)
      cachedOutlineObjectId =
          findGpuPickObjectIdForEntity(*outlineEntity);
    acgs::acgsGetManager()->setSelectionOutlineId(
        outlineEntity ? cachedOutlineObjectId : 0);
    // In-scene selection recolor: the selected text's glyphs draw with the
    // highlight color in their own SDF pass (unified depth buffer).
    acgsView().setTextColorOverride(
        outlineEntity && outlineEntity->kind == VisibilityKind::CadText
            ? outlineEntity->text
            : nullptr,
        acgs::math::Vec4(kSelectionHighlightRGB, 1.0f));
    gpuPickSceneDebugQueueActive =
        acgs::acgsGetManager()->requestGpuPick(sceneRequest) != 0;
    // No clearRegistry here either: ids are keyed by entity identity
    // and must stay STABLE across frames -- the selection-outline
    // uniform and the pixel read both resolve against the standing
    // registry.  A per-frame rebuild shifts every id and made the
    // outline flicker across unrelated meshes.  Entities that leave
    // the view keep a harmless stale entry; returning entities
    // re-register under the same id.
  }

  const acgs::math::DVec3 cameraRight(orbitCam.Right);
  const acgs::math::DVec3 cameraUp(orbitCam.Up);
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
  AcGePoint3d originWorld(0.0, 0.0, 0.0);
  acgs::math::Vec3 planeOriginRelative(0.0f);
  acgs::math::Vec3 orthoPlaneCenter(0.0f);
  acgs::math::Vec3 orthoRight(0.0f);
  acgs::math::Vec3 orthoUp(0.0f);
  acgs::math::Vec2 axisVisible(0.0f);
  acgs::math::Vec2 axisOriginGridRelative(0.0f);
  acgs::math::Vec3 axisLineX(0.0f);
  acgs::math::Vec3 axisLineZ(0.0f);
  acgs::math::Vec3 planeTangentU(0.0f);
  acgs::math::Vec3 planeTangentV(0.0f);
  acgs::math::Vec3 axisColorU(0.0f);
  acgs::math::Vec3 axisColorV(0.0f);
  acgs::math::Vec3 startAxisOrigin(0.0f);
  acgs::math::Vec3 startAxisDirection(0.0f);
  float startAxisVisible = 0.0f;
  acgs::math::Vec3 startAxisLine(0.0f);
  AcGeVector3d normalizedAxisLineX(0.0, 0.0, 0.0);
  AcGeVector3d normalizedAxisLineZ(0.0, 0.0, 0.0);
  bool orthoPlaneValid = false;

  {
      const AcGeVector3d planeDelta = planeCenter - gridPlaneOrigin;
      const double centerU = planeDelta.dotProduct(tangentUGe);
      const double centerV = planeDelta.dotProduct(tangentVGe);
      const double snappedU = std::round(centerU / s) * s;
      const double snappedV = std::round(centerV / s) * s;
      originWorld = gridPlaneOrigin +
                    tangentUGe * snappedU +
                    tangentVGe * snappedV;
  }

  if (useOrthoProjection())
  {
    const acgs::math::DVec3 front(orbitCam.Front);
    const acgs::math::DVec3 right(orbitCam.Right);
    const acgs::math::DVec3 up(orbitCam.Up);
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
      const AcGeVector3d imageRight = cameraRightGe * halfW;
      const AcGeVector3d imageUp = cameraUpGe * halfH;
      const AcGeVector3d groundRight =
          imageRight - cameraFrontGe *
                           (imageRight.dotProduct(planeNormal) / frontOnNormal);
      const AcGeVector3d groundUp =
          imageUp - cameraFrontGe *
                        (imageUp.dotProduct(planeNormal) / frontOnNormal);
      orthoRight = acgs::math::Vec3(static_cast<acgs::math::DVec3>(groundRight));
      orthoUp = acgs::math::Vec3(static_cast<acgs::math::DVec3>(groundUp));
      // The point is relative to the snapped plane anchor.  Its normal-axis
      // component also cancels the rebase translation (the anchor has normal
      // coordinate zero), which is exactly what the shader depth path adds
      // back when it reconstructs a rebased clip position.
      orthoPlaneCenter = acgs::math::Vec3(static_cast<acgs::math::DVec3>(
          planeCenter - originWorld));

      // World axes are projected as exact NDC lines while their plane
      // mapping is still in CPU doubles.  For an arbitrary plane, a point
      // on the ortho image is planeCenter + groundRight*x + groundUp*y;
      // therefore the line coefficients are its tangent-space projections.
      normalizedAxisLineX = normalizedNdcLine(
          AcGeVector3d(groundRight.dotProduct(tangentUGe),
                       groundUp.dotProduct(tangentUGe),
                       planeCenter.asVector().dotProduct(tangentUGe)));
      normalizedAxisLineZ = normalizedNdcLine(
          AcGeVector3d(groundRight.dotProduct(tangentVGe),
                       groundUp.dotProduct(tangentVGe),
                       planeCenter.asVector().dotProduct(tangentVGe)));
      axisLineX = anchoredNdcLine(normalizedAxisLineX);
      axisLineZ = anchoredNdcLine(normalizedAxisLineZ);
      axisVisible = isSpecialGridPlane(gridPlane)
          ? acgs::math::Vec2(
                lineIntersectsNdcSquare(normalizedAxisLineX) ? 1.0f : 0.0f,
                lineIntersectsNdcSquare(normalizedAxisLineZ) ? 1.0f : 0.0f)
          : acgs::math::Vec2(0.0f);
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
    axisVisible = isSpecialGridPlane(gridPlane) ? acgs::math::Vec2(1.0f)
                                                : acgs::math::Vec2(0.0f);
  }

  // Rebase-relative grid anchor (was originWorld - cameraPos).
  // The shader receives the rebased offset and never sees the absolute
  // world coordinate; magnitude is bounded by chunkSize/2 instead of
  // step/2.
  planeOriginRelative = acgs::math::Vec3(static_cast<acgs::math::DVec3>(
      originWorld - AcGePoint3d(rebase)));
  // The shader's p is already relative to the snapped grid anchor, so this
  // value intentionally stays grid-relative.  Resolving it against the frame
  // rebase would shift the world axes by one rebase chunk.
  // A world axis is a single line, not a repeating pattern, so its anchored
  // constant must stay exact: reducing it modulo any period would repaint
  // the axis on the nearest period-multiple grid anchor instead of at the
  // true world origin.  Float32 is still safe here because the active LOD
  // keeps one pixel worth of plane space at or above ULP(|constant|).
  {
    const AcGeVector3d gridPlaneToAnchor = gridPlaneOrigin - originWorld;
    const double axisU = gridPlaneToAnchor.dotProduct(tangentUGe);
    const double axisV = gridPlaneToAnchor.dotProduct(tangentVGe);
    axisOriginGridRelative = acgs::math::Vec2(
        static_cast<float>(axisU),
        static_cast<float>(axisV));
  }

  planeTangentU = acgs::math::Vec3(static_cast<acgs::math::DVec3>(tangentUGe));
  planeTangentV = acgs::math::Vec3(static_cast<acgs::math::DVec3>(tangentVGe));
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
      AcGePoint3d startAxisWorld = gridPlaneStartAxisOrigin -
          planeNormal * (gridPlaneStartAxisOrigin - gridPlaneOrigin)
                            .dotProduct(planeNormal);
      startAxisOrigin = acgs::math::Vec3(static_cast<acgs::math::DVec3>(
          startAxisWorld - originWorld));
      AcGeVector3d startAxisDirection =
          gridPlaneStartAxisDirection -
          planeNormal * gridPlaneStartAxisDirection.dotProduct(planeNormal);
      if (startAxisDirection.length() < 1e-9)
          startAxisDirection = tangentUGe;
      startAxisDirection.normalize();
      // Project a short segment around the point where the start axis is
      // closest to the camera's gaze.  This avoids endpoint/w-plane problems
      // while still producing the true infinite line's NDC coefficients.
      const AcGePoint3d axisCenterOnPlane =
          startAxisWorld +
          startAxisDirection *
              (planeCenter - startAxisWorld).dotProduct(startAxisDirection);
      const double axisSegmentLength =
          0.1 * std::max(1.0, planeCenter.distanceTo(AcGePoint3d(cameraPos)));
      const AcGePoint3d axisPoint0 =
          axisCenterOnPlane - startAxisDirection * axisSegmentLength;
      const AcGePoint3d axisPoint1 =
          axisCenterOnPlane + startAxisDirection * axisSegmentLength;
      const acgs::math::Vec4 clip0 = overlayProjection * view *
          acgs::math::Vec4(acgs::math::Vec3(static_cast<acgs::math::DVec3>(
                        axisPoint0 - AcGePoint3d(rebase))),
                    1.0f);
      const acgs::math::Vec4 clip1 = overlayProjection * view *
          acgs::math::Vec4(acgs::math::Vec3(static_cast<acgs::math::DVec3>(
                        axisPoint1 - AcGePoint3d(rebase))),
                    1.0f);
      if (clip0.w > 0.0f && clip1.w > 0.0f)
      {
          const acgs::math::DVec2 ndc0(clip0.x / clip0.w, clip0.y / clip0.w);
          const acgs::math::DVec2 ndc1(clip1.x / clip1.w, clip1.y / clip1.w);
          if (acgs::math::length(ndc1 - ndc0) > 1e-12)
          {
              const AcGeVector3d normalizedLine = normalizedNdcLine(AcGeVector3d(
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

  const acgs::math::Mat4 objectViewProj = projection * view;
  const acgs::math::Mat4 overlayViewProj = overlayProjection * view;
  const acgs::math::Mat4 gridViewProj = gridProjection * view;

  bool gridPlaneVisible = false;
  if (useOrthoProjection())
  {
    gridPlaneVisible = orthoPlaneValid;
    if (gridPlaneVisible && std::abs(frontOnNormal) > kMinGridPlaneCos)
    {
      const double halfH = orbitCam.orthoSize();
      const double halfW = (double)halfH * (double)aspect;
      const AcGeVector3d right(orbitCam.Right);
      const AcGeVector3d up(orbitCam.Up);
      const double cameraPlaneDistance =
          (AcGePoint3d(cameraPos) - gridPlaneOrigin).dotProduct(planeNormal);
      const double groundCenterDepth =
          -cameraPlaneDistance / frontOnNormal;
      const double groundDepthRadius =
          (std::abs(right.dotProduct(planeNormal)) * halfW +
           std::abs(up.dotProduct(planeNormal)) * halfH) /
          std::abs(frontOnNormal);
      gridPlaneVisible =
          (groundCenterDepth + groundDepthRadius >= activeNear) &&
          (groundCenterDepth - groundDepthRadius <= activeFar);
    }
  }
  else
  {
    const AcGeVector3d front = AcGeVector3d(orbitCam.Front).normal();
    const double planeCos = std::abs(front.dotProduct(planeNormal));
    gridPlaneVisible = planeCos >= kMinGridPlaneCos;
    if (gridPlaneVisible)
    {
      const double denom = front.dotProduct(planeNormal);
      const double t =
          (gridPlaneOrigin - AcGePoint3d(cameraPos)).dotProduct(planeNormal) /
          denom;
      gridPlaneVisible = t > 0.0;
    }
  }

  if (frustumCaptureRequested)
  {
    frustumCaptureRequested = false;
    frustumWireframeVisible = true;
    const acgs::math::Mat4 invVP = acgs::math::inverse(overlayViewProj);
    for (int i = 0; i < 8; ++i)
    {
      const float ndcX = (i & 1) ? 1.0f : -1.0f;
      const float ndcY = (i & 2) ? 1.0f : -1.0f;
      const float ndcZ = (i & 4) ? 1.0f : -1.0f;
      acgs::math::Vec4 p = invVP * acgs::math::Vec4(ndcX, ndcY, ndcZ, 1.0f);
      if (std::abs(p.w) > 1e-10f)
        p /= p.w;
      frustumCorners[i] =
          AcGePoint3d(p.x, p.y, p.z) +
          AcGeVector3d(rebase.x, rebase.y, rebase.z);
    }

    // The grid plane's visible extent inside the frustum.  This is the
    // plane clipped by all four side planes *and* by near/far.  Without
    // the depth clip the debug polygon would show regions whose grid
    // fragments are discarded outside the active depth slab.
    std::array<acgs::math::DVec3, 16> gridPolygon{};
    int gridPolygonCount = 0;
    if (useOrthoProjection() && orthoPlaneValid)
    {
      const AcGeVector3d center(orthoPlaneCenter);
      const AcGeVector3d right(orthoRight);
      const AcGeVector3d up(orthoUp);
      gridPolygon[0] = originWorld + (center - right - up);
      gridPolygon[1] = originWorld + (center + right - up);
      gridPolygon[2] = originWorld + (center + right + up);
      gridPolygon[3] = originWorld + (center - right + up);
      gridPolygonCount = 4;
      gridVisibleQuadValid = true;
    }
    else if (!useOrthoProjection())
    {
      const AcGePoint3d camPos(orbitCam.Position);
      const AcGeVector3d n = planeNormal;
      const double planeDist =
          (gridPlaneOrigin - camPos).dotProduct(n);
      gridVisibleQuadValid = true;
      for (int i = 0; i < 4; ++i)
      {
        const AcGeVector3d dir =
            (AcGePoint3d(frustumCorners[4 + i]) - camPos).normal();
        const double denom = dir.dotProduct(n);
        if (std::abs(denom) < 1e-10 || planeDist / denom < 0.0)
        {
          gridVisibleQuadValid = false;
          break;
        }
        gridPolygon[i] = acgs::math::DVec3(
            camPos + dir * (planeDist / denom));
      }
      gridPolygonCount = gridVisibleQuadValid ? 4 : 0;
    }
    else
    {
      gridVisibleQuadValid = false;
    }

    if (gridVisibleQuadValid)
    {
      std::array<acgs::math::DVec3, 16> clippedGridPolygon{};
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
        .invViewProj = acgs::math::inverse(gridViewProj),
      .viewProj = gridViewProj,
      .camFront = AcGeVector3d(orbitCam.Front),
      .orthoPlaneCenter = AcGePoint3d(acgs::math::DVec3(orthoPlaneCenter)),
      .orthoRight = AcGeVector3d(acgs::math::DVec3(orthoRight)),
      .orthoUp = AcGeVector3d(acgs::math::DVec3(orthoUp)),
      .planeOriginRelative = AcGePoint3d(acgs::math::DVec3(planeOriginRelative)),
      .plane = (float)(gridPlane == GridPlaneType::XZ ? 0
                      : gridPlane == GridPlaneType::XY ? 1
                       : gridPlane == GridPlaneType::YZ ? 2
                                                        : 3),
      .planeNormal = AcGeVector3d(planeNormal),
      .planeTangentU = AcGeVector3d(acgs::math::DVec3(planeTangentU)),
      .planeTangentV = AcGeVector3d(acgs::math::DVec3(planeTangentV)),
      .axisColorU = axisColorU,
      .axisColorV = axisColorV,
      .startAxisOrigin = AcGePoint3d(acgs::math::DVec3(startAxisOrigin)),
      .startAxisDirection = AcGeVector3d(acgs::math::DVec3(startAxisDirection)),
      .startAxisVisible = startAxisVisible,
      .startAxisLine = AcGeVector3d(acgs::math::DVec3(startAxisLine)),
      .axisOriginGridRelative =
          AcGePoint2d(axisOriginGridRelative.x, axisOriginGridRelative.y),
      .axisLineX = AcGeVector3d(acgs::math::DVec3(axisLineX)),
      .axisLineZ = AcGeVector3d(acgs::math::DVec3(axisLineZ)),
      .orthoPlaneValid = orthoPlaneValid ? 1.0f : 0.0f,
      .groundRelativeY = 0.0f, // unused in current shader; was (float)(-rebase.y) which lost precision
      .isOrtho = useOrthoProjection() ? 1.0f : 0.0f,
      .step = step,
      .axisVisible = axisVisible,
      .screenHeight = static_cast<float>(drawableHeight),
      .screenWidth = static_cast<float>(drawableWidth),
      .gridColorMajor = acgs::math::Vec3(0.5f, 0.5f, 0.5f),
      .gridColorMinor = acgs::math::Vec3(0.3f, 0.3f, 0.3f),
      .gridOpacity = 0.6f,
      .logDepth = logDepth,
  };
  static acgs::AcGsModel sceneOverlay;
  sceneOverlay.clear();
  if (gridPlaneVisible)
    sceneOverlay.setGrid(gridRenderData);

  // Opaque geometry first so transparent passes can depth-test against it.
  const MeshEntityRecord centerCube = getCenterCubeEntity();
  if (centerCubeInFrame)
  {
    // In-scene selection recolor: the selected center cube draws with the
    // highlight color in its own pass.
    const bool cubeSelected = outlineEntity.has_value() &&
                              outlineEntity->kind == VisibilityKind::CenterCube;
    appendMeshEntityToScene(centerCube, sceneOverlay, false,
                            cubeSelected ? &kSelectionHighlightRGB : nullptr);
    queueGpuMeshEntity(centerCube, kGpuPickCenterCubeId);
  }

  // Push the complete per-frame viewport context for every acgi
  // submission this frame (submit, text flush, outline primitives).
  {
    acgs::ViewFrameContext ctx;
    ctx.view = viewRte;
    ctx.projection = projection;
    ctx.overlayProjection = overlayProjection;
    ctx.cameraPos = cameraPos;
    ctx.cameraRight = cameraRight;
    ctx.cameraUp = cameraUp;
    ctx.cameraFront = frontVec;
    ctx.logDepth = logDepth;
    ctx.slabNear = overlayNear;
    ctx.slabFar = overlayFar;
    ctx.viewportWidth = currentDrawableWidth();
    ctx.viewportHeight = currentDrawableHeight();
    ctx.ortho = useOrthoProjection();
    ctx.orthoSize = orbitCam.orthoSize();
    ctx.orbitDistance =
        acgs::math::length(orbitCam.Position - orbitCam.Target);
    ctx.pixelSizeWorld = pixelSize;
    ctx.meshTextureIndex = gMeshTextureIndex;
    ctx.meshHeadlight = meshHeadlight();
    ctx.meshTriplanar = meshTriplanar();
    acgsView().setFrameContext(ctx);
  }

  acgsView().submit(sceneOverlay, {nullptr, pixelSize});

  // Translucent meshes remain sorted far-to-near. They depth-test against
  // opaque geometry but must not overwrite the shared depth buffer.
  drawLargeCoordinateObjects(viewRte, projection, orbitCam.Position, drawOrder,
                             logDepth, pixelSize, 0.15f);

  drawVectorPrimitivesDemo(viewRte, projection, overlayProjection,
                           orbitCam.Position, logDepth,
                           cameraPos, frontVec, cameraRight, cameraUp,
                           pixelSize, visibleCadDraws, tinyCadDraws);

  // SDF text pass: the AcGi text engine expands each queued request into
  // per-glyph quads (one R8 distance-field texture per glyph, uploaded on
  // first use).  Sources: Text/MText entities and the standalone demo
  // string, both registered in the request queue (which also feeds the
  // content bounds so the depth slab covers the glyphs).
  // The AcGi view flushes queued text requests into SDF glyph quads,
  // including the ortho viewport cull.
  // Text pick pass: each queued text request joins the GPU ID pass as
  // PER-GLYPH quads from the same layout walk the visible SDF pass uses
  // (layoutGlyphs), so the ID texture matches the visible glyphs pixel
  // for pixel.  Camera-relative positions with the rotation-only view,
  // matching the ID-pass eye convention; double-sided windings so text
  // picks from both facings.
  static const bool textPickEnabled = [] {
    const char *value = std::getenv("GRID_TEXT_PICK");
    return value == nullptr || std::strcmp(value, "0") != 0;
  }();
  if (textPickEnabled && gpuPickEnabled() &&
      (gpuPickFocus.waitingResult || gpuPickSceneDebugQueueActive))
  {
    for (const acgi::TextRequest &request : acgi::textRequests())
    {
      // Shared layout cache with the visible SDF pass (planar layouts are
      // camera-independent; billboard stays per-call inside the engine).
      const std::vector<acgi::TextEngine::GlyphPlacement> &placements =
          acgi::textEngine().glyphLayout(request, cameraPos, cameraRight,
                                         cameraUp, frontVec);
      if (placements.empty())
        continue;
      const uint32_t objectId = registerGpuPickEntity(
          {VisibilityKind::CadText, nullptr, nullptr, nullptr, &request});
      const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
      static std::vector<rendering::FillVertex> textPickVertices;
      textPickVertices.clear();
      acgs::math::DVec3 layoutCenter(0.0);
      // GRID_TEXT_PICK_QUADS=1: regression switch forcing the full-quad
      // pick shape (pre glyph-coverage behavior).
      static const bool textPickQuads = [] {
        const char *value = std::getenv("GRID_TEXT_PICK_QUADS");
        return value != nullptr && *value != '\0' &&
               std::strcmp(value, "0") != 0;
      }();
      for (const acgi::TextEngine::GlyphPlacement &placement : placements)
      {
        for (const acgs::math::DVec3 &worldCorner : placement.corners)
          layoutCenter += worldCorner;
        // Glyph-shaped coverage: the SDF iso-contour soup instead of the
        // full quad, so the ID texture and the selection highlight follow
        // the visible glyph rather than its bounding box.
        const std::vector<acgi::TextEngine::GlyphCoverageUv> *coverage =
            textPickQuads
                ? nullptr
                : &acgi::textEngine().glyphCoverageUv(placement.codepoint);
        if (coverage == nullptr || coverage->empty())
        {
          // Raster unavailable: keep the quad as the conservative shape.
          for (const int index : {0, 1, 2, 0, 2, 3})
            textPickVertices.push_back(
                {acgs::math::Vec3(placement.corners[index] - cameraPos), idColor});
          continue;
        }
        for (size_t tri = 0; tri + 2 < coverage->size(); tri += 3)
        {
          for (int t = 0; t < 3; ++t)
          {
            const acgi::TextEngine::GlyphCoverageUv &uv =
                (*coverage)[tri + t];
            // The visible pass samples texture v=0 at the glyph's TOP row
            // while quad cornerV=1 is the top (see AcGiTextEngine::drawText:
            // cornerV carries uv v = 1 - cornerV). Invert v here so the
            // coverage lands on the glyph instead of its mirror.
            const acgs::math::DVec3 world =
                placement.corners[0] +
                (placement.corners[1] - placement.corners[0]) *
                    double(uv.u) +
                (placement.corners[3] - placement.corners[0]) *
                    double(1.0 - uv.v);
            textPickVertices.push_back(
                {acgs::math::Vec3(world - cameraPos), idColor});
          }
        }
      }
      layoutCenter /= double(placements.size() * 4);
      if (pickDebugEnabled())
      {
        std::printf(
            "[TEXT_PICK] placements=%zu coverage_tris=%zu verts=%zu id=%u",
            placements.size(), textPickVertices.size() / 3,
            textPickVertices.size(), objectId);
        std::puts("");
        static bool soupDumped = false;
        if (!soupDumped && std::getenv("GRID_DUMP_ID"))
        {
          soupDumped = true;
          FILE *obj = std::fopen("text_soup.obj", "wb");
          if (obj)
          {
            for (const rendering::FillVertex &v : textPickVertices)
              std::fprintf(obj, "v %f %f %f\n", v.position.x, v.position.y,
                           v.position.z);
            for (size_t t = 0; t + 2 < textPickVertices.size(); t += 3)
              std::fprintf(obj, "f %zu %zu %zu\n", t + 1, t + 2, t + 3);
            std::fclose(obj);
            std::printf("[TEXT_PICK] wrote text_soup.obj");
            std::puts("");
          }
        }
      }
      // Chunked queueing: dense CJK glyph coverage can exceed the 65535
      // per-primitive vertex cap, which silently drops the whole text ID.
      // The text quads carry their own slab around the text's depth: the
      // content slab tightens around meshes and would discard the glyphs
      // (the visible SDF pass never slab-discards).
      constexpr size_t kMaxTextPickChunkVertices = 3 * 21000;
      const double textPickDepth =
          acgs::math::dot(acgs::math::DVec3(layoutCenter - cameraPos), frontVec);
      const acgs::math::Vec4 textSlab(
          1.0f, float(std::max(1.0, textPickDepth * 0.5)),
          float(std::max(2.0, textPickDepth * 2.0)), 0.0f);
      for (size_t first = 0; first < textPickVertices.size();
           first += kMaxTextPickChunkVertices)
      {
        const size_t count =
            std::min(kMaxTextPickChunkVertices,
                     textPickVertices.size() - first);
        acgs::acgsGetManager()->queueGpuTrianglePick(
            0, textPickVertices.data() + first, uint32_t(count),
            viewRte, projection, logDepth, objectId, 3, textSlab);
      }
      if (pickDebugEnabled())
      {
        const acgs::math::Vec4 clip = projection * viewRte *
                               acgs::math::Vec4(acgs::math::Vec3(layoutCenter - cameraPos),
                                         1.0f);
        const acgs::math::DVec2 ndc(double(clip.x) / clip.w,
                             double(clip.y) / clip.w);
        int winW = 0, winH = 0;
        SDL_GetWindowSize(appPlatform.window(), &winW, &winH);
        acgs::math::DVec2 ndcMin(1e9), ndcMax(-1e9);
        double wMin = 1e9, wMax = -1e9;
        for (const rendering::FillVertex &v : textPickVertices)
        {
          const acgs::math::Vec4 c = projection * viewRte *
                              acgs::math::Vec4(v.position, 1.0f);
          if (c.w <= 1.0e-6)
            continue;
          const acgs::math::DVec2 n(double(c.x) / c.w, double(c.y) / c.w);
          ndcMin = acgs::math::min(ndcMin, n);
          ndcMax = acgs::math::max(ndcMax, n);
          wMin = std::min(wMin, double(c.w));
          wMax = std::max(wMax, double(c.w));
        }
        std::cout << "[TEXT_PICK] id=" << objectId
                  << " depth=" << clip.w
                  << " px=("
                  << int((ndc.x * 0.5 + 0.5) * winW) << ","
                  << int((0.5 - ndc.y * 0.5) * winH) << ")"
                  << " soupNDC=[" << ndcMin.x << "," << ndcMin.y << "]-["
                  << ndcMax.x << "," << ndcMax.y << "] w=[" << wMin << ","
                  << wMax << "]"
                  << " msg='" << request.message.substr(0, 16)
                  << "'" << std::endl;
      }
    }
  }

  acgsView().flushTextRequests();

  // Below the mesh LOD threshold, emit stable center-point impostors.  The
  // renderer projects and batches all points into one GPU submission per
  // transient-buffer chunk.
  sceneOverlay.clear();
  for (const LargeCoordinateObject *object : tinyDraws)
  {
    // In-scene selection recolor: the selected tiny mesh's impostor draws
    // with the highlight color in its own pass.
    const bool meshSelected =
        outlineEntity.has_value() &&
        outlineEntity->kind == VisibilityKind::MeshObject &&
        outlineEntity->mesh == object;
    appendScenePoint(sceneOverlay, object->worldPosition,
                     meshSelected ? acgs::math::Vec3(kSelectionHighlightRGB)
                                  : acgs::math::Vec3(meshEntityColor(*object)),
                     2.0);
  }

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
    const acgs::math::Vec3 pickRight(orbitCam.Right);
    const acgs::math::Vec3 pickUp(orbitCam.Up);
    const double referenceDistance =
        acgs::math::length(orbitCam.Position - orbitCam.Target);
    for (const LargeCoordinateObject *object : tinyDraws)
    {
      if (!object || !meshEntityVisible(*object))
        continue;

      const acgs::math::DVec3 center = object->worldPosition;
      const double depth = acgs::math::dot(center - orbitCam.Position, orbitCam.Front);
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
      const acgs::math::Vec4 idColor = acgs::encodeGpuPickId(objectId);
      // The visible impostor point draws with a half-pixel camera bias;
      // bias the ID quad identically or its depth z-fights with whatever
      // the tiny mesh sits on and the point's ID loses.
      const float pickBias = std::max(0.5f * float(objectPixelSize), 0.01f);
      const acgs::math::Vec3 relative =
          acgs::math::Vec3(center - orbitCam.Position) -
          acgs::math::Vec3(orbitCam.Front) * pickBias;
      const acgs::math::Vec3 right = pickRight * radius;
      const acgs::math::Vec3 up = pickUp * radius;
      tinyMeshPickVertices.clear();
      tinyMeshPickVertices.push_back({relative - right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right + up, idColor});
      tinyMeshPickVertices.push_back({relative - right - up, idColor});
      tinyMeshPickVertices.push_back({relative + right + up, idColor});
      tinyMeshPickVertices.push_back({relative - right + up, idColor});
      acgs::acgsGetManager()->queueGpuTrianglePick(
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
                   acgs::math::Vec3(1.0f, 0.15f, 0.15f), 5.0);

  if (frustumWireframeVisible)
  {
    constexpr int nearEdges[4][2] = {{0,1},{1,2},{2,3},{3,0}};
    constexpr int farEdges[4][2] = {{4,5},{5,6},{6,7},{7,4}};
    constexpr int sideEdges[4][2] = {{0,4},{1,5},{2,6},{3,7}};
    const acgs::math::Vec3 nearColor(1.0f, 0.2f, 0.2f);
    const acgs::math::Vec3 farColor(0.2f, 0.4f, 1.0f);
    const acgs::math::Vec3 sideColor(1.0f, 1.0f, 1.0f);
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
      const acgs::math::Vec3 gridQuadColor(1.0f, 0.85f, 0.1f);
      for (int i = 0; i < gridVisibleQuadCount; ++i)
        appendSceneLine(sceneOverlay, gridVisibleQuad[i],
                        gridVisibleQuad[(i + 1) % gridVisibleQuadCount],
                        gridQuadColor, 0.9f);
    }
  }

  acgsView().submit(sceneOverlay, {nullptr, pixelSize});

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
        acgs::acgsGetManager()->requestDebugScreenshot(screenshot);
        screenshotRequested = true;
        std::cout << "[LINE_DEBUG] requested screenshot: " << screenshot
                  << std::endl;
      }
    }
  }

  logCameraStateIfChanged(orbitCam.Target, activeNear, activeFar,
                          useOrthoProjection());

  if (acgs::acgsGetManager()->deviceReady())
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
    acgs::acgsGetManager()->setSelectionOutlineId(
        outlineEntity ? outlineId : 0);
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
  if (acgs::acgsGetManager()->deviceReady())
  {
    const uint32_t nextId = gpuPickManager().nextIdCounter();
    const uint32_t maxId = nextId > 2 ? nextId - 1
        : 2;
    acgs::acgsGetManager()->setGpuPickIdRange(2, maxId);
  }

  // ---- picture-in-picture top viewport (GRID_PIP=1) ----
  // The second AcGsView ("Top", created here on first use with its own
  // AcDbViewportTableRecord) renders the scene contents into a
  // quarter-size offscreen framebuffer on claimed frames only
  // (time-share guard), and the last image composites as an inset over
  // the presented frame.  v1 scope: candidates are submitted unculled
  // with a fixed generous ortho slab; text and selection highlights stay
  // main-view-only, and the pass is skipped while a GPU pick request is
  // in flight so the re-submission cannot pollute the id queue.  The
  // per-camera visibility-filter factorization upgrades this to full
  // slab/culling fidelity later.
  static const bool pipEnabled = []() {
    const char *value = std::getenv("GRID_PIP");
    return value != nullptr && *value != '\0' &&
           std::strcmp(value, "0") != 0;
  }();
  if (pipEnabled && acgs::acgsGetManager()->deviceReady() &&
      !gpuPickFocusWaiting() && !gpuPickSceneDebugQueueActive)
  {
    acgs::AcGsManager *pipManager = acgs::acgsGetManager();
    if (pipManager->viewCount() < 2)
    {
      acgs::AcGsView *top = pipManager->createView();
      top->setViewportRecordName("Top");
      acdb::AcDbDatabase &document = acdbDocument();
      acdb::AcDbViewportTableRecord &record =
          document.viewportTable().contains("Top")
              ? *document.viewportTable().getMutable("Top")
              : document.viewportTable().add(
                    "Top", document.allocateHandle());
      top->orbitCamera().setOrbit(acgsView().orbitCamera().Target, 2000.0);
      top->orbitCamera().setViewDirection(acgs::math::DVec3(0.0, 0.0, -1.0));
      top->orthoMode() = true;
      top->writeToViewportRecord(record);
    }
    static std::uint64_t pipFrame = 0;
    ++pipFrame;
    if ((pipFrame % 2) == 1 && pipManager->tryClaimFrameRender())
    {
      acgs::AcGsView *pipView = pipManager->views()[1].get();
      const int pipWidth = std::max(1, currentDrawableWidth() / 4);
      const int pipHeight = std::max(1, currentDrawableHeight() / 4);
      const double pipHalfH = pipView->orbitCamera().orthoSize();
      const acgs::math::Mat4 pipProjection = acgs::math::Mat4(acgs::math::ortho(
          -pipHalfH * (double)aspect, pipHalfH * (double)aspect,
          -pipHalfH, pipHalfH, 1.0, 4.0e6));
      acgs::ViewFrameContext pipContext;
      pipContext.view = pipView->orbitCamera().getViewMatrix(rebase);
      pipContext.projection = pipProjection;
      pipContext.overlayProjection = pipProjection;
      pipContext.cameraPos = pipView->orbitCamera().Position;
      pipContext.cameraRight = pipView->orbitCamera().Right;
      pipContext.cameraUp = pipView->orbitCamera().Up;
      pipContext.cameraFront = pipView->orbitCamera().Front;
      pipContext.viewportWidth = pipWidth;
      pipContext.viewportHeight = pipHeight;
      pipContext.ortho = true;
      pipContext.orthoSize = pipHalfH;
      pipContext.orbitDistance = acgs::math::length(
          pipView->orbitCamera().Position - pipView->orbitCamera().Target);
      pipContext.pixelSizeWorld =
          float((2.0 * pipHalfH) / double(pipHeight));
      pipView->setFrameContext(pipContext);
      const int savedViewport = pipManager->activeViewIndex();
      pipManager->setActiveView(1);
      if (pipManager->beginPipScene())
      {
        static std::vector<const VisibilityCandidate *> pipCadDraws;
        pipCadDraws.clear();
        for (const VisibilityCandidate &candidate : visibilityCandidates)
          if (candidate.kind != VisibilityKind::MeshObject)
            pipCadDraws.push_back(&candidate);
        drawVectorPrimitivesDemo(
            pipContext.view, pipProjection, pipProjection, rebase,
            acgs::math::Vec4(0.0f), pipContext.cameraPos, pipContext.cameraFront,
            pipContext.cameraRight, pipContext.cameraUp,
            pipContext.pixelSizeWorld, pipCadDraws, {});
        drawLargeCoordinateObjects(pipContext.view, pipProjection, rebase,
                                   drawOrder, acgs::math::Vec4(0.0f),
                                   pipContext.pixelSizeWorld, 0.15f);
        acgsView().submit(sceneOverlay, {nullptr, pipContext.pixelSizeWorld});
        pipManager->endPipScene();
      }
      pipManager->setActiveView(savedViewport);
      pipManager->resetFrameRender();
    }
    pipManager->compositePip();
  }

    if (appPlatform.imguiEnabled())
    {
      // Refresh the active panel's final every frame.  The blit view (22)
      // executes after the UI view (20), so the panels sample the previous
      // bgfx frame's copy -- one wall-frame behind, refreshed every frame.
      //fprintf(stderr, "[P] blit enter\n");
      acgs::acgsGetManager()->blitSceneToSlot(g_activeSceneSlot);
      //fprintf(stderr, "[P] blit ok\n");
      drawViewCubeOverlay();
      drawUcsIconOverlay();
      ImGui::Render();
      {
        ImDrawData *dd = ImGui::GetDrawData();
        //fprintf(stderr, "[UI] drawData=%p totalIdx=%u display=(%.0f,%.0f)\n",
        //        (const void *)dd,
        //        dd ? (unsigned)dd->TotalIdxCount : 0u,
        //        dd ? dd->DisplaySize.x : -1.f,
        //        dd ? dd->DisplaySize.y : -1.f);
        // Above kViewPresent (17) so the panels composite over the
        // resolved scene; below the final blit (22).
        imguiBgfxRenderDrawData(dd, 20);
      }
    }

    acgs::acgsGetManager()->endFrame();
    // Display stage: the offscreen product becomes the window image
    // (ImGui panels in ImGui mode, backbuffer resolve otherwise).
    acgs::acgsGetManager()->compositeFrame();
}

// FPS badge pinned over the viewport image: the device counts presented
// frames (endFrame cadence, 500 ms smoothing) and this overlay draws the
// value in the UI layer so it sits above the AcGsView output.  NoInputs
// keeps viewport hover/pick routing untouched beneath it.
static void drawFpsOverlay()
{
  if (!appPlatform.imguiEnabled())
    return;
  const float fps = acgs::acgsGetManager()->fps();
  // ViewCube-overlay flag set plus NoInputs: the badge must never take
  // focus or eat viewport hover/pick routing beneath it.  (Adding
  // AlwaysAutoResize or NoBringToFrontOnFocus here renders the window
  // nearly invisible over the panel image -- root cause not chased; the
  // window auto-fits content anyway since NoInputs blocks manual resize.)
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.45f);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
  if (ImGui::Begin("FpsOverlay", nullptr, flags))
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "FPS %.1f", fps);
  ImGui::End();
}

// acgs ImGui panels: fullscreen-anchored windows presenting each
// viewport's persistent final texture, with per-panel input routing
// through the AcGsView interaction facade (wheel zoom at cursor,
// middle-drag pan/orbit, left-click pick through the unified pipeline).
static void drawScenePanels()
{
  if (!appPlatform.imguiEnabled())
    return;
  acgs::AcGsManager *manager = acgs::acgsGetManager();
  const int panelCount =
      multiViewEnabled() && manager->viewCount() > 1 ? 2 : 1;
  ImGuiIO &io = ImGui::GetIO();
  const float panelWidth =
      io.DisplaySize.x / float(panelCount) - (panelCount > 1 ? 6.0f : 0.0f);
  // Panels cover the full window: any uncovered swapchain row would show
  // whatever the backbuffer last held there.
  const ImVec2 panelSize(panelWidth, io.DisplaySize.y);
  g_imguiHoveredPanel = -1;
  for (int slot = 0; slot < panelCount; ++slot)
  {
    const char *title = slot == 0 ? "Model (viewport 1)"
                                  : "Top (viewport 2)";
    ImGui::SetNextWindowPos(
        ImVec2(float(slot) *
                   (panelWidth + (panelCount > 1 ? 6.0f : 0.0f)),
               0.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ImGui::Begin(title, nullptr, flags))
    {
      ImGui::End();
      continue;
    }
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const std::uint32_t textureId = manager->sceneTexture(slot);
    if (textureId != 0)
      ImGui::Image(reinterpret_cast<ImTextureID>(
                       static_cast<uintptr_t>(textureId)),
                   avail);
    else
      ImGui::TextUnformatted("scene unavailable");

    // Per-panel input routing (the panel owns its viewport's camera).
    acgs::AcGsView &panelView = *manager->views()[std::size_t(slot)];
    if (outlineEntity && outlineEntity->kind == VisibilityKind::CadStroke &&
        outlineEntity->cadRange)
    {
      const auto &strokes = getVectorPrimitivesTessellation().geometry.strokes;
      const acgs::AcGsEntityRange &range = *outlineEntity->cadRange;
      const std::size_t first = std::min(range.begin, strokes.size());
      const std::size_t last =
          std::min(range.begin + range.count, strokes.size());
      std::vector<cadui::GripPoint> grips;
      for (std::size_t strokeIndex = first; strokeIndex < last; ++strokeIndex)
      {
        const acdb::Stroke &stroke = strokes[strokeIndex];
        if (!stroke.common.visible || stroke.points.empty() || stroke.closed)
          continue;
        const std::size_t pointCount =
            stroke.semiInfinite ? 1 : stroke.points.size();
        for (std::size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
        {
          grips.push_back({grips.size(), stroke.points[pointIndex],
                           cadui::GripShape::Square, 0, false, std::nullopt});
          if (!stroke.semiInfinite && pointIndex + 1 < stroke.points.size())
          {
            grips.push_back({
                grips.size(),
                0.5 * (stroke.points[pointIndex] + stroke.points[pointIndex + 1]),
                cadui::GripShape::Triangle, 0, true, std::nullopt});
          }
        }
      }
      const acgs::ViewFrameContext &frame = panelView.frame();
      const cadui::GripProjector project =
          [&](const acgs::math::DVec3 &world) -> std::optional<acgs::math::Vec2> {
        const acgs::math::Vec4 clip =
            frame.overlayProjection * frame.view *
            acgs::math::Vec4(acgs::math::Vec3(world - frame.cameraPos), 1.0f);
        if (!(clip.w > 0.0f))
          return std::nullopt;
        const acgs::math::Vec2 ndc = acgs::math::Vec2(clip) / clip.w;
        return acgs::math::Vec2((ndc.x * 0.5f + 0.5f) * avail.x,
                         (0.5f - ndc.y * 0.5f) * avail.y);
      };
      const std::vector<cadui::GripMarker> markers =
          cadui::projectGripMarkers(grips, project);
      cadui::drawGripOverlay(ImGui::GetWindowDrawList(), cursor, avail, markers);
    }
    if (!ImGui::IsWindowHovered())
    {
      ImGui::End();
      continue;
    }
    g_imguiHoveredPanel = slot;
    const double aspect = double(avail.x) / std::max(1.0, double(avail.y));
    const double ndcX =
        (double(io.MousePos.x - cursor.x) /
         std::max(1.0, double(avail.x))) *
            2.0 -
        1.0;
    const double ndcY = 1.0 - (double(io.MousePos.y - cursor.y) /
                               std::max(1.0, double(avail.y))) * 2.0;
    if (io.MouseWheel != 0.0f)
      panelView.zoomAtCursor(io.MouseWheel * 0.5f, ndcX, ndcY, aspect);
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
      if (io.KeyShift)
        panelView.orbit(io.MouseDelta.x, -io.MouseDelta.y);
      else
        panelView.pan(io.MouseDelta.x, io.MouseDelta.y, avail.y);
    }
    // Left click: pick through THIS panel's frustum via the unified
    // pixel read (pin the round-robin to this slot while it resolves).
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
      manager->setActiveView(slot);
      g_pickedSlot = slot;
      gpuPickManager().requestPick(ndcX, ndcY);
    }
    ImGui::End();
  }
}

int main(int argc, char *argv[])
{
  SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "1");

  // ObjectARX host sequence: initialize the working database before any
  // application code touches drawing data (createDefaults parity — see
  // initWorkingDatabase).
  initWorkingDatabase();

  // Hidden kernel self-test: GRID_SELFTEST=kernel runs the geom2d
  // + brep unit tests, writes kernel_selftest.log, exits nonzero
  // on failure.
  if (const char *selfTest = std::getenv("GRID_SELFTEST");
      selfTest && std::strcmp(selfTest, "kernel") == 0)
  {
    const int kernelFails = brep::runKernelSelfTest();
    return kernelFails == 0 ? 0 : 1;
  }
  if (const char *selfTest = std::getenv("GRID_SELFTEST");
      selfTest && std::strcmp(selfTest, "store") == 0)
  {
    const int storeFails = acdb::runStoreSelfTest();
    return storeFails == 0 ? 0 : 1;
  }
  if (const char *selfTest = std::getenv("GRID_SELFTEST");
      selfTest && std::strcmp(selfTest, "fileio") == 0)
  {
    const int fileIoFails = acdb::runFileIoSelfTest();
    return fileIoFails == 0 ? 0 : 1;
  }


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
  fitCameraToImportedEntities(g_dwgImportedHandles);

  // Debug helper: jump the startup camera straight to a world point.
  // GRID_CAMERA_START_TARGET=x,y,z[,distance[,gazeX,gazeY,gazeZ]]
  if (const char *startTarget = std::getenv("GRID_CAMERA_START_TARGET"))
  {
    double tx = 0.0, ty = 0.0, tz = 0.0, dist = 1500.0;
    double gx = 0.0, gy = 0.0, gz = 0.0;
    const int fields = std::sscanf(startTarget, "%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                                   &tx, &ty, &tz, &dist, &gx, &gy, &gz);
    if (fields >= 3)
    {
      orbitCam.setOrbit(acgs::math::DVec3(tx, ty, tz), std::max(1.0, dist));
      if (fields >= 7)
        orbitCam.setViewDirection(acgs::math::DVec3(gx, gy, gz));
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
    const double tanHalfFov = std::tan(acgs::math::radians(orbitCam.Zoom) * 0.5);
    orbitCam.setTargetDistance(requestedOrthoHalfHeight / tanHalfFov);
  }
  acgsView().resetDepthSlabs();
  SDL_SetWindowTitle(
      appPlatform.window(), "grid plane - large-coordinate stress field");

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
  bool testSecondClickApplied = false;


  const Uint64 frameTimerFrequency = SDL_GetPerformanceFrequency();
  Uint64 frameTimerStart = SDL_GetPerformanceCounter();

  while (running)
  {
    float currentFrame = SDL_GetTicks() / 1000.0f;

    // Automated validation: inject the same SDL event path as a real
    // left-button pick click (single click since picking moved from
    // double-click to single-click).  The env names keep their historic
    // DOUBLE_CLICK spelling for script compatibility.
    if (!testDoubleClickApplied)
    {
      const char *testAtValue = std::getenv(
          "GRID_CAMERA_TEST_DOUBLE_CLICK_AT_SECONDS");
      if (testAtValue && currentFrame >= std::atof(testAtValue))
      {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(appPlatform.window(), &winW, &winH);
        const char *testX = std::getenv("GRID_CAMERA_TEST_DOUBLE_CLICK_X");
        const char *testY = std::getenv("GRID_CAMERA_TEST_DOUBLE_CLICK_Y");
        const int x = testX ? std::atoi(testX) : winW / 2;
        const int y = testY ? std::atoi(testY) : winH / 2;
        SDL_Event synthetic{};
        synthetic.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        synthetic.button.windowID = SDL_GetWindowID(appPlatform.window());
        synthetic.button.timestamp = SDL_GetTicks();
        synthetic.button.clicks = 1;
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
        std::printf("[PICK_DEBUG] test click x=%d y=%d", x, y);
        std::puts("");
      }
    }

    // Optional second synthetic click (two-step autofocus + fill pick).
    if (!testSecondClickApplied)
    {
      const char *secondAt = std::getenv(
          "GRID_CAMERA_TEST_SECOND_CLICK_AT_SECONDS");
      if (secondAt && currentFrame >= std::atof(secondAt))
      {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(appPlatform.window(), &winW, &winH);
        const char *testX = std::getenv("GRID_CAMERA_TEST_SECOND_CLICK_X");
        const char *testY = std::getenv("GRID_CAMERA_TEST_SECOND_CLICK_Y");
        const int x = testX ? std::atoi(testX) : winW / 2;
        const int y = testY ? std::atoi(testY) : winH / 2;
        SDL_Event synthetic{};
        synthetic.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        synthetic.button.windowID = SDL_GetWindowID(appPlatform.window());
        synthetic.button.timestamp = SDL_GetTicks();
        synthetic.button.clicks = 1;
        synthetic.button.button = SDL_BUTTON_LEFT;
        synthetic.button.down = true;
        synthetic.button.x = x;
        synthetic.button.y = y;
        testSecondClickApplied = true;
        SDL_PushEvent(&synthetic);
        std::printf("[PICK_DEBUG] test second click x=%d y=%d", x, y);
        std::puts("");
      }
    }

    while (SDL_PollEvent(&evt))
    {
        appPlatform.processEvent(evt);
        bool mouseCaptured = appPlatform.imguiEnabled() &&
                             ImGui::GetIO().WantCaptureMouse;
        if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            evt.type == SDL_EVENT_MOUSE_BUTTON_UP)
          mouseCaptured = mouseCaptured ||
                          isViewCubeScreenPoint(evt.button.x, evt.button.y) ||
                          isUcsIconScreenPoint(evt.button.x, evt.button.y);
        else if (evt.type == SDL_EVENT_MOUSE_MOTION)
          mouseCaptured = mouseCaptured ||
                          isViewCubeScreenPoint(evt.motion.x, evt.motion.y) ||
                          isUcsIconScreenPoint(evt.motion.x, evt.motion.y);
        else if (evt.type == SDL_EVENT_MOUSE_WHEEL)
        {
          float mouseX = 0.0f;
          float mouseY = 0.0f;
          SDL_GetMouseState(&mouseX, &mouseY);
          mouseCaptured = mouseCaptured ||
                          isViewCubeScreenPoint(mouseX, mouseY) ||
                          isUcsIconScreenPoint(mouseX, mouseY);
        }

        if (evt.type == SDL_EVENT_QUIT)
        {
          running = false;
        }
        if (evt.type == SDL_EVENT_KEY_DOWN &&
            (!appPlatform.imguiEnabled() ||
             !ImGui::GetIO().WantCaptureKeyboard))
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
          if (acgs::acgsGetManager()->deviceReady())
            acgs::acgsGetManager()->setGpuPickDebugVisible(gpuPickDebugVisible);
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
          if ((evt.key.mod & SDL_KMOD_SHIFT) != 0)
          {
            frustumCaptureRequested = true;
            std::cout << "Frustum wireframe: captured" << std::endl;
          }
          else
          {
            g_dwgDialogRequested = true;
          }
        }
        if (evt.key.key == SDLK_K)
        {
          gpuPickSceneDebug = !gpuPickSceneDebug;
          if (acgs::acgsGetManager()->deviceReady())
            acgs::acgsGetManager()->setGpuPickSceneDebug(gpuPickSceneDebug);
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
          if (acgs::acgsGetManager()->deviceReady())
            acgs::acgsGetManager()->setSelectionOutlineAll(outlineAllTest);
          if (outlineAllTest)
            std::puts("Outline all: on");
          else
            std::puts("Outline all: off");
        }
        // G -- glTF demo: G loads/cycles models from resources/models,
        // Ctrl+G exports the loaded model back out as .glb + .gltf.  The
        // model switch bumps the draw-list glTF serial, so the next frame
        // rebuilds the tessellation with the new records.
        if (evt.key.scancode == SDL_SCANCODE_G)
        {
          if (evt.key.mod & SDL_KMOD_CTRL)
            exportGltfDemoRecords(
                getVectorPrimitivesTessellation().meshes);
          else
            cycleGltfDemoModel();
        }
        // V -- cycle the visual style through all render modes.
        if (evt.key.key == SDLK_V)
        {
          acgsView().visualStyle().cycle();
          if (acgs::acgsGetManager()->deviceReady())
            acgs::acgsGetManager()->syncActiveRenderMode();
          std::cout << "Visual style: "
                    << rendering::renderModeLabel(acgsView().visualStyle().mode())
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
            acgsView().resetDepthSlabs();
            SDL_SetWindowTitle(
                appPlatform.window(),
                "grid plane - large-coordinate stress field");
          }
          else
          {
            cubeWorldPosition = AcGePoint3d(0.0, 0.0, 0.0);
            orbitCam.clearDepthBounds();
            orbitCam.setOrbit(cubeWorldPosition, 15.0);
            acgsView().resetDepthSlabs();
            SDL_SetWindowTitle(appPlatform.window(), "grid plane");
          }
          std::cout << "Camera: "
                    << (largeCoordinateCameraView ? "large-coordinate field"
                                                  : "origin")
                    << std::endl;
        }
        // X -- toggle the erased flag of the newest model-space entity.
        // The mutation rides the identity chain end to end: the reactor
        // lands a Dirty tag in the ECS mirror, the mirror revision bumps,
        // and the revision-keyed draw list rebuild drops/returns the
        // entity from the picture.
        if (evt.key.key == SDLK_X)
        {
          acdb::AcDbDatabase &document = acdbDocument();
          const std::vector<acdb::AcDbHandle> &handles =
              document.modelSpace().entityHandles();
          if (!handles.empty())
          {
            const acdb::AcDbHandle target = handles.back();
            if (document.isErased(target))
            {
              document.uneraseEntity(target);
              std::cout << "ECS mirror: unerased entity " << target.value
                        << std::endl;
            }
            else
            {
              document.eraseEntity(target);
              std::cout << "ECS mirror: erased entity " << target.value
                        << std::endl;
            }
          }
        }
        // Tab -- switch the active viewport (ObjectARX CVPORT).  The
        // first press materializes the second viewport bound to its own
        // AcDbViewportTableRecord ("Top"); every switch performs the
        // ARX DB↔GS parameter transfer: the outgoing view's camera
        // lands in its record (zoom/pan write-back), the incoming
        // record restores into its view.
        if (evt.key.key == SDLK_TAB)
        {
          acgs::AcGsManager *manager = acgs::acgsGetManager();
          acdb::AcDbDatabase &document = acdbDocument();
          if (manager->viewCount() < 2)
          {
            acgs::AcGsView *top = manager->createView();
            top->setViewportRecordName("Top");
            acdb::AcDbViewportTableRecord &record =
                document.viewportTable().contains("Top")
                    ? *document.viewportTable().getMutable("Top")
                    : document.viewportTable().add(
                        "Top", document.allocateHandle());
            // A fresh top viewport looks straight down at whatever the
            // active view is currently framing.
            top->orbitCamera().setOrbit(
                acgsView().orbitCamera().Target, 200.0);
            top->orbitCamera().setViewDirection(
                acgs::math::DVec3(0.0, 0.0, -1.0));
            top->orthoMode() = true;
            top->writeToViewportRecord(record);
          }

          auto boundRecord = [&](acgs::AcGsView &view)
              -> acdb::AcDbViewportTableRecord * {
            const std::string &name = view.viewportRecordName();
            const char *bound = !name.empty()
                                    ? name.c_str()
                                    : acdb::kActiveViewportName;
            return document.viewportTable().contains(bound)
                       ? document.viewportTable().getMutable(bound)
                       : nullptr;
          };
          if (acdb::AcDbViewportTableRecord *outgoing =
                  boundRecord(acgsView()))
            acgsView().writeToViewportRecord(*outgoing);

          manager->setActiveView(manager->activeViewIndex() == 0 ? 1
                                                                 : 0);
          if (acdb::AcDbViewportTableRecord *incoming =
                  boundRecord(acgsView()))
            acgsView().applyViewportRecord(*incoming);
          document.setCvport(manager->activeViewIndex() + 1);
          std::cout << "Active viewport: "
                    << manager->activeViewIndex() + 1 << " ("
                    << (acgsView().viewportRecordName().empty()
                            ? acdb::kActiveViewportName
                            : acgsView().viewportRecordName())
                    << ")" << std::endl;
        }
      }

      if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
          evt.button.button == SDL_BUTTON_MIDDLE && !mouseCaptured)
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

      // Single-click left button: pick + autofocus at cursor position.
      if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
          evt.button.button == SDL_BUTTON_LEFT &&
          evt.button.clicks == 1 && !mouseCaptured)
      {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(appPlatform.window(), &winW, &winH);
        if (pickDebugEnabled())
        {
          std::printf("[PICK_DEBUG] click x=%d y=%d gpu=%d",
                      static_cast<int>(evt.button.x), static_cast<int>(evt.button.y),
                      gpuPickEnabled() ? 1 : 0);
          std::puts("");
        }
        const double ndcX = cursorToNdcX(evt.button.x, winW);
        const double ndcY = cursorToNdcY(evt.button.y, winH);
        if (gpuPickEnabled())
        {
          gpuPickManager().requestPick(ndcX, ndcY);
        }
        else
        {
          outlineEntity.reset();
          if (const std::optional<AutofocusResult> selectedEntity =
                  autofocusAtNdc(ndcX, ndcY))
          {
            reportAutofocus(*selectedEntity);
            if (const GpuPickEntity *entity =
                    findGpuPickEntityForAutofocusName(
                        selectedEntity->entityName))
            {
              outlineEntity = *entity;
              lockedOutlineId = findGpuPickObjectIdForEntity(*entity);
            }
          }
        }
      }

      if (!mouseCaptured)
      {
        handleOrbitMouseMovement(evt, middleMouseDrag);
        handleOrbitZoom(evt);
      }
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
        cubeWorldPosition = acgs::math::DVec3(0.0);
        orbitCam.clearDepthBounds();
        orbitCam.setOrbit(cubeWorldPosition, 15.0);
        switchProjectionMode();
        acgsView().resetDepthSlabs();
        originOrthoScenarioApplied = true;
        std::cout << "Camera test: origin orthographic convergence"
                  << std::endl;
      }
    }

    if (appPlatform.imguiEnabled())
    {
      appPlatform.beginImGuiFrame();
      //fprintf(stderr, "[P] NewFrame ok\n");
      drawScenePanels();
      drawFpsOverlay();
      drawDwgFileDialog();
      //fprintf(stderr, "[P] panels ok\n");
    }
    render();

    if (debugExitFrames > 0)
    {
      --debugExitFrames;
      if (debugExitFrames == 0)
        running = false;
    }

    acgs::acgsGetManager()->present();

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
