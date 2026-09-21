#pragma once

#include "rendering/RendererBackend.h"

#include <bgfx/bgfx.h>
#include <string>

namespace rendering
{

class BgfxRenderer final : public RendererBackend
{
public:
    explicit BgfxRenderer(GraphicsApi api = GraphicsApi::Auto);

    const char *name() const override;
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
    void drawWorldLine(const WorldLineRenderData &data) override;
    void drawTargetPoint(const TargetPointRenderData &data) override;
    void drawTargetPointInstances(const TargetPointInstancesRenderData &data) override;

private:

    bool createRenderResources();

    SDL_Window *m_window = nullptr;
    GraphicsApi m_api = GraphicsApi::Auto;
    bgfx::UniformHandle m_gridInvViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridCamFront = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOrthoPlaneCenter = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOrthoRight = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOrthoUp = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOriginRelative = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridPlaneNormal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridPlaneTangentU = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridPlaneTangentV = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisColorU = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisColorV = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridStartAxisOrigin = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridStartAxisDirection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridStartAxisVisible = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridStartAxisLine = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisOriginGridRelative = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisLineX = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisLineZ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridIsOrtho = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridGroundRelativeY = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridStep = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridAxisVisible = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridScreenHeight = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridScreenWidth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridColorMajor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridColorMinor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOpacity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gridOrthoPlaneValid = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_logDepth = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_gridProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_view = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_projection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeRelativePosition = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeOpacity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeColor = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_cubeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_meshInstanceProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pointInstanceProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cubeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_sphereBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_coneBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_torusBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_aabbBuffer = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_lineStart = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lineEnd = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lineColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lineWidth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lineDepthBias = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_lineProgram = BGFX_INVALID_HANDLE;
    bgfx::DynamicVertexBufferHandle m_lineBuffer = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_pointPosition = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_pointSize = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_pointColor = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pointProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_pointBuffer = BGFX_INVALID_HANDLE;


    // FPS overlay bookkeeping (debug text drawn in endFrame).
    float    m_fps = 0.0f;
    uint32_t m_frameCount = 0;
    uint32_t m_fpsLastTick = 0;

    uint16_t m_width = 0;
    uint16_t m_height = 0;
    bool m_initialized = false;
};

} // namespace rendering
