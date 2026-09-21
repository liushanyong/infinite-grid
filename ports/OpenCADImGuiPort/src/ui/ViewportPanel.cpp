#include "ViewportPanel.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace ui
{
    void drawViewportPanel(
        IAppController& controller,
        ViewportHost& viewportHost,
        const AppSnapshot& snapshot,
        const InputRouter& router
    )
    {
        if (!ImGui::Begin("Viewport"))
        {
            ImGui::End();
            return;
        }

        viewportHost.setScene(snapshot);

        const bool selectionTool = snapshot.activeTool.empty() || snapshot.activeTool == "Select";

        ImGui::TextUnformatted("CAD viewport");
        ImGui::Text(
            "Mouse routing: %s",
            router.viewportMouseAvailable() ? "viewport" : "ImGui UI"
        );
        ImGui::Text(
            "Keyboard routing: %s",
            router.viewportKeyboardAvailable() ? "viewport" : "ImGui UI"
        );
        if (selectionTool)
        {
            ImGui::TextDisabled("Ctrl+click toggles selection; Shift+drag box-selects");
        }

        ImGui::Separator();

        const float reservedHeight = ImGui::GetTextLineHeightWithSpacing() * 4.0f;
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y = std::max(0.0f, canvasSize.y - reservedHeight);
        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();

        const bool hasCanvas = canvasSize.x > 0.0f && canvasSize.y > 0.0f;

        if (hasCanvas)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            viewportHost.draw(drawList, canvasMin, canvasSize);
            ImGui::InvisibleButton("ViewportCanvas", canvasSize);
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
        input.dragging = active
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left)
            && !boxSelectionModifier
            && !boxSelecting;
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

        if (active && boxSelectionModifier && !boxSelecting
            && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, 3.0f))
        {
            const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            boxStart = ImVec2(
                ImGui::GetIO().MousePos.x - dragDelta.x,
                ImGui::GetIO().MousePos.y - dragDelta.y
            );
            boxSelecting = true;
        }

        if (boxSelecting && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 boxEnd = ImGui::GetIO().MousePos;
            const ImVec2 boxMin(
                std::min(boxStart.x, boxEnd.x),
                std::min(boxStart.y, boxEnd.y)
            );
            const ImVec2 boxMax(
                std::max(boxStart.x, boxEnd.x),
                std::max(boxStart.y, boxEnd.y)
            );

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
            const ImVec2 rectMin(
                std::min(boxStart.x, current.x),
                std::min(boxStart.y, current.y)
            );
            const ImVec2 rectMax(
                std::max(boxStart.x, current.x),
                std::max(boxStart.y, current.y)
            );
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
                    action.selectionModifier = pickedId >= 0
                        ? SelectionModifier::Toggle
                        : SelectionModifier::Replace;
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

        ImGui::Separator();
        ImGui::Text("Current layer: %s", snapshot.currentLayer.c_str());
        ImGui::Text(
            "Selected object: %s (%d total)",
            snapshot.selectedObject.c_str(),
            static_cast<int>(snapshot.selectedObjectIds.size())
        );
        const ImVec2 cursorWorld = viewportHost.cursorWorld();
        ImGui::Text("Cursor world: %.2f, %.2f", cursorWorld.x, cursorWorld.y);
        ImGui::Text("Viewport: %s", viewportHost.statusText().c_str());

        ImGui::End();
    }
}
