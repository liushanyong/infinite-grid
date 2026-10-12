#include "StatusBar.hpp"
#include "UiLayout.hpp"
#include "IconSet.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui
{
    namespace
    {
        bool statusButton(const char* id, const ImVec2& o, IconId icon, bool active = false, bool enabled = true)
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
            DrawIconMono(dl, icon, ImVec2(o.x + 16.0f, o.y + 12.0f), 17.0f,
                         active ? IM_COL32(22, 22, 22, 255) : IM_COL32(211, 211, 211, 255));
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

        if (statusButton("MenuButton", ImVec2(base.x + 8.0f, y), IconId::StatusMenu))
        {
        }
        if (textButton("ModelButton", "Model", ImVec2(base.x + 48.0f, y), 64.0f, true)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        if (textButton("LayoutButton", "Layout1", ImVec2(base.x + 116.0f, y), 74.0f, false)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }
        statusButton("NewLayout", ImVec2(base.x + 194.0f, y), IconId::StatusPlus);
        (void)viewportStatus;

        char coordinate[96];
        snprintf(coordinate, sizeof(coordinate), "%.4f, %.4f, 0.0000", cursorWorld.x, cursorWorld.y);
        const float coordinateWidth = ImGui::CalcTextSize(coordinate).x;
        const float coordinateX = std::max(300.0f, base.x + vp->WorkSize.x - 750.0f);
        dl->AddText(ImVec2(coordinateX, y + 3.0f), IM_COL32(222, 222, 222, 255), coordinate);

        float rx = coordinateX + coordinateWidth + 18.0f;
        if (statusButton("CoordPlus", ImVec2(rx, y), IconId::StatusPlus)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("ViewList", ImVec2(rx, y), IconId::StatusList)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("OrthoAngle", ImVec2(rx, y), IconId::StatusOrtho, snapshot.orthoEnabled)) { UiAction a; a.type=UiActionType::ToggleOrtho; controller.execute(a); } rx += 34.0f;
        if (statusButton("SnapGrid", ImVec2(rx, y), IconId::StatusSnap, snapshot.snapEnabled)) { UiAction a; a.type=UiActionType::ToggleSnap; controller.execute(a); } rx += 34.0f;

        // viewport preset combo
        ImGui::PushID("ViewportPreset");
        ImGui::SetCursorScreenPos(ImVec2(rx, y));
        if (ImGui::InvisibleButton("ViewportPreset", ImVec2(46.0f, 24.0f))) { UiAction a; a.type=UiActionType::ToggleGrid; controller.execute(a); }
        const bool vpHover = ImGui::IsItemHovered();
        const bool vpActive = snapshot.gridEnabled;
        dl->AddRectFilled(ImVec2(rx, y), ImVec2(rx + 46.0f, y + 24.0f), vpActive ? IM_COL32(19, 144, 204, 255) : (vpHover ? IM_COL32(58, 58, 58, 255) : IM_COL32(52, 52, 52, 255)), 2.0f);
        DrawIconMono(dl, IconId::StatusViewport, ImVec2(rx + 14.0f, y + 12.0f), 17.0f, IM_COL32(22, 22, 22, 255));
        dl->AddTriangleFilled(ImVec2(rx + 26.0f, y + 10.0f), ImVec2(rx + 36.0f, y + 10.0f), ImVec2(rx + 31.0f, y + 16.0f), IM_COL32(40, 40, 40, 255));
        ImGui::PopID();
        rx += 50.0f;
        if (textButton("ScaleButton", "1:1", ImVec2(rx, y), 38.0f)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 42.0f;
        if (statusButton("LayersStatus", ImVec2(rx, y), IconId::StatusLayers)) { UiAction a; a.type=UiActionType::None; controller.execute(a); } rx += 34.0f;
        if (statusButton("EyeStatus", ImVec2(rx, y), IconId::StatusFilter, false, false)) {} rx += 34.0f;
        if (statusButton("DocStatus", ImVec2(rx, y), IconId::StatusExtents)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("FilterStatus", ImVec2(rx, y), IconId::StatusFilter)) { UiAction a; a.type=UiActionType::SelectAll; controller.execute(a); } rx += 34.0f;
        if (statusButton("ExtentsStatus", ImVec2(rx, y), IconId::StatusExtents)) { UiAction a; a.type=UiActionType::FocusViewport; controller.execute(a); } rx += 34.0f;
        if (statusButton("SettingsStatus", ImVec2(rx, y), IconId::StatusSettings)) { UiAction a; a.type=UiActionType::None; controller.execute(a); }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(1);
    }
}
