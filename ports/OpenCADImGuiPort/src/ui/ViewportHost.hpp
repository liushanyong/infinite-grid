#pragma once

#include "../app/AppSnapshot.hpp"

#include <imgui.h>
#include <string>
#include <vector>

struct ViewportInput
{
    bool hovered{false};
    bool clicked{false};
    bool dragging{false};
    float wheelDelta{0.0f};
    ImVec2 mousePos{};
    ImVec2 dragDelta{};
};

class ViewportHost
{
public:
    virtual ~ViewportHost() = default;

    virtual void draw(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size) = 0;
    virtual void handleInput(const ViewportInput& input) = 0;

    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;
    virtual void zoomExtents() = 0;
    virtual void reset() = 0;

    virtual void setGridEnabled(bool enabled) = 0;
    virtual void setSnapEnabled(bool enabled) = 0;
    virtual void setOrthoEnabled(bool enabled) = 0;

    // Legacy grid hosts keep these as no-ops. The CAD viewport host uses them
    // to render document geometry and perform screen-space picking.
    virtual void setScene(const AppSnapshot& snapshot) { (void)snapshot; }
    virtual int pickAt(const ImVec2& screenPos) const { (void)screenPos; return -1; }
    virtual std::vector<int> pickInRect(const ImVec2& min, const ImVec2& max) const
    {
        (void)min;
        (void)max;
        return {};
    }
    virtual ImVec2 cursorWorld() const { return ImVec2(0.0f, 0.0f); }
    virtual ImVec2 worldAt(const ImVec2& screenPos) const { (void)screenPos; return ImVec2(0.0f, 0.0f); }

    virtual std::string statusText() const = 0;
};
