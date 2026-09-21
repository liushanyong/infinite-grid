#pragma once

#include "ViewportHost.hpp"

class GridViewportHost final : public ViewportHost
{
public:
    GridViewportHost();

    void draw(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size) override;
    void handleInput(const ViewportInput& input) override;

    void zoomIn() override;
    void zoomOut() override;
    void zoomExtents() override;
    void reset() override;

    void setGridEnabled(bool enabled) override;
    void setSnapEnabled(bool enabled) override;
    void setOrthoEnabled(bool enabled) override;

    std::string statusText() const override;

private:
    float zoom_{1.0f};
    float panX_{0.0f};
    float panY_{0.0f};
    bool gridEnabled_{true};
    bool snapEnabled_{true};
    bool orthoEnabled_{false};
    ImVec2 lastMousePos_{};
    bool hasMousePos_{false};
};
