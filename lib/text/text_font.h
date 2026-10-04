#pragma once

// Text font subsystem: SDF (TTF/OTF via stb_truetype) + AutoCAD SHX/SHP
// big-font stroke parsing.  Pure data layer — produces per-codepoint
// glyph descriptors and a CPU-side SDF atlas.  The renderer (BgfxRenderer)
// consumes these directly with bgfx APIs; nothing here depends on bgfx,
// so the same font subsystem can drive a WebGPU renderer later.
//
// Style is intentionally minimal: font files are loaded by path; glyphs
// are rendered either as a Signed Distance Field atlas (SDF, TTF/OTF) or
// as vector strokes (SHX).  AcGiTextStyle's textSize / xScale / obliqueAngle
// fields are honored by the renderer when they apply to the chosen path.

#include <cstdint>
#include <string>
#include <vector>

namespace rendering
{

// Configuration for the SDF (TTF/OTF) path.  stb_truetype does the heavy
// lifting; the resulting glyph atlas is a BGRA8 texture with a distance
// field computed on the CPU.
struct SdfFontConfig
{
    std::string ttfPath;
    float pixelsPerEm = 64.0f;
    int firstGlyph = 32;   // inclusive (typically ' ')
    int lastGlyph = 127;  // inclusive (typically '~')
};

// Configuration for the SHX/SHP big-font path.
struct ShxFontConfig
{
    std::string shxPath;
};

// Per-glyph descriptor for the SDF path: UV box in the atlas and
// horizontal metrics in em-units.
struct SdfGlyphSlot
{
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    float advanceWidth = 0.0f;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    bool valid = false;
};

// Per-glyph descriptor for the SHX path: line segments in em-units.
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

// SDF atlas: a single BGRA8 image + a per-codepoint slot table.
// `pixels` holds RGBA bytes, row-major from bottom to top.
struct SdfAtlas
{
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    float pixelRange = 0.0f;
};

// Loaded font: holds either the SDF atlas + slots (preferred) or the SHX
// glyph table.  The AcGi callback chooses which to use.
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

    // SDF atlas release: caller takes ownership of `pixels` and uploads it
    // to a bgfx texture, then queries sdfGlyph(...) for layout.
    void releaseSdfAtlas(SdfAtlas &outAtlas);

    const SdfGlyphSlot &sdfGlyph(std::uint32_t codepoint) const;
    const ShxGlyphSlot &shxGlyph(std::uint32_t codepoint) const;

private:
    bool loaded_ = false;
    bool sdfLoaded_ = false;
    bool shxLoaded_ = false;

    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineHeight_ = 0.0f;

    // Owned stb_truetype data (opaque pointer to keep stb out of the
    // public header) and SHX glyph table (opaque pointer to SHX parser
    // internals).  Released by the destructor.
    void *ttfData_ = nullptr;
    void *shxState_ = nullptr;

    int firstGlyph_ = 0;
    int lastGlyph_ = 0;

    SdfGlyphSlot sdfSlots_[256];
    ShxGlyphSlot shxSlots_[256];

    // The most recently rasterized SDF atlas (CPU-side pixels) is kept
    // around until the renderer calls releaseSdfAtlas().
    SdfAtlas pendingAtlas_;
    bool pendingAtlasReady_ = false;
};

} // namespace rendering
