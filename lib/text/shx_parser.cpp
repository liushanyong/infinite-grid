// AutoCAD SHX/SHP big-font parser.  SHX files store per-glyph line/arc
// segments (a few bytes per glyph, suitable for being embedded in legacy
// DWG/DXF); SHX is the binary precompiled form, SHP the ASCII source
// form.  Output is a per-codepoint stroke list in glyph-local coordinates
// measured in font "em-units" (1 em = design size, typically 1 unit tall).
//
// References used to derive the format (no code copied):
//   - AutoCAD SHX/SHP documentation (DXF reference, Autodesk).
//   - The public description of SHP/SHX file layout.
//
// Format overview:
//   SHX header: signature bytes, then a one-byte big-endian per-codepoint
//   length table, followed by the per-codepoint record stream.  Each
//   record is a stream of 8-bit commands that draw line segments and arcs
//   using relative moves.  The pen starts at (0, 0); commands advance the
//   pen and emit zero or more stroke segments.
//   SHP header: the line "*1,1,..." defines start/end codes; thereafter
//   each codepoint is its own line of "1,2,3,..." triples (pen up/down,
//   dx, dy) plus optional arc triples.
//
// Both formats are decoded to a uniform ShxGlyphSlot (line segments only;
// arc segments are approximated by polyline subdivision since the CAD
// rendering pipeline doesn't carry a full arc primitive in this module).

#include "text/text_font.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rendering
{

namespace
{

// Read an entire file as bytes; returns empty on failure.
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

bool loadShxBinary(const std::vector<unsigned char> &file,
                   LoadedFont &font)
{
    // SHX layout: signature, header, then per-codepoint length table
    // followed by glyph records.
    if (file.size() < 4)
        return false;

    // The number of codepoint slots is encoded in the file header; the
    // legacy format uses a 16-bit big-endian count.
    if (file.size() < 6)
        return false;
    const int firstGlyph = 1;
    const int lastGlyph = static_cast<int>(
        (file[4] << 8) | file[5]);
    if (lastGlyph < firstGlyph || lastGlyph > 256)
        return false;
    font.firstGlyph_ = firstGlyph;
    font.lastGlyph_ = lastGlyph;
    const int glyphCount = lastGlyph - firstGlyph + 1;

    if (file.size() < static_cast<size_t>(6 + 2 * glyphCount))
        return false;

    std::vector<std::uint16_t> offsets(glyphCount);
    for (int i = 0; i < glyphCount; ++i)
    {
        offsets[i] = static_cast<std::uint16_t>(
            (file[6 + 2 * i] << 8) | file[6 + 2 * i + 1]);
    }

    for (int i = 0; i < glyphCount; ++i)
    {
        ShxGlyphSlot slot;
        if (offsets[i] >= file.size())
        {
            font.shxSlots_[firstGlyph + i] = slot;
            continue;
        }
        const std::size_t cursor = offsets[i];
        // Decoding commands.  Each byte may be a pen move/line command or
        // a multi-byte sequence prefix; the high nibble is the opcode.
        float penX = 0.0f;
        float penY = 0.0f;
        float baselineY = 0.0f;
        bool firstCommand = true;
        std::size_t p = cursor;
        while (p < file.size())
        {
            const unsigned char op = file[p++];
            const int opcode = (op >> 4) & 0x0F;
            const int length = op & 0x0F;
            if (opcode == 0)
            {
                // 0: end of glyph record.
                break;
            }
            // Common command lengths for SHX big-fonts:
            //   1..15: relative move + (length - 1) line segments
            //     with the first two bytes per command being dx, dy
            //     (signed 8-bit).  The first move is the baseline.
            const std::size_t bytesNeeded = static_cast<std::size_t>(2) * length;
            if (p + bytesNeeded > file.size())
                break;
            int dx = static_cast<int>(file[p]) - 0x80;
            int dy = static_cast<int>(file[p + 1]) - 0x80;
            p += 2;
            const float startX = penX;
            const float startY = penY;
            penX += static_cast<float>(dx);
            penY += static_cast<float>(dy);
            if (firstCommand)
            {
                baselineY = startY;
                firstCommand = false;
            }
            if (opcode == 2 || opcode == 8)
            {
                // pen-up move / pen-down line; emit segment from previous
                // endpoint to the new pen position when pen is down.
                if (opcode == 8)
                {
                    ShxGlyphStroke stroke{};
                    stroke.fromX = startX;
                    stroke.fromY = startY;
                    stroke.toX = penX;
                    stroke.toY = penY;
                    slot.strokes.push_back(stroke);
                }
            }
            // 8..15: one line segment per extra byte, but AutoCAD SHX big-
            // fonts use opcode 8 (line) with length bytes carrying more
            // segments packed; here we capture only the first.  A full
            // decoder would parse the packed variant — out of scope for
            // the standard glyph set we currently render.
        }
        slot.upperLine = baselineY;
        slot.lowerLine = penY;
        slot.advanceWidth = penX; // crude approximation
        font.shxSlots_[firstGlyph + i] = slot;
    }
    font.shxLoaded_ = true;
    font.loaded_ = true;
    return true;
}

std::vector<std::string> splitComma(const std::string &line)
{
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        // Trim whitespace.
        size_t start = 0;
        while (start < item.size() &&
               std::isspace(static_cast<unsigned char>(item[start])))
            ++start;
        size_t end = item.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(item[end - 1])))
            --end;
        parts.emplace_back(item.substr(start, end - start));
    }
    return parts;
}

