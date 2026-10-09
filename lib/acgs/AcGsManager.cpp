#include "acgs/AcGsManager.h"

struct ImDrawData;

#include <SDL.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "rendering/RendererBackend.h"

#include "acgs/AcGsView.h"

namespace acgs
{

namespace
{

// The backend flavor the host asked for (former main.cpp
// resolveRequestedBackend): WINDOW_RENDERER picks the device; empty
// means the default bgfx device with an auto-chosen graphics API.
struct RequestedDevice
{
    rendering::BackendType type = rendering::BackendType::Bgfx;
    rendering::GraphicsApi api = rendering::GraphicsApi::Auto;
};

RequestedDevice resolveRequestedDevice(std::string *error)
{
    RequestedDevice requested;
    const char *backend = std::getenv("WINDOW_RENDERER");
    if (!backend)
        return requested;

    const std::string_view backendName(backend);
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
        if (error != nullptr)
            *error = "Unknown WINDOW_RENDERER value '" +
                     std::string(backendName) +
                     "'. Supported: bgfx, dx11, dx12, webgpu, gl, vk.";
    }
    return requested;
}

} // namespace

AcGsManager::AcGsManager()
{
    // The default view keeps the historical AcGsView::instance() camera
    // (origin target, 15 distance, -45 yaw, 20 pitch) so the demo's
    // startup framing is unchanged after the de-singletion.
    views_.push_back(std::make_unique<AcGsView>());
    active_ = 0;
}

// ---- device lifecycle ----

bool AcGsManager::prepareDevice(std::string *error)
{
    if (device_ != nullptr)
        return true;
    const RequestedDevice requested = resolveRequestedDevice(error);
    rendering::RendererBackend *created =
        rendering::createRenderer(requested.type, requested.api)
            .release();
    if (created == nullptr)
    {
        if (error != nullptr && error->empty())
            *error = "renderer backend creation failed";
        return false;
    }
    device_ = created;
    if (!device_->configureSDL())
    {
        if (error != nullptr)
            *error = std::string("Failed to configure render backend: ") +
                     device_->name();
        shutdownDevice();
        return false;
    }
    return true;
}

unsigned int AcGsManager::deviceWindowFlags() const
{
    return device_ != nullptr ? device_->windowFlags() : 0u;
}

bool AcGsManager::initializeDevice(void *sdlWindow, std::string *error)
{
    if (device_ == nullptr)
    {
        if (error != nullptr)
            *error = "initializeDevice before prepareDevice";
        return false;
    }
    if (!device_->initialize(static_cast<SDL_Window *>(sdlWindow)))
    {
        if (error != nullptr)
            *error = std::string("Failed to initialize render backend: ") +
                     device_->name();
        return false;
    }
    return true;
}

void AcGsManager::shutdownDevice()
{
    if (device_ == nullptr)
        return;
    device_->shutdown();
    delete device_;
    device_ = nullptr;
}

const char *AcGsManager::deviceName() const
{
    return device_ != nullptr ? device_->name() : "none";
}

const char *AcGsManager::deviceApiName() const
{
    return device_ != nullptr ? device_->graphicsApiName() : "none";
}

// ---- host frame pump ----

bool AcGsManager::beginFrame()
{
    return device_ != nullptr &&
           device_->beginFrame(kClearColor);
}

void AcGsManager::endFrame()
{
    if (device_ != nullptr)
        device_->endFrame();
}

void AcGsManager::present()
{
    if (device_ != nullptr)
        device_->present();
}

float AcGsManager::fps() const
{
    return device_ != nullptr ? device_->fps() : 0.0f;
}

// ---- semantic device configuration ----

void AcGsManager::syncActiveRenderMode()
{
    if (AcGsView *view = activeView(); view != nullptr && device_ != nullptr)
        device_->setRenderMode(view->visualStyle().mode());
}

rendering::RenderModeFlags AcGsManager::deviceRenderModeFlags() const
{
    return device_ != nullptr ? device_->renderModeFlags()
                              : rendering::RenderModeFlags{};
}

void AcGsManager::setSelectionOutlineId(std::uint32_t objectId)
{
    if (device_ != nullptr)
        device_->setSelectionOutlineId(objectId);
}

void AcGsManager::setSelectionOutlineAll(bool all)
{
    if (device_ != nullptr)
        device_->setSelectionOutlineAll(all);
}

int AcGsManager::loadMeshTexture(const std::string &path)
{
    return device_ != nullptr
               ? static_cast<int>(device_->loadMeshTexture(path))
               : 0;
}

void AcGsManager::requestDebugScreenshot(const std::string &path)
{
    if (device_ != nullptr)
        device_->requestDebugScreenShot(path);
}

bool AcGsManager::beginPipScene()
{
    return device_ != nullptr && device_->beginPipScene();
}

