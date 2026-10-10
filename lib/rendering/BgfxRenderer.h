#pragma once

#include "rendering/RendererBackend.h"
#include "ImGuiBgfxPass.h"

#include <bgfx/bgfx.h>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

namespace rendering
{

class BgfxRenderer final : public RendererBackend
{
public:
    explicit BgfxRenderer(GraphicsApi api = GraphicsApi::Auto);

    const char *name() const override;
    const char *graphicsApiName() const override;
    Uint32 windowFlags() const override;
    bool configureSDL() override;
    bool initialize(SDL_Window *window) override;
    void shutdown() override;
    bool beginFrame(const glm::vec4 &clearColor) override;
    void endFrame() override;
    void present() override;
    float fps() const override { return m_fps; }
    bool beginPipScene() override;
    void endPipScene() override;
    void compositePip() override;
    void drawGrid(const GridRenderData &data) override;
    void drawCube(const CubeRenderData &data) override;
    void drawMeshInstances(const MeshInstancesRenderData &data) override;
    void drawAabb(const AabbRenderData &data) override;
    void drawTargetPoint(const TargetPointRenderData &data) override;
    void drawTargetPointInstances(const TargetPointInstancesRenderData &data) override;
    void drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data) override;
    void setRenderMode(RenderMode mode) override;
    RenderMode renderMode() const override;
    RenderModeFlags renderModeFlags() const override;
    void drawPolylines(const PolylineRenderData &data) override;
    void drawLineInstances(const LineInstancesRenderData &data) override;
    void drawFilledTriangles(const FilledTrianglesRenderData &data) override;

    // SDF text (lib/text subsystem).  uploadGlyphSdf registers a glyph's
    // R8 distance field as a texture and returns its id (0 = invalid);
    // drawSdfGlyphQuad submits one 6-vertex quad (9 floats per vertex:
    // view-space pos3 + uv2 + rgba4) with that texture bound.
    uint32_t uploadGlyphSdf(const unsigned char *sdf, int width, int height);
    void drawSdfGlyphQuad(const glm::mat4 &view, const glm::mat4 &projection,
                          uint32_t textureId, const float *vertices,
                          bool selectionHighlight = false);
    void requestDebugScreenShot(const std::string &filePath) override;
    // Displays the finished offscreen frame: in ImGui mode the scene is
    // blitted into the active viewport's persistent final texture and the
    // ImGui draw data owns the window; otherwise the scene resolves into
    // the backbuffer (debug overlays included) and the frame kicks.
    void compositeFrame() override;
    void setImGuiActive(bool active) override;
    void drawImGui(ImDrawData *drawData) override;
    std::uint32_t sceneTexture(int slot) const override;
    // Resizes the MAIN scene offscreen target independently of the window
    // (ImGui mode renders each viewport at its panel's own size).
    void setSceneRenderSize(std::uint32_t width, std::uint32_t height) override;
    // Resizes the full-scene ID debug target (ImGui ID panels).
    void setGpuPickDebugSize(std::uint32_t width, std::uint32_t height) override;
    // Selects which viewport the next blit targets (round-robin slot).
    void setActiveSceneSlot(int slot) { m_activeSceneSlot = slot; }
    // Copies the scene target into the viewport's persistent final texture
    // (ImGui samples the finals, which keep the last rendered image).
    void blitSceneToSlot(int slot);
    void setGpuPickDebugVisible(bool visible) override;
    void setGpuPickSceneDebug(bool visible) override;
    void setGpuPickScenePassEnabled(bool enabled) override;
    void setGpuPickIdRange(uint32_t minId, uint32_t maxId) override;
    void setSelectionOutlineId(uint32_t objectId) override;
    void setSelectionOutlineAll(bool enabled) override;
    void drawCurves(const CurveRenderData &data) override;
    uint32_t requestGpuPick(const GpuPickRequest &request) override;
    void queueGpuMeshPick(const MeshInstance &instance,
                          MeshType mesh, uint32_t objectId) override;
    GpuPickQueueStats gpuPickQueueStats() const override;
    void queueGpuTrianglePick(uint64_t geometryKey,
                              const FillVertex *vertices,
                              uint32_t vertexCount,
                              const glm::mat4 &view,
                              const glm::mat4 &projection,
                              const glm::vec4 &logDepth,
                              uint32_t objectId,
                              uint8_t occlusionRank = 2) override;
    GpuPickResult pollGpuPick() override;
    // Unified picking: samples the always-on full-scene ID texture at the
    // click texel (no per-click re-render, no candidate filter).
    uint32_t requestGpuPickPixel(float ndcX, float ndcY) override;
    void cancelGpuPick() override;
    uint32_t loadMeshTexture(const std::string &path) override;
    void setRealisticLights(const RealisticLightsRenderData &lights) override;

private:

