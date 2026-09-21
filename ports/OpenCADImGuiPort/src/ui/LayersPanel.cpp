#include "LayersPanel.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui
{
    void drawLayersPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        if (!ImGui::Begin("Layers"))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Current layer: %s", snapshot.currentLayer.c_str());
        ImGui::Separator();

        static char newLayerName[128] = "";
        ImGui::InputTextWithHint("##newLayer", "New layer name...", newLayerName, sizeof(newLayerName));
        ImGui::SameLine();
        if (ImGui::Button("Add Layer") && newLayerName[0] != '\0')
        {
            UiAction action;
            action.type = UiActionType::AddLayer;
            action.payload = newLayerName;
            controller.execute(action);
            newLayerName[0] = '\0';
        }

        ImGui::Separator();

        if (ImGui::BeginTable("LayersTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Locked", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (const auto& layer : snapshot.layers)
            {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                const bool isCurrent = layer.name == snapshot.currentLayer;
                if (ImGui::Selectable(layer.name.c_str(), isCurrent))
                {
                    UiAction action;
                    action.type = UiActionType::SelectLayer;
                    action.payload = layer.name;
                    controller.execute(action);
                }

                ImGui::TableNextColumn();
                bool visible = layer.visible;
                if (ImGui::Checkbox(("##visible_" + layer.name).c_str(), &visible))
                {
                    UiAction action;
                    action.type = UiActionType::ToggleLayerVisibility;
                    action.payload = layer.name;
                    controller.execute(action);
                }

                ImGui::TableNextColumn();
                const int objectCount = static_cast<int>(std::count_if(
                    snapshot.objects.begin(),
                    snapshot.objects.end(),
                    [&layer](const SceneObject& object) { return object.layer == layer.name; }
                ));
                ImGui::TextUnformatted(std::to_string(objectCount).c_str());

                ImGui::TableNextColumn();
                bool locked = layer.locked;
                if (ImGui::Checkbox(("##locked_" + layer.name).c_str(), &locked))
                {
                    UiAction action;
                    action.type = UiActionType::ToggleLayerLocked;
                    action.payload = layer.name;
                    controller.execute(action);
                }
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }
}
