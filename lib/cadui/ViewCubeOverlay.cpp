#include "ViewCubeOverlay.hpp"

#include <acgs/AcGsView.h>

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <iostream>

namespace cadui
{
    bool ViewCubeOverlay::create(bgfx::TextureHandle fontTexture)
    {
        if (renderer.create())
        {
            renderer.setFontTexture(fontTexture);
            return true;
        }
        std::cerr << "ViewCube BGFX renderer unavailable; using ImGui fallback"
                  << std::endl;
        return false;
    }

    void ViewCubeOverlay::destroy()
    {
        renderer.destroy();
    }

    bool ViewCubeOverlay::capturesScreenPoint(float x, float y,
                                              float displayWidth,
                                              float displayHeight) const
    {
        return x >= displayWidth - 198.0f && x <= displayWidth - 12.0f &&
               y >= 0.0f && y <= 200.0f;
    }

    void ViewCubeOverlay::applyAction(acgs::AcGsView& view,
                                      const ViewCubeAction& action)
    {
        if (action.kind == ViewCubeActionKind::None)
        {
            return;
        }

        const glm::dvec3 eyeDirection = view.eyeDirection();

        // Re-gaze at a direction, re-levelling the roll with world +Z — the
        // turntable stores the eye direction, so the gaze negates here.
        const auto setEyeDirection = [&](const glm::dvec3& direction)
        {
            if (glm::length(direction) < 1e-12)
            {
                return;
            }
            view.setViewDirection(-glm::normalize(direction));
            view.setWorldUp(glm::dvec3(0.0, 0.0, 1.0));
        };

        constexpr double quarterTurn = 1.57079632679489661923;
        switch (action.kind)
        {
        case ViewCubeActionKind::Region:
        {
            // OpenCADStudio's snap_view_region flips to the opposite region
            // when the camera already faces the clicked one, so re-clicking a
            // region walks to the far side instead of doing nothing.
            ViewCubeRegion target = action.region;
            const glm::dvec3 targetDirection(
                ViewCubeWidget::snapDirection(target));
            if (glm::dot(eyeDirection, glm::normalize(targetDirection)) > 0.9999)
            {
                target = ViewCubeWidget::oppositeRegion(target);
            }
            setEyeDirection(glm::dvec3(ViewCubeWidget::snapDirection(target)));
            break;
        }
        case ViewCubeActionKind::Cardinal:
            setEyeDirection(glm::dvec3(
                ViewCubeWidget::cardinalDirection(action.region.index)));
            break;
        case ViewCubeActionKind::Home:
            setEyeDirection(glm::dvec3(0.0, 0.0, 1.0));
            break;
        case ViewCubeActionKind::RollLeft:
        case ViewCubeActionKind::RollRight:
        {
            const double angle =
                action.kind == ViewCubeActionKind::RollLeft ? -quarterTurn
                                                            : quarterTurn;
            const glm::dvec3 rolledUp =
                glm::angleAxis(angle, eyeDirection) * view.upDirection();
            view.setWorldUp(rolledUp);
            break;
        }
        case ViewCubeActionKind::NudgeUp:
        case ViewCubeActionKind::NudgeDown:
        case ViewCubeActionKind::NudgeLeft:
        case ViewCubeActionKind::NudgeRight:
        {
            const bool horizontal =
                action.kind == ViewCubeActionKind::NudgeLeft ||
                action.kind == ViewCubeActionKind::NudgeRight;
            const bool positive =
                action.kind == ViewCubeActionKind::NudgeDown ||
                action.kind == ViewCubeActionKind::NudgeRight;
            const glm::dvec3 axis =
                horizontal ? view.upDirection() : view.rightDirection();
            const double angle = positive ? quarterTurn : -quarterTurn;
            setEyeDirection(glm::angleAxis(angle, glm::normalize(axis)) *
                            eyeDirection);
            break;
        }
        default:
            return;
        }
    }

    bool ViewCubeOverlay::render(acgs::AcGsView& view)
    {
        ImGuiIO& io = ImGui::GetIO();
        constexpr float overlayWidth = 174.0f;
        constexpr float overlayHeight = 188.0f;
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - overlayWidth - 12.0f, 12.0f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;
        if (!ImGui::Begin("UcsViewCubeOverlay", nullptr, flags))
        {
            ImGui::End();
            return false;
        }

        widget.setBgfxRenderer(&renderer);
        ViewCubeOptions options;
        options.showControls = true;
        options.showUcsPicker = false;
        // The widget projects cube-local points with eye = rotation * local
        // and +Z toward the camera, i.e. it needs the world->eye basis.
        // Rotation is the OpenCADStudio turntable quaternion (cam-local ->
        // world, eye = Rotation * +Z), so pass its conjugate — the same
        // matrix OpenCADStudio's view_rotation_mat() feeds its ViewCube.
        // Passing Rotation un-conjugated transposes the cube: hit tests
        // resolve to the wrong regions and corner clicks snap to the wrong
        // directions.
        const glm::mat3 cameraRotation(
            glm::mat3_cast(glm::conjugate(view.cameraRotation())));
        const ViewCubeResult result = widget.render(
            "UcsViewCube", ImVec2(160.0f, 160.0f), cameraRotation,
            glm::mat3(1.0f), options);
        const bool cameraMoved = result.action.kind != ViewCubeActionKind::None;
        applyAction(view, result.action);
        ImGui::End();
        return cameraMoved;
    }
} // namespace cadui
