#include "rendering/WebGpuRenderer.h"

#include "rendering/ProceduralMesh.h"

#include <webgpu/webgpu.h>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace rendering
{
namespace
{

constexpr uint64_t kWgpuWholeSize = ~0ull;
constexpr uint32_t kUniformAlignment = 256;
constexpr size_t kMeshVertexStride = 8 * sizeof(float);

uint64_t alignUniformOffset(uint64_t offset)
{
    return (offset + kUniformAlignment - 1) / kUniformAlignment *
           kUniformAlignment;
}

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
    using PFN_DeviceCreateBuffer = WGPUBuffer (*)(WGPUDevice,
                                                  const WGPUBufferDescriptor *);
    using PFN_DeviceCreateBindGroupLayout = WGPUBindGroupLayout (*)(
        WGPUDevice, const WGPUBindGroupLayoutDescriptor *);
    using PFN_DeviceCreateBindGroup = WGPUBindGroup (*)(
        WGPUDevice, const WGPUBindGroupDescriptor *);
    using PFN_DeviceCreatePipelineLayout = WGPUPipelineLayout (*)(
        WGPUDevice, const WGPUPipelineLayoutDescriptor *);
    using PFN_DeviceCreateShaderModule = WGPUShaderModule (*)(
        WGPUDevice, const WGPUShaderModuleDescriptor *);
    using PFN_DeviceCreateRenderPipeline = WGPURenderPipeline (*)(
        WGPUDevice, const WGPURenderPipelineDescriptor *);
    using PFN_DeviceCreateTexture = WGPUTexture (*)(
        WGPUDevice, const WGPUTextureDescriptor *);
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
    using PFN_QueueWriteBuffer = void (*)(WGPUQueue, WGPUBuffer, uint64_t,
                                          const void *, size_t);
    using PFN_TextureCreateView =
        WGPUTextureView (*)(WGPUTexture, const WGPUTextureViewDescriptor *);
    using PFN_RenderPassEncoderSetPipeline = void (*)(
        WGPURenderPassEncoder, WGPURenderPipeline);
    using PFN_RenderPassEncoderSetBindGroup = void (*)(
        WGPURenderPassEncoder, uint32_t, WGPUBindGroup, size_t,
        const uint32_t *);
    using PFN_RenderPassEncoderSetVertexBuffer = void (*)(
        WGPURenderPassEncoder, uint32_t, WGPUBuffer, uint64_t, uint64_t);
    using PFN_RenderPassEncoderDraw = void (*)(WGPURenderPassEncoder, uint32_t,
                                               uint32_t, uint32_t, uint32_t);
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
          deviceCreateBuffer(load<PFN_DeviceCreateBuffer>(
              runtime, "wgpuDeviceCreateBuffer")),
          deviceCreateBindGroupLayout(load<PFN_DeviceCreateBindGroupLayout>(
              runtime, "wgpuDeviceCreateBindGroupLayout")),
          deviceCreateBindGroup(load<PFN_DeviceCreateBindGroup>(
              runtime, "wgpuDeviceCreateBindGroup")),
          deviceCreatePipelineLayout(load<PFN_DeviceCreatePipelineLayout>(
              runtime, "wgpuDeviceCreatePipelineLayout")),
          deviceCreateShaderModule(load<PFN_DeviceCreateShaderModule>(
              runtime, "wgpuDeviceCreateShaderModule")),
          deviceCreateRenderPipeline(load<PFN_DeviceCreateRenderPipeline>(
              runtime, "wgpuDeviceCreateRenderPipeline")),
          deviceCreateTexture(load<PFN_DeviceCreateTexture>(
              runtime, "wgpuDeviceCreateTexture")),
          deviceCreateCommandEncoder(load<PFN_DeviceCreateCommandEncoder>(
              runtime, "wgpuDeviceCreateCommandEncoder")),
          commandEncoderBeginRenderPass(load<PFN_CommandEncoderBeginRenderPass>(
              runtime, "wgpuCommandEncoderBeginRenderPass")),
          commandEncoderFinish(load<PFN_CommandEncoderFinish>(
              runtime, "wgpuCommandEncoderFinish")),
          queueSubmit(load<PFN_QueueSubmit>(runtime, "wgpuQueueSubmit")),
          queueWriteBuffer(load<PFN_QueueWriteBuffer>(
              runtime, "wgpuQueueWriteBuffer")),
          textureCreateView(load<PFN_TextureCreateView>(
              runtime, "wgpuTextureCreateView")),
          renderPassEncoderSetPipeline(load<PFN_RenderPassEncoderSetPipeline>(
              runtime, "wgpuRenderPassEncoderSetPipeline")),
          renderPassEncoderSetBindGroup(load<PFN_RenderPassEncoderSetBindGroup>(
              runtime, "wgpuRenderPassEncoderSetBindGroup")),
          renderPassEncoderSetVertexBuffer(
              load<PFN_RenderPassEncoderSetVertexBuffer>(
                  runtime, "wgpuRenderPassEncoderSetVertexBuffer")),
          renderPassEncoderDraw(load<PFN_RenderPassEncoderDraw>(
              runtime, "wgpuRenderPassEncoderDraw")),
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
              GetProcAddress(runtime, "wgpuTextureViewRelease"))),
          bufferRelease(reinterpret_cast<void (*)(WGPUBuffer)>(
              GetProcAddress(runtime, "wgpuBufferRelease"))),
          bindGroupRelease(reinterpret_cast<void (*)(WGPUBindGroup)>(
              GetProcAddress(runtime, "wgpuBindGroupRelease"))),
          bindGroupLayoutRelease(reinterpret_cast<void (*)(WGPUBindGroupLayout)>(
              GetProcAddress(runtime, "wgpuBindGroupLayoutRelease"))),
          pipelineLayoutRelease(reinterpret_cast<void (*)(WGPUPipelineLayout)>(
              GetProcAddress(runtime, "wgpuPipelineLayoutRelease"))),
          shaderModuleRelease(reinterpret_cast<void (*)(WGPUShaderModule)>(
              GetProcAddress(runtime, "wgpuShaderModuleRelease"))),
          renderPipelineRelease(reinterpret_cast<void (*)(WGPURenderPipeline)>(
              GetProcAddress(runtime, "wgpuRenderPipelineRelease")))
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
    PFN_DeviceCreateBuffer deviceCreateBuffer;
    PFN_DeviceCreateBindGroupLayout deviceCreateBindGroupLayout;
    PFN_DeviceCreateBindGroup deviceCreateBindGroup;
    PFN_DeviceCreatePipelineLayout deviceCreatePipelineLayout;
    PFN_DeviceCreateShaderModule deviceCreateShaderModule;
    PFN_DeviceCreateRenderPipeline deviceCreateRenderPipeline;
    PFN_DeviceCreateTexture deviceCreateTexture;
    PFN_DeviceCreateCommandEncoder deviceCreateCommandEncoder;
    PFN_CommandEncoderBeginRenderPass commandEncoderBeginRenderPass;
    PFN_CommandEncoderFinish commandEncoderFinish;
    PFN_QueueSubmit queueSubmit;
    PFN_QueueWriteBuffer queueWriteBuffer;
    PFN_TextureCreateView textureCreateView;
    PFN_RenderPassEncoderSetPipeline renderPassEncoderSetPipeline;
    PFN_RenderPassEncoderSetBindGroup renderPassEncoderSetBindGroup;
    PFN_RenderPassEncoderSetVertexBuffer renderPassEncoderSetVertexBuffer;
    PFN_RenderPassEncoderDraw renderPassEncoderDraw;
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
    void (*bufferRelease)(WGPUBuffer);
    void (*bindGroupRelease)(WGPUBindGroup);
    void (*bindGroupLayoutRelease)(WGPUBindGroupLayout);
    void (*pipelineLayoutRelease)(WGPUPipelineLayout);
    void (*shaderModuleRelease)(WGPUShaderModule);
    void (*renderPipelineRelease)(WGPURenderPipeline);
};

