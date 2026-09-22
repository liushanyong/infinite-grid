#include "UiHost.hpp"

#include "CadViewportHost.hpp"
#include "CommandLinePanel.hpp"
#include "PropertiesPanel.hpp"
#include "StatusBar.hpp"
#include "ToolbarPanel.hpp"
#include "ViewportPanel.hpp"

#include <imgui.h>

namespace ui
{
    UiHost::UiHost(IAppController& controller, InputRouter& router)
        : controller_(controller)
        , router_(router)
        , viewportHost_(std::make_unique<CadViewportHost>())
    {
    }

    void UiHost::draw(float frameMs)
    {
        AppSnapshot snapshot = controller_.snapshot();
        snapshot.fps = ImGui::GetIO().Framerate;
        snapshot.frameMs = frameMs;

        drawGlobalShortcuts(snapshot);
        drawToolbar(snapshot);
        drawPanels(snapshot);
    }

    void UiHost::drawPanels(const AppSnapshot& snapshot)
    {
        viewportHost_->setGridEnabled(snapshot.gridEnabled);
        viewportHost_->setSnapEnabled(snapshot.snapEnabled);
        viewportHost_->setOrthoEnabled(snapshot.orthoEnabled);

        drawPropertiesPanel(controller_, snapshot);
        drawViewportPanel(controller_, *viewportHost_, snapshot, router_);
        drawCommandLinePanel(controller_, snapshot, viewportHost_.get());
        drawStatusBar(controller_, snapshot, viewportHost_->cursorWorld(), viewportHost_->statusText());
    }

    void UiHost::drawToolbar(const AppSnapshot& snapshot)
    {
        drawToolbarPanel(controller_, snapshot);
    }

    void UiHost::drawGlobalShortcuts(const AppSnapshot& snapshot)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput)
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            if (snapshot.activeTool != "Select" || snapshot.hasPendingDrawPoint)
            {
                UiAction action;
                action.type = UiActionType::SetTool;
                action.payload = "Select";
                controller_.execute(action);
            }
            else
            {
                UiAction action;
                action.type = UiActionType::SelectObject;
                action.objectId = -1;
                controller_.execute(action);
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && snapshot.selectedObjectId >= 0)
        {
            UiAction action;
            action.type = UiActionType::DeleteObject;
            action.objectId = -1;
            controller_.execute(action);
        }

        const bool ctrl = ImGui::IsKeyDown(ImGuiMod_Ctrl);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O))
        {
            UiAction action;
            action.type = UiActionType::OpenFile;
            action.payload = "demo.dxf";
            controller_.execute(action);
        }

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            UiAction action;
            action.type = UiActionType::Save;
            controller_.execute(action);
        }

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            UiAction action;
            action.type = UiActionType::SelectAll;
            controller_.execute(action);
        }

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            UiAction action;
            action.type = UiActionType::Undo;
            controller_.execute(action);
        }

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y))
        {
            UiAction action;
            action.type = UiActionType::Redo;
            controller_.execute(action);
        }
    }
}
