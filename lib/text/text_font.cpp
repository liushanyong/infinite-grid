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

const std::vector<GlyphFillTriangle> &LoadedFont::glyphFillTriangles(
    std::uint32_t codepoint)
{
    auto it = fillTriangleCache_.find(codepoint);
    if (it != fillTriangleCache_.end())
        return it->second;

    std::vector<GlyphFillTriangle> triangles;
    auto *info = static_cast<stbtt_fontinfo *>(fontInfo_);
    if (info && sdfLoaded_)
    {
        const float scale = stbtt_ScaleForPixelHeight(info, pixelsPerEm_);
        stbtt_vertex *vertices = nullptr;
        const int vertexCount =
            stbtt_GetGlyphShape(info, stbtt_FindGlyphIndex(
                                          info, static_cast<int>(codepoint)),
                                &vertices);

        // Flatten the outline (move/line/quadratic) into polylines in em
        // units, y-up (stbtt shape coordinates are font-up).
        std::vector<std::vector<float>> contourX;
        std::vector<std::vector<float>> contourY;
        float currentX = 0.0f;
        float currentY = 0.0f;
        constexpr int kCurveSegments = 10;
        for (int i = 0; i < vertexCount; ++i)
        {
            const stbtt_vertex &v = vertices[i];
            const float tx = v.x * scale;
            const float ty = v.y * scale;
            const float tcx = v.cx * scale;
            const float tcy = v.cy * scale;
            switch (v.type)
            {
            case STBTT_vmove:
                contourX.emplace_back();
                contourY.emplace_back();
                contourX.back().push_back(tx);
                contourY.back().push_back(ty);
                currentX = tx;
                currentY = ty;
                break;
            case STBTT_vline:
                contourX.back().push_back(tx);
                contourY.back().push_back(ty);
                currentX = tx;
                currentY = ty;
                break;
            case STBTT_vcurve:
            {
                // Quadratic Bezier from (current) via (tcx,tcy) to (tx,ty).
                for (int s = 1; s <= kCurveSegments; ++s)
                {
                    const float t = float(s) / kCurveSegments;
                    const float it = 1.0f - t;
                    const float px = it * it * currentX +
                                     2.0f * it * t * tcx + t * t * tx;
                    const float py = it * it * currentY +
                                     2.0f * it * t * tcy + t * t * ty;
                    contourX.back().push_back(px);
                    contourY.back().push_back(py);
                }
                currentX = tx;
                currentY = ty;
                break;
            }
            default: // cubic (cff): same flatten with two controls
            {
                for (int s = 1; s <= kCurveSegments; ++s)
                {
                    const float t = float(s) / kCurveSegments;
                    const float it = 1.0f - t;
                    const float px = it * it * it * currentX +
                                     3.0f * it * it * t * tcx +
                                     3.0f * it * t * t * v.cx1 +
                                     t * t * t * tx;
                    const float py = it * it * it * currentY +
                                     3.0f * it * it * t * tcy +
                                     3.0f * it * t * t * v.cy1 +
                                     t * t * t * ty;
                    contourX.back().push_back(px);
                    contourY.back().push_back(py);
                }
                currentX = tx;
                currentY = ty;
                break;
            }
            }
        }
        stbtt_FreeShape(info, vertices);

        // Even-odd trapezoid decomposition: bands split at every vertex Y,
        // spans interpolated on the (straight) edges — exact geometry.
        std::vector<double> bandYs;
        for (size_t c = 0; c < contourX.size(); ++c)
            for (size_t p = 0; p < contourX[c].size(); ++p)
                bandYs.push_back(contourY[c][p]);
        std::sort(bandYs.begin(), bandYs.end());
        bandYs.erase(std::unique(bandYs.begin(), bandYs.end()),
                     bandYs.end());

        struct SpanEdge
        {
            float xAtMid;
            float x0, x1; // x at band bottom/top
        };
        for (size_t b = 0; b + 1 < bandYs.size(); ++b)
        {
            const double yLow = bandYs[b];
            const double yHigh = bandYs[b + 1];
            if (yHigh - yLow < 1.0e-12)
                continue;
            const double yMid = 0.5 * (yLow + yHigh);
            std::vector<SpanEdge> spans;
            for (size_t c = 0; c < contourX.size(); ++c)
            {
                const size_t pointCount = contourX[c].size();
                for (size_t p = 0; p < pointCount; ++p)
                {
                    const size_t q = (p + 1) % pointCount;
                    const double y0 = contourY[c][p];
                    const double y1 = contourY[c][q];
                    if ((y0 <= yLow && y1 <= yLow) ||
                        (y0 >= yHigh && y1 >= yHigh))
                        continue;
                    if (y0 == y1)
                        continue;
                    const double t0 = (yLow - y0) / (y1 - y0);
                    const double t1 = (yHigh - y0) / (y1 - y0);
                    const double x0 =
                        contourX[c][p] + t0 * (contourX[c][q] - contourX[c][p]);
                    const double x1 =
                        contourX[c][p] + t1 * (contourX[c][q] - contourX[c][p]);
                    const double tm = (yMid - y0) / (y1 - y0);
                    spans.push_back({float(contourX[c][p] +
                                           tm * (contourX[c][q] -
                                                 contourX[c][p])),
                                     float(x0), float(x1)});
                }
            }
            std::sort(spans.begin(), spans.end(),
                      [](const SpanEdge &a, const SpanEdge &b) {
                          return a.xAtMid < b.xAtMid;
                      });
            // Even-odd pairing: (0,1), (2,3), ...
            for (size_t i = 0; i + 1 < spans.size(); i += 2)
            {
                const SpanEdge &left = spans[i];
                const SpanEdge &right = spans[i + 1];
                triangles.push_back({left.x0, float(yLow), right.x0,
                                     float(yLow), right.x1, float(yHigh)});
                triangles.push_back({left.x0, float(yLow), right.x1,
                                     float(yHigh), left.x1, float(yHigh)});
            }
        }
    }

    auto inserted =
        fillTriangleCache_.emplace(codepoint, std::move(triangles));
    return inserted.first->second;
}

const ShxGlyphSlot &LoadedFont::shxGlyph(std::uint32_t codepoint) const
{
    static const ShxGlyphSlot invalid;
    auto it = shxSlots_.find(codepoint);
    return it != shxSlots_.end() ? it->second : invalid;
}

} // namespace rendering
