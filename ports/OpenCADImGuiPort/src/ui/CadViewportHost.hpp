#pragma once

#include "ViewportHost.hpp"

#include <string>
#include <vector>

class CadViewportHost final : public ViewportHost
{
public:
    void draw(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size) override;
    void handleInput(const ViewportInput& input) override;

    void zoomIn() override;
    void zoomOut() override;
    void zoomExtents() override;
    void reset() override;

    void setGridEnabled(bool enabled) override;
    void setSnapEnabled(bool enabled) override;
    void setOrthoEnabled(bool enabled) override;

    void setScene(const AppSnapshot& snapshot) override;
    int pickAt(const ImVec2& screenPos) const override;
    std::vector<int> pickInRect(const ImVec2& min, const ImVec2& max) const override;
    ImVec2 cursorWorld() const override;
    ImVec2 worldAt(const ImVec2& screenPos) const override;

    std::string statusText() const override;

private:
    ImVec2 worldToScreen(float worldX, float worldY, const ImVec2& origin, const ImVec2& size) const;
    ImVec2 screenToWorld(const ImVec2& screenPos, const ImVec2& origin, const ImVec2& size) const;
    ImVec2 snappedScreenPos(const ImVec2& screenPos, const ImVec2& origin, const ImVec2& size) const;
    bool layerVisible(const std::string& layerName) const;

    float zoom_{1.0f};
    float panX_{0.0f};
    float panY_{0.0f};
    bool gridEnabled_{true};
    bool snapEnabled_{true};
    bool orthoEnabled_{false};
    ImVec2 lastMousePos_{};
    bool hasMousePos_{false};

    std::vector<SceneObject> objects_;
    std::vector<LayerInfo> layers_;
    int selectedObjectId_{-1};
    std::vector<int> selectedObjectIds_;

    bool hasPendingDrawPoint_{false};
    float pendingDrawX_{0.0f};
    float pendingDrawY_{0.0f};

    ImVec2 lastCanvasOrigin_{};
    ImVec2 lastCanvasSize_{};
};
