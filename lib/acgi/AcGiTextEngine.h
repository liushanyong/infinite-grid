#pragma once

#include "AcGiPort.h"

// AcGi text engine: the consolidated text-rendering entry point for the
// acgi system, following the ObjectARX pattern where AcGiTextStyle
// carries the font reference and AcGiGeometry::text() performs the draw.
//
// The engine owns the font subsystem (SDF TTF/OTF via stb_truetype +
// AutoCAD SHX stroke fonts), the per-glyph texture cache on the active
// renderer, and the draw path for AcGiTextRequest records collected by
// AcGiWorldDraw::text().  Host code only needs:
//
//   acgi::textEngine().loadSdfFont(path);
//   acgi::textEngine().drawText(backend, view, projection, basis, request);

#include <set>
#include <map>
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "AcGiTextDevice.h"
#include "AcGiTextQueue.h"
#include "../text/text_font.h"


namespace acgi
{

class ACGI_PORT TextEngine
{
public:
    // ---- font management (AcGiTextStyle::setFont analogue) ----
    // AutoCAD bigfont pairing: ASCII glyphs come from the regular font,
    // double-byte (CJK) glyphs from the big font.
    bool loadSdfFont(const std::string &ttfPath, float pixelsPerEm = 96.0f);
    bool loadShxRegularFont(const std::string &shxPath);
    bool loadShxBigFont(const std::string &shxPath);
    bool sdfReady() const { return sdfFont_.isLoaded() && sdfFont_.hasSdf(); }
    bool shxReady() const
    {
        return shxRegularFont_.isLoaded() || shxBigFont_.isLoaded();
    }
    rendering::LoadedFont &sdfFont() { return sdfFont_; }
    rendering::LoadedFont &shxRegularFont() { return shxRegularFont_; }
    rendering::LoadedFont &shxBigFont() { return shxBigFont_; }

    // ---- drawing (AcGiGeometry::text analogue) ----
    // Draws one text request through the renderer's SDF glyph path.  The
    // basis vectors and camera position are the current camera frame;
    // quads are expanded in view space for the identity-view submission.
    // Returns the number of glyphs drawn.
    int drawText(TextDevice &device,
                 const glm::mat4 &view, const glm::mat4 &projection,
                 const glm::dvec3 &cameraPos, const glm::dvec3 &cameraRight,
                 const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront,
                 const TextRequest &request,
                 bool selectionHighlight = false);

    // Frustum culling (ortho): builds the text's oriented bounding
    // rectangle (baseline direction × line count in the entity plane) and
    // tests it against the ortho viewport [±halfW]×[±halfH] in camera
    // space.  Depth is ignored on purpose — text request depths feed the
    // slab, so they are always inside it.  Returns false only when the
    // text lies entirely outside the viewport rectangle.
    bool intersectsOrthoViewport(const TextRequest &request,
                                 const glm::dvec3 &cameraPos,
                                 const glm::dvec3 &cameraRight,
                                 const glm::dvec3 &cameraUp,
                                 const glm::dvec3 &cameraFront,
                                 double halfWidth, double halfHeight) const;

    // Per-glyph layout for picking AND drawing: one walk of the exact
    // drawText() rules (codepoints, per-glyph left/top/height/advance,
    // newline at 1.35 line spacing, billboard or entity-plane
    // orientation).  Corners are WORLD-space (c00, c10, c11, c01); the
    // GPU pick pass builds its ID geometry from these so the ID texture
    // matches the visible glyphs pixel for pixel.
    struct GlyphPlacement
    {
        std::array<glm::dvec3, 4> corners;
        std::uint32_t codepoint = 0;
    };
    struct GlyphCoverageUv
    {
        float u = 0.0f;
        float v = 0.0f;
    };
    std::vector<GlyphPlacement> layoutGlyphs(
        const TextRequest &request,
        const glm::dvec3 &cameraPos, const glm::dvec3 &cameraRight,
        const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront);
    // Triangulated 0.5 SDF coverage in glyph-quad UV space, cached per
    // codepoint and invalidated when the SDF font is reloaded.
    const std::vector<GlyphCoverageUv> &glyphCoverageUv(
        std::uint32_t codepoint);

    // Cached layout access for the two per-frame consumers (visible SDF
    // pass and pick soup). Planar text layouts are camera-independent, so
    // they cache per request pointer (guarded by a field hash); billboard
    // layouts stay per-call. clearLayoutCache() drops everything when the
    // request vector is rebuilt (document revision bump).
    const std::vector<GlyphPlacement> &glyphLayout(
        const TextRequest &request,
        const glm::dvec3 &cameraPos, const glm::dvec3 &cameraRight,
        const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront);
    void clearLayoutCache() { layoutCache_.clear(); }

    // SHX stroke geometry for a message, in em units (cap height 1.0);
    // ASCII glyphs resolve through the regular font, double-byte codes
    // through the big font.  Consumed by the stroke pipeline as ordinary
    // CAD geometry.
    std::vector<rendering::ShxGlyphStroke> shxStrokes(
        const std::string &message) const;

    // Total SHX advance width of a message in em units.
    double shxAdvance(const std::string &message) const;

    // Style-name binding: the acdb font loader marks which logical text
    // style (AcDbTextStyleTableRecord name) the currently loaded SHX fonts
    // serve; the worldText callback routes a style to SHX strokes when its
    // name is bound here.
    void bindShxStyleFont(const std::string &styleName);
    bool shxStyleFontBound(const std::string &styleName) const;

private:
    const rendering::LoadedFont &shxFontFor(unsigned char character) const;

    rendering::LoadedFont sdfFont_;
    std::set<std::string> shxStyleFonts_;

    rendering::LoadedFont shxRegularFont_;
    rendering::LoadedFont shxBigFont_;
    std::map<unsigned int, uint32_t> glyphTextureIds_;

    struct CachedGlyphLayout
    {
        std::uint64_t fieldsHash = 0;
        std::vector<GlyphPlacement> placements;
    };
    std::unordered_map<const TextRequest *, CachedGlyphLayout> layoutCache_;
    std::vector<GlyphPlacement> billboardLayout_;
    std::unordered_map<std::uint32_t, std::vector<GlyphCoverageUv>>
        glyphCoverageCache_;
};

// Global engine instance (the host loads fonts into it at startup).
ACGI_PORT TextEngine &textEngine();

} // namespace acgi
