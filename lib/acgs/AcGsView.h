#pragma once

// AcGsView — the single app-facing render gateway, modeled on ObjectARX's
// AcGsView + AcGiDefaultContext.  Everything the application draws goes
// through here:
//   * scene::SceneDrawList submissions (AcGiDrawable::collect output) are
//     expanded into the renderer's line-instance / ribbon / fill / point /
//     mesh-instance channels,
//   * queued acgi::TextRequest records are flushed through the text engine,
//   * selection highlighters and transient overlays reuse the shared
//     ribbon / frustum-clip primitives exposed below.
//
// The per-frame viewport state travels in ViewFrameContext instead of the
// former main.cpp globals (camera basis, overlay depth slab, viewport
// rectangle, demo mesh flags); render() fills it once per frame.

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "acgs/AcGsOrbitCamera.h"
#include "rendering/RenderMode.h"
#include "scene/SceneDrawList.h"

namespace entities
{
struct Stroke;
}

namespace rendering
{
struct DoubleSingleVec3;
}

namespace acgs
{

// Background the demo composites against; beginFrame and every contrast
// decision start from it.
constexpr glm::vec4 kClearColor(0.1f, 0.1f, 0.1f, 1.0f);

// Demo compositing layer override (GRID_LINE_LAYER / GRID_FILL_LAYER /
// GRID_MESH_LAYER environment variables; 0 = the default layer).
float envLayer(const char *name);

// Camera-space projection of a world point: lateral coordinates on the
// camera basis plus the +Z-forward depth.  Equivalent to transforming by
// the inverse view matrix but evaluated in double precision — required for
// the rebased large-coordinate scene, where a float mat4 loses precision
// before depth bounds are computed.
struct CameraSpacePoint
{
    double x;
    double y;
    double depth;
};

CameraSpacePoint toCameraSpace(const glm::dvec3 &worldPosition,
                               const glm::dvec3 &cameraPosition,
                               const glm::dvec3 &cameraRight,
                               const glm::dvec3 &cameraUp,
                               const glm::dvec3 &cameraFront);

// AcGiMaterial -> renderer surface-material translation.
rendering::SurfaceMaterial toSurfaceMaterial(
    const scene::AcGiMaterial &material);

// Per-frame viewport state (ObjectARX: AcGsView + AcGsViewportData).
struct ViewFrameContext
{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 overlayProjection{1.0f};
    glm::dvec3 cameraPos{0.0};
    glm::dvec3 cameraRight{1.0, 0.0, 0.0};
    glm::dvec3 cameraUp{0.0, 1.0, 0.0};
    glm::dvec3 cameraFront{0.0, 0.0, -1.0};
    glm::vec4 logDepth{0.0f};

    // Overlay depth slab: the depth range the stroke pass clips against, so
    // infinite entities (Ray/XLine) emit only their visible span.
    double slabNear = 0.0;
    double slabFar = 1.0e9;

    // Viewport rectangle and ortho frame (camera-space clipping).
    int viewportWidth = 1;
    int viewportHeight = 1;
    bool ortho = false;
    double orthoSize = 1.0;
    double orbitDistance = 1.0; // ortho ray flattening target depth

    // Stroke / point sizing.
    float pixelSizeWorld = 0.0f;
    float edgeSoftness = 0.15f;
    float pointSize = 2.0f;

    // Mesh-instance pass demo state.
    std::uint32_t meshTextureIndex = 0;
    float meshHeadlight = 0.0f;
    float meshTriplanar = 0.0f;
};

class AcGsView
{
public:
    static AcGsView &instance();

    // View camera state (ObjectARX: AcGsView::setView/setEye/setTarget).
    // The demo keeps calling it through the orbitCamera() alias.
    AcGsOrbitCamera &orbitCamera() { return orbitCamera_; }
    // Projection toggle; exposed as a mutable reference so the demo's
    // useOrthoProjection() wrapper keeps its existing call sites.
    bool &orthoMode() { return orthoMode_; }

    // ---- depth-slab scheduling (near/far auto-management) ----

    // One independent slab per projection channel: the ortho object pass,
    // the perspective object pass, and the perspective overlay pass.
    enum class DepthSlab
    {
        Ortho,
        Perspective,
        Overlay
    };

    // Re-arm every stabilizer after a view-state change (projection
    // switch, grid-plane change, camera teleport): the next candidate slab
    // is applied immediately instead of waiting out the hysteresis.
    void resetDepthSlabs();

    // Fit the view around a world-space bounds (ObjectARX:
    // AcGsView::zoomExtents).  Uses the current drawable aspect.
    void zoomExtents(const glm::dvec3 &minimum, const glm::dvec3 &maximum);

    // Place the orbit camera at an explicit target/distance and fit its
    // depth interval to the same point (autofocus-style framing).
    void focusOn(const glm::dvec3 &target, double distance);

    // Apply the slab policy to one candidate interval: expansion is applied
    // immediately (nothing clips while moving), shrinkage waits until the
    // candidate has been stable for twenty frames.  The stable interval is
    // then rounded outward to the enclosing float32 values, because the
    // projection matrices are float32 while the slab bounds accumulate in
    // double - rounding must never move a plane inside the bounds it was
    // computed to contain.
    void stabilizeDepthSlab(DepthSlab slab, double candidateNear,
                            double candidateFar, double &outNear,
                            double &outFar);

    void attach(rendering::RendererBackend *backend);
    rendering::RendererBackend *backend() const { return backend_; }

    void setFrameContext(const ViewFrameContext &context) { frame_ = context; }
    const ViewFrameContext &frame() const { return frame_; }
    ViewFrameContext &mutableFrame() { return frame_; }

