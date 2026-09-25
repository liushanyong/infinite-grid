#include "UiHost.hpp"

#include "CadViewportHost.hpp"
#include "CommandLinePanel.hpp"
#include "PropertiesPanel.hpp"
#include "StatusBar.hpp"
#include "DocumentPanel.hpp"
#include "LayersPanel.hpp"
#include "OutlinePanel.hpp"
#include "ToolbarPanel.hpp"
#include "ViewportPanel.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "UiLayout.hpp"
#include <algorithm>

namespace ui
{
    namespace
    {
        void drawDockHost()
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 position(
                viewport->WorkPos.x,
                viewport->WorkPos.y + kContentTop
            );
            const ImVec2 size(
                std::max(0.0f, viewport->WorkSize.x),
                std::max(0.0f, viewport->WorkSize.y - kContentTop - kStatusBarHeight)
            );

            ImGui::SetNextWindowPos(position, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;
            ImGui::Begin("##DockShellHost", nullptr, hostFlags);

            const ImGuiID dockspaceId = ImGui::GetID("OpenCADStudioMainDockSpace");
            ImGuiDockNode* existingNode = ImGui::DockBuilderGetNode(dockspaceId);
            if (!existingNode || dockLayoutResetRequest())
            {
                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspaceId, size);

                ImGuiID leftId = 0;
                ImGuiID remainderId = 0;
                ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.22f, &leftId, &remainderId);

                ImGuiID commandId = 0;
                ImGuiID centerId = 0;
                ImGui::DockBuilderSplitNode(remainderId, ImGuiDir_Down, 0.16f, &commandId, &centerId);

                ImGui::DockBuilderDockWindow("特性", leftId);
                ImGui::DockBuilderDockWindow("视口", centerId);
                ImGui::DockBuilderDockWindow("命令行", commandId);

                // Optional tool windows keep stable dock targets and become visible
                // from the ribbon Window strip.
                ImGui::DockBuilderDockWindow("图层", leftId);
                ImGui::DockBuilderDockWindow("大纲", leftId);
                ImGui::DockBuilderDockWindow("文档", leftId);

                ImGui::DockBuilderFinish(dockspaceId);
                dockLayoutResetRequest() = false;
            }

            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
            ImGui::End();
            ImGui::PopStyleVar(3);
        }
    }

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
        drawDockHost();
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
        drawLayersPanel(controller_, snapshot);
        drawOutlinePanel(controller_, snapshot);
        drawDocumentPanel(controller_, snapshot);
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

        if (ctrl && ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyPressed(ImGuiKey_L))
        {
            dockLayoutResetRequest() = true;
        }
    }
}
