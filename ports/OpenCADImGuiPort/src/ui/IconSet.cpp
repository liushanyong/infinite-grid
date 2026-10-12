#include "IconSet.hpp"

#include "IconGlyphs.hpp"

#include <algorithm>

namespace ui
{
    namespace
    {
        using namespace icons::detail;

        ImU32 RoleColor(const IconPalette& palette, unsigned char role)
        {
            switch (role)
            {
            case kRoleAccent: return palette.accent;
            case kRoleMuted: return palette.muted;
            default: return palette.neutral;
            }
        }
    }

    void DrawIcon(ImDrawList* drawList, IconId id, const ImVec2& center, float size,
                  const IconPalette& palette)
    {
        if (drawList == nullptr || size <= 0.0f)
            return;

        const unsigned index = static_cast<unsigned>(id);
        if (index >= static_cast<unsigned>(kGlyphCount))
            return;

        const Glyph& glyph = kGlyphs[index];
        const float reference = glyph.size > 0.0f ? glyph.size : 24.0f;
        const float scale = size / reference;
        const ImVec2 origin(center.x - size * 0.5f, center.y - size * 0.5f);

        const auto place = [&](const Pt& p) {
            return ImVec2(origin.x + p.x * scale, origin.y + p.y * scale);
        };

        // fills first so outlines stay legible on top of them
        for (unsigned i = 0; i < glyph.nFills; ++i)
        {
            const Fill& fill = kFills[glyph.fills + i];
            const ImU32 color = RoleColor(palette, fill.role);
            for (unsigned t = 0; t < fill.n; ++t)
            {
                const Pt& a = fill.tris[t * 3 + 0];
                const Pt& b = fill.tris[t * 3 + 1];
                const Pt& c = fill.tris[t * 3 + 2];
                drawList->PathClear();
                drawList->PathLineTo(place(a));
                drawList->PathLineTo(place(b));
                drawList->PathLineTo(place(c));
                drawList->PathFillConvex(color);
            }
        }

        for (unsigned i = 0; i < glyph.nStrokes; ++i)
        {
            const Stroke& stroke = kStrokes[glyph.strokes + i];
            if (stroke.n < 2)
                continue;
            drawList->PathClear();
            for (unsigned p = 0; p < stroke.n; ++p)
                drawList->PathLineTo(place(stroke.pts[p]));
            drawList->PathStroke(RoleColor(palette, stroke.role), ImDrawFlags_None,
                                 std::max(1.0f, stroke.w * scale));
        }
    }

    void DrawIconMono(ImDrawList* drawList, IconId id, const ImVec2& center, float size,
                      ImU32 color)
    {
        DrawIcon(drawList, id, center, size, IconPalette{ color, color, color });
    }
}