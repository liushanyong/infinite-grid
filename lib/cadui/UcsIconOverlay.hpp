#pragma once

#include "UcsIcon.hpp"

#include <glm/glm.hpp>

namespace acgs
{
    class AcGsView;
}

namespace cadui
{
    // The bottom-left UCS icon overlay, collapsed out of the app layer: the
    // ImGui window chrome, the tripod widget, selection/grip-drag state, and
    // the camera chase that turns grip drags into pan/orbit all live here.
    // The app keeps only two call sites: capturesScreenPoint() from its input
    // router and render() once per UI frame.  GRID_UCSICON=0 hides the icon.
    class UcsIconOverlay
    {
    public:
        // True when a window-client point must be routed to the overlay
        // instead of the viewport: it sits on the icon, or a grip drag is in
        // flight (mid-drag motion outside the icon must never leak into the
        // app's own orbit/pan handlers — the grip chase is the only mover).
        bool capturesScreenPoint(float x, float y, float displayHeight) const;

        bool dragging() const { return dragKind != UcsGripKind::None; }

        // One UI frame: draws the overlay and feeds grip drags to the camera.
        // Grip semantics are "the arm chases the cursor": the origin grip pans
        // (same convention as the app's middle-drag pan), the X grip yaws with
        // horizontal motion and the Y grip pitches with vertical motion (the
        // turntable orbit speed, with the horizontal sign negated because the
        // orbit itself is camera-follows-cursor).  Returns true when the
        // camera moved, so the caller can refresh its depth slabs.
        bool render(acgs::AcGsView& view,
                    const glm::vec4& clearColor,
                    int drawableHeight,
                    const glm::vec3& xAxis,
                    const glm::vec3& yAxis,
                    const glm::vec3& zAxis);

    private:
        static constexpr float kOverlaySize = 110.0f;

        UcsIconWidget widget;
        bool selected{ false };
        UcsGripKind dragKind{ UcsGripKind::None };
    };
} // namespace cadui
