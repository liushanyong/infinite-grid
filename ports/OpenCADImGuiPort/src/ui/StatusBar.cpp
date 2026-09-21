#include "StatusBar.hpp"

#include <imgui.h>

namespace ui
{
    void drawStatusBar(const AppSnapshot& snapshot)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float height = ImGui::GetFrameHeight();

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));

        ImGuiWindowFlags flags = 0
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("##StatusBar", nullptr, flags);

        ImGui::TextUnformatted(snapshot.statusText.c_str());
        ImGui::SameLine();
        ImGui::Text("| Selection: %d", static_cast<int>(snapshot.selectedObjectIds.size()));
        ImGui::SameLine();
        ImGui::Text("| Layer: %s", snapshot.currentLayer.c_str());
        ImGui::SameLine();
        ImGui::Text("| FPS: %.1f", snapshot.fps);
        ImGui::SameLine();
        ImGui::Text("| Frame: %.2f ms", snapshot.frameMs);

        ImGui::End();
        ImGui::PopStyleVar();
    }
}
