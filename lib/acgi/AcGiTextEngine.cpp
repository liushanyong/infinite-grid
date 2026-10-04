// AcGi text engine implementation.  Drawing logic mirrors the ObjectARX
// model: the request (message + placement + style metrics) is expanded
// into per-glyph quads through the loaded font, and the renderer submits
// each glyph as its own textured quad.

#include "AcGiTextEngine.h"

#include "rendering/RendererBackend.h"

#include <algorithm>
#include <array>
#include <cstring>

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

int TextEngine::drawText(rendering::RendererBackend &backend,
                         const glm::mat4 &view, const glm::mat4 &projection,
                         const glm::dvec3 &cameraPos,
                         const glm::dvec3 &cameraRight,
                         const glm::dvec3 &cameraUp,
                         const glm::dvec3 &cameraFront,
                         const TextRequest &request)
{
    if (!sdfReady())
        return 0;

    // CAD text is planar: the quad lies in the entity's plane defined by
    // its direction (baseline) and normal, not billboarded to the camera.
    glm::dvec3 textRight = request.direction;
    if (glm::dot(textRight, textRight) < 1.0e-18)
        textRight = glm::dvec3(1.0, 0.0, 0.0);
    textRight = glm::normalize(textRight);
    glm::dvec3 textUp =
        glm::cross(glm::normalize(request.normal), textRight);
    if (glm::dot(textUp, textUp) < 1.0e-18)
        textUp = glm::dvec3(0.0, 0.0, 1.0);
    textUp = glm::normalize(textUp);
    const double emToWorld =
        request.height / double(sdfFont_.pixelsPerEm());

    // Multi-line: advance along -up per newline.
    glm::dvec3 lineOrigin = request.position;
    double penX = 0.0;
    int glyphsDrawn = 0;

    for (unsigned char character : request.message)
    {
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

        // Lazily upload this glyph's distance field.
        uint32_t textureId = 0;
        if (const auto it = glyphTextureIds_.find(character);
            it != glyphTextureIds_.end())
        {
            textureId = it->second;
        }
        else
        {
            textureId = backend.uploadGlyphSdf(glyph.sdf.data(), glyph.width,
                                               glyph.height);
            glyphTextureIds_[character] = textureId;
        }

        if (textureId != 0)
        {
            const glm::dvec3 quadLeftEdge =
                lineOrigin +
                textRight * ((penX + glyph.left) * emToWorld) +
                textUp * ((glyph.top - glyph.height) * emToWorld);
            const glm::dvec3 quadWidth =
                textRight * (glyph.width * emToWorld);
            const glm::dvec3 quadHeight =
                textUp * (glyph.height * emToWorld);
            auto pushVertex = [&](double cornerU, double cornerV, float u,
                                  float v) {
                // Transform into VIEW space with the camera basis so the
                // identity view passed to the renderer places the quad
                // exactly where the world position appears on screen.
                const glm::dvec3 relative =
                    quadLeftEdge + quadWidth * cornerU +
                    quadHeight * cornerV - cameraPos;
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
            backend.drawSdfGlyphQuad(identityView, projection, textureId,
                                     vertices);
            ++glyphsDrawn;
        }
        penX += glyph.advanceX;
    }
    return glyphsDrawn;
}

std::vector<rendering::ShxGlyphStroke> TextEngine::shxStrokes(
    const std::string &message) const
{
    std::vector<rendering::ShxGlyphStroke> strokes;
    double penX = 0.0;
    for (unsigned char character : message)
    {
        const rendering::ShxGlyphSlot &glyph =
            shxFontFor(character).shxGlyph(character);
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
    for (unsigned char character : message)
    {
        const rendering::ShxGlyphSlot &glyph =
            shxFontFor(character).shxGlyph(character);
        advance += glyph.valid ? glyph.advanceWidth : 0.5;
    }
    return advance;
}

} // namespace acgi
