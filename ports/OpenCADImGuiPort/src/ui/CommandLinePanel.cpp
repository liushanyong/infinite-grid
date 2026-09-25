#include "CommandLinePanel.hpp"
#include "UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>

namespace ui
{
    namespace
    {
        void submitCommand(IAppController& controller, ViewportHost* viewportHost, const std::string& payload)
        {
            UiAction action;
            action.type = UiActionType::RunCommand;
            action.payload = payload;
            controller.execute(action);

            if (viewportHost)
            {
                if (payload == "zoom in") viewportHost->zoomIn();
                else if (payload == "zoom out") viewportHost->zoomOut();
                else if (payload == "zoom extents") viewportHost->zoomExtents();
                else if (payload == "reset") viewportHost->reset();
            }
        }
    }

    void drawCommandLinePanel(IAppController& controller, const AppSnapshot& snapshot, ViewportHost* viewportHost)
    {
        (void)snapshot;
        ImGui::SetNextWindowSize(ImVec2(900.0f, 44.0f), ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(45, 45, 45, 245));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(35, 35, 35, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 6.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("命令行", nullptr, flags);

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 255));
        ImGui::TextUnformatted("命令:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SmallButton(">");
        ImGui::SameLine();
        static char command[256] = "";
        ImGui::PushItemWidth(-98.0f);
        const bool submitted = ImGui::InputTextWithHint(
            "##Command",
            "",
            command,
            sizeof(command),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::SmallButton(">");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 190, 125, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(95, 205, 138, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(61, 163, 103, 255));
        if (ImGui::SmallButton("MCP"))
        {
        }
        ImGui::PopStyleColor(3);

        if (submitted && command[0] != '\0')
        {
            submitCommand(controller, viewportHost, command);
            command[0] = '\0';
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
}
