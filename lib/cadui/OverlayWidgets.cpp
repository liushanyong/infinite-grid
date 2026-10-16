#include "OverlayWidgets.hpp"

#include <acgs/AcGsView.h>
#include <imgui.h>

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

    void OverlayWidgets::renderFps(float fps) const
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("FpsOverlay", nullptr, flags))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                               "FPS %.1f", fps);
        }
        ImGui::End();
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
