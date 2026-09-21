#include "ToolbarPanel.hpp"

#include <imgui.h>

namespace ui
{
    void drawToolbarPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        ImGuiWindowFlags flags = 0
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoSavedSettings;

        if (!ImGui::Begin("Tools", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        auto setTool = [&controller](const char* tool)
        {
            UiAction action;
            action.type = UiActionType::SetTool;
            action.payload = tool;
            controller.execute(action);
        };

        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 buttonSize = ImVec2(buttonWidth > 1.0f ? buttonWidth : 0.0f, ImGui::GetFrameHeight());

        if (ImGui::Button("Select", buttonSize))
        {
            setTool("Select");
        }

        if (ImGui::Button("Line", buttonSize))
        {
            setTool("Line");
        }

        if (ImGui::Button("Rectangle", buttonSize))
        {
            setTool("Rectangle");
        }

        if (ImGui::Button("Circle", buttonSize))
        {
            setTool("Circle");
        }

        ImGui::Separator();
        ImGui::Text("Active: %s", snapshot.activeTool.c_str());

        if (snapshot.hasPendingDrawPoint)
        {
            ImGui::TextDisabled("Waiting for second point");
        }

        ImGui::End();
    }
}
