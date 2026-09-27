#pragma once

#include "rendering/RendererBackend.h"
#include <webgpu/webgpu.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace rendering
{

struct AsyncResult
{
    void *object = nullptr;
    bool completed = false;
};

bool webGpuRuntimeAvailable();

class WebGpuRenderer final : public RendererBackend
{
public:
    const char *name() const override;
    const char *graphicsApiName() const override;
    Uint32 windowFlags() const override;
    bool configureSDL() override;
    bool initialize(SDL_Window *window) override;
    void shutdown() override;
    bool beginFrame(const glm::vec4 &clearColor) override;
    void endFrame() override;
    void present() override;

    void drawGrid(const GridRenderData &data) override;
    void drawCube(const CubeRenderData &data) override;
    void drawMeshInstances(const MeshInstancesRenderData &data) override;
    void drawAabb(const AabbRenderData &data) override;
    void drawTargetPoint(const TargetPointRenderData &data) override;
    void drawTargetPointInstances(const TargetPointInstancesRenderData &data) override;
    void drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data) override;
    void drawPolylines(const PolylineRenderData &data) override;
    void drawFilledTriangles(const FilledTrianglesRenderData &data) override;

    void setRenderMode(RenderMode mode) override;
    RenderMode renderMode() const override;
    RenderModeFlags renderModeFlags() const override;
    uint32_t loadMeshTexture(const std::string &) override { return 0; }
    void setRealisticLights(const RealisticLightsRenderData &) override {}

private:
    struct DrawUniform
    {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::vec4 style{0.0f};
        glm::vec4 logDepth{0.0f};
        glm::vec4 colorOpacity{1.0f};
        glm::vec4 plane{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 extents{10000.0f, 10000.0f, 1.0f, 0.0f};
        glm::vec4 pad{0.0f};
    };

public:
    struct PointVertex
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 uv;
    };

private:
    enum class Pipeline
    {
        Mesh,
        Ribbon,
        Line,
        Fill,
        Point,
        Grid,
    };

    struct DrawCall
    {
        Pipeline pipeline = Pipeline::Fill;
        uint32_t meshVertexStart = 0;
        uint32_t instanceStart = 0;
        uint32_t lineStart = 0;
        uint32_t fillStart = 0;
        uint32_t pointStart = 0;
        uint32_t count = 0;
        uint32_t meshVertexCount = 0;
        uint32_t uniformOffset = 0;
    };

    void configureSurface(uint32_t width, uint32_t height);
    bool createSceneResources();
    void destroySceneResources();
    void destroyWebGpuObjects();
    WGPUBuffer createBuffer(uint64_t size, WGPUBufferUsage usage);
    bool ensureBuffer(WGPUBuffer *buffer, uint64_t required,
                      WGPUBufferUsage usage);
    void recordDraw(const DrawUniform &uniform, Pipeline pipeline,
                    uint32_t count, uint32_t meshStart = 0,
                    uint32_t instanceStart = 0, uint32_t lineStart = 0,
                    uint32_t fillStart = 0, uint32_t pointStart = 0);
    void appendMeshCall(const MeshInstancesRenderData &data,
                        const glm::mat4 &view, const glm::mat4 &projection,
                        const glm::vec4 &logDepth, uint32_t instanceCount);

    SDL_Window *m_window = nullptr;
    void *m_runtime = nullptr;
    void *m_runtimeModule = nullptr;
    WGPUInstance m_instance = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUTexture m_surfaceTexture = nullptr;
    WGPUTexture m_depthTexture = nullptr;
    WGPUTextureView m_depthView = nullptr;
    WGPUBuffer m_uniformBuffer = nullptr;
    std::array<WGPUBuffer, 4> m_meshVertexBuffers{};
    WGPUBuffer m_instanceBuffer = nullptr;
    WGPUBuffer m_lineBuffer = nullptr;
    WGPUBuffer m_fillBuffer = nullptr;
    WGPUBuffer m_pointBuffer = nullptr;
    WGPUBindGroupLayout m_bindGroupLayout = nullptr;
    WGPUBindGroup m_bindGroup = nullptr;
    std::array<WGPURenderPipeline, 6> m_pipelines{};
    AsyncResult m_asyncResult;
    RenderModeManager m_renderMode;
    glm::vec4 m_clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    std::vector<uint8_t> m_uniformStaging;
    std::vector<MeshInstance> m_instanceStaging;
    std::vector<PrimVertex> m_lineStaging;
    std::vector<FillVertex> m_fillStaging;
    std::vector<PointVertex> m_pointStaging;
    std::vector<DrawCall> m_drawCalls;
    std::unordered_map<WGPUBuffer, uint64_t> m_bufferSizes;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_surfaceConfigured = false;
    bool m_initialized = false;
    bool m_resourcesCreated = false;
};

} // namespace rendering