    bool createRenderResources();
    bool createSceneFrameBuffer();
    bool createSceneFrameBuffer(std::uint32_t width,
                                std::uint32_t height);
    void destroySceneFrameBuffer();
    bool createGpuPickResources();
    void destroyGpuPickResources();
    glm::mat4 gpuPickProjection(const GpuPickRequest &request) const;
    bool gpuPickInstanceIsCandidate(const MeshInstance &instance) const;
    bool gpuPickVerticesAreCandidate(const FillVertex *vertices,
                                     uint32_t vertexCount,
                                     const glm::mat4 &view) const;
    void renderGpuPickPass();
    void completeGpuPickReadback();
    bool createGpuPickDebugResources();
    void destroyGpuPickDebugResources();
    void submitGpuPickPrimitives(bgfx::ViewId view,
                                 const glm::mat4 &projection);
    void renderGpuPickDebugPass(const glm::mat4 &projection);
    void completeGpuPickDebugReadback();
    void setPresentSelectionState(bool enabled);
    void drawEdgeRibbonsForInstances(
        const glm::mat4 &view,
        const glm::mat4 &projection,
        const DoubleSingleVec3 &eye,
        const MeshInstance *instances,
        uint32_t instanceCount,
        MeshType mesh,
        float layer,
        const glm::vec4 &logDepth,
        float edgeHalfWidth,
        float edgeSoftness);

    SDL_Window *m_window = nullptr;
    GraphicsApi m_api = GraphicsApi::Auto;
    bool m_webgpuMigration = false;
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
    bgfx::UniformHandle m_layerOffset = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_gridProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_view = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_projection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_meshEdgeOverride = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeRelativePosition = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeRelativePositionLow = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_eyeHigh = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_eyeLow = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeOpacity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cubeColor = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_cubeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_meshInstanceProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pbrMeshProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pointInstanceProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_cadAlgorithmProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadView = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadProjection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadCameraPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadBaseColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadLightDir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadStyleParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadWireframeColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadStrokeParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_cadFlatShade = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_polylineProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_lineInstanceProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_lineInstanceQuadBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_lineInstanceLayout;
    bgfx::ProgramHandle m_meshEdgeRibbonProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_meshEdgeRibbonParams = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_fillProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_curveProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_curveCP = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_curveKnot = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_curveParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_curveColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_curveArc = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_primParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_depthDisplayParams = BGFX_INVALID_HANDLE;

    // SDF text pass (lib/text subsystem): program + per-glyph R8
    // texture cache (index 0 is reserved as invalid).
    bgfx::ProgramHandle m_textProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_textSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_textParams = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_textLayout;
    std::vector<bgfx::TextureHandle> m_glyphTextures;
    bgfx::UniformHandle m_meshSurface = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_albedoSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_realisticMaterial = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rAmbient = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rDirection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rDirectionColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rPointPositions = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rPointColors = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rParams = BGFX_INVALID_HANDLE;
    RealisticLightsRenderData m_realisticLights;
    bgfx::TextureHandle m_whiteTexture = BGFX_INVALID_HANDLE;
    std::vector<bgfx::TextureHandle> m_meshTextures;
    bgfx::VertexLayout m_polylineLayout;
    bgfx::VertexLayout m_fillLayout;
    bgfx::VertexLayout m_curveLayout;
    bgfx::VertexBufferHandle m_cadCubeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cadSphereBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cadConeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cadTorusBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cubeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_sphereBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_coneBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_torusBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_instanceCubeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cubeEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_sphereEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_coneEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_torusEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_cubeEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_sphereEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_coneEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_torusEdgeRibbonBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_aabbBuffer = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_pointPosition = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_pointSize = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_pointColor = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pointProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_pointBuffer = BGFX_INVALID_HANDLE;

    // The CAD passes render into one explicit MSAA target so every pass shares
    // the same color/depth pair on every backend.
    bgfx::FrameBufferHandle m_sceneFrameBuffer = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_pipFrameBuffer = BGFX_INVALID_HANDLE;
    uint32_t m_pipWidth = 0;
    uint32_t m_pipHeight = 0;
    uint32_t m_clearColorPacked = 0;
    bgfx::ProgramHandle m_presentProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_presentQuadBuffer = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_presentSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_presentIdSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_presentParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_selectionOutlineParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_selectionOutlineColor = BGFX_INVALID_HANDLE;

