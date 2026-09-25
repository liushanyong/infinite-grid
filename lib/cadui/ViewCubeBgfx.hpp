#pragma once

#include <bgfx/bgfx.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include <vector>

namespace cadui
{
    struct ViewCubeLabelVertex
    {
        ImVec2 position{};
        ImVec2 uv{};
        ImU32 color{ IM_COL32_WHITE };
        float depth{ 0.5f };
    };

    // Renders the chamfered ViewCube and world compass ring into a private
    // bgfx framebuffer. The widget displays this texture as an ImGui image and
    // overlays labels, controls and hit testing with the ImGui DrawList.
    class ViewCubeBgfxRenderer
    {
    public:
        ViewCubeBgfxRenderer();
        ~ViewCubeBgfxRenderer();
        ViewCubeBgfxRenderer(const ViewCubeBgfxRenderer&) = delete;
        ViewCubeBgfxRenderer& operator=(const ViewCubeBgfxRenderer&) = delete;

        bool create();
        void destroy();

        void setFontTexture(bgfx::TextureHandle texture);
        bool canRenderText() const;
        bool isValid() const;
        ImTextureID textureId() const;

        void render(const ImVec2& size,
                    const glm::mat3& cubeRotation,
                    const glm::mat3& compassRotation,
                    int hoveredRegionId,
                    const std::vector<ViewCubeLabelVertex>& labelVertices);

    private:
        struct Impl;
        void ensureFrameBufferSize(unsigned width, unsigned height);

        ImTextureID m_textureId{ nullptr };
        Impl* m_impl{ nullptr };
    };
} // namespace cadui