const char *kSceneShader = R"WGSL(
struct Camera
{
    view : mat4x4<f32>,
    projection : mat4x4<f32>,
    style : vec4<f32>,
    logDepth : vec4<f32>,
    colorOpacity : vec4<f32>,
    plane : vec4<f32>,
    extents : vec4<f32>,
    params : vec4<f32>
};

@group(0) @binding(0) var<uniform> camera : Camera;

struct MeshVertex
{
    @location(0) position : vec3<f32>,
    @location(1) normal : vec3<f32>,
    @location(2) uv : vec2<f32>,
    @location(5) column0 : vec4<f32>,
    @location(6) column1 : vec4<f32>,
    @location(7) column2 : vec4<f32>,
    @location(8) positionHigh : vec4<f32>,
    @location(9) positionLow : vec4<f32>
};

struct PrimVertex
{
    @location(0) position : vec3<f32>,
    @location(1) color : vec4<f32>,
    @location(2) uv : vec2<f32>
};

struct FillVertex
{
    @location(0) position : vec3<f32>,
    @location(1) color : vec4<f32>
};

struct PointVertex
{
    @location(0) position : vec3<f32>,
    @location(1) color : vec4<f32>,
    @location(2) uv : vec2<f32>
};

fn mappedDepth(viewDepth : f32) -> f32
{
    let near = max(camera.logDepth.y, 0.000001);
    let depth = max(viewDepth, near);
    let denominator = log2(max(camera.logDepth.z / near, 1.000001));
    return clamp(log2(max(depth / near, 1.0)) / denominator, 0.0, 1.0);
}

fn projectedPosition(worldPosition : vec4<f32>) -> vec4<f32>
{
    let viewPosition = camera.view * worldPosition;
    let clipPosition = camera.projection * viewPosition;
    var result = clipPosition;
    if (camera.logDepth.x > 0.5) {
        result.z = mappedDepth(-viewPosition.z) * clipPosition.w;
    }
    return result;
}

struct MeshOutput
{
    @builtin(position) position : vec4<f32>,
    @location(0) color : vec4<f32>,
    @location(1) normal : vec3<f32>
};

@vertex
fn vs_mesh(vertex : MeshVertex) -> MeshOutput
{
    let translation = vertex.positionHigh.xyz + vertex.positionLow.xyz;
    let localPosition = vertex.column0.xyz * vertex.position.x +
        vertex.column1.xyz * vertex.position.y +
        vertex.column2.xyz * vertex.position.z;
    let worldPosition = vec4<f32>(localPosition + translation, 1.0);
    var output : MeshOutput;
    output.position = projectedPosition(worldPosition);
    output.color = vec4<f32>(
        vertex.column0.w, vertex.column1.w, vertex.column2.w,
        vertex.positionHigh.w);
    output.normal = normalize(vertex.column0.xyz * vertex.normal.x +
        vertex.column1.xyz * vertex.normal.y +
        vertex.column2.xyz * vertex.normal.z);
    return output;
}

@fragment
fn fs_mesh(input : MeshOutput) -> @location(0) vec4<f32>
{
    let lightDirection = normalize(vec3<f32>(0.4, 0.8, 0.55));
    let diffuse = 0.35 + 0.65 * max(dot(normalize(input.normal), lightDirection), 0.0);
    return vec4<f32>(input.color.rgb * diffuse, input.color.a);
}

struct PrimOutput
{
    @builtin(position) position : vec4<f32>,
    @location(0) color : vec4<f32>,
    @location(1) uv : vec2<f32>
};

@vertex
fn vs_ribbon(vertex : PrimVertex) -> PrimOutput
{
    var output : PrimOutput;
    output.position = projectedPosition(vec4<f32>(vertex.position, 1.0));
    output.color = vertex.color;
    output.uv = vertex.uv;
    return output;
}

@fragment
fn fs_ribbon(input : PrimOutput) -> @location(0) vec4<f32>
{
    let across = abs(input.uv.y * 2.0 - 1.0);
    let softness = max(camera.style.x, 0.02);
    let alpha = 1.0 - clamp((across - (1.0 - softness)) / softness, 0.0, 1.0);
    return vec4<f32>(input.color.rgb, input.color.a * alpha);
}

@vertex
fn vs_line(vertex : PrimVertex) -> PrimOutput
{
    var output : PrimOutput;
    if (camera.style.y > 0.5) {
        output.position = vec4<f32>(vertex.position.xy, vertex.position.z, 1.0);
    } else {
        output.position = projectedPosition(vec4<f32>(vertex.position, 1.0));
    }
    output.color = vertex.color;
    output.uv = vertex.uv;
    return output;
}

@fragment
fn fs_line(input : PrimOutput) -> @location(0) vec4<f32>
{
    return input.color;
}

struct FillOutput
{
    @builtin(position) position : vec4<f32>,
    @location(0) color : vec4<f32>
};

@vertex
fn vs_fill(vertex : FillVertex) -> FillOutput
{
    var output : FillOutput;
    output.position = projectedPosition(vec4<f32>(vertex.position, 1.0));
    output.color = vertex.color;
    return output;
}

@fragment
fn fs_fill(input : FillOutput) -> @location(0) vec4<f32>
{
    return input.color;
}

