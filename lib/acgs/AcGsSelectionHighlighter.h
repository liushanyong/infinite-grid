#pragma once

// AcGsSelectionHighlighter — selection highlight geometry, modeled on the
// selection rendering ObjectARX performs through AcGiSubEntityTraits
// selection flags.  The caller owns the selection state (which entity is
// highlighted and its tessellation range); this class owns how the
// highlight is drawn:
//   * fill entities: shared triangle edges culled, boundary edges plus the
//     camera-dependent silhouette of closed 3D solids drawn as wide
//     camera-facing ribbons,
//   * points: a camera-facing annulus around the point impostor whose
//     inner edge matches the impostor's screen radius,
//   * line-like strokes: widened line instances on the same centerline and
//     expansion path as the visible body (semi-infinite rays keep their
//     full visible span),
//   * GPU curves: a widened camera-facing ribbon.

#include <glm/glm.hpp>

#include <cstddef>

#include "acgs/model/AcGsModel.h"

namespace acgs
{

class AcGsView;

// Highlight color and screen-space width shared by every highlight pass.
constexpr glm::vec4 kOutlineColor(1.0f, 0.55f, 0.05f, 1.0f);
constexpr float kOutlineWidthPixels = 3.0f;

// Global outline expansion in world units: for centered ribbons each side
// grows by this half-width, so the full width grows by two copies.
inline float outlineWidthWorld(float pixelSizeWorld)
{
    return kOutlineWidthPixels * pixelSizeWorld;
}

class AcGsSelectionHighlighter
{
public:
    explicit AcGsSelectionHighlighter(AcGsView &view) : view_(&view) {}

    // Follows the active viewport (AcGsManager view switches rebind it).
    void setView(AcGsView &view) { view_ = &view; }

    void drawFillOutline(const acdb::TessellatedEntity &tess,
                         size_t begin, size_t count, float pixelSizeWorld);

    void drawPointHighlight(const acdb::TessellatedEntity &tess,
                            size_t begin, size_t count,
                            float pixelSizeWorld);

    void drawStrokeOutline(const acdb::TessellatedEntity &tess,
                           size_t begin, size_t count, float pixelSizeWorld);

    void drawCurveOutline(const acgs::CurveBatchCommand &curve,
                          float pixelSizeWorld);

private:
    AcGsView *view_;
};

} // namespace acgs
