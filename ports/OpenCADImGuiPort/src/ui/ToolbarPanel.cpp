#include "ToolbarPanel.hpp"
#include "IconSet.hpp"
#include "UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ui
{
    namespace
    {
        ImU32 hoverColor(bool hovered, bool active)
        {
            if (active) return IM_COL32(68, 108, 164, 255);
            if (hovered) return IM_COL32(52, 66, 86, 255);
            return IM_COL32(0, 0, 0, 0);
        }

        // Toolbar icons are drawn at this size inside their button cell.
        constexpr float kIconSize = 26.0f;

        bool iconAt(const char* id, const char* label, const ImVec2& origin, const ImVec2& size, IconId icon, bool enabled)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(origin);
            if (!enabled)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.48f);
                ImGui::BeginDisabled(true);
            }
            bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), hoverColor(hovered, active), 3.0f);
            DrawIcon(dl, icon, ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f - (label ? 6.0f : 0.0f)),
                     kIconSize, MakeToolbarPalette());
            if (label)
            {
                const ImVec2 ts = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(origin.x + (size.x - ts.x) * 0.5f, origin.y + size.y - ts.y - 4.0f), IM_COL32(226, 236, 248, 255), label);
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
                pressed = false;
            }
            ImGui::PopID();
            return pressed;
        }

        bool miniIconAt(const char* id, const ImVec2& origin, const ImVec2& size, IconId icon)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(origin);
            bool pressed = ImGui::InvisibleButton(id, size);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), hoverColor(ImGui::IsItemHovered(), ImGui::IsItemActive()), 3.0f);
            DrawIconMono(dl, icon, ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f), 18.0f,
                         IM_COL32(205, 215, 228, 255));
            ImGui::PopID();
            return pressed;
        }

        void dividerAt(ImDrawList* dl, float x, float y0, float y1)
        {
            dl->AddLine(ImVec2(x, y0), ImVec2(x, y1), IM_COL32(74, 80, 88, 255), 1.0f);
        }

        void groupCaption(ImDrawList* dl, const char* text, float x, float width, float y)
        {
            const ImVec2 ts = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(x + (width - ts.x) * 0.5f, y), IM_COL32(168, 180, 194, 255), text);
        }

        bool comboLike(const char* id, const char* value, const ImVec2& origin, const ImVec2& size)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(origin);
            bool pressed = ImGui::InvisibleButton(id, size);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool hovered = ImGui::IsItemHovered();
            dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), hovered ? IM_COL32(64, 64, 64, 255) : IM_COL32(52, 52, 52, 255), 2.0f);
            dl->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(92, 92, 92, 255), 2.0f);
            dl->AddText(ImVec2(origin.x + 8.0f, origin.y + (size.y - ImGui::CalcTextSize(value).y) * 0.5f), IM_COL32(235, 235, 235, 255), value);
            const float cx = origin.x + size.x - 13.0f;
            const float cy = origin.y + size.y * 0.5f;
            dl->AddTriangle(ImVec2(cx - 5, cy - 3), ImVec2(cx + 5, cy - 3), ImVec2(cx, cy + 3), IM_COL32(190, 190, 190, 255));
            ImGui::PopID();
            return pressed;
        }
    }

    void drawToolbarPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float appWidth = vp->WorkSize.x;
        const float fullHeight = kRibbonHeight + kDocumentTabHeight;

        ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(appWidth, fullHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(42, 42, 42, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        ImGui::Begin("##CadRibbon", nullptr, flags);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 base = vp->WorkPos;

        // Quick-access row + ribbon tabs.
        std::vector<std::pair<IconId, UiActionType>> quick = {
            {IconId::ToolNew, UiActionType::None}, {IconId::ToolOpen, UiActionType::OpenFile},
            {IconId::ToolSave, UiActionType::Save}, {IconId::ToolUndo, UiActionType::Undo},
            {IconId::ToolRedo, UiActionType::Redo}
        };
        float qx = base.x + 8.0f;
        for (size_t i = 0; i < quick.size(); ++i)
        {
            if (i >= 3 && ((i == 3 && !snapshot.canUndo) || (i == 4 && !snapshot.canRedo)))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);
                ImGui::BeginDisabled(true);
            }
            char id[32];
            snprintf(id, sizeof(id), "Quick%d", static_cast<int>(i));
            if (miniIconAt(id, ImVec2(qx, base.y + 4.0f), ImVec2(28.0f, 26.0f), quick[i].first))
            {
                UiAction a; a.type = quick[i].second;
                if (a.type == UiActionType::OpenFile) a.payload = "demo.dxf";
                controller.execute(a);
            }
            if (i >= 3 && ((i == 3 && !snapshot.canUndo) || (i == 4 && !snapshot.canRedo)))
            {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
            qx += 34.0f;
            if (i == 2) qx += 8.0f;
        }

        const char* tabs[] = {"绘制", "参数化", "模型", "插入", "注释", "视图", "管理"};
        static int activeTab = 0;
        float tx = base.x + 292.0f;
        for (int i = 0; i < 7; ++i)
        {
            const ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
            const float w = ts.x + 32.0f;
            const ImVec2 o(tx, base.y + 3.0f);
            ImGui::PushID(tabs[i]);
            ImGui::SetCursorScreenPos(o);
            if (ImGui::InvisibleButton(tabs[i], ImVec2(w, 28.0f))) activeTab = i;
            const bool active = activeTab == i;
            const bool hovered = ImGui::IsItemHovered();
            if (active || hovered)
            {
                dl->AddRectFilled(o, ImVec2(o.x + w, o.y + 28.0f), active ? IM_COL32(64, 64, 64, 255) : IM_COL32(54, 54, 54, 255), 2.0f);
            }
            if (active)
            {
                dl->AddRect(o, ImVec2(o.x + w, o.y + 28.0f), IM_COL32(19, 144, 204, 255), 2.0f, 0, 2.0f);
            }
            dl->AddText(ImVec2(o.x + 16.0f, o.y + 5.0f), active ? IM_COL32(240, 248, 255, 255) : IM_COL32(196, 196, 196, 255), tabs[i]);
            ImGui::PopID();
            tx += w + 4.0f;
        }

        // Shell layout controls. Keep the CAD shell fixed while dockable
        // panels below can be rearranged freely.
        {
            const ImVec2 resetOrigin(base.x + std::max(tx + 18.0f, base.x + appWidth - 112.0f), base.y + 4.0f);
            ImGui::SetCursorScreenPos(resetOrigin);
            if (ImGui::InvisibleButton("ResetDockLayout", ImVec2(96.0f, 26.0f)))
            {
                dockLayoutResetRequest() = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            dl->AddRectFilled(resetOrigin, ImVec2(resetOrigin.x + 96.0f, resetOrigin.y + 26.0f), hovered ? IM_COL32(62, 62, 62, 255) : IM_COL32(50, 50, 50, 255), 2.0f);
            const ImVec2 ts = ImGui::CalcTextSize("重置布局");
            dl->AddText(ImVec2(resetOrigin.x + (96.0f - ts.x) * 0.5f, resetOrigin.y + 4.0f), IM_COL32(225, 225, 225, 255), "重置布局");
        }

        // Shell layout controls. Keep the CAD shell fixed while dockable
        // panels below can be rearranged freely.
        {
            const ImVec2 resetOrigin(base.x + std::max(tx + 18.0f, base.x + appWidth - 112.0f), base.y + 4.0f);
            ImGui::SetCursorScreenPos(resetOrigin);
            if (ImGui::InvisibleButton("ResetDockLayout", ImVec2(96.0f, 26.0f)))
            {
                dockLayoutResetRequest() = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            dl->AddRectFilled(resetOrigin, ImVec2(resetOrigin.x + 96.0f, resetOrigin.y + 26.0f), hovered ? IM_COL32(62, 62, 62, 255) : IM_COL32(50, 50, 50, 255), 2.0f);
            const ImVec2 ts = ImGui::CalcTextSize("重置布局");
            dl->AddText(ImVec2(resetOrigin.x + (96.0f - ts.x) * 0.5f, resetOrigin.y + 4.0f), IM_COL32(225, 225, 225, 255), "重置布局");
        }

        // Ribbon body. Fixed positions deliberately mirror the reference shell.
        const float by = base.y + 36.0f;
        const float capY = base.y + kRibbonHeight - 20.0f;
        dividerAt(dl, base.x + 342.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 480.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 618.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 874.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 992.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 1180.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 1298.0f, by + 3.0f, capY - 4.0f);
        dividerAt(dl, base.x + 1384.0f, by + 3.0f, capY - 4.0f);

        auto setTool = [&](const char* tool)
        {
            UiAction a; a.type = UiActionType::SetTool; a.payload = tool; controller.execute(a);
        };
        if (iconAt("DrawLine", "直线", ImVec2(base.x + 10, by + 4), ImVec2(60, 88), IconId::ToolLine, true)) setTool("Line");
        if (iconAt("DrawPoly", "多段线", ImVec2(base.x + 74, by + 4), ImVec2(60, 88), IconId::ToolPolyline, true)) setTool("Polyline");
        if (iconAt("DrawCircle", "圆", ImVec2(base.x + 138, by + 4), ImVec2(60, 88), IconId::ToolCircle, true)) setTool("Circle");
        if (iconAt("DrawArc", "弧", ImVec2(base.x + 202, by + 4), ImVec2(60, 88), IconId::ToolArc, true)) setTool("Arc");
        if (iconAt("DrawRect", "矩形", ImVec2(base.x + 266, by + 4), ImVec2(60, 88), IconId::ToolRect, true)) setTool("Rectangle");
        groupCaption(dl, "绘制", base.x, 342.0f, capY);

        if (iconAt("Copy", "复制", ImVec2(base.x + 350, by + 4), ImVec2(58, 88), IconId::ToolCopy, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        if (iconAt("Move", "移动", ImVec2(base.x + 412, by + 4), ImVec2(58, 88), IconId::ToolMove, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        groupCaption(dl, "修改", base.x + 342.0f, 138.0f, capY);

        if (iconAt("Text", "文字", ImVec2(base.x + 488, by + 4), ImVec2(58, 88), IconId::ToolText, false)) {}
        if (iconAt("Dim", "尺寸", ImVec2(base.x + 550, by + 4), ImVec2(58, 88), IconId::ToolDimension, false)) {}
        groupCaption(dl, "注释", base.x + 480.0f, 138.0f, capY);

        if (iconAt("Layers", "图层", ImVec2(base.x + 626, by + 4), ImVec2(64, 88), IconId::ToolLayers, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        comboLike("LayerMode", "随层", ImVec2(base.x + 698, by + 5), ImVec2(168, 23));
        comboLike("LayerPlot", "随层", ImVec2(base.x + 698, by + 33), ImVec2(168, 23));
        comboLike("LayerWide", "随层", ImVec2(base.x + 698, by + 61), ImVec2(168, 23));
        groupCaption(dl, "图层", base.x + 618.0f, 256.0f, capY);

        if (iconAt("MakeBlock", "创建块", ImVec2(base.x + 882, by + 4), ImVec2(62, 88), IconId::ToolMakeBlock, false)) {}
        if (iconAt("InsertBlock", "插入块", ImVec2(base.x + 948, by + 4), ImVec2(62, 88), IconId::ToolInsertBlock, false)) {}
        groupCaption(dl, "块", base.x + 874.0f, 118.0f, capY);

        if (iconAt("Match", "匹配", ImVec2(base.x + 1000, by + 4), ImVec2(60, 88), IconId::ToolMatch, false)) {}
        comboLike("Prop1", "随层", ImVec2(base.x + 1066, by + 5), ImVec2(106, 23));
        comboLike("Prop2", "随层", ImVec2(base.x + 1066, by + 33), ImVec2(106, 23));
        comboLike("Prop3", "随层", ImVec2(base.x + 1066, by + 61), ImVec2(106, 23));
        groupCaption(dl, "特性", base.x + 992.0f, 188.0f, capY);

        if (iconAt("Group", "组", ImVec2(base.x + 1188, by + 4), ImVec2(54, 88), IconId::ToolGroup, false)) {}
        if (iconAt("Ungroup", "取消组", ImVec2(base.x + 1246, by + 4), ImVec2(54, 88), IconId::ToolUngroup, false)) {}
        groupCaption(dl, "组", base.x + 1180.0f, 118.0f, capY);

        if (iconAt("Paste", "粘贴", ImVec2(base.x + 1306, by + 4), ImVec2(56, 88), IconId::ToolPaste, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        miniIconAt("Cut", ImVec2(base.x + 1364, by + 8), ImVec2(24, 22), IconId::ToolClipboard);
        groupCaption(dl, "剪贴板", base.x + 1298.0f, 86.0f, capY);

        if (iconAt("Measure", "测量", ImVec2(base.x + 1392, by + 4), ImVec2(62, 88), IconId::ToolMeasure, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        groupCaption(dl, "测量", base.x + 1384.0f, 90.0f, capY);

        // Document tab strip.
        const ImVec2 dtab(base.x, base.y + kRibbonHeight);
        dl->AddRectFilled(dtab, ImVec2(base.x + appWidth, base.y + fullHeight), IM_COL32(35, 35, 35, 255));
        dl->AddText(ImVec2(base.x + 18.0f, dtab.y + 10.0f), IM_COL32(215, 215, 215, 255), "起点");
        const ImVec2 tabOrigin(base.x + 70.0f, dtab.y + 4.0f);
        dl->AddRectFilled(tabOrigin, ImVec2(tabOrigin.x + 178.0f, tabOrigin.y + 30.0f), IM_COL32(19, 144, 204, 255), 3.0f);
        dl->AddCircleFilled(ImVec2(tabOrigin.x + 18.0f, tabOrigin.y + 15.0f), 5.0f, IM_COL32(196, 145, 245, 255));
        dl->AddText(ImVec2(tabOrigin.x + 34.0f, tabOrigin.y + 6.0f), IM_COL32(24, 24, 24, 255), "Drawing1");
        dl->AddText(ImVec2(tabOrigin.x + 144.0f, tabOrigin.y + 7.0f), IM_COL32(240, 248, 255, 255), "x");
        ImGui::SetCursorScreenPos(tabOrigin);
        if (ImGui::InvisibleButton("##ActiveDocTab", ImVec2(178.0f, 30.0f))) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        const ImVec2 plus(tabOrigin.x + 188.0f, tabOrigin.y + 1.0f);
        dl->AddRect(plus, ImVec2(plus.x + 28.0f, plus.y + 28.0f), IM_COL32(94, 94, 94, 255), 3.0f);
        dl->AddLine(ImVec2(plus.x + 14.0f, plus.y + 8.0f), ImVec2(plus.x + 14.0f, plus.y + 20.0f), IM_COL32(225, 225, 225, 255), 1.8f);
        dl->AddLine(ImVec2(plus.x + 8.0f, plus.y + 14.0f), ImVec2(plus.x + 20.0f, plus.y + 14.0f), IM_COL32(225, 225, 225, 255), 1.8f);
        ImGui::SetCursorScreenPos(plus);
        if (ImGui::InvisibleButton("##NewDoc", ImVec2(28.0f, 28.0f))) { UiAction a; a.type=UiActionType::None; controller.execute(a); }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(1);
    }
}
