#pragma once

#include "ViewCube.hpp"
#include "ViewCubeBgfx.hpp"

#include <glm/glm.hpp>

namespace acgs
{
    class AcGsView;
}

namespace cadui
{
    // The top-right ViewCube overlay, collapsed out of the app layer: the
    // ImGui window chrome, the bgfx offscreen renderer (with its ImGui
    // fallback), the navigation actions, and their camera application all
    // live here.  The app keeps only: create()/destroy() around the device
    // lifetime, capturesScreenPoint() from its input router, and render()
    // once per UI frame.
    class ViewCubeOverlay
    {
    public:
        // Creates the bgfx renderer and feeds it the shared ImGui font
        // texture; on failure the widget keeps drawing through the ImGui
        // fallback and the overlay stays usable.
        bool create(bgfx::TextureHandle fontTexture);
        void destroy();

        // True when a window-client point must be routed to the overlay
        // instead of the viewport (clicks there are navigation, not picking).
        bool capturesScreenPoint(float x, float y,
                                 float displayWidth, float displayHeight) const;

        // One UI frame: draws the cube and applies navigation actions to the
        // camera (region/cardinal/home snaps, roll, nudge).  Returns true
        // when the camera moved, so the caller can refresh its depth slabs.
        bool render(acgs::AcGsView& view);

    private:
        void applyAction(acgs::AcGsView& view, const ViewCubeAction& action);

        ViewCubeWidget widget;
        ViewCubeBgfxRenderer renderer;
    };
} // namespace cadui