    // Visual style state (ObjectARX: AcGiVisualStyle); the V key cycles it.
    rendering::RenderModeManager &visualStyle() { return visualStyle_; }

    // Per-submission sizing overrides; 0 = take the value from the frame
    // context.  overlayProjection, when set, replaces the frame's overlay
    // projection for the stroke/fill/point channels (mesh batching always
    // uses the frame projection).  eye, when set, replaces the frame
    // camera position as the mesh-instance eye (large-coordinate rebase
    // anchors pass their anchor here).
    struct SubmitOptions
    {
        const glm::mat4 *overlayProjection = nullptr;
        float pixelSizeWorld = 0.0f;
        float edgeSoftness = 0.0f;
        float pointSize = 0.0f;
        const rendering::DoubleSingleVec3 *eye = nullptr;
    };

    // Expand a collected draw list into the renderer's channels.
    void submit(scene::SceneDrawList &drawList,
                const SubmitOptions &options = {});

    // Flush the queued text requests as SDF glyph quads; returns the number
    // of glyphs drawn.
    int flushTextRequests();

    // ---- low-level drawing primitives (selection highlighter, overlays) ----

    // Build one camera-facing ribbon segment between world-space endpoints
    // (converted to camera-relative here).
    void appendRibbon(std::vector<rendering::PrimVertex> &vertices,
                      const glm::dvec3 &startWorld,
                      const glm::dvec3 &endWorld, float halfWidth, float u0,
                      float u1, bool centered,
                      const glm::vec4 &color) const;

    // Submit collected ribbon vertices through the polyline channel and
    // clear the buffer.
    void drawRibbonVertices(std::vector<rendering::PrimVertex> &vertices,
                            float edgeSoftness, float layer) const;

    // Submit prebuilt camera-relative line instances and clear the buffer.
    void drawLineInstanceBatch(std::vector<rendering::LineInstance> &instances,
                               float edgeSoftness, float layer) const;

    // Submit fill triangles from prebuilt vertices with an explicit view
    // (the CAD fill cache stores anchor-relative vertices).
    void drawFillTriangles(std::vector<rendering::FillVertex> &vertices,
                           const glm::mat4 &view,
                           const glm::mat4 &projection, bool is3DFace,
                           float layer) const;

    // Wireframe-mode fill boundary: emit the single-use triangle edges of
    // a fill range as camera-facing ribbons (shared edges stay hidden).
    void drawFillBoundary(const entities::TessellatedEntity &tess,
                          size_t begin, size_t count, float halfWidth,
                          const glm::vec4 &color) const;

    // CPU sampling of a curve batch command (de Casteljau / de Boor),
    // matching the GPU evaluator's clamped 16-slot windows.
    static std::vector<glm::dvec3> sampleCurveBatch(
        const scene::CurveBatchCommand &curve);

    // ---- shared primitives (selection highlighter, transient overlays) ----

    // Flip colors that would disappear against the clear color.
    glm::vec4 contrastColor(const glm::vec4 &color) const;

    // Camera-facing side axis of a screen-space line ribbon.
    glm::vec3 ribbonSide(const glm::vec3 &direction,
                         const glm::vec3 &front, float halfWidth) const;

    float strokeHalfWidth(const entities::Stroke &stroke,
                          float fallback = 2.0f) const;

    // Clip one world segment to the frame's frustum slab and viewport.
    // Returns false when the segment is entirely outside; semi-infinite
    // rays (Ray/XLine proxies) clip their analytic half-line instead.
    bool clipStrokeSegment(const glm::dvec3 &startWorld,
                           const glm::dvec3 &endWorld,
                           glm::dvec3 &clippedStart,
                           glm::dvec3 &clippedEnd,
                           bool semiInfiniteRay = false) const;

    // Clip a semi-infinite ray against an explicit slab (ID/pick geometry
    // uses different slab bounds than the visible stroke pass).
    bool clipSemiInfiniteRay(const glm::dvec3 &startWorld,
                             const glm::dvec3 &endWorld,
                             double nearDepth, double farDepth,
                             glm::dvec3 &clippedStart,
                             glm::dvec3 &clippedEnd,
                             bool flattenToSlabCenter = true) const;

    // Clip a finite segment against an explicit ortho/perspective frustum
    // (reference-line clipping uses the active depth planes, not the
    // overlay slab).
    bool clipSegmentToOrtho(const glm::dvec3 &startWorld,
                            const glm::dvec3 &endWorld, double nearPlane,
                            double farPlane, double halfWidth,
                            double halfHeight, glm::dvec3 &clippedStart,
                            glm::dvec3 &clippedEnd) const;
    bool clipSegmentToPerspective(
        const glm::dvec3 &startWorld, const glm::dvec3 &endWorld,
        double nearPlane, double farPlane, double tanHalfVertical,
        double tanHalfHorizontal, glm::dvec3 &clippedStart,
        glm::dvec3 &clippedEnd) const;

private:
    AcGsView()
        : orbitCamera_(glm::vec3(0.0f), 15.0f, -45.0f, 20.0f)
    {
    }

    void submitMeshBatch(scene::MeshBatchCommand &command,
                         const rendering::DoubleSingleVec3 &eye,
                         const SubmitOptions &options);

    rendering::RendererBackend *backend_ = nullptr;
    ViewFrameContext frame_;
    AcGsOrbitCamera orbitCamera_;
    bool orthoMode_ = false;
    rendering::RenderModeManager visualStyle_;

    struct DepthSlabStabilizer;
    DepthSlabStabilizer &slabStabilizer(DepthSlab slab);
};

} // namespace acgs
