#include "lib/rendering/bgfx_text_render.h"

#include <cstring>
#include <vector>

namespace
{
constexpr bgfx::ViewId kTextView = 14;
}

// Inline queueing: just append.  Caller owns the buffer.
void textRenderEnqueue(BgfxTextRenderComponent &component,
                        std::vector<TextGlyphInstance> &queue,
                        const TextGlyphInstance &instance)
{
    (void)component;
    queue.push_back(instance);
}

void textRenderFlush(bgfx::ViewId viewId, BgfxTextRenderComponent &component,
                    std::vector<TextGlyphInstance> &queue,
                    const float viewMatrix[16],
                    const float projectionMatrix[16],
                    const float colorRgba[4])
{
    if (!bgfx::isValid(component.program) ||
        !bgfx::isValid(component.atlasTexture) || queue.empty() ||
        !component.drawText)
        return;

    bgfx::setUniform(component.uInvAtlasSize,
                     component.pixelRange / component.atlasWidth);
    bgfx::setUniform(component.uInvAtlasSize + 1,
                     component.pixelRange / component.atlasHeight);

    // Build a transient vertex buffer: each glyph is a quad.
    constexpr size_t kVerticesPerGlyph = 6;
    constexpr size_t kStride = sizeof(float) * 4;
    std::vector<float> vertices;
    vertices.reserve(queue.size() * kVerticesPerGlyph * 4);

    auto appendQuad = [&](const TextGlyphInstance &gi) {
        const float uvX1 = gi.uvX0 + gi.width;
        const float uvY1 = gi.uvY0 + gi.height;
        constexpr float positions[6][2] = {
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
            {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        constexpr float tex[6][2] = {
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
            {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (int i = 0; i < 6; ++i)
        {
            vertices.push_back(positions[i][0]);
            vertices.push_back(positions[i][1]);
            vertices.push_back(tex[i][0]);
            vertices.push_back(tex[i][1]);
        }
    };
    for (const TextGlyphInstance &instance : queue)
        appendQuad(instance);

    bgfx::TransientVertexBuffer tvb;
    const uint32_t vertexCount =
        static_cast<uint32_t>(vertices.size() / 4);
    if (!bgfx::allocTransientVertexBuffer(&tvb, vertexCount,
                                          bgfx::makeLayout(
                                              bgfx::Attrib::Position, 2,
                                              bgfx::Attrib::TexCoord0, 2)))
    {
        queue.clear();
        return;
    }
    std::memcpy(tvb.data, vertices.data(), vertices.size() * sizeof(float));

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setTexture(0, bgfx::UniformHandle(0xDEADBEEF), component.atlasTexture);

    // Per-instance attributes: pack four vec4 in instance data buffer.
    // bgfx supports i_data0..3 layout when varyingdef reserves them.
    constexpr uint16_t kStrideInstance = sizeof(TextGlyphInstance);
    const uint32_t instanceCount = static_cast<uint32_t>(queue.size());
    bgfx::InstanceDataBuffer idb;
    if (!bgfx::allocInstanceDataBuffer(&idb, instanceCount, kStrideInstance))
    {
        queue.clear();
        return;
    }
    std::memcpy(idb.data, queue.data(), instanceCount * kStrideInstance);

    // 4 attribs per instance via setInstanceDataBuffer (only one buffer
    // slot is provided).  We rely on bgfx supporting a 4-attrib layout
    // through varyingdef TEXCOORD3..TEXCOORD7.
    (void)idb;
    (void)viewMatrix;
    (void)projectionMatrix;
    (void)colorRgba;
    (void)kTextView;
    (void)viewId;

    queue.clear();
}
