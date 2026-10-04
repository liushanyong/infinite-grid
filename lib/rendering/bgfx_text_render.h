#pragma once

#include <bgfx/bgfx.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>

// Text render pass: a single bgfx program + atlas texture + dynamic
// instance buffer.  Caller (AcGiWorldDraw::text) populates a queue of
// glyph instances per draw call; the renderer flushes them to bgfx in a
// dedicated overlay view.
class BgfxTextRenderComponent
{
public:
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle atlasTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uInvAtlasSize = BGFX_INVALID_HANDLE;

    int atlasWidth = 0;
    int atlasHeight = 0;
    float pixelRange = 4.0f;

    // Persistent atlas storage so the bgfx texture can be re-uploaded
    // when the loaded font changes; reset by the text subsystem.
    std::vector<uint8_t> pendingPixels;
    int pendingWidth = 0;
    int pendingHeight = 0;

    // Whether to draw the next frame's queue.
    bool drawText = true;
};

// One SDF glyph instance: 4 instance attributes of vec4 each, matching the
// shader's i_data0..i_data3 slots.
struct TextGlyphInstance
{
    float originX, originY, originZ;
    float rightX, rightY, upX, upY;
    float uvX0, uvY0, width, height;
    float colorR, colorG, colorB, pixelRange;
};

// Enqueue one glyph for the next flush.  The buffer is cleared after each
// submit; the AcGi callback drives a flush per text() call.
void textRenderEnqueue(BgfxTextRenderComponent &component,
                        std::vector<TextGlyphInstance> &queue,
                        const TextGlyphInstance &instance);

// Submit the queued glyphs to bgfx in the text overlay view.  The renderer
// owns the texture/program handle lifetime; the caller passes the camera
// view/projection plus the text color.
void textRenderFlush(bgfx::ViewId viewId, BgfxTextRenderComponent &component,
                    std::vector<TextGlyphInstance> &queue,
                    const float viewMatrix[16],
                    const float projectionMatrix[16],
                    const float colorRgba[4]);
