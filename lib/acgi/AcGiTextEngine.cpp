#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// AcGi text engine implementation.  Drawing logic mirrors the ObjectARX
// model: the request (message + placement + style metrics) is expanded
// into per-glyph quads through the loaded font, and the renderer submits
// each glyph as its own textured quad.

#include "AcGiTextEngine.h"



#include <algorithm>
#include <array>
#include <cstring>
#ifdef _WIN32
#endif
#include <limits>

namespace acgi
{

TextEngine &textEngine()
{
    static TextEngine engine;
    return engine;
}

bool TextEngine::loadSdfFont(const std::string &ttfPath, float pixelsPerEm)
{
    rendering::SdfFontConfig config;
    config.ttfPath = ttfPath;
    config.pixelsPerEm = pixelsPerEm;
    return sdfFont_.loadSdf(config);
}

bool TextEngine::loadShxRegularFont(const std::string &shxPath)
{
    rendering::ShxFontConfig config;
    config.shxPath = shxPath;
    return shxRegularFont_.loadShx(config);
}

bool TextEngine::loadShxBigFont(const std::string &shxPath)
{
    rendering::ShxFontConfig config;
    config.shxPath = shxPath;
    return shxBigFont_.loadShx(config);
}

const rendering::LoadedFont &TextEngine::shxFontFor(
    unsigned char character) const
{
    // AutoCAD bigfont pairing: single-byte codes from the regular font,
    // double-byte codes from the big font.
    if (character >= 128 && shxBigFont_.hasShx())
        return shxBigFont_;
    return shxRegularFont_;
}

// Shared layout walk: the single source of glyph placement for BOTH the
// visible SDF pass and the GPU pick pass, so the ID texture matches the
// visible glyphs pixel for pixel.
std::vector<TextEngine::GlyphPlacement> TextEngine::layoutGlyphs(
    const TextRequest &request,
    const glm::dvec3 &cameraPos, const glm::dvec3 &cameraRight,
    const glm::dvec3 &cameraUp, const glm::dvec3 &cameraFront)
{
    std::vector<GlyphPlacement> placements;
    if (!sdfReady())
        return placements;

    glm::dvec3 textRight;
    glm::dvec3 textUp;
    if (request.billboard)
    {
        // Billboard mode: the quad always faces the camera (kept as a
        // separate selectable mode for screen-anchored labels).
        textUp = glm::dvec3(0.0, 0.0, 1.0);
        textRight = glm::normalize(
            glm::cross(textUp, glm::normalize(cameraPos - request.position)));
    }
    else
    {
        // CAD text is planar: the quad lies in the entity's plane defined
        // by its direction (baseline) and normal.
        textRight = request.direction;
        if (glm::dot(textRight, textRight) < 1.0e-18)
            textRight = glm::dvec3(1.0, 0.0, 0.0);
        textRight = glm::normalize(textRight);
        textUp = glm::cross(glm::normalize(request.normal), textRight);
        if (glm::dot(textUp, textUp) < 1.0e-18)
            textUp = glm::dvec3(0.0, 0.0, 1.0);
        textUp = glm::normalize(textUp);
    }
    const double emToWorld =
        request.height / double(sdfFont_.pixelsPerEm());

    // Multi-line: advance along -up per newline.  The message is UTF-8:
    // decode to codepoints so CJK text resolves through the font's cmap.
    glm::dvec3 lineOrigin = request.position;
    double penX = 0.0;

    auto nextCodepoint = [](const std::string &text, size_t &i) -> uint32_t {
        const unsigned char lead = text[i];
        const size_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2
                           : lead >= 0xC0 ? 1
                                          : 0;
        if (extra == 0)
        {
            ++i;
            return lead;
        }
        if (i + extra >= text.size())
        {
            // Truncated sequence: consume the lead byte as Latin-1.
            ++i;
            return lead;
        }
        uint32_t codepoint = lead & uint32_t(0x3F >> extra);
        for (size_t k = 1; k <= extra; ++k)
            codepoint = (codepoint << 6) | uint32_t(text[i + k] & 0x3F);
        i += extra + 1;
        return codepoint;
    };

    for (size_t position = 0; position < request.message.size();)
    {
        const uint32_t character = nextCodepoint(request.message, position);
        if (character == 10) // newline
        {
            lineOrigin -= textUp * request.height * 1.35;
            penX = 0.0;
            continue;
        }

        const rendering::GlyphSdfRaster &glyph =
            sdfFont_.glyphSdf(character);
        if (!glyph.valid)
        {
            penX += 0.5 * double(sdfFont_.pixelsPerEm());
            continue;
        }

        const glm::dvec3 quadLeftEdge =
            lineOrigin +
            textRight * ((penX + glyph.left) * emToWorld) +
            textUp * ((glyph.top - glyph.height) * emToWorld);
        const glm::dvec3 quadWidth =
            textRight * (glyph.width * emToWorld);
        const glm::dvec3 quadHeight =
            textUp * (glyph.height * emToWorld);
        GlyphPlacement placement;
        placement.codepoint = character;
        placement.corners = {quadLeftEdge,
                             quadLeftEdge + quadWidth,
                             quadLeftEdge + quadWidth + quadHeight,
                             quadLeftEdge + quadHeight};
        placements.push_back(std::move(placement));
        penX += glyph.advanceX;
    }
    return placements;
}

int TextEngine::drawText(TextDevice &device,
                         const glm::mat4 &view, const glm::mat4 &projection,
                         const glm::dvec3 &cameraPos,
                         const glm::dvec3 &cameraRight,
                         const glm::dvec3 &cameraUp,
                         const glm::dvec3 &cameraFront,
                         const TextRequest &request)
{
    const std::vector<GlyphPlacement> placements =
        layoutGlyphs(request, cameraPos, cameraRight, cameraUp,
                     cameraFront);

    int glyphsDrawn = 0;
    for (const GlyphPlacement &placement : placements)
    {
        const rendering::GlyphSdfRaster &glyph =
            sdfFont_.glyphSdf(placement.codepoint);

        // Lazily upload this glyph's distance field.
        uint32_t textureId = 0;
        if (const auto it = glyphTextureIds_.find(placement.codepoint);
            it != glyphTextureIds_.end())
        {
            textureId = it->second;
        }
        else
        {
            textureId = device.uploadGlyphSdf(glyph.sdf.data(), glyph.width,
                                               glyph.height);
            glyphTextureIds_[placement.codepoint] = textureId;
        }

        if (textureId != 0)
        {
            auto pushVertex = [&](double cornerU, double cornerV, float u,
                                  float v) {
                // Transform into VIEW space with the camera basis so the
                // identity view passed to the renderer places the quad
                // exactly where the world position appears on screen.
                const glm::dvec3 corner =
                    placement.corners[0] +
                    (placement.corners[1] - placement.corners[0]) * cornerU +
                    (placement.corners[3] - placement.corners[0]) * cornerV;
                const glm::dvec3 relative = corner - cameraPos;
                return std::array<float, 9>{
                    float(glm::dot(relative, cameraRight)),
                    float(glm::dot(relative, cameraUp)),
                    float(-glm::dot(relative, cameraFront)),
                    u,
                    v,
                    request.color.r,
                    request.color.g,
                    request.color.b,
                    request.color.a};
            };
            const std::array<float, 9> v00 = pushVertex(0, 0, 0.0f, 1.0f);
            const std::array<float, 9> v10 = pushVertex(1, 0, 1.0f, 1.0f);
            const std::array<float, 9> v11 = pushVertex(1, 1, 1.0f, 0.0f);
            const std::array<float, 9> v01 = pushVertex(0, 1, 0.0f, 0.0f);
            float vertices[54];
            std::memcpy(vertices + 0, v00.data(), sizeof(v00));
            std::memcpy(vertices + 9, v10.data(), sizeof(v10));
            std::memcpy(vertices + 18, v11.data(), sizeof(v11));
            std::memcpy(vertices + 27, v00.data(), sizeof(v00));
            std::memcpy(vertices + 36, v11.data(), sizeof(v11));
            std::memcpy(vertices + 45, v01.data(), sizeof(v01));
            constexpr glm::mat4 identityView(1.0f);
            device.drawSdfGlyphQuad(identityView, projection, textureId,
                                     vertices);
            ++glyphsDrawn;
        }
    }
    return glyphsDrawn;
}
bool TextEngine::intersectsOrthoViewport(
    const TextRequest &request, const glm::dvec3 &cameraPos,
    const glm::dvec3 &cameraRight, const glm::dvec3 &cameraUp,
    const glm::dvec3 &cameraFront, double halfWidth,
    double halfHeight) const
{
    // Oriented text rectangle in the entity plane: baseline direction x
    // message width, in-plane up x line count.
    glm::dvec3 textRight = request.direction;
    if (glm::dot(textRight, textRight) < 1.0e-18)
        textRight = glm::dvec3(1.0, 0.0, 0.0);
    textRight = glm::normalize(textRight);
    glm::dvec3 textUp = glm::cross(glm::normalize(request.normal), textRight);
    if (glm::dot(textUp, textUp) < 1.0e-18)
        textUp = glm::dvec3(0.0, 0.0, 1.0);
    textUp = glm::normalize(textUp);

    size_t lineCount = 1;
    double longest = 0.0;
    size_t current = 0;
    for (char character : request.message)
    {
        if (character == 10)
        {
            ++lineCount;
            longest = std::max(longest, double(current));
            current = 0;
        }
        else
        {
            ++current;
        }
    }
    longest = std::max(longest, double(current));
    const double width = std::max(double(longest) * request.height *
                                      request.xScale,
                                  request.height);
    const double height = request.height * double(lineCount);

    // Camera-space AABB over the rectangle's four corners (plus a small
    // thickness along the normal so edge-on text still counts).
    const glm::dvec3 normal = glm::normalize(request.normal);
    glm::dvec3 minimum(std::numeric_limits<double>::max());
    glm::dvec3 maximum(std::numeric_limits<double>::lowest());
    for (int i = 0; i < 4; ++i)
    {
        const glm::dvec3 corner =
            request.position +
            textRight * (i & 1 ? width : 0.0) +
            textUp * (i & 2 ? height : 0.0) +
            normal * (request.height * 0.1);
        const glm::dvec3 relative = corner - cameraPos;
        const double cx = glm::dot(relative, cameraRight);
        const double cy = glm::dot(relative, cameraUp);
        const double cz = glm::dot(relative, cameraFront);
        minimum = glm::min(minimum, glm::dvec3(cx, cy, cz));
        maximum = glm::max(maximum, glm::dvec3(cx, cy, cz));
    }
    // Clamp z to a small thickness band so the AABB is not flat in depth.
    minimum.z -= request.height * 0.1;
    maximum.z += request.height * 0.1;

    return maximum.x >= -halfWidth && minimum.x <= halfWidth &&
           maximum.y >= -halfHeight && minimum.y <= halfHeight;
}

namespace
{

// Convert a UTF-8 message to AutoCAD bigfont query bytes: codepage 936
// (GB2312), where each CJK glyph is addressed by a two-byte code (lead in
// the font's escape ranges).  Non-lead bytes keep their identity so the
// regular font serves ASCII.
std::vector<unsigned char> toBigfontBytes(const std::string &utf8)
{
#ifdef _WIN32
    const int wideLength =
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()),
                            nullptr, 0);
    std::wstring wide;
    if (wideLength > 0)
        wide.resize(size_t(wideLength));
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()),
                        wide.data(), wideLength);
    const int byteLength = WideCharToMultiByte(
        936, 0, wide.c_str(), wideLength, nullptr, 0, nullptr, nullptr);
    std::vector<unsigned char> bytes(size_t(byteLength), 0);
    WideCharToMultiByte(936, 0, wide.c_str(), wideLength,
                        reinterpret_cast<char *>(bytes.data()), byteLength,
                        nullptr, nullptr);
    return bytes;
