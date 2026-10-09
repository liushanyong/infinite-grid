#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "acgi/AcGiTextDevice.h"
#include "RenderMode.h"
#include "RenderTypes.h"

// Global namespace: ImGui owns the type; a namespace-scoped forward
// declaration would silently mismatch the override signature.
struct ImDrawData;

namespace rendering
{

enum class BackendType
{
    Bgfx,
    WebGpu,
    WebGpuMigration,
};

enum class GraphicsApi
{
    Auto,
    Direct3D11,
    Direct3D12,
    WebGPU,
    OpenGL,
    Vulkan,
};

class RendererBackend : public acgi::TextDevice
{
public:
    virtual ~RendererBackend() = default;

    virtual const char *name() const = 0;
    // The API that actually owns the swapchain. A migration backend can be
    // selected as WebGPU while reporting the native compatibility API it uses.
    virtual const char *graphicsApiName() const = 0;
    virtual Uint32 windowFlags() const = 0;
    virtual bool configureSDL() = 0;
    virtual bool initialize(SDL_Window *window) = 0;
    virtual void shutdown() = 0;
    virtual bool beginFrame(const glm::vec4 &clearColor) = 0;
    virtual void endFrame() = 0;
    // Displays the finished offscreen frame (offscreen product -> window
    // backbuffer / ImGui panels) and kicks it.  Default no-op: backends
    // that present inside endFrame() ignore the split.
    virtual void compositeFrame() {}
    virtual void present() = 0;
    // ---- ImGui presentation mode (optional capability) ----
    // When active, compositeFrame() clears the backbuffer and lets the
    // host's ImGui draw data own the window: the offscreen scenes reach
    // the screen as panel images (sceneTexture slots).  Backends without
    // ImGui support ignore the flag.
    virtual void setImGuiActive(bool active) { (void)active; }
    virtual void drawImGui(ImDrawData *drawData) { (void)drawData; }
    // Persistent per-viewport final images for ImGui::Image panels:
    // slot 0/1 = the two viewports' last blit, slot 2 = the full-scene
    // GPU ID debug texture.  0 = no texture.
    virtual std::uint32_t sceneTexture(int slot) const { return 0; }
    // Resizes the MAIN scene offscreen target independently of the
    // window (ImGui mode renders each viewport at its panel's own size).
    virtual void setSceneRenderSize(std::uint32_t width,
                                    std::uint32_t height)
    {
        (void)width;
        (void)height;
    }
    // Resizes the full-scene ID debug target independently of the window
    virtual void setGpuPickDebugSize(std::uint32_t width,
                                     std::uint32_t height)
    {
        (void)width;
        (void)height;
    }
    // Selects which viewport the next blit targets (round-robin slot).
    virtual void setActiveSceneSlot(int slot) { (void)slot; }
    virtual void blitSceneToSlot(int slot) { (void)slot; }
    virtual void drawGrid(const GridRenderData &data) = 0;
    virtual void drawCube(const CubeRenderData &data) = 0;
    virtual void drawMeshInstances(const MeshInstancesRenderData &data) = 0;
    virtual void drawAabb(const AabbRenderData &data) = 0;
    virtual void drawTargetPoint(const TargetPointRenderData &data) = 0;
    virtual void drawTargetPointInstances(const TargetPointInstancesRenderData &data) = 0;
    virtual void drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data) = 0;
    virtual void setRenderMode(RenderMode mode) = 0;
    virtual RenderMode renderMode() const = 0;
    virtual RenderModeFlags renderModeFlags() const = 0;
    virtual void drawPolylines(const PolylineRenderData &data) = 0;
    virtual void drawLineInstances(const LineInstancesRenderData &data) = 0;
    virtual void drawFilledTriangles(const FilledTrianglesRenderData &data) = 0;
    virtual void drawCurves(const CurveRenderData &data) {}

