#pragma once

// CAD icon set for the ImGui port.
//
// Glyph geometry is baked from the ORIGINAL in-repo designs in
// resources/icons/cad/*.svg into IconGlyphs.hpp by tools/svg2icon.py.
// Everything is drawn through ImDrawList path commands, so icons stay crisp at
// any DPI and keep honouring ImGui's own clipping, without needing a texture
// atlas or an icon font.

#include <imgui.h>

#include "IconGlyphs.hpp"

namespace ui
{
    using IconId = icons::detail::GlyphId;

    // Each glyph is a near-white body plus an accent that marks exactly one
    // semantic focus (the datum of a circle, the span of a dimension, the brush
    // tip of match-property).  `muted` carries ghosted construction geometry.
    struct IconPalette
    {
        ImU32 neutral;
        ImU32 accent;
        ImU32 muted;
    };

    // Tuned against this app's dark ribbon background. The neutral sits close to
    // white so thin outlines stay crisp at 26px / 17px; the accent is pushed more
    // saturated than the neutral so that a very small accent mark (a 2.2-unit grip)
    // still reads as the brightest thing on the icon.
    inline IconPalette MakeToolbarPalette()
    {
        return IconPalette{
            IM_COL32(216, 224, 234, 255),   // #D8E0EA neutral (near-white body)
            IM_COL32(79, 163, 242, 255),    // #4FA3F2 accent (saturated, small area)
            IM_COL32(139, 150, 166, 255)    // #8B96A6 muted / construction
        };
    }

    // Draw `id` centred on `center`, fitted into a `size` x `size` box.
    void DrawIcon(ImDrawList* drawList, IconId id, const ImVec2& center, float size,
                  const IconPalette& palette);

    // Single colour variant -- every role uses `color`. The status bar relies on
    // this because it recolours the whole icon for its active / hover states.
    void DrawIconMono(ImDrawList* drawList, IconId id, const ImVec2& center, float size,
                      ImU32 color);
}