#else
    return std::vector<unsigned char>(utf8.begin(), utf8.end());
#endif
}

} // namespace

std::vector<rendering::ShxGlyphStroke> TextEngine::shxStrokes(
    const std::string &message) const
{
    std::vector<rendering::ShxGlyphStroke> strokes;
    double penX = 0.0;
    const std::vector<unsigned char> bytes = toBigfontBytes(message);
    for (size_t position = 0; position < bytes.size();)
    {
        const unsigned char lead = bytes[position];
        uint32_t character = lead;
        size_t width = 1;
        // Bigfont escape-lead bytes introduce a two-byte CJK code
        // (lead << 8 | second) queried against the big-font index.
        if (lead >= 0x81 && position + 1 < bytes.size())
        {
            character = uint32_t(lead << 8) | bytes[position + 1];
            width = 2;
        }
        position += width;

        const rendering::ShxGlyphSlot &glyph =
            shxFontFor(lead).shxGlyph(character);
        if (!glyph.valid)
        {
            penX += 0.5;
            continue;
        }
        for (const rendering::ShxGlyphStroke &stroke : glyph.strokes)
        {
            rendering::ShxGlyphStroke offset = stroke;
            offset.fromX += float(penX);
            offset.toX += float(penX);
            strokes.push_back(offset);
        }
        penX += glyph.advanceWidth;
    }
    return strokes;
}

double TextEngine::shxAdvance(const std::string &message) const
{
    double advance = 0.0;
    const std::vector<unsigned char> bytes = toBigfontBytes(message);
    for (size_t position = 0; position < bytes.size();)
    {
        const unsigned char lead = bytes[position];
        uint32_t character = lead;
        size_t width = 1;
        if (lead >= 0x81 && position + 1 < bytes.size())
        {
            character = uint32_t(lead << 8) | bytes[position + 1];
            width = 2;
        }
        position += width;
        const rendering::ShxGlyphSlot &glyph =
            shxFontFor(lead).shxGlyph(character);
        advance += glyph.valid ? glyph.advanceWidth : 0.5;
    }
    return advance;
}

} // namespace acgi
