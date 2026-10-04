// LoadedFont implementation: stb_truetype-based SDF rasterization for TTF/OTF
// + a stubbed SHX loader (the real SHX parser lives in shx_parser.cpp).
// Pure CPU work — the renderer takes the resulting atlas pixels + slot
// table and uploads them however it likes (bgfx::createTexture2D).

// stb_truetype is single-header and exposes its entire API (types +
// functions) only inside the #ifdef STB_TRUETYPE_IMPLEMENTATION block.
// Define the macro *before* any standard headers in case they transitively
// pull stb_truetype through PCH, and include it once here so the symbols
// are visible to the rest of this translation unit.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "text/text_font.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace rendering
{

namespace
{

std::vector<unsigned char> readWholeFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    const std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!in.read(reinterpret_cast<char *>(buffer.data()), size))
        return {};
    return buffer;
}

// SDF cell padding in pixels at the rasterization size.  The shader maps
// distance values back to [0, 1] using 4 * cellSize / (2 * pixelRange)
// which gives sub-pixel sharpness up to ~4 px on screen.
constexpr int kSdfPixelRange = 4;

} // namespace

// stb_truetype heap allocations live in the loaded font file buffer; we
// pin every loaded file in this process-global list so the buffers stay
// alive for the duration of the renderer.
namespace
{
std::vector<std::vector<unsigned char>> &pinnedFontFiles()
{
    static std::vector<std::vector<unsigned char>> storage;
    return storage;
}
} // namespace

LoadedFont::LoadedFont() = default;

LoadedFont::~LoadedFont()
{
    // The font file buffer is owned by pinnedFontFiles() (process
    // lifetime); stbtt_fontinfo holds no allocations of its own, so
    // nothing to free here.
    ttfData_ = nullptr;
    shxState_ = nullptr;
}

