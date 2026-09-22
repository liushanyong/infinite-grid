#include "StatusBar.hpp"
#include "UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui
{
    namespace
    {
        enum class SIcon
        {
            Menu,
            Plus,
            List,
            Angle,
            Snap,
            View,
            Layers,
            Filter,
            Extents,
            Settings
        };

        void drawSIcon(ImDrawList* dl, const ImVec2& c, SIcon icon, ImU32 color)
        {
            switch (icon)
            {
            case SIcon::Menu:
                for (int i = -1; i <= 1; ++i) dl->AddLine(ImVec2(c.x - 8, c.y + i * 5), ImVec2(c.x + 8, c.y + i * 5), color, 1.7f);
                break;
            case SIcon::Plus:
                dl->AddLine(ImVec2(c.x - 7, c.y), ImVec2(c.x + 7, c.y), color, 1.8f);
                dl->AddLine(ImVec2(c.x, c.y - 7), ImVec2(c.x, c.y + 7), color, 1.8f);
                break;
            case SIcon::List:
                for (int i = -1; i <= 1; ++i)
                {
                    dl->AddLine(ImVec2(c.x - 8, c.y + i * 5), ImVec2(c.x - 3, c.y + i * 5), color, 2.0f);
                    dl->AddLine(ImVec2(c.x + 1, c.y + i * 5), ImVec2(c.x + 8, c.y + i * 5), color, 1.4f);
                }
                break;
            case SIcon::Angle:
                dl->AddLine(ImVec2(c.x - 7, c.y + 6), ImVec2(c.x + 7, c.y + 6), color, 1.6f);
                dl->AddLine(ImVec2(c.x - 2, c.y + 6), ImVec2(c.x + 6, c.y - 5), color, 1.6f);
                break;
            case SIcon::Snap:
                dl->AddCircle(c, 6.0f, color, 20, 1.6f);
                dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), color, 1.3f);
                dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), color, 1.3f);
                break;
            case SIcon::View:
                dl->AddRect(ImVec2(c.x - 8, c.y - 7), ImVec2(c.x + 8, c.y + 7), color, 0, 0, 1.5f);
                dl->AddLine(ImVec2(c.x - 8, c.y + 1), ImVec2(c.x + 8, c.y + 1), color, 1.3f);
                break;
            case SIcon::Layers:
                dl->AddLine(ImVec2(c.x - 7, c.y), ImVec2(c.x, c.y - 5), color, 1.5f);
                dl->AddLine(ImVec2(c.x, c.y - 5), ImVec2(c.x + 7, c.y), color, 1.5f);
                dl->AddLine(ImVec2(c.x + 7, c.y), ImVec2(c.x, c.y + 5), color, 1.5f);
                dl->AddLine(ImVec2(c.x, c.y + 5), ImVec2(c.x - 7, c.y), color, 1.5f);
                break;
            case SIcon::Filter:
                dl->AddTriangle(ImVec2(c.x - 7, c.y - 6), ImVec2(c.x + 7, c.y - 6), ImVec2(c.x + 1, c.y + 1), color);
                dl->AddRect(ImVec2(c.x - 1, c.y + 1), ImVec2(c.x + 1, c.y + 7), color, 0, 0, 1.4f);
                break;
            case SIcon::Extents:
                dl->AddRect(ImVec2(c.x - 7, c.y - 7), ImVec2(c.x + 7, c.y + 7), color, 0, 0, 1.4f);
                dl->AddLine(ImVec2(c.x - 3, c.y + 3), ImVec2(c.x + 3, c.y - 3), color, 1.4f);
                break;
            case SIcon::Settings:
                dl->AddCircle(c, 5.0f, color, 20, 1.5f);
                for (int i = 0; i < 8; ++i)
                {
                    const float a = i * 0.7853981634f;
                    dl->AddLine(ImVec2(c.x + 6 * std::cos(a), c.y + 6 * std::sin(a)), ImVec2(c.x + 8 * std::cos(a), c.y + 8 * std::sin(a)), color, 1.4f);
                }
                break;
            }
        }

        bool statusButton(const char* id, const ImVec2& o, SIcon icon, bool active = false, bool enabled = true)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(o);
            if (!enabled)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);
                ImGui::BeginDisabled(true);
            }
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(32.0f, 24.0f));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool hovered = ImGui::IsItemHovered();
            const ImU32 bg = active ? IM_COL32(19, 144, 204, 255) : (hovered ? IM_COL32(58, 58, 58, 255) : IM_COL32(0, 0, 0, 0));
            if (bg) dl->AddRectFilled(o, ImVec2(o.x + 32.0f, o.y + 24.0f), bg, 2.0f);
            drawSIcon(dl, ImVec2(o.x + 16.0f, o.y + 12.0f), icon, active ? IM_COL32(22, 22, 22, 255) : IM_COL32(211, 211, 211, 255));
            if (!enabled)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
            ImGui::PopID();
            return pressed;
        }

        bool textButton(const char* id, const char* text, const ImVec2& o, float width, bool active = false)
        {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(o);
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, 24.0f));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool hovered = ImGui::IsItemHovered();
            if (active || hovered) dl->AddRectFilled(o, ImVec2(o.x + width, o.y + 24.0f), active ? IM_COL32(19, 144, 204, 255) : IM_COL32(58, 58, 58, 255), 2.0f);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(o.x + (width - ts.x) * 0.5f, o.y + (24.0f - ts.y) * 0.5f), active ? IM_COL32(20, 20, 20, 255) : IM_COL32(212, 212, 212, 255), text);
            ImGui::PopID();
            return pressed;
        }
    }

    void drawStatusBar(
        IAppController& controller,
        const AppSnapshot& snapshot,
        const ImVec2& cursorWorld,
        const std::string& viewportStatus
    )
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kStatusBarHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(42, 42, 42, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##CadStatusBar", nullptr, flags);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 base = vp->WorkPos;
        const float y = base.y + vp->WorkSize.y - kStatusBarHeight + 8.0f;

        if (statusButton("MenuButton", ImVec2(base.x + 8.0f, y), SIcon::Menu))
        {
        }
        if (textButton("ModelButton", "Model", ImVec2(base.x + 48.0f, y), 64.0f, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        if (textButton("LayoutButton", "Layout1", ImVec2(base.x + 116.0f, y), 74.0f, false)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        statusButton("NewLayout", ImVec2(base.x + 194.0f, y), SIcon::Plus);
        (void)viewportStatus;

        char coordinate[96];
        snprintf(coordinate, sizeof(coordinate), "%.4f, %.4f, 0.0000", cursorWorld.x, cursorWorld.y);
        const float coordinateWidth = ImGui::CalcTextSize(coordinate).x;
        const float coordinateX = std::max(300.0f, base.x + vp->WorkSize.x - 750.0f);
        dl->AddText(ImVec2(coordinateX, y + 3.0f), IM_COL32(222, 222, 222, 255), coordinate);

        float rx = coordinateX + coordinateWidth + 18.0f;
        if (statusButton("CoordPlus", ImVec2(rx, y), SIcon::Plus)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("ViewList", ImVec2(rx, y), SIcon::List)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("OrthoAngle", ImVec2(rx, y), SIcon::Angle, snapshot.orthoEnabled)) { UiAction a; a.type=UiActionType::ToggleOrtho; controller.execute(a); } rx += 34.0f;
        if (statusButton("SnapGrid", ImVec2(rx, y), SIcon::Snap, snapshot.snapEnabled)) { UiAction a; a.type=UiActionType::ToggleSnap; controller.execute(a); } rx += 34.0f;

        // viewport preset combo
        ImGui::PushID("ViewportPreset");
        ImGui::SetCursorScreenPos(ImVec2(rx, y));
        if (ImGui::InvisibleButton("ViewportPreset", ImVec2(46.0f, 24.0f))) { UiAction a; a.type=UiActionType::ToggleGrid; controller.execute(a); }
        const bool vpHover = ImGui::IsItemHovered();
        const bool vpActive = snapshot.gridEnabled;
        dl->AddRectFilled(ImVec2(rx, y), ImVec2(rx + 46.0f, y + 24.0f), vpActive ? IM_COL32(19, 144, 204, 255) : (vpHover ? IM_COL32(58, 58, 58, 255) : IM_COL32(52, 52, 52, 255)), 2.0f);
        drawSIcon(dl, ImVec2(rx + 14.0f, y + 12.0f), SIcon::View, IM_COL32(22, 22, 22, 255));
        dl->AddTriangleFilled(ImVec2(rx + 26.0f, y + 10.0f), ImVec2(rx + 36.0f, y + 10.0f), ImVec2(rx + 31.0f, y + 16.0f), IM_COL32(40, 40, 40, 255));
        ImGui::PopID();
        rx += 50.0f;
        if (textButton("ScaleButton", "1:1", ImVec2(rx, y), 38.0f)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 42.0f;
        if (statusButton("LayersStatus", ImVec2(rx, y), SIcon::Layers)) { UiAction a; a.type=UiActionType::None; controller.execute(a); } rx += 34.0f;
        if (statusButton("EyeStatus", ImVec2(rx, y), SIcon::Filter, false, false)) {} rx += 34.0f;
        if (statusButton("DocStatus", ImVec2(rx, y), SIcon::Extents)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("FilterStatus", ImVec2(rx, y), SIcon::Filter)) { UiAction a; a.type=UiActionType::SelectAll; controller.execute(a); } rx += 34.0f;
        if (statusButton("ExtentsStatus", ImVec2(rx, y), SIcon::Extents)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("SettingsStatus", ImVec2(rx, y), SIcon::Settings)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(1);
    }
}
