#include "ToolbarPanel.hpp"
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
        enum class Icon
        {
            New,
            Open,
            Save,
            Undo,
            Redo,
            Line,
            Polyline,
            Circle,
            Arc,
            Rect,
            Copy,
            Move,
            Rotate,
            Scale,
            Text,
            Dimension,
            Leader,
            Layers,
            MakeBlock,
            InsertBlock,
            Match,
            Group,
            Ungroup,
            Paste,
            Clipboard,
            Measure
        };

        ImU32 hoverColor(bool hovered, bool active)
        {
            if (active) return IM_COL32(68, 108, 164, 255);
            if (hovered) return IM_COL32(52, 66, 86, 255);
            return IM_COL32(0, 0, 0, 0);
        }

        void drawIcon(ImDrawList* dl, const ImVec2& c, Icon icon, ImU32 color)
        {
            const float s = 1.0f;
            switch (icon)
            {
            case Icon::New:
                dl->AddRect(ImVec2(c.x - 8, c.y - 10), ImVec2(c.x + 8, c.y + 10), color, 1.0f, 0, 1.6f);
                dl->AddLine(ImVec2(c.x - 4, c.y + 3), ImVec2(c.x + 4, c.y + 3), color, 1.5f);
                dl->AddLine(ImVec2(c.x - 4, c.y + 6), ImVec2(c.x + 2, c.y + 6), color, 1.5f);
                break;
            case Icon::Open:
                dl->AddRect(ImVec2(c.x - 10, c.y - 6), ImVec2(c.x + 10, c.y + 8), color, 1.0f, 0, 1.6f);
                dl->AddLine(ImVec2(c.x - 10, c.y - 6), ImVec2(c.x - 5, c.y - 10), color, 1.6f);
                dl->AddLine(ImVec2(c.x - 5, c.y - 10), ImVec2(c.x + 1, c.y - 10), color, 1.6f);
                dl->AddLine(ImVec2(c.x + 1, c.y - 10), ImVec2(c.x + 3, c.y - 6), color, 1.6f);
                break;
            case Icon::Save:
                dl->AddRect(ImVec2(c.x - 9, c.y - 9), ImVec2(c.x + 9, c.y + 9), color, 1.0f, 0, 1.6f);
                dl->AddRect(ImVec2(c.x - 4, c.y - 9), ImVec2(c.x + 4, c.y - 2), color, 0.0f, 0, 1.4f);
                dl->AddRect(ImVec2(c.x - 5, c.y + 1), ImVec2(c.x + 5, c.y + 9), color, 0.0f, 0, 1.4f);
                break;
            case Icon::Undo:
                dl->PathClear();
                dl->PathArcTo(c, 7.0f, 0.55f, 4.2f, 16);
                dl->PathStroke(color, 0, 1.8f);
                dl->AddLine(ImVec2(c.x - 8, c.y - 5), ImVec2(c.x - 8, c.y + 2), color, 1.8f);
                dl->AddLine(ImVec2(c.x - 8, c.y - 5), ImVec2(c.x - 1, c.y - 5), color, 1.8f);
                break;
            case Icon::Redo:
                dl->PathClear();
                dl->PathArcTo(c, 7.0f, -1.05f, 2.6f, 16);
                dl->PathStroke(color, 0, 1.8f);
                dl->AddLine(ImVec2(c.x + 8, c.y - 5), ImVec2(c.x + 8, c.y + 2), color, 1.8f);
                dl->AddLine(ImVec2(c.x + 8, c.y - 5), ImVec2(c.x + 1, c.y - 5), color, 1.8f);
                break;
            case Icon::Line:
                dl->AddCircleFilled(ImVec2(c.x - 12, c.y + 8), 3.0f, color);
                dl->AddCircleFilled(ImVec2(c.x + 12, c.y - 8), 3.0f, color);
                dl->AddLine(ImVec2(c.x - 10, c.y + 7), ImVec2(c.x + 10, c.y - 7), color, 2.3f);
                break;
            case Icon::Polyline:
                dl->AddLine(ImVec2(c.x - 14, c.y + 7), ImVec2(c.x - 5, c.y - 6), color, 2.2f);
                dl->AddLine(ImVec2(c.x - 5, c.y - 6), ImVec2(c.x + 4, c.y + 7), color, 2.2f);
                dl->AddLine(ImVec2(c.x + 4, c.y + 7), ImVec2(c.x + 14, c.y - 7), color, 2.2f);
                break;
            case Icon::Circle:
                dl->AddCircle(c, 12.0f, color, 56, 2.3f);
                dl->AddLine(ImVec2(c.x, c.y), ImVec2(c.x + 9, c.y - 7), color, 1.6f);
                break;
            case Icon::Arc:
                dl->PathClear();
                dl->PathArcTo(ImVec2(c.x + 1, c.y + 4), 13.0f, -2.5f, -0.7f, 20);
                dl->PathStroke(color, 0, 2.3f);
                break;
            case Icon::Rect:
                dl->AddRect(ImVec2(c.x - 12, c.y - 8), ImVec2(c.x + 12, c.y + 8), color, 0.0f, 0, 2.2f);
                break;
            case Icon::Copy:
                dl->AddRect(ImVec2(c.x - 9, c.y - 9), ImVec2(c.x + 4, c.y + 4), color, 0.0f, 0, 1.7f);
                dl->AddRect(ImVec2(c.x - 4, c.y - 4), ImVec2(c.x + 9, c.y + 9), color, 0.0f, 0, 1.7f);
                break;
            case Icon::Move:
                dl->AddLine(ImVec2(c.x - 10, c.y), ImVec2(c.x + 10, c.y), color, 1.8f);
                dl->AddLine(ImVec2(c.x, c.y - 10), ImVec2(c.x, c.y + 10), color, 1.8f);
                dl->AddLine(ImVec2(c.x + 6, c.y - 4), ImVec2(c.x + 10, c.y), color, 1.8f);
                dl->AddLine(ImVec2(c.x + 6, c.y + 4), ImVec2(c.x + 10, c.y), color, 1.8f);
                break;
            case Icon::Rotate:
                dl->PathClear();
                dl->PathArcTo(c, 9.0f, -0.8f, 2.2f, 18);
                dl->PathStroke(color, 0, 2.0f);
                dl->AddLine(ImVec2(c.x + 9, c.y - 2), ImVec2(c.x + 4, c.y - 6), color, 2.0f);
                break;
            case Icon::Scale:
                dl->AddRect(ImVec2(c.x - 9, c.y - 3), ImVec2(c.x - 1, c.y + 5), color, 0, 0, 1.6f);
                dl->AddRect(ImVec2(c.x - 1, c.y - 8), ImVec2(c.x + 9, c.y + 8), color, 0, 0, 1.6f);
                break;
            case Icon::Text:
                dl->AddLine(ImVec2(c.x - 10, c.y - 9), ImVec2(c.x + 10, c.y - 9), color, 2.2f);
                dl->AddLine(ImVec2(c.x, c.y - 9), ImVec2(c.x, c.y + 9), color, 2.2f);
                dl->AddLine(ImVec2(c.x - 6, c.y + 9), ImVec2(c.x + 6, c.y + 9), color, 2.2f);
                break;
            case Icon::Dimension:
                dl->AddLine(ImVec2(c.x - 12, c.y), ImVec2(c.x + 12, c.y), color, 1.8f);
                dl->AddLine(ImVec2(c.x - 12, c.y - 7), ImVec2(c.x - 12, c.y + 7), color, 1.8f);
                dl->AddLine(ImVec2(c.x + 12, c.y - 7), ImVec2(c.x + 12, c.y + 7), color, 1.8f);
                dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x - 3, c.y), color, 2.4f);
                break;
            case Icon::Leader:
                dl->AddLine(ImVec2(c.x - 10, c.y + 8), ImVec2(c.x + 5, c.y - 4), color, 2.0f);
                dl->AddLine(ImVec2(c.x + 5, c.y - 4), ImVec2(c.x + 12, c.y - 4), color, 2.0f);
                break;
            case Icon::Layers:
                for (int i = -1; i <= 1; ++i)
                {
                    dl->AddLine(ImVec2(c.x - 11, c.y + i * 7), ImVec2(c.x, c.y - 6 + i * 7), color, 1.9f);
                    dl->AddLine(ImVec2(c.x, c.y - 6 + i * 7), ImVec2(c.x + 11, c.y + i * 7), color, 1.9f);
                    dl->AddLine(ImVec2(c.x + 11, c.y + i * 7), ImVec2(c.x, c.y + 6 + i * 7), color, 1.9f);
                    dl->AddLine(ImVec2(c.x, c.y + 6 + i * 7), ImVec2(c.x - 11, c.y + i * 7), color, 1.9f);
                }
                break;
            case Icon::MakeBlock:
                dl->AddRect(ImVec2(c.x - 11, c.y - 9), ImVec2(c.x + 11, c.y + 9), color, 1, 0, 1.8f);
                dl->AddCircle(ImVec2(c.x - 3, c.y + 2), 4.0f, color, 20, 1.5f);
                dl->AddRect(ImVec2(c.x + 1, c.y - 5), ImVec2(c.x + 7, c.y + 1), color, 0, 0, 1.4f);
                break;
            case Icon::InsertBlock:
                dl->AddRect(ImVec2(c.x - 11, c.y - 9), ImVec2(c.x + 11, c.y + 9), color, 1, 0, 1.8f);
                dl->AddLine(ImVec2(c.x - 3, c.y + 2), ImVec2(c.x + 5, c.y - 5), color, 1.8f);
                dl->AddCircleFilled(ImVec2(c.x - 3, c.y + 2), 2.5f, color);
                break;
            case Icon::Match:
                dl->AddRect(ImVec2(c.x - 7, c.y - 10), ImVec2(c.x + 5, c.y - 1), color, 0, 0, 1.6f);
                dl->AddLine(ImVec2(c.x - 1, c.y - 1), ImVec2(c.x + 5, c.y + 10), color, 2.4f);
                break;
            case Icon::Group:
                dl->AddRect(ImVec2(c.x - 12, c.y - 8), ImVec2(c.x + 2, c.y + 6), color, 0, 0, 1.6f);
                dl->AddRect(ImVec2(c.x - 2, c.y - 8), ImVec2(c.x + 12, c.y + 6), color, 0, 0, 1.4f);
                break;
            case Icon::Ungroup:
                dl->AddRect(ImVec2(c.x - 12, c.y - 8), ImVec2(c.x + 2, c.y + 6), color, 0, 0, 1.6f);
                dl->AddLine(ImVec2(c.x + 4, c.y - 8), ImVec2(c.x + 12, c.y + 6), color, 1.6f);
                break;
            case Icon::Paste:
                dl->AddRect(ImVec2(c.x - 8, c.y - 9), ImVec2(c.x + 8, c.y + 9), color, 1, 0, 1.7f);
                dl->AddRect(ImVec2(c.x - 4, c.y - 11), ImVec2(c.x + 4, c.y - 6), color, 1, 0, 1.7f);
                break;
            case Icon::Clipboard:
                dl->AddLine(ImVec2(c.x - 5, c.y + 8), ImVec2(c.x + 5, c.y - 8), color, 2.0f);
                dl->AddLine(ImVec2(c.x + 5, c.y - 8), ImVec2(c.x + 8, c.y - 4), color, 1.7f);
                break;
            case Icon::Measure:
                dl->PathClear();
                dl->PathLineTo(ImVec2(c.x - 12, c.y + 9));
                dl->PathLineTo(ImVec2(c.x + 2, c.y + 9));
                dl->PathLineTo(ImVec2(c.x + 12, c.y - 9));
                dl->PathStroke(color, 0, 2.2f);
                break;
            }
        }

        bool iconAt(const char* id, const char* label, const ImVec2& origin, const ImVec2& size, Icon icon, bool enabled)
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
            drawIcon(dl, ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f - (label ? 6.0f : 0.0f)), icon, IM_COL32(116, 195, 248, 255));
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

        bool miniIconAt(const char* id, const ImVec2& origin, const ImVec2& size, Icon icon)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(origin);
            bool pressed = ImGui::InvisibleButton(id, size);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), hoverColor(ImGui::IsItemHovered(), ImGui::IsItemActive()), 3.0f);
            drawIcon(dl, ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f), icon, IM_COL32(205, 215, 228, 255));
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
        std::vector<std::pair<Icon, UiActionType>> quick = {
            {Icon::New, UiActionType::None}, {Icon::Open, UiActionType::OpenFile},
            {Icon::Save, UiActionType::Save}, {Icon::Undo, UiActionType::Undo},
            {Icon::Redo, UiActionType::Redo}
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
        if (iconAt("DrawLine", "直线", ImVec2(base.x + 10, by + 4), ImVec2(60, 88), Icon::Line, true)) setTool("Line");
        if (iconAt("DrawPoly", "多段线", ImVec2(base.x + 74, by + 4), ImVec2(60, 88), Icon::Polyline, true)) setTool("Polyline");
        if (iconAt("DrawCircle", "圆", ImVec2(base.x + 138, by + 4), ImVec2(60, 88), Icon::Circle, true)) setTool("Circle");
        if (iconAt("DrawArc", "弧", ImVec2(base.x + 202, by + 4), ImVec2(60, 88), Icon::Arc, true)) setTool("Arc");
        if (iconAt("DrawRect", "矩形", ImVec2(base.x + 266, by + 4), ImVec2(60, 88), Icon::Rect, true)) setTool("Rectangle");
        groupCaption(dl, "绘制", base.x, 342.0f, capY);

        if (iconAt("Copy", "复制", ImVec2(base.x + 350, by + 4), ImVec2(58, 88), Icon::Copy, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        if (iconAt("Move", "移动", ImVec2(base.x + 412, by + 4), ImVec2(58, 88), Icon::Move, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        groupCaption(dl, "修改", base.x + 342.0f, 138.0f, capY);

        if (iconAt("Text", "文字", ImVec2(base.x + 488, by + 4), ImVec2(58, 88), Icon::Text, false)) {}
        if (iconAt("Dim", "尺寸", ImVec2(base.x + 550, by + 4), ImVec2(58, 88), Icon::Dimension, false)) {}
        groupCaption(dl, "注释", base.x + 480.0f, 138.0f, capY);

        if (iconAt("Layers", "图层", ImVec2(base.x + 626, by + 4), ImVec2(64, 88), Icon::Layers, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        comboLike("LayerMode", "随层", ImVec2(base.x + 698, by + 5), ImVec2(168, 23));
        comboLike("LayerPlot", "随层", ImVec2(base.x + 698, by + 33), ImVec2(168, 23));
        comboLike("LayerWide", "随层", ImVec2(base.x + 698, by + 61), ImVec2(168, 23));
        groupCaption(dl, "图层", base.x + 618.0f, 256.0f, capY);

        if (iconAt("MakeBlock", "创建块", ImVec2(base.x + 882, by + 4), ImVec2(62, 88), Icon::MakeBlock, false)) {}
        if (iconAt("InsertBlock", "插入块", ImVec2(base.x + 948, by + 4), ImVec2(62, 88), Icon::InsertBlock, false)) {}
        groupCaption(dl, "块", base.x + 874.0f, 118.0f, capY);

        if (iconAt("Match", "匹配", ImVec2(base.x + 1000, by + 4), ImVec2(60, 88), Icon::Match, false)) {}
        comboLike("Prop1", "随层", ImVec2(base.x + 1066, by + 5), ImVec2(106, 23));
        comboLike("Prop2", "随层", ImVec2(base.x + 1066, by + 33), ImVec2(106, 23));
        comboLike("Prop3", "随层", ImVec2(base.x + 1066, by + 61), ImVec2(106, 23));
        groupCaption(dl, "特性", base.x + 992.0f, 188.0f, capY);

        if (iconAt("Group", "组", ImVec2(base.x + 1188, by + 4), ImVec2(54, 88), Icon::Group, false)) {}
        if (iconAt("Ungroup", "取消组", ImVec2(base.x + 1246, by + 4), ImVec2(54, 88), Icon::Ungroup, false)) {}
        groupCaption(dl, "组", base.x + 1180.0f, 118.0f, capY);

        if (iconAt("Paste", "粘贴", ImVec2(base.x + 1306, by + 4), ImVec2(56, 88), Icon::Paste, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        miniIconAt("Cut", ImVec2(base.x + 1364, by + 8), ImVec2(24, 22), Icon::Clipboard);
        groupCaption(dl, "剪贴板", base.x + 1298.0f, 86.0f, capY);

        if (iconAt("Measure", "测量", ImVec2(base.x + 1392, by + 4), ImVec2(62, 88), Icon::Measure, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
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
