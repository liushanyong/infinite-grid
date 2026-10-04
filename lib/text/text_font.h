#pragma once

// Text font subsystem: per-glyph SDF rasterization (TTF/OTF via
// stb_truetype) + AutoCAD SHX/SHP stroke parsing.  Pure data layer — no
// bgfx dependency; the renderer uploads each glyph's R8 SDF texture and
// draws camera-facing quads.
//
// Architecture follows the CADplatformer reference (MIT): one SDF texture
// per glyph cached on demand, instead of a shared atlas.  This removes
// the atlas UV/stride bug class entirely.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rendering
{

struct SdfFontConfig
{
    std::string ttfPath;
    float pixelsPerEm = 96.0f;
    int sdfSpread = 6; // SDF distance range in texels
};

struct ShxFontConfig
{
    std::string shxPath;
};

// Rasterized glyph SDF (R channel values).  Metrics are in pixels at
// pixelsPerEm; since pixelsPerEm is also the em size, the numbers read
// directly as em units.
struct GlyphSdfRaster
{
    std::vector<std::uint8_t> sdf;
    int width = 0;
    int height = 0;
    int left = 0;          // pen to glyph left edge (pixels)
    int top = 0;           // pen (baseline) to glyph top edge (pixels)
    float advanceX = 0.0f; // pen advance (pixels = em)
    bool valid = false;
};

// SHX vector glyph: line segments in em units (cap height normalized to
// 1.0), decoded from the compiled shape commands.
struct ShxGlyphStroke
{
    float fromX = 0.0f, fromY = 0.0f;
    float toX = 0.0f, toY = 0.0f;
};

struct ShxGlyphSlot
{
    std::vector<ShxGlyphStroke> strokes;
    float advanceWidth = 0.0f; // em
    bool valid = false;
};

class LoadedFont
{
public:
    LoadedFont();
    ~LoadedFont();
    LoadedFont(const LoadedFont &) = delete;
    LoadedFont &operator=(const LoadedFont &) = delete;

    // TTF/OTF: initializes stb_truetype; glyphs rasterize on demand.
    bool loadSdf(const SdfFontConfig &config);
    // SHX/SHP: parses the whole file up front.
    bool loadShx(const ShxFontConfig &config);

    bool isLoaded() const { return loaded_; }
    bool hasSdf() const { return sdfLoaded_; }
    bool hasShx() const { return shxLoaded_; }

    float pixelsPerEm() const { return pixelsPerEm_; }
    float ascent() const { return ascent_; }

    // Per-glyph SDF raster, computed and cached on first request.
    const GlyphSdfRaster &glyphSdf(std::uint32_t codepoint);

    const ShxGlyphSlot &shxGlyph(std::uint32_t codepoint) const;

public:
    // Load-state flags and the SHX slot table are public: the SHX parser
    // (shx_parser.cpp) fills them after decoding a font file.
    bool loaded_ = false;
    bool sdfLoaded_ = false;
    bool shxLoaded_ = false;
    std::map<std::uint32_t, ShxGlyphSlot> shxSlots_;

private:

    float pixelsPerEm_ = 96.0f;
    float ascent_ = 0.0f;
    float sdfSpread_ = 6.0f;

    // Font file buffer must outlive stbtt_fontinfo.
    std::vector<unsigned char> fontData_;
    void *fontInfo_ = nullptr; // stbtt_fontinfo*

    std::map<std::uint32_t, GlyphSdfRaster> glyphCache_;

    // SHX parsed state (owned by the parser implementation).
    void *shxState_ = nullptr;
};

} // namespace rendering
