#include "CommandLinePanel.hpp"

#include <imgui.h>

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
                if (payload == "zoom in")
                {
                    viewportHost->zoomIn();
                }
                else if (payload == "zoom out")
                {
                    viewportHost->zoomOut();
                }
                else if (payload == "zoom extents")
                {
                    viewportHost->zoomExtents();
                }
                else if (payload == "reset")
                {
                    viewportHost->reset();
                }
            }
        }
    }

    void drawCommandLinePanel(IAppController& controller, const AppSnapshot& snapshot, ViewportHost* viewportHost)
    {
        ImGui::SetNextWindowSize(ImVec2(520.0f, 180.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Command Line"))
        {
            ImGui::End();
            return;
        }

        static char command[256] = "";
        bool submitted = ImGui::InputTextWithHint("##command", "Type a command...", command, sizeof(command), ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("Run") && command[0] != '\0')
        {
            submitted = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Grid"))
        {
            UiAction action;
            action.type = UiActionType::ToggleGrid;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Snap"))
        {
            UiAction action;
            action.type = UiActionType::ToggleSnap;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Ortho"))
        {
            UiAction action;
            action.type = UiActionType::ToggleOrtho;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Zoom Extents") && viewportHost)
        {
            viewportHost->zoomExtents();
        }

        if (submitted && command[0] != '\0')
        {
            submitCommand(controller, viewportHost, command);
            command[0] = '\0';
        }

        ImGui::Separator();
        ImGui::Text("History");

        ImGui::SameLine();
        if (ImGui::Button("Clear History"))
        {
            UiAction action;
            action.type = UiActionType::ClearHistory;
            controller.execute(action);
        }

        if (snapshot.commandHistory.empty())
        {
            ImGui::TextDisabled("No commands executed yet.");
        }
        else
        {
            const float footerHeight = ImGui::GetFrameHeightWithSpacing();
            if (ImGui::BeginChild("CommandHistory", ImVec2(0.0f, -footerHeight), 0))
            {
                for (int index = static_cast<int>(snapshot.commandHistory.size()) - 1; index >= 0; --index)
                {
                    const std::string& historyCommand = snapshot.commandHistory[static_cast<size_t>(index)];
                    ImGui::PushID(index);
                    if (ImGui::Selectable(historyCommand.c_str()))
                    {
                        submitCommand(controller, viewportHost, historyCommand);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Click to run again");
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }
}
