#pragma once

// acgi::TextDevice — the device capabilities the text protocol needs
// (ObjectARX: the AcGi device interface consumed by AcGiTextEngine).
// The concrete device (rendering::RendererBackend) implements this
// alongside its other capabilities; the text layer never names the
// device type, so the protocol stays implementation-agnostic.

#include <cstdint>

#include <glm/glm.hpp>

namespace acgi
{

class TextDevice
{
public:
    virtual ~TextDevice() = default;

    // Registers a glyph's R8 distance field and returns a texture id
    // (0 = invalid).
    virtual std::uint32_t uploadGlyphSdf(const unsigned char *data,
                                         int width, int height) = 0;

    // Submits one 6-vertex quad (view-space, 9 floats per vertex:
    // pos3 + uv2 + rgba4) with that texture.
    virtual void drawSdfGlyphQuad(const glm::mat4 &view,
                                  const glm::mat4 &projection,
                                  std::uint32_t textureId,
                                  const float *vertices) = 0;
};

} // namespace acgi
