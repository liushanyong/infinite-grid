#include "PropertiesPanel.hpp"

#include <imgui.h>

#include <string>

namespace ui
{
    void drawPropertiesPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        if (!ImGui::Begin("Properties"))
        {
            ImGui::End();
            return;
        }

        const SceneObject* selected = nullptr;
        for (const auto& object : snapshot.objects)
        {
            if (object.id == snapshot.selectedObjectId)
            {
                selected = &object;
                break;
            }
        }

        const char* selectedLabel = selected ? selected->name.c_str() : "None";
        if (ImGui::BeginCombo("Object", selectedLabel))
        {
            if (ImGui::Selectable("None", selected == nullptr))
            {
                UiAction action;
                action.type = UiActionType::SelectObject;
                action.objectId = -1;
                controller.execute(action);
            }

            for (const auto& object : snapshot.objects)
            {
                const bool isSelected = selected && object.id == selected->id;
                if (ImGui::Selectable(object.name.c_str(), isSelected))
                {
                    UiAction action;
                    action.type = UiActionType::SelectObject;
                    action.objectId = object.id;
                    controller.execute(action);
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (selected)
        {
            ImGui::Separator();
            static bool applyToAllSelected = false;
            if (snapshot.selectedObjectIds.size() > 1)
            {
                ImGui::Checkbox("Apply to all selected", &applyToAllSelected);
            }
            else
            {
                applyToAllSelected = false;
            }
            ImGui::Text("Type: %s", selected->type.c_str());
            if (ImGui::BeginCombo("Assign Layer", selected->layer.c_str()))
            {
                for (const auto& layer : snapshot.layers)
                {
                    const bool isCurrent = layer.name == selected->layer;
                    if (ImGui::Selectable(layer.name.c_str(), isCurrent) && layer.name != selected->layer)
                    {
                        UiAction action;
                        action.type = UiActionType::AssignObjectLayer;
                        action.objectId = applyToAllSelected ? -1 : selected->id;
                        action.payload = layer.name;
                        controller.execute(action);
                    }

                    if (isCurrent)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            float x1 = selected->x1;
            float y1 = selected->y1;
            float x2 = selected->x2;
            float y2 = selected->y2;
            float radius = selected->radius;
            bool changed = false;

            if (selected->type == "Circle")
            {
                changed |= ImGui::InputFloat("Center X", &x1);
                changed |= ImGui::InputFloat("Center Y", &y1);
                changed |= ImGui::InputFloat("Radius", &radius);
            }
            else
            {
                changed |= ImGui::InputFloat("Start X", &x1);
                changed |= ImGui::InputFloat("Start Y", &y1);
                changed |= ImGui::InputFloat("End X", &x2);
                changed |= ImGui::InputFloat("End Y", &y2);
            }

            if (ImGui::Button("Delete Object"))
            {
                UiAction action;
                action.type = UiActionType::DeleteObject;
                action.objectId = applyToAllSelected ? -1 : selected->id;
                controller.execute(action);
            }

            if (changed)
            {
                UiAction action;
                action.type = UiActionType::UpdateObject;
                action.objectId = selected->id;
                action.x1 = x1;
                action.y1 = y1;
                action.x2 = x2;
                action.y2 = y2;
                action.radius = radius;
                controller.execute(action);
            }
        }
        else
        {
            ImGui::Separator();
            ImGui::TextDisabled("No object selected.");
        }

        ImGui::Separator();

        bool grid = snapshot.gridEnabled;
        bool snap = snapshot.snapEnabled;
        bool ortho = snapshot.orthoEnabled;

        if (ImGui::Checkbox("Grid", &grid))
        {
            UiAction action;
            action.type = UiActionType::ToggleGrid;
            controller.execute(action);
        }

        if (ImGui::Checkbox("Snap", &snap))
        {
            UiAction action;
            action.type = UiActionType::ToggleSnap;
            controller.execute(action);
        }

        if (ImGui::Checkbox("Ortho", &ortho))
        {
            UiAction action;
            action.type = UiActionType::ToggleOrtho;
            controller.execute(action);
        }

        ImGui::End();
    }
}
