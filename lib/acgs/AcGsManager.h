#pragma once

// acgs::AcGsManager (namespace acgs) — the GS top-level manager, aligned
// with ObjectARX AcGsManager and reached through acgsGetManager().
//
// ObjectARX splits the graphics session into three layers:
//   AcGsManager  the factory and owner of devices and views;
//   AcGsDevice   one render target (window / print / offscreen) hosting
//                N views;
//   AcGsView     one live viewport (camera + visual style), a first-class
//                citizen: the count follows the layout, never a singleton.
//
// Route-B device encapsulation: the device role is played by a
// rendering::RendererBackend, and the type is invisible outside this
// layer — the app hands over a native window handle (ObjectARX
// createAutoCADDevice(HWND)) and afterwards speaks only the semantic
// device services below (frame pump, pick service, style sync).  The
// only rendering names that cross this header are protocol value types
// (RenderTypes.h / RenderMode.h); the device class itself stays a
// forward declaration.
//
// Model-space tiled viewports are N views on one device, each bound to
// its AcDbViewportTableRecord through AcGsView::applyViewportRecord /
// writeToViewportRecord; the active view rides the database CVPORT.
//
// Per-viewport FBO scheduling (the SYCAD offscreen-viewport pattern):
// the renderFrameClaim guard is here already — when the per-viewport
// texture pipeline lands, only the claimed view renders per frame and
// the rest display their last frame.  Until then every view renders
// through the shared scene framebuffer as before.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "rendering/RenderTypes.h"
#include "rendering/RenderMode.h"

namespace rendering
{
class RendererBackend; // acgs-internal: the app never names this type
struct FillVertex;
} // namespace rendering

// ImGui owns the type; the forward declaration must be global.
struct ImDrawData;
namespace acgs
{

class AcGsView;

class AcGsManager
{
public:
    AcGsManager();

    AcGsManager(const AcGsManager &) = delete;
    AcGsManager &operator=(const AcGsManager &) = delete;

    // ---- device lifecycle (ObjectARX: createAutoCADDevice) ----

    // Pre-window: resolves the requested backend (WINDOW_RENDERER
    // environment), creates the device, and applies its SDL window
    // configuration.  Returns false and fills |error| when the request
    // is unknown or the device refuses.
    bool prepareDevice(std::string *error = nullptr);
    // SDL window creation flags the prepared device needs.
    unsigned int deviceWindowFlags() const;
    // Post-window: hands the native window to the device.  The manager
    // binds every current and future view to it.
    bool initializeDevice(void *sdlWindow, std::string *error = nullptr);
    void shutdownDevice();
    bool deviceReady() const { return device_ != nullptr; }
    const char *deviceName() const;
    const char *deviceApiName() const;

    // ---- host frame pump (the app drives; the device executes) ----
    bool beginFrame();
    void endFrame();
    void present();

    // ---- semantic device configuration ----
    // Pushes the active view's visual style to the device (call after
    // visualStyle().set / cycle).
    void syncActiveRenderMode();
    rendering::RenderModeFlags deviceRenderModeFlags() const;
    void setSelectionOutlineId(std::uint32_t objectId);
    void setSelectionOutlineAll(bool all);
    int loadMeshTexture(const std::string &path);
    void requestDebugScreenshot(const std::string &path);
    // ---- picture-in-picture secondary scene target (optional) ----
    // Capability forwards; see RendererBackend for the contract.  The
    // host re-submits content through an inactive view between begin
    // and end, and composites every frame before endFrame.
    bool beginPipScene();
    void endPipScene();
    void compositePip();

    // ---- GPU pick service (ObjectARX: the GS owns the id pass) ----
    // The host still assembles pick soups from its visibility walk;
    // queueing, id-range policy, and readback all live here so the pick
    // pass cannot drift from the visible pass's depth semantics.
    void setGpuPickIdRange(std::uint32_t minId, std::uint32_t maxId);
    void setGpuPickScenePassEnabled(bool enabled);
    void setGpuPickDebugVisible(bool visible);
    void setGpuPickSceneDebug(bool visible);
    std::uint32_t requestGpuPick(const rendering::GpuPickRequest &request);
    rendering::GpuPickResult pollGpuPick();
    // ---- ImGui presentation mode ----
    void compositeFrame();
    void setImGuiActive(bool active);
    void drawImGui(ImDrawData *drawData);
    std::uint32_t sceneTexture(int slot) const;
    void setSceneRenderSize(std::uint32_t width, std::uint32_t height);
    void setGpuPickDebugSize(std::uint32_t width, std::uint32_t height);
    void setActiveSceneSlot(int slot);
    void blitSceneToSlot(int slot);
    // Unified picking: sample the full-scene ID texture (see RendererBackend).
    std::uint32_t requestGpuPickPixel(float ndcX, float ndcY);
    void cancelGpuPick();
    rendering::GpuPickQueueStats gpuPickQueueStats() const;
    void queueGpuTrianglePick(
        std::uint64_t geometryKey, const rendering::FillVertex *vertices,
        std::uint32_t vertexCount, const glm::mat4 &view,
        const glm::mat4 &projection, const glm::vec4 &logDepth,
        std::uint32_t objectId, std::uint8_t occlusionRank = 2);
    void queueGpuMeshPick(const rendering::MeshInstance &instance,
                          rendering::MeshType mesh, std::uint32_t objectId);

    // ---- views (ObjectARX: AcGsManager::createView) ----
    AcGsView *createView();
    void destroyView(AcGsView *view);
    const std::vector<std::unique_ptr<AcGsView>> &views() const
    {
        return views_;
    }
    std::size_t viewCount() const { return views_.size(); }

    // The active viewport (ObjectARX CVPORT semantics; the number itself
    // is database state — acdb::AcDbDatabase::cvport).
    AcGsView *activeView();
    int activeViewIndex() const { return active_; }
    void setActiveView(int index);

    // ---- time-share guard (SYCAD offscreen-viewport pattern) ----
    // Claims this frame's single render slot.  Returns false when another
    // view already claimed it; resetFrameRender() opens the next frame.
    bool tryClaimFrameRender()
    {
        if (frameClaimed_)
            return false;
        frameClaimed_ = true;
        return true;
    }
    void resetFrameRender() { frameClaimed_ = false; }

    // acgs-internal: views submit through this; the app cannot use it
    // without naming the device type, which it must not.
    rendering::RendererBackend *device() const { return device_; }

private:
    rendering::RendererBackend *device_ = nullptr;
    std::vector<std::unique_ptr<AcGsView>> views_;
    int active_ = 0;
    bool frameClaimed_ = false;
};

// ObjectARX: AcGsManager* acgsGetManager(void).
AcGsManager *acgsGetManager();

} // namespace acgs