void AcGsManager::endPipScene()
{
    if (device_ != nullptr)
        device_->endPipScene();
}

void AcGsManager::compositePip()
{
    if (device_ != nullptr)
        device_->compositePip();
}

// ---- GPU pick service ----

void AcGsManager::setGpuPickIdRange(std::uint32_t minId,
                                    std::uint32_t maxId)
{
    if (device_ != nullptr)
        device_->setGpuPickIdRange(minId, maxId);
}

void AcGsManager::setGpuPickScenePassEnabled(bool enabled)
{
    if (device_ != nullptr)
        device_->setGpuPickScenePassEnabled(enabled);
}

void AcGsManager::setGpuPickDebugVisible(bool visible)
{
    if (device_ != nullptr)
        device_->setGpuPickDebugVisible(visible);
}

void AcGsManager::setGpuPickSceneDebug(bool visible)
{
    if (device_ != nullptr)
        device_->setGpuPickSceneDebug(visible);
}

std::uint32_t AcGsManager::requestGpuPick(
    const rendering::GpuPickRequest &request)
{
    return device_ != nullptr ? device_->requestGpuPick(request) : 0;
}

rendering::GpuPickResult AcGsManager::pollGpuPick()
{
    return device_ != nullptr ? device_->pollGpuPick()
                              : rendering::GpuPickResult{};
}
std::uint32_t AcGsManager::requestGpuPickPixel(float ndcX, float ndcY)
{
    return device_ != nullptr ? device_->requestGpuPickPixel(ndcX, ndcY) : 0;
}
void AcGsManager::compositeFrame()
{
    if (device_ != nullptr)
        device_->compositeFrame();
}

void AcGsManager::setImGuiActive(bool active)
{
    if (device_ != nullptr)
        device_->setImGuiActive(active);
}

void AcGsManager::drawImGui(ImDrawData *drawData)
{
    if (device_ != nullptr)
        device_->drawImGui(drawData);
}

std::uint32_t AcGsManager::sceneTexture(int slot) const
{
    return device_ != nullptr ? device_->sceneTexture(slot) : 0;
}

void AcGsManager::setSceneRenderSize(std::uint32_t width, std::uint32_t height)
{
    if (device_ != nullptr)
        device_->setSceneRenderSize(width, height);
}

void AcGsManager::setGpuPickDebugSize(std::uint32_t width, std::uint32_t height)
{
    if (device_ != nullptr)
        device_->setGpuPickDebugSize(width, height);
}

void AcGsManager::setActiveSceneSlot(int slot)
{
    if (device_ != nullptr)
        device_->setActiveSceneSlot(slot);
}

void AcGsManager::blitSceneToSlot(int slot)
{
    if (device_ != nullptr)
        device_->blitSceneToSlot(slot);
}

void AcGsManager::cancelGpuPick()
{
    if (device_ != nullptr)
        device_->cancelGpuPick();
}

rendering::GpuPickQueueStats AcGsManager::gpuPickQueueStats() const
{
    return device_ != nullptr ? device_->gpuPickQueueStats()
                              : rendering::GpuPickQueueStats{};
}

void AcGsManager::queueGpuTrianglePick(
    std::uint64_t geometryKey, const rendering::FillVertex *vertices,
    std::uint32_t vertexCount, const glm::mat4 &view,
    const glm::mat4 &projection, const glm::vec4 &logDepth,
    std::uint32_t objectId, std::uint8_t occlusionRank)
{
    if (device_ != nullptr)
        device_->queueGpuTrianglePick(geometryKey, vertices, vertexCount,
                                      view, projection, logDepth, objectId,
                                      occlusionRank);
}

void AcGsManager::queueGpuMeshPick(
    const rendering::MeshInstance &instance, rendering::MeshType mesh,
    std::uint32_t objectId)
{
    if (device_ != nullptr)
        device_->queueGpuMeshPick(instance, mesh, objectId);
}

// ---- views ----

AcGsView *AcGsManager::createView()
{
    views_.push_back(std::make_unique<AcGsView>());
    return views_.back().get();
}

void AcGsManager::destroyView(AcGsView *view)
{
    for (auto it = views_.begin(); it != views_.end(); ++it)
    {
        if (it->get() == view)
        {
            views_.erase(it);
            if (active_ >= int(views_.size()))
                active_ = int(views_.size()) - 1;
            return;
        }
    }
}

AcGsView *AcGsManager::activeView()
{
    if (views_.empty())
        return nullptr;
    return views_[std::size_t(active_ < 0 ? 0 : active_)].get();
}

void AcGsManager::setActiveView(int index)
{
    if (index >= 0 && index < int(views_.size()))
        active_ = index;
}

AcGsManager *acgsGetManager()
{
    static AcGsManager manager;
    return &manager;
}

} // namespace acgs