struct PointOutput
{
    @builtin(position) position : vec4<f32>,
    @location(0) color : vec4<f32>,
    @location(1) uv : vec2<f32>
};

@vertex
fn vs_point(vertex : PointVertex) -> PointOutput
{
    var output : PointOutput;
    output.position = vec4<f32>(vertex.position.xy, vertex.position.z, 1.0);
    output.color = vertex.color;
    output.uv = vertex.uv;
    return output;
}

@fragment
fn fs_point(input : PointOutput) -> @location(0) vec4<f32>
{
    let distance = length(input.uv);
    if (distance > 1.0) {
        discard;
    }
    let alpha = 1.0 - smoothstep(0.72, 1.0, distance);
    return vec4<f32>(input.color.rgb, input.color.a * alpha);
}
)WGSL";

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

glm::mat4 projectionForWebGpu(const glm::mat4 &projection)
{
    glm::mat4 depthRange(1.0f);
    depthRange[2][2] = 0.5f;
    depthRange[3][2] = 0.5f;
    return depthRange * projection;
}

float normalizedLogDepth(float viewDepth, const glm::vec4 &logDepth)
{
    if (logDepth.x < 0.5f)
        return viewDepth;

    const float nearDepth = std::max(logDepth.y, 1.0e-6f);
    const float depth = std::max(viewDepth, nearDepth);
    const float numerator = std::log2(std::max(depth / nearDepth, 1.0f));
    const float denominator =
        std::log2(std::max(logDepth.z / nearDepth, 1.000001f));
    return std::clamp(numerator / denominator, 0.0f, 1.0f);
}

void appendLineQuad(std::vector<PrimVertex> &out,
                    const glm::vec3 &viewStart, const glm::vec3 &viewEnd,
                    const glm::vec4 &color, float lineWidth,
                    const glm::mat4 &projection, const glm::vec4 &logDepth)
{
    const glm::vec4 startClip = projection * glm::vec4(viewStart, 1.0f);
    const glm::vec4 endClip = projection * glm::vec4(viewEnd, 1.0f);
    if (startClip.w <= 0.0f || endClip.w <= 0.0f)
        return;

    const glm::vec2 startNdc(startClip.x / startClip.w, startClip.y / startClip.w);
    const glm::vec2 endNdc(endClip.x / endClip.w, endClip.y / endClip.w);
    glm::vec2 direction = endNdc - startNdc;
    if (glm::dot(direction, direction) < 1.0e-18f)
        return;
    direction = glm::normalize(direction);
    glm::vec2 perpendicular(-direction.y, direction.x);
    const glm::vec2 halfWidth = 0.5f * lineWidth * perpendicular;

    auto depth = [&](const glm::vec3 &viewPosition, const glm::vec4 &clip) {
        if (logDepth.x > 0.5f)
            return normalizedLogDepth(std::max(-viewPosition.z, logDepth.y), logDepth);
        return std::clamp(clip.z / clip.w, 0.0f, 1.0f);
    };

    const float startDepth = depth(viewStart, startClip);
    const float endDepth = depth(viewEnd, endClip);
    const auto vertex = [](const glm::vec3 &position,
                           const glm::vec4 &vertexColor) {
        return PrimVertex{position, vertexColor, glm::vec2(0.0f, 0.5f)};
    };
    const glm::vec3 a(startNdc - halfWidth, startDepth);
    const glm::vec3 b(startNdc + halfWidth, startDepth);
    const glm::vec3 c(endNdc + halfWidth, endDepth);
    const glm::vec3 d(endNdc - halfWidth, endDepth);
    out.push_back(vertex(a, color));
    out.push_back(vertex(b, color));
    out.push_back(vertex(c, color));
    out.push_back(vertex(a, color));
    out.push_back(vertex(c, color));
    out.push_back(vertex(d, color));
}

void appendPointQuad(std::vector<WebGpuRenderer::PointVertex> &out,
                     const glm::vec3 &ndcPosition, const glm::vec4 &color,
                     const glm::vec2 &pixelSizeNdc)
{
    const auto vertex = [&](const glm::vec2 &uv) {
        return WebGpuRenderer::PointVertex{
            glm::vec3(ndcPosition.x + uv.x * pixelSizeNdc.x,
                      ndcPosition.y + uv.y * pixelSizeNdc.y,
                      ndcPosition.z),
            color, uv};
    };
    out.push_back(vertex(glm::vec2(-1.0f, -1.0f)));
    out.push_back(vertex(glm::vec2(1.0f, -1.0f)));
    out.push_back(vertex(glm::vec2(1.0f, 1.0f)));
    out.push_back(vertex(glm::vec2(-1.0f, -1.0f)));
    out.push_back(vertex(glm::vec2(1.0f, 1.0f)));
    out.push_back(vertex(glm::vec2(-1.0f, 1.0f)));
}

WGPUVertexAttribute makeAttribute(uint32_t location, WGPUVertexFormat format,
                                  uint64_t offset)
{
    WGPUVertexAttribute attribute{};
    attribute.format = format;
    attribute.offset = offset;
    attribute.shaderLocation = location;
    return attribute;
}