    bgfx::FrameBufferHandle m_gpuPickFrameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_gpuPickReadback = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_gpuPickProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickCubeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickSphereBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickConeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickTorusBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickCubeEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickSphereEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickConeEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_gpuPickTorusEdgeBuffer = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_gpuPickLayout;
    GpuPickRequest m_gpuPickRequest;
    struct GpuPickPrimitive
    {
        enum class Kind
        {
            Mesh,
            Edge,
            Triangle
        };

        Kind kind = Kind::Mesh;
        MeshInstance meshInstance;
        MeshType meshType = MeshType::Cube;
        uint64_t geometryKey = 0;
        glm::mat4 view;
        glm::mat4 projection;
        uint32_t objectId = 0;
        // 0: opaque blocker, 1: non-depth-writing mesh/edge, 2: CAD overlay,
        // 3: screen-overlay text.
        uint8_t occlusionRank = 2;
        std::vector<FillVertex> transientVertices;
    };
    struct GpuTrianglePickGeometry
    {
        bgfx::VertexBufferHandle buffer = BGFX_INVALID_HANDLE;
        glm::vec3 center{0.0f};
        float radius = 0.0f;
    };
    bool gpuPickCachedVerticesAreCandidate(
        const GpuTrianglePickGeometry &geometry,
        const glm::mat4 &view) const;
    std::vector<GpuPickPrimitive> m_gpuPickPrimitives;
    std::unordered_map<uint64_t, GpuTrianglePickGeometry>
        m_gpuPickTriangleGeometry;
    GpuPickQueueStats m_gpuPickQueueStats;
    GpuPickResult m_gpuPickLastResult;
    std::array<uint8_t, 4> m_gpuPickReadbackData{};
    uint32_t m_gpuPickNextToken = 1;
    bool m_gpuPickActive = false;
    bool m_gpuPickReadPending = false;
    // Unified-pick pixel read state (sample of the full-scene ID texture).
    bool m_pixelReadPending = false;
    float m_pixelReadNdcX = 0.0f;
    float m_pixelReadNdcY = 0.0f;
    uint32_t m_pixelReadToken = 0;
    uint32_t m_gpuPickReadFrame = 0;

    bgfx::FrameBufferHandle m_gpuPickDebugFrameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_gpuPickDebugReadback = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_gpuPickDebugVisualTexture = BGFX_INVALID_HANDLE;
    std::vector<uint8_t> m_gpuPickDebugReadbackData;
    bool m_gpuPickDebugReadPending = false;
    uint32_t m_gpuPickDebugReadFrame = 0;
    // ---- ImGui presentation mode ----
    bool m_imguiActive = false;
    int m_activeSceneSlot = 0;
    ImGuiBgfxPass m_imguiPass;
    // Main scene offscreen target size (window size unless ImGui mode
    // renders a viewport at its panel's own size).
    uint32_t m_sceneW = 0;
    uint32_t m_sceneH = 0;
    bgfx::TextureHandle m_finalTexture[2] = {BGFX_INVALID_HANDLE,
                                             BGFX_INVALID_HANDLE};
    bgfx::FrameBufferHandle m_finalFrameBuffer[2] = {BGFX_INVALID_HANDLE,
                                                     BGFX_INVALID_HANDLE};
    uint32_t m_finalWidth[2] = {0, 0};
    uint32_t m_finalHeight[2] = {0, 0};
    // Full-scene ID debug target wanted size (per-panel ID finals).
    uint32_t m_gpuPickDebugWantW = 0;
    uint32_t m_gpuPickDebugWantH = 0;
    bool m_gpuPickDebugVisible = false;
    uint32_t m_gpuPickDebugMinId = 0u;
    uint32_t m_gpuPickDebugMaxId = 0u;
    bool m_gpuPickSceneDebug = false;
    bool m_gpuPickScenePassEnabled = false;
    bool m_selectionOutlineAll = false;
    uint32_t m_selectionOutlineId = 0;
    uint16_t m_gpuPickDebugWidth = 0;
    uint16_t m_gpuPickDebugHeight = 0;

    // FPS overlay bookkeeping (debug text drawn in endFrame).
    float    m_fps = 0.0f;
    uint32_t m_frame = 0;
    uint32_t m_frameCount = 0;
    uint32_t m_fpsLastTick = 0;

    uint16_t m_width = 0;
    uint16_t m_height = 0;
    RenderModeManager m_renderMode;
    bool m_initialized = false;
};

} // namespace rendering