bool LoadedFont::loadSdf(const SdfFontConfig &config)
{
    auto fontFile = readWholeFile(config.ttfPath);
    if (fontFile.empty())
        return false;

    // Pin the file buffer for the life of the LoadedFont.
    pinnedFontFiles().push_back(std::move(fontFile));
    unsigned char *ttfData = pinnedFontFiles().back().data();

    stbtt_fontinfo info{};
    if (!stbtt_InitFont(&info, ttfData, 0))
        return false;

    const int pixelsPerEm = std::max(8, static_cast<int>(config.pixelsPerEm));
    const float scale = stbtt_ScaleForPixelHeight(&info,
                                                  static_cast<float>(pixelsPerEm));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    ascent_ = ascent * scale;
    descent_ = -descent * scale; // stb reports descent as positive downward
    lineHeight_ = (ascent - descent + lineGap) * scale;
    sdfPixelRange_ = static_cast<float>(kSdfPixelRange);

    const int firstGlyph = std::max(0, config.firstGlyph);
    const int lastGlyph = std::min(255, config.lastGlyph);
    if (lastGlyph < firstGlyph)
        return false;
    firstGlyph_ = firstGlyph;
    lastGlyph_ = lastGlyph;
    const int cellCount = lastGlyph - firstGlyph + 1;

    const int cellSize = pixelsPerEm + 2 * kSdfPixelRange;
    const int atlasSide = std::max(
        1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(cellCount)))));
    const int atlasWidth = atlasSide * cellSize;
    const int atlasHeight = atlasWidth;

    std::vector<unsigned char> pixels(static_cast<size_t>(atlasWidth) *
                                          static_cast<size_t>(atlasHeight) *
                                          4,
                                      0);

    auto sampleOutside = [&](std::vector<float> &distances,
                             const std::vector<unsigned char> &mask) {
        for (size_t i = 0; i < mask.size(); ++i)
            distances[i] = mask[i] > 128 ? 1.0e30f
                                          : static_cast<float>(i);
        // Forward EDT pass.
        for (int y = 0; y < cellSize; ++y)
        {
            for (int x = 0; x < cellSize; ++x)
            {
                const size_t idx = static_cast<size_t>(y) * cellSize + x;
                if (x > 0)
                    distances[idx] = std::min(distances[idx],
                                              distances[idx - 1] + 1);
                if (y > 0)
                {
                    distances[idx] = std::min(distances[idx],
                                              distances[idx - cellSize] + 1);
                    if (x > 0)
                        distances[idx] =
                            std::min(distances[idx],
                                     distances[idx - cellSize - 1] +
                                         std::sqrt(2.0f));
                    if (x < cellSize - 1)
                        distances[idx] =
                            std::min(distances[idx],
                                     distances[idx - cellSize + 1] +
                                         std::sqrt(2.0f));
                }
            }
        }
        // Backward EDT pass.
        for (int y = cellSize - 1; y >= 0; --y)
        {
            for (int x = cellSize - 1; x >= 0; --x)
            {
                const size_t idx = static_cast<size_t>(y) * cellSize + x;
                if (x < cellSize - 1)
                    distances[idx] = std::min(distances[idx],
                                              distances[idx + 1] + 1);
                if (y < cellSize - 1)
                {
                    distances[idx] = std::min(distances[idx],
                                              distances[idx + cellSize] + 1);
                    if (x > 0)
                        distances[idx] =
                            std::min(distances[idx],
                                     distances[idx + cellSize - 1] +
                                         std::sqrt(2.0f));
                    if (x < cellSize - 1)
                        distances[idx] =
                            std::min(distances[idx],
                                     distances[idx + cellSize + 1] +
                                         std::sqrt(2.0f));
                }
            }
        }
    };

    for (int codepoint = firstGlyph; codepoint <= lastGlyph; ++codepoint)
    {
        const int index = codepoint - firstGlyph;
        const int row = index / atlasSide;
        const int col = index % atlasSide;
        const int cellOriginX = col * cellSize;
        const int cellOriginY = row * cellSize;

        SdfGlyphSlot slot;
        int advanceWidthUnits = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(&info, codepoint, &advanceWidthUnits,
                                   &leftSideBearing);
        slot.advanceWidth = static_cast<float>(advanceWidthUnits) * scale;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(
            &info, codepoint, scale, scale,
            &x0, &y0, &x1, &y1);
        const int gw = x1 - x0;
        const int gh = y1 - y0;
        const int baselineInCell = pixelsPerEm - y0;
        const int leftInCell = -x0 + kSdfPixelRange;
        slot.bearingX = static_cast<float>(-x0) / pixelsPerEm;
        slot.bearingY = static_cast<float>(y1) / pixelsPerEm;

        // Render the glyph mask at the configured size.
        std::vector<unsigned char> mask(static_cast<size_t>(gw * gh), 0);
        if (gw > 0 && gh > 0)
        {
            stbtt_MakeCodepointBitmap(
                &info, mask.data(), gw, gh, gw, scale, scale,
                codepoint);
        }

        // 8-bit signed distance field: 0 = far outside, 128 = boundary,
        // 255 = far inside.
        std::vector<unsigned char> cell(static_cast<size_t>(cellSize * cellSize),
                                       0);
        for (int y = 0; y < gh; ++y)
        {
            const int cellY = baselineInCell - y + kSdfPixelRange;
            if (cellY < 0 || cellY >= cellSize)
                continue;
            for (int x = 0; x < gw; ++x)
            {
                const int cellX = leftInCell + x;
                if (cellX < 0 || cellX >= cellSize)
                    continue;
                cell[static_cast<size_t>(cellY) * cellSize + cellX] =
                    mask[static_cast<size_t>(y) * gw + x];
            }
        }

        std::vector<float> distances(static_cast<size_t>(cellSize * cellSize),
                                     0.0f);
        std::vector<float> outside(static_cast<size_t>(cellSize * cellSize),
                                  0.0f);
        std::vector<float> inside(static_cast<size_t>(cellSize * cellSize),
                                 0.0f);

        // Outside pass: seeds are inside pixels (cell > 128).
        sampleOutside(distances, cell);
        for (size_t i = 0; i < outside.size(); ++i)
            outside[i] = std::sqrt(distances[i]);

        // Inside pass: swap "inside" and "outside" by flipping the mask.
        for (size_t i = 0; i < distances.size(); ++i)
            distances[i] = cell[i] > 128
                               ? static_cast<float>(i)
                               : 1.0e30f;
        sampleOutside(distances, cell);
        for (size_t i = 0; i < inside.size(); ++i)
            inside[i] = std::sqrt(distances[i]);

        // Encode: signed distance = inside - outside, mapped to 0..255
        // across the SDF range.  Boundary (zero distance) maps to 128.
        std::vector<unsigned char> sdf(static_cast<size_t>(cellSize * cellSize),
                                       0);
        for (size_t i = 0; i < sdf.size(); ++i)
        {
            const float signedDistance = inside[i] - outside[i];
            const float normalized =
                std::clamp(0.5f - 0.5f * signedDistance / kSdfPixelRange,
                           0.0f, 1.0f);
            sdf[i] = static_cast<unsigned char>(
                std::round(normalized * 255.0f));
        }

        // Compose into the BGRA8 atlas.
        for (int y = 0; y < cellSize; ++y)
        {
            for (int x = 0; x < cellSize; ++x)
            {
                const size_t srcIdx = static_cast<size_t>(y) * cellSize + x;
                const size_t dstIdx =
                    (static_cast<size_t>(cellOriginY + y) * atlasWidth +
                     (cellOriginX + x)) *
                    4;
                const unsigned char v = sdf[srcIdx];
                pixels[dstIdx + 0] = 255;
                pixels[dstIdx + 1] = 255;
                pixels[dstIdx + 2] = 255;
                pixels[dstIdx + 3] = v;
            }
        }

        slot.u0 = static_cast<float>(cellOriginX) / atlasWidth;
        slot.v0 = static_cast<float>(cellOriginY) / atlasHeight;
        slot.u1 = static_cast<float>(cellOriginX + cellSize) / atlasWidth;
        slot.v1 = static_cast<float>(cellOriginY + cellSize) / atlasHeight;
        slot.valid = true;
        sdfSlots_[codepoint] = slot;
    }

    pendingAtlas_.pixels = std::move(pixels);
    pendingAtlas_.width = atlasWidth;
    pendingAtlas_.height = atlasHeight;
    pendingAtlas_.pixelRange = sdfPixelRange_;
    pendingAtlasReady_ = true;
    sdfLoaded_ = true;
    ttfData_ = ttfData;
    loaded_ = true;
    return true;
}

void LoadedFont::releaseSdfAtlas(SdfAtlas &outAtlas)
{
    outAtlas = pendingAtlas_;
    pendingAtlas_.pixels.clear();
    pendingAtlas_ = SdfAtlas{};
    pendingAtlasReady_ = false;
}

const SdfGlyphSlot &LoadedFont::sdfGlyph(std::uint32_t codepoint) const
{
    if (codepoint > 255u)
        return sdfSlots_[firstGlyph_];
    return sdfSlots_[codepoint];
}

const ShxGlyphSlot &LoadedFont::shxGlyph(std::uint32_t codepoint) const
{
    if (codepoint > 255u)
        return shxSlots_[firstGlyph_];
    return shxSlots_[codepoint];
}

} // namespace rendering
