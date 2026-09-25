#include "ViewportPanel.hpp"
#include "UiLayout.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace ui
{
    namespace
    {
        void drawNavigator(const ImVec2& canvasMin, const ImVec2& canvasSize)
        {
            if (canvasSize.x < 340.0f || canvasSize.y < 330.0f) return;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 c(canvasMin.x + canvasSize.x - 150.0f, canvasMin.y + 170.0f);
            const ImU32 pale = IM_COL32(160, 199, 226, 255);
            const ImU32 dark = IM_COL32(20, 20, 20, 255);

            dl->AddCircleFilled(ImVec2(c.x, c.y - 50.0f), 9.0f, IM_COL32(205, 205, 205, 255), 16);
            dl->AddText(ImVec2(c.x - 4.0f, c.y - 76.0f), IM_COL32(235, 235, 235, 255), "N");
            dl->AddText(ImVec2(c.x - 3.0f, c.y + 66.0f), IM_COL32(235, 235, 235, 255), "S");
            dl->AddText(ImVec2(c.x - 80.0f, c.y - 8.0f), IM_COL32(235, 235, 235, 255), "W");
            dl->AddText(ImVec2(c.x + 71.0f, c.y - 8.0f), IM_COL32(235, 235, 235, 255), "E");
            dl->AddCircleFilled(c, 70.0f, pale, 80);
            dl->AddRect(ImVec2(c.x - 42.0f, c.y - 42.0f), ImVec2(c.x + 42.0f, c.y + 42.0f), IM_COL32(222, 235, 245, 255), 2.0f, 0, 8.0f);
            dl->AddRect(ImVec2(c.x - 48.0f, c.y - 47.0f), ImVec2(c.x + 48.0f, c.y + 47.0f), dark, 2.0f, 0, 1.0f);
            dl->AddLine(ImVec2(c.x - 48.0f, c.y - 47.0f), ImVec2(c.x + 48.0f, c.y - 47.0f), dark, 4.0f);
            dl->AddLine(ImVec2(c.x - 3.0f, c.y - 8.0f), ImVec2(c.x + 7.0f, c.y - 1.0f), dark, 3.0f);
            dl->AddLine(ImVec2(c.x - 3.0f, c.y - 8.0f), ImVec2(c.x - 3.0f, c.y + 7.0f), dark, 3.0f);
            const ImU32 arrow = IM_COL32(42, 42, 42, 255);
            dl->AddTriangle(ImVec2(c.x - 6, c.y - 56), ImVec2(c.x + 6, c.y - 56), ImVec2(c.x, c.y - 47), arrow);
            dl->AddTriangle(ImVec2(c.x - 6, c.y + 56), ImVec2(c.x + 6, c.y + 56), ImVec2(c.x, c.y + 47), arrow);
            dl->AddTriangle(ImVec2(c.x - 56, c.y - 6), ImVec2(c.x - 56, c.y + 6), ImVec2(c.x - 47, c.y), arrow);
            dl->AddTriangle(ImVec2(c.x + 56, c.y - 6), ImVec2(c.x + 56, c.y + 6), ImVec2(c.x + 47, c.y), arrow);

            const ImVec2 wcs(c.x - 55.0f, c.y + 92.0f);
            dl->AddRectFilled(wcs, ImVec2(wcs.x + 120.0f, wcs.y + 26.0f), IM_COL32(54, 54, 54, 255), 2.0f);
            dl->AddRect(wcs, ImVec2(wcs.x + 120.0f, wcs.y + 26.0f), IM_COL32(100, 100, 100, 255), 2.0f);
            dl->AddText(ImVec2(wcs.x + 10.0f, wcs.y + 5.0f), IM_COL32(235, 235, 235, 255), "WCS");
            dl->AddTriangleFilled(ImVec2(wcs.x + 94.0f, wcs.y + 10.0f), ImVec2(wcs.x + 110.0f, wcs.y + 10.0f), ImVec2(wcs.x + 102.0f, wcs.y + 19.0f), IM_COL32(210, 210, 210, 255));
        }

        void drawTopOverlay(IAppController& controller, ViewportHost& viewportHost, const ImVec2& canvasMin)
        {
            ImGui::SetCursorScreenPos(ImVec2(canvasMin.x + 10.0f, canvasMin.y + 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 45, 45, 235));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(62, 62, 62, 240));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(19, 144, 204, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(52, 52, 52, 240));
            if (ImGui::SmallButton("G"))
            {
                UiAction action; action.type = UiActionType::ToggleGrid; controller.execute(action);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("U")) viewportHost.reset();
            ImGui::SameLine();
            ImGui::PushItemWidth(96.0f);
            static int renderMode = 0;
            ImGui::Combo("##RenderMode", &renderMode, "线框 2D\0三维线框\0\0");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SmallButton("P");
            ImGui::SameLine();
            ImGui::SmallButton("D");
            ImGui::PopStyleColor(4);
        }
    }

    void drawViewportPanel(
        IAppController& controller,
        ViewportHost& viewportHost,
        const AppSnapshot& snapshot,
        const InputRouter& router
    )
    {
        ImGui::SetNextWindowSize(ImVec2(1100.0f, 700.0f), ImGuiCond_FirstUseEver);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;
        const bool open = ImGui::Begin("视口", nullptr, flags);
        if (!open)
        {
            ImGui::End();
            ImGui::PopStyleVar(3);
            return;
        }

        viewportHost.setScene(snapshot);
        const bool selectionTool = snapshot.activeTool.empty() || snapshot.activeTool == "Select";
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const bool hasCanvas = canvasSize.x > 0.0f && canvasSize.y > 0.0f;

        if (hasCanvas)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(canvasMin, ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y), IM_COL32(22, 22, 22, 255));
            viewportHost.draw(drawList, canvasMin, canvasSize);
            ImGui::InvisibleButton("ViewportCanvas", canvasSize);
            drawNavigator(canvasMin, canvasSize);
            drawTopOverlay(controller, viewportHost, canvasMin);
        }

        const bool hovered = hasCanvas && ImGui::IsItemHovered();
        const bool active = hasCanvas && ImGui::IsItemActive();
        const bool clicked = hasCanvas && ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool boxSelectionModifier = selectionTool && ImGui::GetIO().KeyShift;

        static bool boxSelecting = false;
        static ImVec2 boxStart{};

        ViewportInput input;
        input.hovered = hovered;
        input.clicked = clicked;
        input.dragging = active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !boxSelectionModifier && !boxSelecting;
        input.wheelDelta = ImGui::GetIO().MouseWheel;
        input.mousePos = ImGui::GetIO().MousePos;
        input.dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

        if (hovered && router.viewportMouseAvailable() && !boxSelectionModifier && !boxSelecting)
        {
            viewportHost.handleInput(input);
            if (input.dragging)
            {
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
        }

        if (active && boxSelectionModifier && !boxSelecting && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, 3.0f))
        {
            const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            boxStart = ImVec2(ImGui::GetIO().MousePos.x - dragDelta.x, ImGui::GetIO().MousePos.y - dragDelta.y);
            boxSelecting = true;
        }

        if (boxSelecting && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 boxEnd = ImGui::GetIO().MousePos;
            const ImVec2 boxMin(std::min(boxStart.x, boxEnd.x), std::min(boxStart.y, boxEnd.y));
            const ImVec2 boxMax(std::max(boxStart.x, boxEnd.x), std::max(boxStart.y, boxEnd.y));

            UiAction action;
            action.type = UiActionType::SetSelection;
            action.objectIds = viewportHost.pickInRect(boxMin, boxMax);
            action.selectionModifier = ImGui::GetIO().KeyCtrl
                ? SelectionModifier::Toggle
                : SelectionModifier::Replace;
            controller.execute(action);

            boxSelecting = false;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }

        if (boxSelecting)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 current = ImGui::GetIO().MousePos;
            const ImVec2 rectMin(std::min(boxStart.x, current.x), std::min(boxStart.y, current.y));
            const ImVec2 rectMax(std::max(boxStart.x, current.x), std::max(boxStart.y, current.y));
            drawList->AddRectFilled(rectMin, rectMax, IM_COL32(90, 170, 255, 45));
            drawList->AddRect(rectMin, rectMax, IM_COL32(120, 200, 255, 180), 0.0f, 0, 1.5f);
        }

        if (hovered && router.viewportMouseAvailable() && clicked && !boxSelectionModifier)
        {
            if (!selectionTool)
            {
                const ImVec2 worldPoint = viewportHost.worldAt(input.mousePos);
                UiAction action;
                action.type = UiActionType::ViewportClick;
                action.x1 = worldPoint.x;
                action.y1 = worldPoint.y;
                controller.execute(action);
            }
            else
            {
                const int pickedId = viewportHost.pickAt(input.mousePos);
                const bool toggleSelection = ImGui::GetIO().KeyCtrl;
                if (toggleSelection)
                {
                    UiAction action;
                    action.type = UiActionType::SelectObject;
                    action.objectId = pickedId;
                    action.selectionModifier = pickedId >= 0 ? SelectionModifier::Toggle : SelectionModifier::Replace;
                    controller.execute(action);
                }
                else if (pickedId != snapshot.selectedObjectId)
                {
                    UiAction action;
                    action.type = UiActionType::SelectObject;
                    action.objectId = pickedId;
                    controller.execute(action);
                }
                else
                {
                    UiAction action;
                    action.type = UiActionType::FocusViewport;
                    controller.execute(action);
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
    }
}
