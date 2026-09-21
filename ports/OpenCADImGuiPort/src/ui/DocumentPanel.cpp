#include "DocumentPanel.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui
{
    void drawDocumentPanel(IAppController& controller, const AppSnapshot& snapshot)
    {
        if (!ImGui::Begin("Document"))
        {
            ImGui::End();
            return;
        }

        static char filePath[512] = "untitled.ocad";
        ImGui::InputTextWithHint("File", "Drawing file...", filePath, sizeof(filePath));

        if (ImGui::Button("Save"))
        {
            UiAction action;
            action.type = UiActionType::Save;
            action.payload = filePath;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Open"))
        {
            UiAction action;
            action.type = UiActionType::OpenFile;
            action.payload = filePath;
            controller.execute(action);
        }

        ImGui::SameLine();

        if (ImGui::Button("Use Current"))
        {
            const size_t copyLength = std::min(snapshot.currentFile.size(), sizeof(filePath) - 1);
            std::copy(snapshot.currentFile.begin(), snapshot.currentFile.begin() + copyLength, filePath);
            filePath[copyLength] = '\0';
        }

        ImGui::Separator();
        ImGui::Text("Current file: %s", snapshot.currentFile.empty() ? "None" : snapshot.currentFile.c_str());
        ImGui::Text("Objects: %d", static_cast<int>(snapshot.objects.size()));
        ImGui::Text("Layers: %d", static_cast<int>(snapshot.layers.size()));

        ImGui::End();
    }
}
