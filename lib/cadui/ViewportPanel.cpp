#include "ViewportPanel.hpp"

namespace cadui
{
    ViewportPanelFrame drawViewportPanel(
        const char *title, const ImVec2 &position, const ImVec2 &size,
        std::uint32_t textureId, const ViewportPanelOverlay &drawOverlay)
    {
        ViewportPanelFrame frame;
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (!ImGui::Begin(title, nullptr, flags))
        {
            ImGui::End();
            return frame;
        }

        frame.origin = ImGui::GetCursorScreenPos();
        frame.size = ImGui::GetContentRegionAvail();
        if (textureId != 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(
                             static_cast<std::uintptr_t>(textureId)),
                         frame.size);
        }
        else
        {
            ImGui::TextUnformatted("scene unavailable");
        }
        if (drawOverlay)
        {
            drawOverlay(ImGui::GetWindowDrawList(), frame.origin, frame.size);
        }
        frame.hovered = ImGui::IsWindowHovered();
        ImGui::End();
        return frame;
    }
} // namespace cadui