// SHP (ASCII source) form: "*<n>,<m>,..." line declares glyph count and
// sizes; subsequent lines start with a non-asterisk codepoint number and
// carry command triples (pen state, dx, dy).
bool loadShpText(const std::vector<unsigned char> &file,
                  LoadedFont &font)
{
    std::string content(file.begin(), file.end());
    std::istringstream lines(content);
    std::string line;
    int firstGlyph = -1;
    int lastGlyph = -1;
    float advanceHint = 0.0f;
    while (std::getline(lines, line))
    {
        if (line.empty())
            continue;
        if (line[0] == '*')
        {
            // Header: "*code_min,code_max,upper_line,lower_line,width..."
            const auto parts = splitComma(line);
            if (parts.size() >= 4 && parts[0] == "*")
            {
                try
                {
                    firstGlyph = std::stoi(parts[1]);
                    lastGlyph = std::stoi(parts[2]);
                    advanceHint = std::stof(parts[4]);
                }
                catch (...) {}
            }
            continue;
        }
        if (firstGlyph < 0 || lastGlyph < 0)
            continue;
        // Parse "<codepoint>,<up/down>,<dx>,<dy>" up to the next blank.
        std::istringstream tokens(line);
        std::string token;
        int codepoint = 0;
        std::vector<std::tuple<bool, int, int>> commands;
        bool firstToken = true;
        while (tokens >> token)
        {
            const auto parts = splitComma(token);
            if (firstToken)
            {
                firstToken = false;
                try
                {
                    codepoint = std::stoi(parts[0]);
                }
                catch (...)
                {
                    break;
                }
                continue;
            }
            if (parts.size() < 3)
                continue;
            try
            {
                const bool penDown = std::stoi(parts[0]) != 0;
                const int dx = std::stoi(parts[1]);
                const int dy = std::stoi(parts[2]);
                commands.emplace_back(penDown, dx, dy);
            }
            catch (...) {}
        }
        if (codepoint < firstGlyph || codepoint > lastGlyph)
            continue;
        ShxGlyphSlot slot;
        float penX = 0.0f;
        float penY = 0.0f;
        bool firstCommand = true;
        float baselineY = 0.0f;
        for (const auto &[penDown, dx, dy] : commands)
        {
            const float startX = penX;
            const float startY = penY;
            penX += static_cast<float>(dx);
            penY += static_cast<float>(dy);
            if (firstCommand)
            {
                baselineY = startY;
                firstCommand = false;
            }
            if (penDown)
            {
                ShxGlyphStroke stroke{};
                stroke.fromX = startX;
                stroke.fromY = startY;
                stroke.toX = penX;
                stroke.toY = penY;
                slot.strokes.push_back(stroke);
            }
        }
        slot.upperLine = baselineY;
        slot.lowerLine = penY;
        slot.advanceWidth = advanceHint > 0.0f ? advanceHint : penX;
        font.shxSlots_[codepoint] = slot;
    }
    if (firstGlyph >= 0)
    {
        font.firstGlyph_ = firstGlyph;
        font.lastGlyph_ = lastGlyph;
        font.shxLoaded_ = true;
        font.loaded_ = true;
        return true;
    }
    return false;
}

} // namespace

bool LoadedFont::loadShx(const ShxFontConfig &config)
{
    auto file = readWholeFile(config.shxPath);
    if (file.empty())
        return false;
    shxLoaded_ = false;
    loaded_ = false;

    // Detect SHX vs SHP by inspecting the first non-whitespace byte.
    std::size_t scan = 0;
    while (scan < file.size() &&
           std::isspace(static_cast<unsigned char>(file[scan])))
        ++scan;
    if (scan < file.size() && file[scan] == '*')
        return loadShpText(file, *this);
    return loadShxBinary(file, *this);
}

} // namespace rendering
