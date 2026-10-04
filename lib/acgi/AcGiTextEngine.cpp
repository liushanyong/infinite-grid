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

    glm::dvec3 textRight = request.direction;
    if (glm::dot(textRight, textRight) < 1.0e-18)
        textRight = glm::dvec3(1.0, 0.0, 0.0);
    textRight = glm::normalize(textRight);
    glm::dvec3 textUp = glm::cross(glm::normalize(request.normal), textRight);
    if (glm::dot(textUp, textUp) < 1.0e-18)
        textUp = glm::dvec3(0.0, 0.0, 1.0);
    textUp = glm::normalize(textUp);
    const double emToWorld =
        request.height / double(sdfFont_.pixelsPerEm());

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

        const std::vector<rendering::GlyphFillTriangle> &triangles =
            sdfFont_.glyphFillTriangles(character);
        if (triangles.empty())
        {
            penX += 0.5 * double(sdfFont_.pixelsPerEm());
            continue;
        }

        // Expand every fill triangle into view-space vertices.  The quad
        // basis (textRight/textUp) places the glyph plane in the world;
        // the camera-basis dot products produce view-space coordinates so
        // the identity view + projection place it exactly on screen.
        std::vector<float> vertices;
        vertices.reserve(triangles.size() * 3 * 9);
        for (const rendering::GlyphFillTriangle &triangle : triangles)
        {
            const glm::dvec2 corners[3] = {
                {triangle.ax + float(penX * sdfFont_.pixelsPerEm()),
                 triangle.ay},
                {triangle.bx + float(penX * sdfFont_.pixelsPerEm()),
                 triangle.by},
                {triangle.cx + float(penX * sdfFont_.pixelsPerEm()),
                 triangle.cy}};
            for (int i = 0; i < 3; ++i)
            {
                const glm::dvec3 world =
                    lineOrigin +
                    textRight * (corners[i].x * emToWorld) +
                    textUp * (corners[i].y * emToWorld);
                const glm::dvec3 relative = world - cameraPos;
                vertices.push_back(float(glm::dot(relative, cameraRight)));
                vertices.push_back(float(glm::dot(relative, cameraUp)));
                vertices.push_back(
                    float(-glm::dot(relative, cameraFront)));
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(request.color.r);
                vertices.push_back(request.color.g);
                vertices.push_back(request.color.b);
                vertices.push_back(request.color.a);
            }
        }
        constexpr glm::mat4 identityView(1.0f);
        backend.drawTextTriangles(identityView, projection, vertices.data(),
                                  uint32_t(vertices.size() / 9));
        ++glyphsDrawn;
        // Advance from the glyph metrics: max triangle X (em px units).
        float maxX = 0.0f;
        for (const rendering::GlyphFillTriangle &triangle : triangles)
            maxX = std::max({maxX, triangle.ax, triangle.bx, triangle.cx});
        penX += maxX;
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
