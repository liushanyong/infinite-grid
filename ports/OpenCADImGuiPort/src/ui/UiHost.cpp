#include "UiHost.hpp"

#include "CommandLinePanel.hpp"
#include "DocumentPanel.hpp"
#include "LayersPanel.hpp"
#include "OutlinePanel.hpp"
#include "PropertiesPanel.hpp"
#include "StatusBar.hpp"
#include "ToolbarPanel.hpp"
#include "CadViewportHost.hpp"
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
        drawDockspace();
        drawToolbar(snapshot);
        drawMainMenuBar(snapshot);
        drawPanels(snapshot);

        if (showDemo_)
        {
            ImGui::ShowDemoWindow(&showDemo_);
        }
    }

    void UiHost::drawDockspace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags hostFlags = 0
            | ImGuiWindowFlags_MenuBar
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("OpenCADImGuiDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }

    void UiHost::drawMainMenuBar(const AppSnapshot& snapshot)
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Demo Drawing", "Ctrl+O"))
            {
                UiAction action;
                action.type = UiActionType::OpenFile;
                action.payload = "demo.dxf";
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Save", "Ctrl+S"))
            {
                UiAction action;
                action.type = UiActionType::Save;
                controller_.execute(action);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                UiAction action;
                action.type = UiActionType::Exit;
                controller_.execute(action);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, snapshot.canUndo))
            {
                UiAction action;
                action.type = UiActionType::Undo;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, snapshot.canRedo))
            {
                UiAction action;
                action.type = UiActionType::Redo;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Select All", "Ctrl+A"))
            {
                UiAction action;
                action.type = UiActionType::SelectAll;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Invert Selection"))
            {
                UiAction action;
                action.type = UiActionType::InvertSelection;
                controller_.execute(action);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Clear Selection", "Esc"))
            {
                UiAction action;
                action.type = UiActionType::SelectObject;
                action.objectId = -1;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Delete Selected", "Del", false, snapshot.selectedObjectId >= 0))
            {
                UiAction action;
                action.type = UiActionType::DeleteObject;
                action.objectId = -1;
                controller_.execute(action);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Focus Viewport"))
            {
                UiAction action;
                action.type = UiActionType::FocusViewport;
                controller_.execute(action);
            }

            ImGui::Separator();

            ImGui::Separator();

            if (ImGui::MenuItem("Zoom In"))
            {
                viewportHost_->zoomIn();
            }

            if (ImGui::MenuItem("Zoom Out"))
            {
                viewportHost_->zoomOut();
            }

            if (ImGui::MenuItem("Zoom Extents"))
            {
                viewportHost_->zoomExtents();
            }

            if (ImGui::MenuItem("Reset Viewport"))
            {
                viewportHost_->reset();
            }

            ImGui::Separator();

            ImGui::MenuItem("ImGui Demo", nullptr, &showDemo_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            bool grid = snapshot.gridEnabled;
            bool snap = snapshot.snapEnabled;
            bool ortho = snapshot.orthoEnabled;

            if (ImGui::MenuItem("Grid", nullptr, &grid))
            {
                UiAction action;
                action.type = UiActionType::ToggleGrid;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Snap", nullptr, &snap))
            {
                UiAction action;
                action.type = UiActionType::ToggleSnap;
                controller_.execute(action);
            }

            if (ImGui::MenuItem("Ortho", nullptr, &ortho))
            {
                UiAction action;
                action.type = UiActionType::ToggleOrtho;
                controller_.execute(action);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void UiHost::drawPanels(const AppSnapshot& snapshot)
    {
        viewportHost_->setGridEnabled(snapshot.gridEnabled);
        viewportHost_->setSnapEnabled(snapshot.snapEnabled);
        viewportHost_->setOrthoEnabled(snapshot.orthoEnabled);

        drawViewportPanel(controller_, *viewportHost_, snapshot, router_);
        drawPropertiesPanel(controller_, snapshot);
        drawLayersPanel(controller_, snapshot);
        drawOutlinePanel(controller_, snapshot);
        drawDocumentPanel(controller_, snapshot);
        drawCommandLinePanel(controller_, snapshot, viewportHost_.get());
        drawStatusBar(snapshot);
    }

    void UiHost::drawToolbar(const AppSnapshot& snapshot)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(72.0f, viewport->WorkSize.y));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        drawToolbarPanel(controller_, snapshot);
        ImGui::PopStyleVar();
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
