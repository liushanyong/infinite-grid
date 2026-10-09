#pragma once

// rendering::ImGuiBgfxPass — renders ImGui draw data through bgfx
// (adapted from the SYCAD imgui_pass).  The ImGui context and the SDL3
// platform backend live in the host; this pass owns only the GPU side:
// the ImDrawVert vertex layout, the font atlas texture, the embedded
// imgui_pass program, and the transient-buffer draw-data submission.

#include <cstdint>

#include <bgfx/bgfx.h>

struct ImDrawData;

namespace rendering
{

class ImGuiBgfxPass
{
public:
    // Requires a live ImGui context (font atlas access) and an
    // initialized bgfx.  Safe to call again after shutdown.
    bool init();
    void shutdown();

    // Submits the draw data into |view| (expected bound to the window
    // backbuffer with its clear already configured and touched by the
    // caller).
    void renderDrawData(ImDrawData *drawData, bgfx::ViewId viewId,
                        std::uint16_t width, std::uint16_t height);

private:
    bool m_initialized = false;
    bgfx::VertexLayout m_vertexLayout;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_fontTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_fontSampler = BGFX_INVALID_HANDLE;
};

} // namespace rendering