MeshInstance cubeInstance(const CubeRenderData &data)
{
    const glm::mat3 transform(data.model);
    const glm::vec3 translation =
        (data.object.high - data.eye.high) +
        (data.object.low - data.eye.low);
    MeshInstance result{};
    result.transformColumn0 = glm::vec4(transform[0], data.objectColor.r);
    result.transformColumn1 = glm::vec4(transform[1], data.objectColor.g);
    result.transformColumn2 = glm::vec4(transform[2], data.objectColor.b);
    result.positionHigh = glm::vec4(translation, data.opacity);
    return result;
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

WGPUBuffer WebGpuRenderer::createBuffer(uint64_t size, WGPUBufferUsage usage)
{
    if (!m_runtime || !m_device || size == 0)
        return nullptr;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    WGPUBufferDescriptor descriptor{};
    descriptor.usage = usage;
    descriptor.size = size;
    descriptor.mappedAtCreation = WGPU_FALSE;
    return procs->deviceCreateBuffer(m_device, &descriptor);
}

bool WebGpuRenderer::ensureBuffer(WGPUBuffer *buffer, uint64_t required,
                                  WGPUBufferUsage usage)
{
    if (!buffer)
        return false;
    if (*buffer && required <= m_bufferSizes[*buffer])
        return true;

    const uint64_t nextSize = std::max<uint64_t>(1024 * 1024, required);
    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    if (*buffer && procs->bufferRelease)
    {
        m_bufferSizes.erase(*buffer);
        procs->bufferRelease(*buffer);
    }
    *buffer = createBuffer(nextSize, usage);
    if (!*buffer)
    {
        std::cerr << "Failed to allocate a WebGPU buffer." << std::endl;
        return false;
    }
    m_bufferSizes[*buffer] = nextSize;
    return true;
}

bool WebGpuRenderer::createSceneResources()
{
    if (!m_runtime || !m_device || m_resourcesCreated)
        return m_resourcesCreated;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    bool ok = true;
    for (uint32_t mesh = 0; mesh < m_meshVertexBuffers.size(); ++mesh)
    {
        const auto &vertices = proceduralMeshVertices(static_cast<MeshType>(mesh));
        const uint64_t size = vertices.size() * sizeof(float);
        ok = ensureBuffer(&m_meshVertexBuffers[mesh], size,
                          WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
        if (ok && vertices.data())
            procs->queueWriteBuffer(m_queue, m_meshVertexBuffers[mesh], 0,
                                    vertices.data(), size);
    }

    ok = ensureBuffer(&m_uniformBuffer, sizeof(DrawUniform) * 256,
                      WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_instanceBuffer, sizeof(MeshInstance) * 1024,
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_lineBuffer, sizeof(PrimVertex) * 8192,
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_fillBuffer, sizeof(FillVertex) * 8192,
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_pointBuffer, sizeof(PointVertex) * 1024,
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    if (!ok || !procs->deviceCreateBindGroupLayout || !procs->deviceCreatePipelineLayout ||
        !procs->deviceCreateShaderModule || !procs->deviceCreateRenderPipeline ||
        !procs->deviceCreateBindGroup)
    {
        std::cerr << "The WebGPU runtime lacks scene-pipeline exports." << std::endl;
        return false;
    }

    WGPUBindGroupLayoutEntry uniformEntry{};
    uniformEntry.binding = 0;
    uniformEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    uniformEntry.buffer.type = WGPUBufferBindingType_Uniform;
    uniformEntry.buffer.hasDynamicOffset = WGPU_TRUE;
    uniformEntry.buffer.minBindingSize = sizeof(DrawUniform);
    WGPUBindGroupLayoutDescriptor layoutDescriptor{};
    layoutDescriptor.entryCount = 1;
    layoutDescriptor.entries = &uniformEntry;
    m_bindGroupLayout =
        procs->deviceCreateBindGroupLayout(m_device, &layoutDescriptor);
    WGPUBindGroupLayoutEntry bufferEntry{};
    bufferEntry.binding = 1;
    bufferEntry.visibility = WGPUShaderStage_Vertex;
    bufferEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    if (!m_bindGroupLayout)
        return false;
    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor{};
    pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
    pipelineLayoutDescriptor.bindGroupLayouts = &m_bindGroupLayout;
    WGPUPipelineLayout pipelineLayout =
        procs->deviceCreatePipelineLayout(m_device, &pipelineLayoutDescriptor);
    if (!pipelineLayout)
        return false;

    WGPUShaderSourceWGSL shaderSource{};
    shaderSource.chain.next = nullptr;
    shaderSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderSource.code.data = kSceneShader;
    shaderSource.code.length = std::strlen(kSceneShader);
    WGPUShaderModuleDescriptor shaderDescriptor{};
    shaderDescriptor.nextInChain = &shaderSource.chain;
    WGPUShaderModule shaderModule =
        procs->deviceCreateShaderModule(m_device, &shaderDescriptor);
    if (!shaderModule)
    {
        procs->pipelineLayoutRelease(pipelineLayout);
        return false;
    }

    WGPUBlendState blendState{};
    WGPUBlendComponent blendComponent{};
    blendComponent.operation = WGPUBlendOperation_Add;
    blendComponent.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendComponent.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color = blendComponent;
    blendState.alpha = blendComponent;

    WGPUColorTargetState colorTarget{};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUDepthStencilState depthState{};
    depthState.format = WGPUTextureFormat_Depth24Plus;
    depthState.depthWriteEnabled = WGPUOptionalBool_True;
    depthState.depthCompare = WGPUCompareFunction_LessEqual;

    std::array<WGPUVertexAttribute, 3> primAttributes{};
    primAttributes[0] = makeAttribute(0, WGPUVertexFormat_Float32x3, 0);
    primAttributes[1] = makeAttribute(1, WGPUVertexFormat_Float32x4, 12);
    primAttributes[2] = makeAttribute(2, WGPUVertexFormat_Float32x2, 28);
    std::array<WGPUVertexAttribute, 2> fillAttributes{};
    fillAttributes[0] = makeAttribute(0, WGPUVertexFormat_Float32x3, 0);
    fillAttributes[1] = makeAttribute(1, WGPUVertexFormat_Float32x4, 12);
    std::array<WGPUVertexAttribute, 3> pointAttributes{};
    pointAttributes[0] = makeAttribute(0, WGPUVertexFormat_Float32x3, 0);
    pointAttributes[1] = makeAttribute(1, WGPUVertexFormat_Float32x4, 12);
    pointAttributes[2] = makeAttribute(2, WGPUVertexFormat_Float32x2, 28);

    auto makePipeline = [&](const char *vs, const char *fs,
                            WGPUPrimitiveTopology topology,
                            uint64_t vertexStride,
                            const WGPUVertexAttribute *vertexAttributes,
                            size_t vertexAttributeCount) {
        WGPUVertexBufferLayout vertexLayouts[2]{};
        std::array<WGPUVertexAttribute, 3> meshAttributes{};
        meshAttributes[0] = makeAttribute(0, WGPUVertexFormat_Float32x3, 0);
        meshAttributes[1] = makeAttribute(1, WGPUVertexFormat_Float32x3, 12);
        meshAttributes[2] = makeAttribute(2, WGPUVertexFormat_Float32x2, 24);
        std::array<WGPUVertexAttribute, 5> instanceAttributes{};
        instanceAttributes[0] = makeAttribute(5, WGPUVertexFormat_Float32x4, 0);
        instanceAttributes[1] = makeAttribute(6, WGPUVertexFormat_Float32x4, 16);
        instanceAttributes[2] = makeAttribute(7, WGPUVertexFormat_Float32x4, 32);
        instanceAttributes[3] = makeAttribute(8, WGPUVertexFormat_Float32x4, 48);
        instanceAttributes[4] = makeAttribute(9, WGPUVertexFormat_Float32x4, 64);
        meshAttributes[3] = makeAttribute(5, WGPUVertexFormat_Float32x4, 0);
        meshAttributes[4] = makeAttribute(6, WGPUVertexFormat_Float32x4, 16);
        const bool hasMeshInstances = vertexStride == 32;
        const WGPUVertexAttribute *attributes =
            hasMeshInstances ? meshAttributes.data() : vertexAttributes;

        vertexLayouts[0].stepMode = WGPUVertexStepMode_Vertex;
        vertexLayouts[0].arrayStride = vertexStride;
        vertexLayouts[0].attributeCount = vertexAttributeCount;
        vertexLayouts[0].attributes = attributes;
        vertexLayouts[1].stepMode = WGPUVertexStepMode_Instance;
        vertexLayouts[1].arrayStride = 80;
        vertexLayouts[1].attributeCount = 5;
        vertexLayouts[1].attributes = instanceAttributes.data();

        WGPUFragmentState fragmentState{};
        fragmentState.module = shaderModule;
        fragmentState.entryPoint.data = fs;
        fragmentState.entryPoint.length = std::strlen(fs);
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;

        WGPURenderPipelineDescriptor descriptor{};
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint.data = vs;
        descriptor.vertex.entryPoint.length = std::strlen(vs);
        descriptor.vertex.bufferCount = hasMeshInstances ? 2 : 1;
        descriptor.vertex.buffers = vertexLayouts;
        descriptor.primitive.topology = topology;
        descriptor.primitive.frontFace = WGPUFrontFace_CCW;
        descriptor.primitive.cullMode = WGPUCullMode_None;
        descriptor.depthStencil = &depthState;
        descriptor.multisample.count = 1;
        descriptor.fragment = &fragmentState;
        WGPURenderPipeline pipeline =
            procs->deviceCreateRenderPipeline(m_device, &descriptor);
        return pipeline;
    };

    m_pipelines[static_cast<size_t>(Pipeline::Mesh)] =
        makePipeline("vs_mesh", "fs_mesh", WGPUPrimitiveTopology_TriangleList,
                     32, nullptr, 3);
    m_pipelines[static_cast<size_t>(Pipeline::Ribbon)] =
        makePipeline("vs_ribbon", "fs_ribbon", WGPUPrimitiveTopology_TriangleList,
                     sizeof(PrimVertex), primAttributes.data(), 3);
    m_pipelines[static_cast<size_t>(Pipeline::Line)] =
        makePipeline("vs_line", "fs_line", WGPUPrimitiveTopology_TriangleList,
                     sizeof(PrimVertex), primAttributes.data(), 3);
    m_pipelines[static_cast<size_t>(Pipeline::Fill)] =
        makePipeline("vs_fill", "fs_fill", WGPUPrimitiveTopology_TriangleList,
                     sizeof(FillVertex), fillAttributes.data(), 2);
    m_pipelines[static_cast<size_t>(Pipeline::Point)] =
        makePipeline("vs_point", "fs_point", WGPUPrimitiveTopology_TriangleList,
                     sizeof(PointVertex), pointAttributes.data(), 3);
    m_pipelines[static_cast<size_t>(Pipeline::Grid)] = m_pipelines[static_cast<size_t>(Pipeline::Line)];

    WGPUBindGroupEntry bindEntry{};
    bindEntry.binding = 0;
    bindEntry.buffer = m_uniformBuffer;
    bindEntry.offset = 0;
    bindEntry.size = sizeof(DrawUniform);
    WGPUBindGroupDescriptor bindDescriptor{};
    bindDescriptor.layout = m_bindGroupLayout;
    bindDescriptor.entryCount = 1;
    bindDescriptor.entries = &bindEntry;
    m_bindGroup = procs->deviceCreateBindGroup(m_device, &bindDescriptor);

    if (shaderModule && procs->shaderModuleRelease)
        procs->shaderModuleRelease(shaderModule);
    if (pipelineLayout && procs->pipelineLayoutRelease)
        procs->pipelineLayoutRelease(pipelineLayout);

    m_resourcesCreated = m_bindGroup &&
        std::all_of(m_pipelines.begin(), m_pipelines.end(),
                    [](WGPURenderPipeline pipeline) { return pipeline != nullptr; });
    if (!m_resourcesCreated)
        std::cerr << "Failed to create one or more WebGPU scene pipelines." << std::endl;
    return m_resourcesCreated;
}

void WebGpuRenderer::destroySceneResources()
{
    if (!m_runtime)
        return;
    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    auto release = [this, procs](WGPUBuffer &buffer) {
        if (buffer && procs->bufferRelease)
            procs->bufferRelease(buffer);
        m_bufferSizes.erase(buffer);
        buffer = nullptr;
    };
    if (m_bindGroup && procs->bindGroupRelease)
        procs->bindGroupRelease(m_bindGroup);
    m_bindGroup = nullptr;
    if (m_bindGroupLayout && procs->bindGroupLayoutRelease)
        procs->bindGroupLayoutRelease(m_bindGroupLayout);
    m_bindGroupLayout = nullptr;
    for (uint32_t index = 0; index < 5; ++index)
    {
        if (m_pipelines[index] && procs->renderPipelineRelease)
            procs->renderPipelineRelease(m_pipelines[index]);
        m_pipelines[index] = nullptr;
    }
    release(m_uniformBuffer);
    for (WGPUBuffer &buffer : m_meshVertexBuffers)
        release(buffer);
    release(m_instanceBuffer);
    release(m_lineBuffer);
    release(m_fillBuffer);
    release(m_pointBuffer);
    if (m_depthView && procs->textureViewRelease)
        procs->textureViewRelease(m_depthView);
    if (m_depthTexture && procs->textureRelease)
        procs->textureRelease(m_depthTexture);
    m_depthView = nullptr;
    m_depthTexture = nullptr;
    m_bufferSizes.clear();
    m_resourcesCreated = false;
}

void WebGpuRenderer::destroyWebGpuObjects()
{
    if (!m_runtime)
        return;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
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
        !procs->surfaceConfigure || !procs->surfaceGetCurrentTexture ||
        !procs->surfacePresent || !procs->deviceGetQueue ||
        !procs->deviceCreateCommandEncoder ||
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
    procs->instanceRequestAdapter(instance, &adapterOptions, adapterCallbackInfo);
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
    procs->adapterRequestDevice(m_adapter, &deviceDescriptor, deviceCallbackInfo);
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
    if (!createSceneResources())
    {
        shutdown();
        return false;
    }
    m_initialized = true;
    std::cout << "WebGPU adapter/device initialized." << std::endl;
    return true;
}

void WebGpuRenderer::shutdown()
{
    destroySceneResources();
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
    if (!m_surfaceConfigured || nextWidth != m_width || nextHeight != m_height)
        configureSurface(nextWidth, nextHeight);
    m_clearColor = clearColor;
    m_uniformStaging.clear();
    m_instanceStaging.clear();
    m_lineStaging.clear();
    m_fillStaging.clear();
    m_pointStaging.clear();
    m_drawCalls.clear();
    return m_surfaceConfigured && m_resourcesCreated;
}

void WebGpuRenderer::recordDraw(const DrawUniform &uniform, Pipeline pipeline,
                                uint32_t count, uint32_t meshStart,
                                uint32_t instanceStart, uint32_t lineStart,
                                uint32_t fillStart, uint32_t pointStart)
{
    const uint64_t alignedOffset = alignUniformOffset(m_uniformStaging.size());
    const uint32_t offset = static_cast<uint32_t>(alignedOffset);
    m_uniformStaging.resize(offset + sizeof(DrawUniform), 0);
    std::memcpy(m_uniformStaging.data() + offset, &uniform, sizeof(DrawUniform));
    DrawCall call{};
    call.pipeline = pipeline;
    call.count = count;
    call.meshVertexStart = meshStart;
    call.instanceStart = instanceStart;
    call.lineStart = lineStart;
    call.fillStart = fillStart;
    call.pointStart = pointStart;
    call.uniformOffset = offset;
    m_drawCalls.push_back(call);
}

void WebGpuRenderer::appendMeshCall(const MeshInstancesRenderData &data,
                                    const glm::mat4 &view,
                                    const glm::mat4 &projection,
                                    const glm::vec4 &logDepth,
                                    uint32_t instanceCount)
{
    const uint32_t instanceStart =
        static_cast<uint32_t>(m_instanceStaging.size());
    m_instanceStaging.insert(m_instanceStaging.end(), data.instances,
                             data.instances + instanceCount);
    const uint32_t meshVertexCount =
        static_cast<uint32_t>(proceduralMeshVertices(data.mesh).size() /
                              kProceduralMeshFloatStride);
    DrawUniform uniform{};
    uniform.view = view;
    uniform.projection = projectionForWebGpu(projection);
    uniform.logDepth = logDepth;
    uniform.style = glm::vec4(
        data.opaque || m_renderMode.mode() == RenderMode::DepthBuffer ? 1.0f : 0.0f,
        data.headlight, data.triplanarUv, 0.0f);
    recordDraw(uniform, Pipeline::Mesh, instanceCount,
               static_cast<uint32_t>(data.mesh), instanceStart);
    m_drawCalls.back().meshVertexCount = meshVertexCount;
}

void WebGpuRenderer::endFrame()
{
    if (!m_initialized || !m_surfaceConfigured || !m_resourcesCreated)
        return;

    auto *procs = static_cast<const WebGpuProcs *>(m_runtime);
    bool ok = ensureBuffer(&m_uniformBuffer,
                           m_uniformStaging.size(),
                           WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    ok = ensureBuffer(&m_instanceBuffer,
                      m_instanceStaging.size() * sizeof(MeshInstance),
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_lineBuffer, m_lineStaging.size() * sizeof(PrimVertex),
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_fillBuffer, m_fillStaging.size() * sizeof(FillVertex),
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    ok = ensureBuffer(&m_pointBuffer,
                      m_pointStaging.size() * sizeof(PointVertex),
                      WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst) && ok;
    if (!ok || !procs->queueWriteBuffer || !procs->renderPassEncoderSetPipeline)
        return;

    if (!m_uniformStaging.empty())
        procs->queueWriteBuffer(m_queue, m_uniformBuffer, 0,
                                m_uniformStaging.data(), m_uniformStaging.size());
    if (!m_instanceStaging.empty())
        procs->queueWriteBuffer(m_queue, m_instanceBuffer, 0,
                                m_instanceStaging.data(),
                                m_instanceStaging.size() * sizeof(MeshInstance));
    if (!m_lineStaging.empty())
        procs->queueWriteBuffer(m_queue, m_lineBuffer, 0, m_lineStaging.data(),
                                m_lineStaging.size() * sizeof(PrimVertex));
    if (!m_fillStaging.empty())
        procs->queueWriteBuffer(m_queue, m_fillBuffer, 0, m_fillStaging.data(),
                                m_fillStaging.size() * sizeof(FillVertex));
    if (!m_pointStaging.empty())
        procs->queueWriteBuffer(m_queue, m_pointBuffer, 0, m_pointStaging.data(),
                                m_pointStaging.size() * sizeof(PointVertex));

    WGPUBindGroupEntry bindEntry{};
    bindEntry.binding = 0;
    bindEntry.buffer = m_uniformBuffer;
    bindEntry.offset = 0;
    bindEntry.size = sizeof(DrawUniform);
    WGPUBindGroupDescriptor bindDescriptor{};
    bindDescriptor.layout = m_bindGroupLayout;
    bindDescriptor.entryCount = 1;
    bindDescriptor.entries = &bindEntry;
    if (m_bindGroup && procs->bindGroupRelease)
        procs->bindGroupRelease(m_bindGroup);
    m_bindGroup = procs->deviceCreateBindGroup(m_device, &bindDescriptor);
    if (!m_bindGroup)
        return;

    if (m_depthView && procs->textureViewRelease)
        procs->textureViewRelease(m_depthView);
    if (m_depthTexture && procs->textureRelease)
        procs->textureRelease(m_depthTexture);
    WGPUTextureDescriptor depthDescriptor{};
    depthDescriptor.usage = WGPUTextureUsage_RenderAttachment;
    depthDescriptor.dimension = WGPUTextureDimension_2D;
    depthDescriptor.size = {m_width, m_height, 1};
    depthDescriptor.format = WGPUTextureFormat_Depth24Plus;
    depthDescriptor.mipLevelCount = 1;
    depthDescriptor.sampleCount = 1;
    m_depthTexture = procs->deviceCreateTexture(m_device, &depthDescriptor);
    m_depthView = m_depthTexture ? procs->textureCreateView(m_depthTexture, nullptr)
                                 : nullptr;

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
    WGPURenderPassDepthStencilAttachment depthAttachment{};
    depthAttachment.view = m_depthView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
    WGPURenderPassDescriptor renderPass{};
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorAttachment;
    renderPass.depthStencilAttachment = m_depthView ? &depthAttachment : nullptr;

    WGPUCommandEncoder encoder =
        procs->deviceCreateCommandEncoder(m_device, nullptr);
    WGPURenderPassEncoder pass =
        encoder ? procs->commandEncoderBeginRenderPass(encoder, &renderPass)
                : nullptr;
    if (pass && procs->renderPassEncoderSetBindGroup &&
        procs->renderPassEncoderSetVertexBuffer && procs->renderPassEncoderDraw)
    {
        for (const DrawCall &call : m_drawCalls)
        {
            WGPURenderPipeline pipeline =
                m_pipelines[static_cast<size_t>(call.pipeline)];
            if (!pipeline)
                continue;
            const uint32_t dynamicOffset = call.uniformOffset;
            procs->renderPassEncoderSetPipeline(pass, pipeline);
            procs->renderPassEncoderSetBindGroup(pass, 0, m_bindGroup, 1,
                                                 &dynamicOffset);
            if (call.pipeline == Pipeline::Mesh)
            {
                procs->renderPassEncoderSetVertexBuffer(
                    pass, 0, m_meshVertexBuffers[call.meshVertexStart], 0,
                    kWgpuWholeSize);
                procs->renderPassEncoderSetVertexBuffer(
                    pass, 1, m_instanceBuffer,
                    call.instanceStart * sizeof(MeshInstance), kWgpuWholeSize);
                procs->renderPassEncoderDraw(pass, call.meshVertexCount,
                                             call.count, 0, 0);
            }
            else if (call.pipeline == Pipeline::Ribbon ||
                     call.pipeline == Pipeline::Line ||
                     call.pipeline == Pipeline::Grid)
            {
                procs->renderPassEncoderSetVertexBuffer(
                    pass, 0, m_lineBuffer,
                    call.lineStart * sizeof(PrimVertex), kWgpuWholeSize);
                procs->renderPassEncoderDraw(pass, call.count, 1, 0, 0);
            }
            else if (call.pipeline == Pipeline::Fill)
            {
                procs->renderPassEncoderSetVertexBuffer(
                    pass, 0, m_fillBuffer,
                    call.fillStart * sizeof(FillVertex), kWgpuWholeSize);
                procs->renderPassEncoderDraw(pass, call.count, 1, 0, 0);
            }
            else if (call.pipeline == Pipeline::Point)
            {
                procs->renderPassEncoderSetVertexBuffer(
                    pass, 0, m_pointBuffer,
                    call.pointStart * sizeof(PointVertex), kWgpuWholeSize);
                procs->renderPassEncoderDraw(pass, call.count, 1, 0, 0);
            }
        }
        procs->renderPassEncoderEnd(pass);
        procs->renderPassEncoderRelease(pass);
    }
    else if (pass)
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

void WebGpuRenderer::drawGrid(const GridRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || data.step <= 0.0f)
        return;

    const glm::mat4 projection = projectionForWebGpu(data.projection);
    const uint32_t start = static_cast<uint32_t>(m_lineStaging.size());
    const glm::vec3 center = data.orthoPlaneValid > 0.5f
                                 ? data.orthoPlaneCenter
                                 : data.planeOriginRelative;
    const glm::vec3 axisU = glm::normalize(data.planeTangentU);
    const glm::vec3 axisV = glm::normalize(data.planeTangentV);
    const int lineCount = 48;
    const float extent = std::max({data.screenWidth, data.screenHeight,
                                   64.0f}) * data.step;
    const glm::vec4 lineColor(data.gridColorMajor, data.gridOpacity);
    const glm::vec4 axisColorU(data.axisColorU, data.gridOpacity);
    const glm::vec4 axisColorV(data.axisColorV, data.gridOpacity);
    for (int index = -lineCount; index <= lineCount; ++index)
    {
        const float offset = static_cast<float>(index) * data.step;
        const glm::vec4 color = index == 0 ? axisColorU : lineColor;
        appendLineQuad(m_lineStaging,
                       center + axisU * offset - axisV * extent,
                       center + axisU * offset + axisV * extent,
                       color, 1.0f, projection, data.logDepth);
        const glm::vec4 vColor = index == 0 ? axisColorV : lineColor;
        appendLineQuad(m_lineStaging,
                       center + axisV * offset - axisU * extent,
                       center + axisV * offset + axisU * extent,
                       vColor, 1.0f, projection, data.logDepth);
    }
    const uint32_t count = static_cast<uint32_t>(m_lineStaging.size() - start);
    if (!count)
        return;

    DrawUniform uniform{};
    uniform.view = data.view;
    uniform.projection = projection;
    uniform.logDepth = data.logDepth;
    uniform.style = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    recordDraw(uniform, Pipeline::Grid, count, 0, 0, start);
}

void WebGpuRenderer::drawCube(const CubeRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated ||
        !m_renderMode.flags().meshFill || data.opacity <= 0.0f)
        return;

    MeshInstancesRenderData renderData{};
    renderData.view = data.view;
    renderData.projection = data.projection;
    renderData.mesh = data.mesh;
    renderData.opaque = data.opacity >= 0.999f;
    renderData.logDepth = data.logDepth;
    renderData.eye = data.eye;
    MeshInstance instance = cubeInstance(data);
    renderData.instances = &instance;
    renderData.instanceCount = 1;
    appendMeshCall(renderData, data.view, data.projection, data.logDepth, 1);
}

void WebGpuRenderer::drawMeshInstances(const MeshInstancesRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || !data.instances ||
        data.instanceCount == 0 || !m_renderMode.flags().meshFill)
        return;
    appendMeshCall(data, data.view, data.projection, data.logDepth,
                   data.instanceCount);
}

void WebGpuRenderer::drawAabb(const AabbRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || data.opacity <= 0.0f)
        return;

    const glm::mat4 projection = projectionForWebGpu(data.projection);
    const uint32_t start = static_cast<uint32_t>(m_lineStaging.size());
    const glm::vec3 minimum = glm::min(data.relativeMin, data.relativeMax);
    const glm::vec3 maximum = glm::max(data.relativeMin, data.relativeMax);
    const std::array<glm::vec3, 8> corners{
        glm::vec3(minimum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, maximum.y, maximum.z),
        glm::vec3(minimum.x, maximum.y, maximum.z),
    };
    const std::array<uint32_t, 24> edges{0,1,1,2,2,3,3,0,
                                         4,5,5,6,6,7,7,4,
                                         0,4,1,5,2,6,3,7};
    const glm::vec4 color(data.color, data.opacity);
    for (size_t index = 0; index < edges.size(); index += 2)
    {
        const glm::vec3 viewStart = data.view *
            glm::vec4(corners[edges[index]], 1.0f);
        const glm::vec3 viewEnd = data.view *
            glm::vec4(corners[edges[index + 1]], 1.0f);
        appendLineQuad(m_lineStaging, viewStart, viewEnd, color, 1.5f,
                       projection, data.logDepth);
    }
    const uint32_t count = static_cast<uint32_t>(m_lineStaging.size() - start);
    if (!count)
        return;

    DrawUniform uniform{};
    uniform.view = data.view;
    uniform.projection = projection;
    uniform.logDepth = data.logDepth;
    uniform.style = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    recordDraw(uniform, Pipeline::Line, count, 0, 0, start);
}

void WebGpuRenderer::drawWorldLine(const WorldLineRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || data.opacity <= 0.0f)
        return;

    const uint32_t start = static_cast<uint32_t>(m_lineStaging.size());
    appendLineQuad(m_lineStaging, data.viewStart, data.viewEnd,
                   glm::vec4(data.color, data.opacity),
                   std::max(1.0f, data.lineWidth),
                   projectionForWebGpu(data.projection), data.logDepth);
    const uint32_t count = static_cast<uint32_t>(m_lineStaging.size() - start);
    if (!count)
        return;

    DrawUniform uniform{};
    uniform.projection = projectionForWebGpu(data.projection);
    uniform.logDepth = data.logDepth;
    uniform.style = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    recordDraw(uniform, Pipeline::Line, count, 0, 0, start);
}

void WebGpuRenderer::drawTargetPoint(const TargetPointRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated)
        return;
    const glm::mat4 projection = projectionForWebGpu(data.projection);
    const glm::vec4 viewPosition =
        data.view * glm::vec4(data.relativePosition, 1.0f);
    const glm::vec4 clip = projection * viewPosition;
    if (clip.w <= 0.0f)
        return;

    float depth = clip.z / clip.w;
    if (data.isOrtho <= 0.5f && data.logDepth.x > 0.5f)
    {
        const float biasWorld = std::max(0.5f * data.pixelSizeWorld, 0.01f);
        depth = normalizedLogDepth(
            std::max(-viewPosition.z - biasWorld, data.logDepth.y),
            data.logDepth);
    }
    const uint32_t start = static_cast<uint32_t>(m_pointStaging.size());
    appendPointQuad(
        m_pointStaging, glm::vec3(clip.x / clip.w, clip.y / clip.w, depth),
        glm::vec4(data.color, 1.0f),
        glm::vec2(data.pointSize * 2.0f / float(m_width),
                  data.pointSize * 2.0f / float(m_height)));
    DrawUniform uniform{};
    uniform.logDepth = data.logDepth;
    recordDraw(uniform, Pipeline::Point, 6, 0, 0, 0, 0, start);
}

