// AutoCAD SHX/SHP font parser.  Format knowledge derived from the public
// AutoCAD shape-file documentation and cross-checked against the
// CADplatformer reference (MIT); the implementation here is original.
//
// Compiled SHX types (header string at byte offset 11 selects):
//   unifont — legacy single-byte font: u16 glyph count at [25], shape
//             definitions at [31] as (code, defbytes, data...) records.
//   bigfont — multi-byte font: u16 index count at [27], u16 range count
//             at [29], code ranges at [31..], then a (code, defbytes,
//             offset) index per glyph.
//   shapes  — regular font / shape file: u16 count at [28], fixed-size
//             index at [30], glyph data after the index.
//
// Glyph definition commands (after the name string):
//   0 end; 1 pen down; 2 pen up; 3/4 scale divide/multiply;
//   5/6 position push/pop; 7 subshape; 8 (dx,dy) signed move;
//   9 repeated moves until (0,0); 0xA octant arc; 0x10..0x1F the classic
//   length/direction vectors (high nibble length, low nibble direction).

#include "text/text_font.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stack>
#include <vector>

namespace rendering
{

namespace
{

constexpr double kPi = 3.14159265358979323846;

enum class ShxKind
{
    Unknown,
    Unifont,
    Bigfont,
    Regfont,
    Shapefile
};

// Decoding state shared across one glyph definition.
struct ShxDecodeContext
{
    std::vector<ShxGlyphStroke> strokes;
    float penX = 0.0f;
    float penY = 0.0f;
    float scale = 1.0f;
    bool drawMode = true;
    std::stack<float> positionStack;
};

void emitSegment(ShxDecodeContext &state, float toX, float toY)
{
    if (!state.drawMode)
        return;
    ShxGlyphStroke stroke;
    stroke.fromX = state.penX;
    stroke.fromY = state.penY;
    stroke.toX = toX;
    stroke.toY = toY;
    state.strokes.push_back(stroke);
}

// Command 8/9: signed (dx,dy) pair; (0,0) terminates command 9 runs.
bool decodeMovePair(ShxDecodeContext &state, const unsigned char *&data,
                    int &remaining)
{
    if (remaining < 2)
        return false;
    const int dx = static_cast<signed char>(data[0]);
    const int dy = static_cast<signed char>(data[1]);
    data += 2;
    remaining -= 2;
    if (dx == 0 && dy == 0)
        return false;
    const float toX = state.penX + state.scale * dx;
    const float toY = state.penY + state.scale * dy;
    emitSegment(state, toX, toY);
    state.penX = toX;
    state.penY = toY;
    return true;
}

// Command 0xA: octant arc (radius byte, signed octant scheme byte).
void decodeOctantArc(ShxDecodeContext &state, const unsigned char *&data,
                     int &remaining)
{
    if (remaining < 2)
    {
        remaining = 0;
        return;
    }
    const int radius = data[0];
    const signed char scheme = static_cast<signed char>(data[1]);
    data += 2;
    remaining -= 2;
    const int sign = scheme < 0 ? -1 : 1;
    const int startOctant = (scheme & 0x70) >> 4;
    const int octantCount = (scheme & 0x07) * sign;
    const double r = radius * state.scale;
    const double startAngle = startOctant * kPi / 4.0;
    const double endAngle = (startOctant + octantCount) * kPi / 4.0;
    const double centerX = state.penX - r * std::cos(startAngle);
    const double centerY = state.penY - r * std::sin(startAngle);
    constexpr int kSegmentsPerOctant = 4;
    const int segments =
        std::max(1, std::abs(octantCount) * kSegmentsPerOctant);
    for (int i = 1; i <= segments; ++i)
    {
        const double angle =
            startAngle + (endAngle - startAngle) * i / segments;
        const float toX = float(centerX + r * std::cos(angle));
        const float toY = float(centerY + r * std::sin(angle));
        emitSegment(state, toX, toY);
        state.penX = toX;
        state.penY = toY;
    }
}

// Commands 0x10..0x1F: classic length/direction vectors — high nibble
// length, low nibble one of 16 directions.
void decodeLengthDirection(ShxDecodeContext &state, unsigned char code)
{
    static const float kDirections[16][2] = {
        {1, 0},     {1, 0.5},   {1, 1},     {0.5, 1},   {0, 1},
        {-0.5, 1},  {-1, 1},    {-1, 0.5},  {-1, 0},    {-1, -0.5},
        {-1, -1},   {-0.5, -1}, {0, -1},    {0.5, -1},  {1, -1},
        {1, -0.5}};
    const int length = (code & 0xF0) >> 4;
    const int direction = code & 0x0F;
    const float toX =
        state.penX + kDirections[direction][0] * length * state.scale;
    const float toY =
        state.penY + kDirections[direction][1] * length * state.scale;
    emitSegment(state, toX, toY);
    state.penX = toX;
    state.penY = toY;
}

void decodeOneCommand(ShxDecodeContext &state, const unsigned char *&data,
                      int &remaining)
{
    if (remaining <= 0)
        return;
    const unsigned char code = data[0];
    ++data;
    --remaining;
    switch (code)
    {
    case 0:
        remaining = 0; // end of shape definition
        break;
    case 1:
        state.drawMode = true;
        break;
    case 2:
        state.drawMode = false;
        break;
    case 3:
        if (remaining > 0)
        {
            state.scale /= data[0];
            ++data;
            --remaining;
        }
        break;
    case 4:
        if (remaining > 0)
        {
            state.scale *= data[0];
            ++data;
            --remaining;
        }
        break;
    case 5:
        state.positionStack.push(state.penX);
        state.positionStack.push(state.penY);
        break;
    case 6:
        if (state.positionStack.size() >= 2)
        {
            const float y = state.positionStack.top();
            state.positionStack.pop();
            const float x = state.positionStack.top();
            state.positionStack.pop();
            emitSegment(state, x, y);
            state.penX = x;
            state.penY = y;
        }
        break;
    case 8:
        decodeMovePair(state, data, remaining);
        break;
    case 9:
        while (remaining > 0 && decodeMovePair(state, data, remaining))
        {
        }
        break;
    case 0x0A:
        decodeOctantArc(state, data, remaining);
        break;
    default:
        // 0x0B (pie arcs) and 0x0E (odd-even pen toggling) are rare in
        // stroke fonts; everything else is a length/direction vector.
        decodeLengthDirection(state, code);
        break;
    }
}

ShxDecodeContext decodeGlyph(const unsigned char *data, int defBytes)
{
    ShxDecodeContext state;
    while (defBytes > 0)
        decodeOneCommand(state, data, defBytes);
    return state;
}

float maxPenX(const ShxDecodeContext &state)
{
    float maximum = 0.0f;
    for (const ShxGlyphStroke &stroke : state.strokes)
        maximum = std::max({maximum, stroke.fromX, stroke.toX});
    return maximum;
}

void storeGlyph(LoadedFont &font, std::uint32_t codepoint,
                const ShxDecodeContext &state, double fontHeight)
{
    const float emPerUnit = float(1.0 / fontHeight);
    ShxGlyphSlot slot;
    slot.strokes.reserve(state.strokes.size());
    for (const ShxGlyphStroke &stroke : state.strokes)
    {
        ShxGlyphStroke normalized;
        normalized.fromX = stroke.fromX * emPerUnit;
        // SHX shape space is y-down (baseline at 0, glyph body at -1);
        // normalize to y-up so consumers place glyphs with a plain
        // positive up vector.
        normalized.fromY = -stroke.fromY * emPerUnit;
        normalized.toX = stroke.toX * emPerUnit;
        normalized.toY = -stroke.toY * emPerUnit;
        slot.strokes.push_back(normalized);
    }
    slot.advanceWidth = std::max(maxPenX(state) * emPerUnit, 0.4f);
    slot.valid = true;
    font.shxSlots_[codepoint] = std::move(slot);
}

bool shxLoadCompiled(const std::vector<unsigned char> &file,
                     LoadedFont &font)
{
    if (file.size() < 36)
        return false;

    ShxKind kind = ShxKind::Unknown;
    std::uint16_t glyphCount = 0;
    double fontHeight = 0.0;
    const unsigned char *shapeDefs = nullptr;
    const unsigned char *index = nullptr;

    if (std::memcmp(&file[11], "unifont", 7) == 0)
    {
        kind = ShxKind::Unifont;
        glyphCount = *reinterpret_cast<const std::uint16_t *>(&file[25]);
        shapeDefs = &file[31];
        while (shapeDefs < file.data() + file.size() && *shapeDefs != 0)
            ++shapeDefs;
        ++shapeDefs;
        fontHeight = shapeDefs[0];
        const double descendHeight = shapeDefs[1];
        if (fontHeight == 0)
            fontHeight = descendHeight;
        shapeDefs += 6;
    }
    else if (std::memcmp(&file[11], "bigfont", 7) == 0)
    {
        kind = ShxKind::Bigfont;
        glyphCount = *reinterpret_cast<const std::uint16_t *>(&file[27]);
        const std::uint16_t rangeCount =
            *reinterpret_cast<const std::uint16_t *>(&file[29]);
        index = &file[31 + size_t(rangeCount) * 4];
    }
    else if (std::memcmp(&file[11], "shapes", 6) == 0)
    {
        kind = ShxKind::Regfont;
        glyphCount = *reinterpret_cast<const std::uint16_t *>(&file[28]);
        index = &file[30];
    }

    if (kind == ShxKind::Unknown)
        return false;
    if (fontHeight <= 0.0)
        fontHeight = 1.0;

    if (kind == ShxKind::Unifont)
    {
        const unsigned char *entry = shapeDefs;
        for (std::uint16_t i = 0; i < glyphCount && entry + 4 < file.data() + file.size();
             ++i)
        {
            const std::uint16_t code = entry[0] | (entry[1] << 8);
            const std::uint16_t defBytes = entry[2] | (entry[3] << 8);
            entry += 4;
            // Record: name string (zero terminated), then definition bytes.
            const unsigned char *nameEnd = entry;
            while (nameEnd < file.data() + file.size() && *nameEnd != 0)
                ++nameEnd;
            ++nameEnd;
            int remaining = int(defBytes) - int(nameEnd - entry);
            if (code > 0 && remaining > 0 && nameEnd < file.data() + file.size())
                storeGlyph(font, code, decodeGlyph(nameEnd, remaining),
                           fontHeight);
            entry += defBytes;
        }
    }
    else if (kind == ShxKind::Bigfont)
    {
        // Index entries are 4 x u16: code, defBytes, offset lo, offset hi.
        const std::uint16_t *entry =
            reinterpret_cast<const std::uint16_t *>(index);
        for (std::uint16_t i = 0; i < glyphCount; ++i)
        {
            const std::uint16_t code = entry[0];
            const std::uint16_t defBytes = entry[1];
            const std::uint32_t offset =
                entry[2] | (std::uint32_t(entry[3]) << 16);
            if (defBytes > 0 && offset < file.size())
            {
                const unsigned char *data = &file[offset];
                while (data < file.data() + file.size() && *data != 0)
                    ++data; // skip name string
                ++data;
                int remaining = int(defBytes) - 1;
                if (remaining > 0 && data < file.data() + file.size())
                    storeGlyph(font, code, decodeGlyph(data, remaining),
                               fontHeight);
            }
            entry += 4;
        }
    }
    else // Regfont / shapefile
    {
        const std::uint16_t *entry =
            reinterpret_cast<const std::uint16_t *>(index);
        const unsigned char *firstGlyph = index + size_t(glyphCount) * 4;
        std::uint32_t offset = 0;
        for (std::uint16_t i = 0; i < glyphCount; ++i)
        {
            const std::uint16_t code = entry[0];
            const std::uint16_t defBytes = entry[1];
            const unsigned char *data = firstGlyph + offset;
            offset += defBytes;
            if (defBytes > 0 && data < file.data() + file.size())
            {
                const unsigned char *nameEnd = data;
                while (nameEnd < file.data() + file.size() && *nameEnd != 0)
                    ++nameEnd;
                ++nameEnd;
                int remaining = int(defBytes) - int(nameEnd - data);
                if (remaining > 0 && nameEnd < file.data() + file.size())
                    storeGlyph(font, code, decodeGlyph(nameEnd, remaining),
                               fontHeight);
            }
            entry += 2;
        }
    }

    font.shxLoaded_ = !font.shxSlots_.empty();
    font.loaded_ = font.shxLoaded_;
    return font.shxLoaded_;
}

} // namespace

bool LoadedFont::loadShx(const ShxFontConfig &config)
{
    std::ifstream in(config.shxPath, std::ios::binary | std::ios::ate);
    if (!in)
        return false;
    const std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> file(static_cast<size_t>(size));
    if (!in.read(reinterpret_cast<char *>(file.data()), size))
        return false;

    shxSlots_.clear();
    shxLoaded_ = false;
    loaded_ = sdfLoaded_; // SHX failure must not invalidate a loaded TTF

    // Compiled SHX starts with "AutoCAD-86 "; the ASCII SHP source form
    // starts with '*' and is not needed for the demo (compiled forms ship
    // under resources/fonts).
    if (file.size() > 36 && std::memcmp(&file[0], "AutoCAD", 7) == 0)
        return shxLoadCompiled(file, *this);
    return false;
}

} // namespace rendering
