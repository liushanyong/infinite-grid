// LoadedFont implementation: per-glyph SDF rasterization via
// stb_truetype.  The SDF generation follows the CADplatformer reference
// (MIT): grayscale glyph bitmap -> edge pixels -> brute-force distance
// transform with a bounded spread -> 0.5-boundary byte encoding in the
// R channel.

#include "text/text_font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
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

bool pixelInside(const std::vector<uint8_t> &alpha, int w, int h, int x,
                 int y)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return false;
    return alpha[y * w + x] > 0;
}

} // namespace

LoadedFont::LoadedFont() = default;

LoadedFont::~LoadedFont()
{
    delete static_cast<stbtt_fontinfo *>(fontInfo_);
    fontInfo_ = nullptr;
}

bool LoadedFont::loadSdf(const SdfFontConfig &config)
{
    fontData_ = readWholeFile(config.ttfPath);
    if (fontData_.empty())
        return false;

    auto *info = new stbtt_fontinfo();
    if (!stbtt_InitFont(info, fontData_.data(),
                        stbtt_GetFontOffsetForIndex(fontData_.data(), 0)))
    {
        delete info;
        fontData_.clear();
        return false;
    }
    fontInfo_ = info;
    pixelsPerEm_ = config.pixelsPerEm;
    sdfSpread_ = static_cast<float>(config.sdfSpread);

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(info, pixelsPerEm_);
    ascent_ = ascent * scale;

    glyphCache_.clear();
    sdfLoaded_ = true;
    loaded_ = true;
    return true;
}

const GlyphSdfRaster &LoadedFont::glyphSdf(std::uint32_t codepoint)
{
    auto it = glyphCache_.find(codepoint);
    if (it != glyphCache_.end())
        return it->second;

    GlyphSdfRaster entry;
    auto *info = static_cast<stbtt_fontinfo *>(fontInfo_);
    if (info && sdfLoaded_)
    {
        const float scale = stbtt_ScaleForPixelHeight(info, pixelsPerEm_);
        const int glyphIndex =
            stbtt_FindGlyphIndex(info, static_cast<int>(codepoint));

        int advanceWidth = 0, leftSideBearing = 0;
        stbtt_GetGlyphHMetrics(info, glyphIndex, &advanceWidth,
                               &leftSideBearing);

        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetGlyphBitmapBox(info, glyphIndex, scale, scale, &ix0, &iy0,
                                &ix1, &iy1);
        const int bitmapWidth = ix1 - ix0;
        const int bitmapHeight = iy1 - iy0;

        if (bitmapWidth > 0 && bitmapHeight > 0)
        {
            std::vector<uint8_t> bitmap(
                static_cast<size_t>(bitmapWidth * bitmapHeight), 0);
            stbtt_MakeGlyphBitmap(info, bitmap.data(), bitmapWidth,
                                  bitmapHeight, bitmapWidth, scale, scale,
                                  glyphIndex);

            const int spread = static_cast<int>(sdfSpread_);
            const int pad = spread + 2;
            const int dstWidth = bitmapWidth + pad * 2;
            const int dstHeight = bitmapHeight + pad * 2;

            // Binary coverage with the glyph padded by the spread.
            std::vector<uint8_t> alpha(
                static_cast<size_t>(dstWidth) *
                    static_cast<size_t>(dstHeight),
                0);
            for (int y = 0; y < bitmapHeight; ++y)
                for (int x = 0; x < bitmapWidth; ++x)
                    alpha[static_cast<size_t>(y + pad) * dstWidth +
                          (x + pad)] =
                        bitmap[static_cast<size_t>(y) * bitmapWidth + x];

            // Edge pixels: inside/outside toggles against 4-neighbours.
            struct EdgePoint
            {
                int x;
                int y;
            };
            std::vector<EdgePoint> edges;
            edges.reserve(alpha.size() / 4);
            for (int y = 0; y < dstHeight; ++y)
                for (int x = 0; x < dstWidth; ++x)
                {
                    const bool inside =
                        pixelInside(alpha, dstWidth, dstHeight, x, y);
                    const bool n0 =
                        pixelInside(alpha, dstWidth, dstHeight, x - 1, y);
                    const bool n1 =
                        pixelInside(alpha, dstWidth, dstHeight, x + 1, y);
                    const bool n2 =
                        pixelInside(alpha, dstWidth, dstHeight, x, y - 1);
                    const bool n3 =
                        pixelInside(alpha, dstWidth, dstHeight, x, y + 1);
                    if (inside != n0 || inside != n1 || inside != n2 ||
                        inside != n3)
                        edges.push_back({x, y});
                }

            if (!edges.empty())
            {
                std::vector<uint8_t> sdf(alpha.size(), 0);
                const float spreadF = static_cast<float>(spread);
                for (int y = 0; y < dstHeight; ++y)
                    for (int x = 0; x < dstWidth; ++x)
                    {
                        const bool inside =
                            pixelInside(alpha, dstWidth, dstHeight, x, y);
                        float minDistanceSquared =
                            std::numeric_limits<float>::max();
                        for (const EdgePoint &edge : edges)
                        {
                            const float dx = float(x - edge.x);
                            const float dy = float(y - edge.y);
                            minDistanceSquared = std::min(
                                minDistanceSquared, dx * dx + dy * dy);
                        }
                        float normalized =
                            std::sqrt(minDistanceSquared) / spreadF;
                        normalized = std::min(normalized, 1.0f);
                        const float value =
                            inside ? 0.5f + 0.5f * normalized
                                   : 0.5f - 0.5f * normalized;
                        sdf[static_cast<size_t>(y) * dstWidth + x] =
                            static_cast<uint8_t>(value * 255.0f + 0.5f);
                    }

                entry.sdf = std::move(sdf);
                entry.width = dstWidth;
                entry.height = dstHeight;
                entry.left = ix0 - pad;
                entry.top = -iy0 + pad;
                entry.valid = true;
            }
        }

        entry.advanceX = static_cast<float>(advanceWidth) * scale;
    }

    auto inserted = glyphCache_.emplace(codepoint, std::move(entry));
    return inserted.first->second;
}

const ShxGlyphSlot &LoadedFont::shxGlyph(std::uint32_t codepoint) const
{
    static const ShxGlyphSlot invalid;
    auto it = shxSlots_.find(codepoint);
    return it != shxSlots_.end() ? it->second : invalid;
}

} // namespace rendering
