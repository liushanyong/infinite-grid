#include "UcsIconOverlay.hpp"

#include <acgs/AcGsOrbitCamera.h>

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace cadui
{
    namespace
    {
        // Read once; the icon is either in the build or out of it.
        const bool kEnabled = [] {
            const char* flag = std::getenv("GRID_UCSICON");
            return !(flag && flag[0] != '\0' && std::strcmp(flag, "0") == 0);
        }();
    } // namespace

    bool UcsIconOverlay::capturesScreenPoint(float x, float y,
                                             float displayHeight) const
    {
        if (!kEnabled)
        {
            return false;
        }
        if (dragKind != UcsGripKind::None)
        {
            return true;
        }
        return x >= 0.0f && x <= kOverlaySize &&
               y >= displayHeight - kOverlaySize && y <= displayHeight;
    }

    bool UcsIconOverlay::render(AcGsOrbitCamera& camera,
                                const glm::vec4& clearColor,
                                int drawableHeight)
    {
        if (!kEnabled)
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - kOverlaySize),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kOverlaySize, kOverlaySize),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;
        if (!ImGui::Begin("UcsIconOverlay", nullptr, flags))
        {
            ImGui::End();
            return false;
        }

        UcsIconState state;
        // The widget projects world direction vectors, so it needs the
        // world->eye basis — the conjugate of the turntable rotation, exactly
        // like the ViewCube (un-conjugated would tumble the tripod against
        // the camera).
        state.viewRotation = glm::mat3_cast(glm::conjugate(camera.Rotation));
        state.selected = selected;
        state.backgroundLuminance = 0.299f * clearColor.r +
                                    0.587f * clearColor.g +
                                    0.114f * clearColor.b;

        const UcsIconResult result =
            widget.render("UcsIcon", ImVec2(kOverlaySize, kOverlaySize), state);

        bool cameraMoved = false;
        if (dragKind != UcsGripKind::None)
        {
            const ImVec2 delta = io.MouseDelta;
            if (dragKind == UcsGripKind::Origin)
            {
                camera.panScreen(delta.x, delta.y, (float)drawableHeight);
            }
            else
            {
                const bool xAxis = dragKind == UcsGripKind::XAxis;
                camera.orbitAroundPivot(xAxis ? -delta.x : 0.0f,
                                        xAxis ? 0.0f : -delta.y, camera.Target);
            }
            cameraMoved = true;
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                std::cout << "[UCSICON] grip drag end" << std::endl;
                dragKind = UcsGripKind::None;
            }
        }
        else if (result.clickedGrip != UcsGripKind::None)
        {
            dragKind = result.clickedGrip;
            selected = true;
            std::cout << "[UCSICON] grip drag begin grip="
                      << static_cast<int>(result.clickedGrip) << std::endl;
        }
        else if (result.clickedAxis != UcsAxis::None)
        {
            selected = true;
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            selected = false; // press landed on the overlay but off the tripod
        }

        ImGui::End();
        return cameraMoved;
    }
} // namespace cadui
