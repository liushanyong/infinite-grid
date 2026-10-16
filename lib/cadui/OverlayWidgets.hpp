#pragma once

#include "UcsIconOverlay.hpp"
#include "ViewCubeOverlay.hpp"

namespace cadui
{
    class OverlayWidgets
    {
    public:
        bool create(bgfx::TextureHandle fontTexture);
        void destroy();

        bool capturesScreenPoint(float x, float y,
                                 float displayWidth, float displayHeight) const;

        void renderFps(float fps) const;

        void render(acgs::AcGsView& view,
                    const glm::vec4& clearColor,
                    int drawableHeight,
                    const glm::vec3& xAxis,
                    const glm::vec3& yAxis,
                    const glm::vec3& zAxis);

    private:
        ViewCubeOverlay viewCube;
        UcsIconOverlay ucsIcon;
    };
} // namespace cadui
