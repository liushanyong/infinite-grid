#include "OverlayWidgets.hpp"

#include <acgs/AcGsView.h>

namespace cadui
{
    bool OverlayWidgets::create(bgfx::TextureHandle fontTexture)
    {
        return viewCube.create(fontTexture);
    }

    void OverlayWidgets::destroy()
    {
        viewCube.destroy();
    }

    bool OverlayWidgets::capturesScreenPoint(float x, float y,
                                              float displayWidth,
                                              float displayHeight) const
    {
        return viewCube.capturesScreenPoint(x, y, displayWidth, displayHeight) ||
               ucsIcon.capturesScreenPoint(x, y, displayHeight);
    }

    void OverlayWidgets::render(acgs::AcGsView& view,
                                const glm::vec4& clearColor,
                                int drawableHeight,
                                const glm::vec3& xAxis,
                                const glm::vec3& yAxis,
                                const glm::vec3& zAxis)
    {
        bool cameraMoved = viewCube.render(view);
        cameraMoved = ucsIcon.render(view, clearColor, drawableHeight,
                                     xAxis, yAxis, zAxis) || cameraMoved;
        if (cameraMoved)
        {
            view.resetDepthSlabs();
        }
    }
} // namespace cadui