void WebGpuRenderer::drawTargetPointInstances(
    const TargetPointInstancesRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || !data.instances ||
        data.instanceCount == 0)
        return;

    const glm::mat4 projection = projectionForWebGpu(data.projection);
    const uint32_t start = static_cast<uint32_t>(m_pointStaging.size());
    for (uint32_t index = 0; index < data.instanceCount; ++index)
    {
        const TargetPointInstance &input = data.instances[index];
        const glm::vec4 viewPosition =
            data.view * glm::vec4(input.relativePosition, 1.0f);
        const glm::vec4 clip = projection * viewPosition;
        if (clip.w <= 0.0f)
            continue;
        float depth = clip.z / clip.w;
        if (data.isOrtho <= 0.5f && data.logDepth.x > 0.5f)
        {
            const float biasWorld = std::max(0.5f * data.pixelSizeWorld, 0.01f);
            depth = normalizedLogDepth(
                std::max(-viewPosition.z - biasWorld, data.logDepth.y),
                data.logDepth);
        }
        const float pointSize = input.pointSize > 0.0f
                                    ? input.pointSize
                                    : data.pointSize;
        appendPointQuad(
            m_pointStaging, glm::vec3(clip.x / clip.w, clip.y / clip.w, depth),
            glm::vec4(input.color, 1.0f),
            glm::vec2(pointSize * 2.0f / float(m_width),
                      pointSize * 2.0f / float(m_height)));
    }
    const uint32_t count = static_cast<uint32_t>(m_pointStaging.size() - start);
    if (!count)
        return;

    DrawUniform uniform{};
    uniform.logDepth = data.logDepth;
    recordDraw(uniform, Pipeline::Point, count, 0, 0, 0, 0, start);
}

