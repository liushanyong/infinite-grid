#include "OutlinePanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace ui
{
    void drawOutlinePanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        if (!ImGui::Begin("大纲"))
        {
            ImGui::End();
            return;
        }

        static char filter[128] = "";
        ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##outline_filter", "Filter objects...", filter, sizeof(filter));

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            filter[0] = '\0';
        }

        if (ImGui::Button("All"))
        {
            UiAction action;
            action.type = UiActionType::SelectAll;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Invert"))
        {
            UiAction action;
            action.type = UiActionType::InvertSelection;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Deselect"))
        {
            UiAction action;
            action.type = UiActionType::SelectObject;
            action.objectId = -1;
            controller.execute(action);
        }

        ImGui::Separator();

        const std::string filterText = filter;
        const float footerHeight = ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("OutlineObjects", ImVec2(0.0f, -footerHeight), 0))
        {
            for (const auto& object : snapshot.objects)
            {
                if (!filterText.empty())
                {
                    const std::string haystack = object.name + " " + object.type + " " + object.layer;
                    if (haystack.find(filterText) == std::string::npos)
                    {
                        continue;
                    }
                }

                const bool isSelected = std::find(
                    snapshot.selectedObjectIds.begin(),
                    snapshot.selectedObjectIds.end(),
                    object.id
                ) != snapshot.selectedObjectIds.end();
                ImGui::PushID(object.id);
                const std::string label = object.name + "  [" + object.type + " / " + object.layer + "]";
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    UiAction action;
                    action.type = UiActionType::SelectObject;
                    action.objectId = object.id;
                    action.selectionModifier = ImGui::GetIO().KeyCtrl
                        ? SelectionModifier::Toggle
                        : SelectionModifier::Replace;
                    controller.execute(action);
                }
                ImGui::PopID();
            }

            if (snapshot.objects.empty())
            {
                ImGui::TextDisabled("No objects.");
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
}
