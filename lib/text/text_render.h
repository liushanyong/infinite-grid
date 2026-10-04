#pragma once

// Text rendering module: SDF (TTF/OTF via stb_truetype) + AutoCAD SHX/SHP
// big-font stroke parsing.  Designed for the AcGi protocol layer; the
// existing layout-frame fallback in AcGiWorldDraw::text() is preserved when
// no font is available.
//
// The module is header-only for the API; implementation files (sdf_text.cpp
// + shx_parser.cpp) compile into lib/text and link into BgfxRenderer.
//
// Style is intentionally minimal so that callers do not depend on a
// particular font subsystem: font files are loaded by path; glyphs are
// rendered either as a Signed Distance Field atlas (SDF, TTF/OTF) or as
// vector strokes (SHX).  The AcGiTextStyle fields already carry textSize,
// xScale and obliqueAngle; we honor them when they make sense for the
// chosen font path.

#include <cstdint>
#include <string>
#include <vector>

struct bgfx_texture_handle_t;
typedef struct bgfx_texture_handle_t *bgfxTextureHandle;

namespace rendering
{

// Bitmap font source for the SDF path.  stb_truetype does the heavy
// lifting; the resulting glyph atlas is uploaded to the GPU as a single
// 1024x1024 (or larger, when needed) BGRA8 texture with a distance field
// computed on the CPU.  The renderer chooses the cell size from the font's
// metrics so the atlas holds 256 glyphs of the configured size class.
struct SdfFontConfig
{
    std::string ttfPath;    // path to TTF/OTF
    float pixelsPerEm = 64.0f; // rasterization size for the atlas
    int firstGlyph = 32;        // inclusive (typically ' ')
    int lastGlyph = 127;       // inclusive (typically '~')
};

// Vector font source for the SHX path.  AutoCAD SHX big-fonts encode each
// glyph as a sequence of line segments and arcs; we parse them into a
// 2D stroke list per code point.
struct ShxFontConfig
{
    std::string shxPath;
};

// One SdfGlyph: position in the atlas + metrics.
struct SdfGlyphSlot
{
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // atlas UV box
    float advanceWidth = 0.0f;  // horizontal advance in em-units
    float bearingX = 0.0f;       // left side bearing in em-units
    float bearingY = 0.0f;       // top side bearing in em-units
    bool valid = false;
};

// One ShxGlyph: line segments in the glyph's local 2D space (em-units).
struct ShxGlyphStroke
{
    float fromX = 0.0f, fromY = 0.0f;
    float toX = 0.0f, toY = 0.0f;
};

struct ShxGlyphSlot
{
    std::vector<ShxGlyphStroke> strokes;
    float advanceWidth = 0.0f;
    float upperLine = 0.0f;
    float lowerLine = 0.0f;
    bool valid = false;
};

// Loaded fonts.  Either SDF or SHX; the AcGi callback chooses based on
// which load succeeded (priority: SDF > SHX > fallback frame).
class LoadedFont
{
public:
    LoadedFont();
    ~LoadedFont();

    LoadedFont(const LoadedFont &) = delete;
    LoadedFont &operator=(const LoadedFont &) = delete;

    bool loadSdf(const SdfFontConfig &config);
    bool loadShx(const ShxFontConfig &config);

    bool isLoaded() const { return loaded_; }
    bool hasSdf() const { return sdfLoaded_; }
    bool hasShx() const { return shxLoaded_; }

    float ascent() const { return ascent_; }
    float descent() const { return descent_; }
    float lineHeight() const { return lineHeight_; }

    // SDF path: each query returns the atlas slot for the codepoint.
    const SdfGlyphSlot &sdfGlyph(uint32_t codepoint) const;

    // SHX path: strokes are in em-units, drawn at world-units per em.
    const ShxGlyphSlot &shxGlyph(uint32_t codepoint) const;

    // GPU-side resources (texture handle + dimensions + sampling params);
    // returned to the renderer so it can bind the atlas for SDF draws.
    void *gpuTexture() const; // opaque bgfx::TextureHandle pointer
    int atlasWidth() const { return atlasWidth_; }
    int atlasHeight() const { return atlasHeight_; }
    float sdfPixelRange() const { return sdfPixelRange_; }

private:
    bool loaded_ = false;
    bool sdfLoaded_ = false;
    bool shxLoaded_ = false;

    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineHeight_ = 0.0f;
    int atlasWidth_ = 0;
    int atlasHeight_ = 0;
    float sdfPixelRange_ = 0.0f;

    void *atlasTexture_ = nullptr; // owned bgfx::TextureHandle
    void *tessInfo_ = nullptr;     // owned stb_truetype data
    void *shxGlyphs_ = nullptr;   // owned glyph table
    int firstGlyph_ = 0;
    int lastGlyph_ = 0;

    // Per-codepoint cache.  Fixed-size tables of 256 entries; the
    // renderer uses the lower byte of the codepoint for indexing
    // (sufficient for ASCII/Latin-1; full Unicode is not in scope here).
    SdfGlyphSlot sdfSlots_[256];
    ShxGlyphSlot shxSlots_[256];
};

// Render a text string at world position with the given style.  Called by
// AcGiWorldDraw::text().  When `font` is null or has neither SDF nor SHX,
// the renderer falls back to a layout frame (current demo behaviour).
//
// view / projection: camera view and projection matrices.
// pos / normal / direction: insertion point, plane normal, and tangent
//   direction (matches the AcGiWorldDraw::text() signature).
// scaleX: width factor from AcGiTextStyle.xScale.
// thicknessEm: stroke thickness in em-units (SHX path; ignored for SDF).
void drawText(class BgfxRenderer &renderer, const LoadedFont *font,
              const glm::dmat4 &view, const glm::mat4 &projection,
              const double pos[3], const double normal[3],
              const double direction[3], const char *message,
              double heightPixels, double scaleX, double thicknessEm,
              const double *colorRgba);

} // namespace rendering
