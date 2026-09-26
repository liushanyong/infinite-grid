#pragma once

#include "rendering/RendererBackend.h"
#include <webgpu/webgpu.h>

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

    // The shell proves the native WebGPU device, surface, command, and
    // present path. Scene passes are migrated into this backend incrementally.
    void drawGrid(const GridRenderData &) override {}
    void drawCube(const CubeRenderData &) override {}
    void drawMeshInstances(const MeshInstancesRenderData &) override {}
    void drawAabb(const AabbRenderData &) override {}
    void drawWorldLine(const WorldLineRenderData &) override {}
    void drawTargetPoint(const TargetPointRenderData &) override {}
    void drawTargetPointInstances(const TargetPointInstancesRenderData &) override {}
    void drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &) override {}
    void drawPolylines(const PolylineRenderData &) override {}
    void drawFilledTriangles(const FilledTrianglesRenderData &) override {}

    void setRenderMode(RenderMode mode) override;
    RenderMode renderMode() const override;
    RenderModeFlags renderModeFlags() const override;
    uint32_t loadMeshTexture(const std::string &) override { return 0; }
    void setRealisticLights(const RealisticLightsRenderData &) override {}

private:
    void configureSurface(uint32_t width, uint32_t height);
    void destroyWebGpuObjects();

    SDL_Window *m_window = nullptr;
    void *m_runtime = nullptr;
    void *m_runtimeModule = nullptr;
    WGPUInstance m_instance = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUTexture m_surfaceTexture = nullptr;
    AsyncResult m_asyncResult;
    RenderModeManager m_renderMode;
    glm::vec4 m_clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_surfaceConfigured = false;
    bool m_initialized = false;
};

} // namespace rendering