    // SDF text (lib/text subsystem).  uploadGlyphSdf registers a glyph's
    // R8 distance field and returns a texture id (0 = invalid);
    // drawSdfGlyphQuad submits one 6-vertex quad (view-space, 9 floats
    // per vertex: pos3 + uv2 + rgba4) with that texture.
    virtual uint32_t uploadGlyphSdf(const unsigned char *, int, int) override
    {
        return 0;
    }
    virtual void drawSdfGlyphQuad(const glm::mat4 &, const glm::mat4 &,
                                  uint32_t, const float *) override
    {
    }
    virtual void requestDebugScreenShot(const std::string &) {}
    virtual void setGpuPickDebugVisible(bool visible) { (void)visible; }
    virtual void setGpuPickSceneDebug(bool visible) { (void)visible; }
    virtual void setGpuPickScenePassEnabled(bool enabled) { (void)enabled; }
    // Full-scene ID debug normalizes against the CPU-assigned id range instead
    // of forcing an asynchronous full-frame readback every frame.
    virtual void setSelectionOutlineId(uint32_t objectId) { (void)objectId; }
    virtual void setSelectionOutlineAll(bool enabled) { (void)enabled; }
    virtual void setGpuPickIdRange(uint32_t minId, uint32_t maxId)
    {
        (void)minId; (void)maxId;
    }

    // Optional asynchronous mesh picking. Calls to queueGpuMeshPick are valid
    // only between requestGpuPick() and the next endFrame().
    // Returns the token for an accepted request, or zero while the previous
    // asynchronous readback is still pending.
    virtual uint32_t requestGpuPick(const GpuPickRequest &request) { return 0; }
    virtual void queueGpuMeshPick(const MeshInstance &instance,
                                  MeshType mesh, uint32_t objectId) {}
    virtual GpuPickQueueStats gpuPickQueueStats() const { return {}; }
    // Triangle vertices are expressed in the coordinate space represented by
    // view. A non-zero geometryKey marks reusable static geometry; transient
    // soups use zero and are copied for the pick request.
    // The fragment color is the entity ID; this lets lines, fills, and point
    // impostors share one small asynchronous ID submission.
    virtual void queueGpuTrianglePick(uint64_t geometryKey,
                                      const FillVertex *vertices,
                                      uint32_t vertexCount,
                                      const glm::mat4 &view,
                                      const glm::mat4 &projection,
                                      const glm::vec4 &logDepth,
                                      uint32_t objectId,
                                      uint8_t occlusionRank = 2) {}
    virtual GpuPickResult pollGpuPick() { return {}; }
    // Unified picking: reads the entity id under |ndc| from the always-on
    // full-scene ID texture instead of re-rendering a 1x1 pass.  Returns a
    // token for pollGpuPick(), or 0 when the pixel pipeline is unavailable.
    virtual uint32_t requestGpuPickPixel(float ndcX, float ndcY)
    {
        (void)ndcX;
        (void)ndcY;
        return 0;
    }
    // Abort an in-flight asynchronous pick (e.g. after a readback timeout)
    // so the renderer stops re-submitting it every frame.
    virtual void cancelGpuPick() {}

    // ---- picture-in-picture secondary scene target (optional) ----
    // SYCAD-style offscreen viewport: beginPipScene rebinds the scene
    // channels into a quarter-size offscreen framebuffer and clears it;
    // the host re-submits content through an inactive view; endPipScene
    // restores the main target; compositePip (every frame) draws the
    // last pip image as a top-right inset over the presented frame.
    // Backends without the capability no-op / return false.
    virtual bool beginPipScene() { return false; }
    virtual void endPipScene() {}
    virtual void compositePip() {}

    // Zero is a renderer-provided white texture; other ids are allocated by
    // the active backend and remain valid until shutdown().
    virtual uint32_t loadMeshTexture(const std::string &path) { return 0; }
    virtual void setRealisticLights(const RealisticLightsRenderData &lights) {}
};

std::unique_ptr<RendererBackend> createRenderer(
    BackendType type, GraphicsApi api = GraphicsApi::Auto);

} // namespace rendering
