#pragma once

// Text-draw request queue for the AcGi layer.  worldDraw(Text/MText) calls
// AcGiWorldDraw::text(); the callback records WHAT to draw here (message,
// placement, style) without depending on any font subsystem.  The host
// application drains the queue per frame and renders the glyphs with
// whichever text backend is available (SDF atlas, SHX strokes, or the
// layout-frame fallback baked into the cached tessellation).

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace acgi
{

struct TextRequest
{
    glm::dvec3 position{0.0};
    glm::dvec3 normal{0.0, 0.0, 1.0};
    glm::dvec3 direction{1.0, 0.0, 0.0};
    std::string message;
    double height = 1.0;
    double xScale = 1.0;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    // false (default): CAD planar text — the quad lies in the entity's
    // normal/direction plane.  true: billboard — the quad always faces
    // the camera (screen-anchored labels, view annotations).
    bool billboard = false;
};

// Global request list.  Entities tessellate once into the cached draw
// list, so requests accumulate at tessellation time and persist for the
// lifetime of the cache; the renderer reads them every frame.
inline std::vector<TextRequest> &textRequests()
{
    static std::vector<TextRequest> requests;
    return requests;
}

// When true (no glyph backend available), AcGiWorldDraw::text() also emits
// the layout-frame stroke so text entities stay visible as boxes.  When a
// glyph backend is loaded, the host sets this to false and renders the
// queued requests instead.
inline bool &textFrameFallback()
{
    static bool fallback = true;
    return fallback;
}

} // namespace acgi
