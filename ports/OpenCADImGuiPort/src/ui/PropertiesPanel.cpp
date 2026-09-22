#include "PropertiesPanel.hpp"
#include "UiLayout.hpp"

#include <imgui.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace ui
{
    namespace
    {
        const char* typeLabel(const SceneObject* object)
        {
            if (!object) return "特性";
            if (object->type == "Line") return "直线";
            if (object->type == "Rectangle") return "矩形";
            if (object->type == "Circle") return "圆";
            return object->type.c_str();
        }

        bool beginProperties()
        {
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            const float height = vp->WorkSize.y - kContentTop - kStatusBarHeight;
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + kContentTop), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(kLeftPanelWidth, height), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(42, 42, 42, 255));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(53, 53, 53, 255));
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(48, 48, 48, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(56, 56, 56, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(61, 61, 61, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(70, 70, 70, 255));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(48, 48, 48, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 3.0f));

            constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
            const bool open = ImGui::Begin("##PropertiesPanel", nullptr, flags);
            if (!open)
            {
                ImGui::End();
                ImGui::PopStyleVar(5);
                ImGui::PopStyleColor(7);
            }
            return open;
        }

        void endProperties()
        {
            ImGui::End();
            ImGui::PopStyleVar(5);
            ImGui::PopStyleColor(7);
        }

        void propertyRow(const char* label)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
        }

        bool fieldText(const char* id, char* value, size_t size, bool highlighted = false)
        {
            if (highlighted)
            {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(19, 144, 204, 255));
            }
            ImGui::SetNextItemWidth(-1.0f);
            const bool edited = ImGui::InputText(id, value, size);
            if (highlighted)
            {
                ImGui::PopStyleColor();
            }
            return edited;
        }

        bool fieldCoordinate(const char* id, float* x, float* y)
        {
            char text[128];
            snprintf(text, sizeof(text), "%.4f, %.4f, 0.0000", *x, *y);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText(id, text, sizeof(text));
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                float parsedX = 0.0f;
                float parsedY = 0.0f;
                if (sscanf(text, "%f , %f", &parsedX, &parsedY) == 2)
                {
                    *x = parsedX;
                    *y = parsedY;
                    return true;
                }
            }
            return false;
        }

        bool fieldFloat(const char* id, float* value)
        {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputFloat(id, value, 0.0f, 0.0f, "%.4f");
            return ImGui::IsItemDeactivatedAfterEdit();
        }
    }

    void drawPropertiesPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        if (!beginProperties()) return;

        const SceneObject* selected = nullptr;
        for (const auto& object : snapshot.objects)
        {
            if (object.id == snapshot.selectedObjectId)
            {
                selected = &object;
                break;
            }
        }

        // Panel header with pin / close affordances.
        const ImVec2 hp = ImGui::GetCursorScreenPos();
        const float headerHeight = 32.0f;
        ImGui::GetWindowDrawList()->AddText(ImVec2(hp.x + 16.0f, hp.y + 10.0f), IM_COL32(226, 226, 226, 255), "特性");
        ImGui::InvisibleButton("PropertyHeaderSpace", ImVec2(std::max(1.0f, ImGui::GetWindowWidth()), headerHeight));
        ImGui::SameLine(ImGui::GetWindowWidth() - 84.0f);
        if (ImGui::InvisibleButton("PinProperty", ImVec2(30.0f, 26.0f)))
        {
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 44.0f);
        if (ImGui::InvisibleButton("CloseProperty", ImVec2(30.0f, 26.0f)))
        {
        }
        ImGui::GetWindowDrawList()->AddRect(ImVec2(hp.x + ImGui::GetWindowWidth() - 84.0f, hp.y + 6.0f), ImVec2(hp.x + ImGui::GetWindowWidth() - 54.0f, hp.y + 30.0f), IM_COL32(82, 82, 82, 255), 3.0f);
        ImGui::GetWindowDrawList()->AddRect(ImVec2(hp.x + ImGui::GetWindowWidth() - 44.0f, hp.y + 6.0f), ImVec2(hp.x + ImGui::GetWindowWidth() - 14.0f, hp.y + 30.0f), IM_COL32(82, 82, 82, 255), 3.0f);

        // Type strip.
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(48, 48, 48, 255));
        ImGui::BeginChild("##PropertyType", ImVec2(0.0f, 30.0f));
        ImGui::SetCursorPosX(14.0f);
        ImGui::SetCursorPosY(5.0f);
        ImGui::TextUnformatted(typeLabel(selected));
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        static char layerText[64] = "0";
        static char linetypeText[64] = "随层";
        static char linetypeScale[32] = "1.0000";
        static char plotStyle[64] = "按颜色";
        static char lineweight[32] = "随层";
        static char transparency[32] = "ByLayer";
        static char hyperlink[128] = "";
        static char thickness[32] = "0.0000";
        static char material[64] = "随层";
        snprintf(layerText, sizeof(layerText), "%s", selected ? selected->layer.c_str() : snapshot.currentLayer.c_str());

        if (ImGui::CollapsingHeader("一般", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("GeneralProps", 2, ImGuiTableFlags_None))
            {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 108.0f);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                propertyRow("颜色");
                fieldText("ColorProp", const_cast<char*>("随层"), 8, true);
                propertyRow("图层");
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("LayerProp", layerText))
                {
                    for (const auto& layer : snapshot.layers)
                    {
                        if (ImGui::Selectable(layer.name.c_str(), layer.name == layerText) && selected)
                        {
                            UiAction action;
                            action.type = UiActionType::AssignObjectLayer;
                            action.objectId = selected->id;
                            action.payload = layer.name;
                            controller.execute(action);
                        }
                    }
                    ImGui::EndCombo();
                }
                propertyRow("线型"); fieldText("Linetype", linetypeText, sizeof(linetypeText));
                propertyRow("线型比例"); fieldText("Ltscale", linetypeScale, sizeof(linetypeScale));
                propertyRow("打印样式"); fieldText("PlotStyle", plotStyle, sizeof(plotStyle));
                propertyRow("线宽"); fieldText("Lineweight", lineweight, sizeof(lineweight));
                propertyRow("透明度"); fieldText("Transparency", transparency, sizeof(transparency));
                propertyRow("超链接"); fieldText("Hyperlink", hyperlink, sizeof(hyperlink));
                propertyRow("厚度"); fieldText("Thickness", thickness, sizeof(thickness));
                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("三维可视化", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("VisualProps", 2, ImGuiTableFlags_None))
            {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 108.0f);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                propertyRow("材质"); fieldText("Material", material, sizeof(material));
                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("几何", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!selected)
            {
                ImGui::SetCursorPosX(14.0f);
                ImGui::TextDisabled("未选择对象");
            }
            else
            {
                float x1 = selected->x1, y1 = selected->y1, x2 = selected->x2, y2 = selected->y2, radius = selected->radius;
                bool geometryChanged = false;
                if (ImGui::BeginTable("GeometryProps", 2, ImGuiTableFlags_None))
                {
                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 108.0f);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    if (selected->type == "Circle")
                    {
                        propertyRow("圆心");
                        if (fieldCoordinate("CircleCenter", &x1, &y1)) { x2 = x1; y2 = y1; geometryChanged = true; }
                        propertyRow("半径");
                        if (fieldFloat("CircleRadius", &radius)) { x2 = x1; y2 = y1; geometryChanged = true; }
                    }
                    else
                    {
                        propertyRow("起点");
                        if (fieldCoordinate("StartPoint", &x1, &y1)) geometryChanged = true;
                        propertyRow("端点");
                        if (fieldCoordinate("EndPoint", &x2, &y2)) geometryChanged = true;

                        float dx = x2 - x1;
                        float dy = y2 - y1;
                        propertyRow("增量");
                        if (fieldCoordinate("Delta", &dx, &dy))
                        {
                            x2 = x1 + dx;
                            y2 = y1 + dy;
                            geometryChanged = true;
                        }

                        const float length = std::sqrt(dx * dx + dy * dy);
                        const float angle = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
                        propertyRow("长度");
                        float editableLength = length;
                        if (fieldFloat("Length", &editableLength) && std::abs(length) > 0.0001f)
                        {
                            const float radians = angle * 3.14159265358979323846f / 180.0f;
                            x2 = x1 + editableLength * std::cos(radians);
                            y2 = y1 + editableLength * std::sin(radians);
                            geometryChanged = true;
                        }
                        propertyRow("角度");
                        float editableAngle = angle;
                        if (fieldFloat("Angle", &editableAngle))
                        {
                            const float radians = editableAngle * 3.14159265358979323846f / 180.0f;
                            x2 = x1 + length * std::cos(radians);
                            y2 = y1 + length * std::sin(radians);
                            geometryChanged = true;
                        }
                    }
                    ImGui::EndTable();
                }

                if (geometryChanged)
                {
                    UiAction action;
                    action.type = UiActionType::UpdateObject;
                    action.objectId = selected->id;
                    action.x1 = x1; action.y1 = y1; action.x2 = x2; action.y2 = y2; action.radius = radius;
                    controller.execute(action);
                }
            }
        }

        endProperties();
    }
}
