#include "rendering/WebGpuRenderer.h"

#include <webgpu/webgpu.h>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace rendering
{
namespace
{

struct WebGpuProcs
{
    using PFN_CreateInstance =
        WGPUInstance (*)(const WGPUInstanceDescriptor *);
    using PFN_InstanceCreateSurface =
        WGPUSurface (*)(WGPUInstance, const WGPUSurfaceDescriptor *);
    using PFN_InstanceRequestAdapter =
        WGPUFuture (*)(WGPUInstance, const WGPURequestAdapterOptions *,
                       WGPURequestAdapterCallbackInfo);
    using PFN_InstanceProcessEvents = void (*)(WGPUInstance);
    using PFN_AdapterRequestDevice =
        WGPUFuture (*)(WGPUAdapter, const WGPUDeviceDescriptor *,
                       WGPURequestDeviceCallbackInfo);
    using PFN_SurfaceConfigure = void (*)(WGPUSurface,
                                          const WGPUSurfaceConfiguration *);
    using PFN_SurfaceGetCurrentTexture = void (*)(WGPUSurface,
                                                  WGPUSurfaceTexture *);
    using PFN_SurfacePresent = WGPUStatus (*)(WGPUSurface);
    using PFN_SurfaceUnconfigure = void (*)(WGPUSurface);
    using PFN_DeviceGetQueue = WGPUQueue (*)(WGPUDevice);
    using PFN_DeviceCreateCommandEncoder =
        WGPUCommandEncoder (*)(WGPUDevice,
                               const WGPUCommandEncoderDescriptor *);
    using PFN_CommandEncoderBeginRenderPass =
        WGPURenderPassEncoder (*)(WGPUCommandEncoder,
                                  const WGPURenderPassDescriptor *);
    using PFN_CommandEncoderFinish =
        WGPUCommandBuffer (*)(WGPUCommandEncoder,
                              const WGPUCommandBufferDescriptor *);
    using PFN_QueueSubmit =
        void (*)(WGPUQueue, size_t, const WGPUCommandBuffer *);
    using PFN_TextureCreateView =
        WGPUTextureView (*)(WGPUTexture, const WGPUTextureViewDescriptor *);
    using PFN_RenderPassEncoderEnd = void (*)(WGPURenderPassEncoder);

    template <typename T>
    T load(HMODULE runtime, const char *name)
    {
        return reinterpret_cast<T>(GetProcAddress(runtime, name));
    }

    explicit WebGpuProcs(HMODULE runtime)
        : createInstance(load<PFN_CreateInstance>(runtime, "wgpuCreateInstance")),
          instanceCreateSurface(load<PFN_InstanceCreateSurface>(
              runtime, "wgpuInstanceCreateSurface")),
          instanceRequestAdapter(load<PFN_InstanceRequestAdapter>(
              runtime, "wgpuInstanceRequestAdapter")),
          instanceProcessEvents(load<PFN_InstanceProcessEvents>(
              runtime, "wgpuInstanceProcessEvents")),
          adapterRequestDevice(load<PFN_AdapterRequestDevice>(
              runtime, "wgpuAdapterRequestDevice")),
          surfaceConfigure(load<PFN_SurfaceConfigure>(
              runtime, "wgpuSurfaceConfigure")),
          surfaceGetCurrentTexture(load<PFN_SurfaceGetCurrentTexture>(
              runtime, "wgpuSurfaceGetCurrentTexture")),
          surfacePresent(load<PFN_SurfacePresent>(runtime, "wgpuSurfacePresent")),
          surfaceUnconfigure(load<PFN_SurfaceUnconfigure>(
              runtime, "wgpuSurfaceUnconfigure")),
          deviceGetQueue(load<PFN_DeviceGetQueue>(runtime, "wgpuDeviceGetQueue")),
          deviceCreateCommandEncoder(load<PFN_DeviceCreateCommandEncoder>(
              runtime, "wgpuDeviceCreateCommandEncoder")),
          commandEncoderBeginRenderPass(load<PFN_CommandEncoderBeginRenderPass>(
              runtime, "wgpuCommandEncoderBeginRenderPass")),
          commandEncoderFinish(load<PFN_CommandEncoderFinish>(
              runtime, "wgpuCommandEncoderFinish")),
          queueSubmit(load<PFN_QueueSubmit>(runtime, "wgpuQueueSubmit")),
          textureCreateView(load<PFN_TextureCreateView>(
              runtime, "wgpuTextureCreateView")),
          renderPassEncoderEnd(load<PFN_RenderPassEncoderEnd>(
              runtime, "wgpuRenderPassEncoderEnd")),
          adapterRelease(reinterpret_cast<void (*)(WGPUAdapter)>(
              GetProcAddress(runtime, "wgpuAdapterRelease"))),
          deviceRelease(reinterpret_cast<void (*)(WGPUDevice)>(
              GetProcAddress(runtime, "wgpuDeviceRelease"))),
          surfaceRelease(reinterpret_cast<void (*)(WGPUSurface)>(
              GetProcAddress(runtime, "wgpuSurfaceRelease"))),
          instanceRelease(reinterpret_cast<void (*)(WGPUInstance)>(
              GetProcAddress(runtime, "wgpuInstanceRelease"))),
          commandBufferRelease(reinterpret_cast<void (*)(WGPUCommandBuffer)>(
              GetProcAddress(runtime, "wgpuCommandBufferRelease"))),
          commandEncoderRelease(reinterpret_cast<void (*)(WGPUCommandEncoder)>(
              GetProcAddress(runtime, "wgpuCommandEncoderRelease"))),
          renderPassEncoderRelease(reinterpret_cast<void (*)(WGPURenderPassEncoder)>(
              GetProcAddress(runtime, "wgpuRenderPassEncoderRelease"))),
          textureRelease(reinterpret_cast<void (*)(WGPUTexture)>(
              GetProcAddress(runtime, "wgpuTextureRelease"))),
          textureViewRelease(reinterpret_cast<void (*)(WGPUTextureView)>(
              GetProcAddress(runtime, "wgpuTextureViewRelease")))
    {
    }

    PFN_CreateInstance createInstance;
    PFN_InstanceCreateSurface instanceCreateSurface;
    PFN_InstanceRequestAdapter instanceRequestAdapter;
    PFN_InstanceProcessEvents instanceProcessEvents;
    PFN_AdapterRequestDevice adapterRequestDevice;
    PFN_SurfaceConfigure surfaceConfigure;
    PFN_SurfaceGetCurrentTexture surfaceGetCurrentTexture;
    PFN_SurfacePresent surfacePresent;
    PFN_SurfaceUnconfigure surfaceUnconfigure;
    PFN_DeviceGetQueue deviceGetQueue;
    PFN_DeviceCreateCommandEncoder deviceCreateCommandEncoder;
    PFN_CommandEncoderBeginRenderPass commandEncoderBeginRenderPass;
    PFN_CommandEncoderFinish commandEncoderFinish;
    PFN_QueueSubmit queueSubmit;
    PFN_TextureCreateView textureCreateView;
    PFN_RenderPassEncoderEnd renderPassEncoderEnd;
    void (*adapterRelease)(WGPUAdapter);
    void (*deviceRelease)(WGPUDevice);
    void (*surfaceRelease)(WGPUSurface);
    void (*instanceRelease)(WGPUInstance);
    void (*commandBufferRelease)(WGPUCommandBuffer);
    void (*commandEncoderRelease)(WGPUCommandEncoder);
    void (*renderPassEncoderRelease)(WGPURenderPassEncoder);
    void (*textureRelease)(WGPUTexture);
    void (*textureViewRelease)(WGPUTextureView);
};

void requestAdapterCallback(WGPURequestAdapterStatus status,
                            WGPUAdapter adapter, WGPUStringView, void *userdata1,
                            void *)
{
    auto *result = static_cast<AsyncResult *>(userdata1);
    result->completed = true;
    if (status == WGPURequestAdapterStatus_Success)
        result->object = adapter;
}

void requestDeviceCallback(WGPURequestDeviceStatus status,
                           WGPUDevice device, WGPUStringView, void *userdata1,
                           void *)
{
    auto *result = static_cast<AsyncResult *>(userdata1);
    result->completed = true;
    if (status == WGPURequestDeviceStatus_Success)
        result->object = device;
}

std::string runtimePath()
{
    if (const char *overridePath = std::getenv("WGPU_NATIVE_DLL"))
        return overridePath;
    return "wgpu_native-release.dll";
}

} // namespace

bool webGpuRuntimeAvailable()
{
    HMODULE runtime = LoadLibraryA(runtimePath().c_str());
    if (!runtime)
        return false;
    FreeLibrary(runtime);
    return true;
}

const char *WebGpuRenderer::name() const
{
    return "webgpu-native";
}

const char *WebGpuRenderer::graphicsApiName() const
{
    return "WebGPU";
}

Uint32 WebGpuRenderer::windowFlags() const
{
    return 0;
}

bool WebGpuRenderer::configureSDL()
{
    return true;
}

void WebGpuRenderer::configureSurface(uint32_t width, uint32_t height)
{
    if (!m_runtime || !m_surface)
        return;

    const auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    WGPUSurfaceConfiguration config{};
    config.device = m_device;
    config.format = WGPUTextureFormat_BGRA8Unorm;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.presentMode = WGPUPresentMode_Fifo;
    procs->surfaceConfigure(m_surface, &config);
    m_width = width;
    m_height = height;
    m_surfaceConfigured = true;
}

bool WebGpuRenderer::initialize(SDL_Window *window)
{
    m_window = window;

    HMODULE module = LoadLibraryA(runtimePath().c_str());
    if (!module)
    {
        std::cerr << "Failed to load wgpu-native runtime: "
                  << runtimePath() << std::endl;
        return false;
    }

    WebGpuProcs *procs = new WebGpuProcs(module);
    m_runtime = procs;
    m_runtimeModule = module;
    if (!procs->createInstance || !procs->instanceCreateSurface ||
        !procs->instanceRequestAdapter || !procs->adapterRequestDevice ||
        !procs->surfaceConfigure ||
        !procs->surfaceGetCurrentTexture || !procs->surfacePresent ||
        !procs->deviceGetQueue || !procs->deviceCreateCommandEncoder ||
        !procs->commandEncoderBeginRenderPass || !procs->commandEncoderFinish ||
        !procs->queueSubmit || !procs->textureCreateView ||
        !procs->renderPassEncoderEnd)
    {
        std::cerr << "wgpu-native runtime is missing required exports."
                  << std::endl;
        shutdown();
        return false;
    }

    WGPUInstanceDescriptor instanceDescriptor{};
    WGPUInstance instance = procs->createInstance(&instanceDescriptor);
    if (!instance)
    {
        std::cerr << "Failed to create WebGPU instance." << std::endl;
        shutdown();
        return false;
    }
    m_instance = instance;

    WGPUSurfaceSourceWindowsHWND hwndSource{};
    hwndSource.chain.next = nullptr;
    hwndSource.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    hwndSource.hinstance = GetModuleHandleW(nullptr);
    hwndSource.hwnd = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr);
    if (!hwndSource.hwnd)
    {
        std::cerr << "Failed to get Win32 HWND for WebGPU surface." << std::endl;
        shutdown();
        return false;
    }

    WGPUSurfaceDescriptor surfaceDescriptor{};
    surfaceDescriptor.nextInChain = &hwndSource.chain;
    m_surface = procs->instanceCreateSurface(instance, &surfaceDescriptor);
    if (!m_surface)
    {
        std::cerr << "Failed to create WebGPU surface." << std::endl;
        shutdown();
        return false;
    }

    WGPURequestAdapterOptions adapterOptions{};
    adapterOptions.featureLevel = WGPUFeatureLevel_Core;
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
    adapterOptions.backendType = WGPUBackendType_D3D12;
    adapterOptions.compatibleSurface = m_surface;
    WGPURequestAdapterCallbackInfo adapterCallbackInfo{};
    adapterCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    adapterCallbackInfo.callback = requestAdapterCallback;
    adapterCallbackInfo.userdata1 = &m_asyncResult;
    procs->instanceRequestAdapter(instance, &adapterOptions,
                                  adapterCallbackInfo);

    if (procs->instanceProcessEvents)
        procs->instanceProcessEvents(instance);
    if (!m_asyncResult.completed || !m_asyncResult.object)
    {
        std::cerr << "No compatible WebGPU adapter was found." << std::endl;
        shutdown();
        return false;
    }
    m_adapter = static_cast<WGPUAdapter>(m_asyncResult.object);
    m_asyncResult = {};

    WGPUDeviceDescriptor deviceDescriptor{};
    WGPURequestDeviceCallbackInfo deviceCallbackInfo{};
    deviceCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCallbackInfo.callback = requestDeviceCallback;
    deviceCallbackInfo.userdata1 = &m_asyncResult;
    procs->adapterRequestDevice(m_adapter, &deviceDescriptor,
                                deviceCallbackInfo);

    if (procs->instanceProcessEvents)
        procs->instanceProcessEvents(instance);
    if (!m_asyncResult.completed || !m_asyncResult.object)
    {
        std::cerr << "Failed to acquire a WebGPU device." << std::endl;
        shutdown();
        return false;
    }
    m_device = static_cast<WGPUDevice>(m_asyncResult.object);
    m_asyncResult = {};
    m_queue = procs->deviceGetQueue(m_device);
    if (!m_queue)
    {
        std::cerr << "WebGPU device has no command queue." << std::endl;
        shutdown();
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    configureSurface(static_cast<uint32_t>(std::max(1, width)),
                     static_cast<uint32_t>(std::max(1, height)));
    m_initialized = true;
    std::cout << "WebGPU adapter/device initialized." << std::endl;
    return true;
}

void WebGpuRenderer::destroyWebGpuObjects()
{
    if (!m_runtime)
        return;

    auto *procs = static_cast<WebGpuProcs *>(m_runtime);
    if (m_surface && procs->surfaceUnconfigure)
        procs->surfaceUnconfigure(m_surface);
    if (m_device && procs->deviceRelease)
        procs->deviceRelease(m_device);
    if (m_adapter && procs->adapterRelease)
        procs->adapterRelease(m_adapter);
    if (m_surface && procs->surfaceRelease)
        procs->surfaceRelease(m_surface);
    if (m_instance && procs->instanceRelease)
        procs->instanceRelease(m_instance);

    m_queue = nullptr;
    m_device = nullptr;
    m_adapter = nullptr;
    m_surface = nullptr;
    m_instance = nullptr;
    m_surfaceTexture = nullptr;
    m_surfaceConfigured = false;
}

void WebGpuRenderer::shutdown()
{
    destroyWebGpuObjects();
    if (m_runtime)
    {
        delete static_cast<WebGpuProcs *>(m_runtime);
        m_runtime = nullptr;
    }
    if (m_runtimeModule)
    {
        FreeLibrary(static_cast<HMODULE>(m_runtimeModule));
        m_runtimeModule = nullptr;
    }
    m_initialized = false;
}

bool WebGpuRenderer::beginFrame(const glm::vec4 &clearColor)
{
    if (!m_initialized || !m_window)
        return false;

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    const uint32_t nextWidth = static_cast<uint32_t>(std::max(1, width));
    const uint32_t nextHeight = static_cast<uint32_t>(std::max(1, height));
    if (!m_surfaceConfigured || nextWidth != m_width ||
        nextHeight != m_height)
    {
        configureSurface(nextWidth, nextHeight);
    }
    m_clearColor = clearColor;
    return m_surfaceConfigured;
}

void WebGpuRenderer::endFrame()
{
    if (!m_initialized || !m_surfaceConfigured)
        return;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    if (m_surfaceTexture && procs->textureRelease)
    {
        procs->textureRelease(m_surfaceTexture);
        m_surfaceTexture = nullptr;
    }
    WGPUSurfaceTexture surfaceTexture{};
    procs->surfaceGetCurrentTexture(m_surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        std::cerr << "WebGPU surface texture is unavailable." << std::endl;
        return;
    }

    WGPUTextureView textureView =
        procs->textureCreateView(surfaceTexture.texture, nullptr);
    if (!textureView)
    {
        if (procs->textureRelease && surfaceTexture.texture)
            procs->textureRelease(surfaceTexture.texture);
        return;
    }

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = textureView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {m_clearColor.r, m_clearColor.g,
                                  m_clearColor.b, m_clearColor.a};

    WGPURenderPassDescriptor renderPass{};
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorAttachment;

    WGPUCommandEncoder encoder =
        procs->deviceCreateCommandEncoder(m_device, nullptr);
    WGPURenderPassEncoder pass =
        encoder ? procs->commandEncoderBeginRenderPass(encoder, &renderPass)
                : nullptr;
    if (pass)
    {
        procs->renderPassEncoderEnd(pass);
        procs->renderPassEncoderRelease(pass);
    }
    WGPUCommandBuffer command =
        encoder ? procs->commandEncoderFinish(encoder, nullptr) : nullptr;
    if (command)
    {
        procs->queueSubmit(m_queue, 1, &command);
        procs->commandBufferRelease(command);
    }
    if (encoder)
        procs->commandEncoderRelease(encoder);
    procs->textureViewRelease(textureView);
    m_surfaceTexture = surfaceTexture.texture;
}

void WebGpuRenderer::present()
{
    if (!m_initialized || !m_runtime)
        return;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    const WGPUStatus presentStatus = procs->surfacePresent(m_surface);
    if (m_surfaceTexture && procs->textureRelease)
    {
        procs->textureRelease(m_surfaceTexture);
        m_surfaceTexture = nullptr;
    }
    if (presentStatus != WGPUStatus_Success)
        std::cerr << "WebGPU surface present failed." << std::endl;
}

void WebGpuRenderer::setRenderMode(RenderMode mode)
{
    m_renderMode.set(mode);
}

RenderMode WebGpuRenderer::renderMode() const
{
    return m_renderMode.mode();
}

RenderModeFlags WebGpuRenderer::renderModeFlags() const
{
    return m_renderMode.flags();
}

} // namespace rendering