void WebGpuRenderer::drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || !data.instances ||
        data.instanceCount == 0)
        return;

    MeshInstancesRenderData renderData{};
    renderData.view = data.view;
    renderData.projection = data.projection;
    renderData.instances = data.instances;
    renderData.instanceCount = data.instanceCount;
    renderData.mesh = data.mesh;
    renderData.opaque = data.transparency <= 0.01f;
    renderData.logDepth = data.logDepth;
    renderData.eye = data.eye;
    appendMeshCall(renderData, data.view, data.projection, data.logDepth,
                   data.instanceCount);
}

void WebGpuRenderer::drawPolylines(const PolylineRenderData &data)
{
    if (!m_initialized || !m_resourcesCreated || !data.vertices ||
        data.vertexCount < 6)
        return;
    const uint32_t start = static_cast<uint32_t>(m_lineStaging.size());
    m_lineStaging.insert(m_lineStaging.end(), data.vertices,
                         data.vertices + data.vertexCount);
    DrawUniform uniform{};
    uniform.view = data.view;
    uniform.projection = projectionForWebGpu(data.projection);
    uniform.logDepth = data.logDepth;
    uniform.style = glm::vec4(data.edgeSoftness, 0.0f, 0.0f, 0.0f);
    recordDraw(uniform, Pipeline::Ribbon, data.vertexCount, 0, 0, start);
}

void WebGpuRenderer::drawFilledTriangles(const FilledTrianglesRenderData &data)
{
    const RenderModeFlags modeFlags = m_renderMode.flags();
    if (!m_initialized || !m_resourcesCreated || !data.vertices ||
        data.vertexCount < 3 ||
        (!data.is3DFace && !modeFlags.show2dSolidFills) ||
        (data.is3DFace && !modeFlags.face3dFill && !modeFlags.hiddenLine))
        return;

    const uint32_t start = static_cast<uint32_t>(m_fillStaging.size());
    m_fillStaging.insert(m_fillStaging.end(), data.vertices,
                         data.vertices + data.vertexCount);
    DrawUniform uniform{};
    uniform.view = data.view;
    uniform.projection = projectionForWebGpu(data.projection);
    uniform.logDepth = data.logDepth;
    uniform.style = glm::vec4(0.0f, data.is3DFace ? 1.0f : 0.0f, 0.0f, 0.0f);
    recordDraw(uniform, Pipeline::Fill, data.vertexCount, 0, 0, 0, start);
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
