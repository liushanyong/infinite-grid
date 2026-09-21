#include "GridViewportHost.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    constexpr float kMinZoom = 0.25f;
    constexpr float kMaxZoom = 10.0f;
    constexpr float kGridStep = 32.0f;
}

GridViewportHost::GridViewportHost() = default;

void GridViewportHost::draw(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size)
{
    if (drawList == nullptr || size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    const ImVec2 min = origin;
    const ImVec2 max = ImVec2(origin.x + size.x, origin.y + size.y);

    drawList->AddRectFilled(min, max, IM_COL32(18, 22, 28, 255));

    if (gridEnabled_)
    {
        const float spacing = kGridStep * zoom_;
        if (spacing >= 8.0f)
        {
            const ImU32 gridColor = IM_COL32(60, 70, 85, 255);

            const float centerX = origin.x + size.x * 0.5f + panX_;
            const float centerY = origin.y + size.y * 0.5f + panY_;

            const int halfCols = static_cast<int>(std::ceil(size.x * 0.5f / spacing)) + 1;
            const int halfRows = static_cast<int>(std::ceil(size.y * 0.5f / spacing)) + 1;

            for (int i = -halfCols; i <= halfCols; ++i)
            {
                const float x = centerX + static_cast<float>(i) * spacing;
                if (x < min.x || x > max.x)
                {
                    continue;
                }
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), gridColor);
            }

            for (int j = -halfRows; j <= halfRows; ++j)
            {
                const float y = centerY + static_cast<float>(j) * spacing;
                if (y < min.y || y > max.y)
                {
                    continue;
                }
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), gridColor);
            }
        }
    }

    const float axisX = origin.x + size.x * 0.5f + panX_;
    const float axisY = origin.y + size.y * 0.5f + panY_;

    drawList->AddLine(ImVec2(min.x, axisY), ImVec2(max.x, axisY), IM_COL32(90, 120, 170, 255), 1.5f);
    drawList->AddLine(ImVec2(axisX, min.y), ImVec2(axisX, max.y), IM_COL32(90, 170, 120, 255), 1.5f);

    if (hasMousePos_)
    {
        float x = lastMousePos_.x;
        float y = lastMousePos_.y;

        if (snapEnabled_)
        {
            const float spacing = kGridStep * zoom_;
            if (spacing > 0.0f)
            {
                x = origin.x + size.x * 0.5f + panX_
                    + std::round((x - origin.x - size.x * 0.5f - panX_) / spacing) * spacing;
                y = origin.y + size.y * 0.5f + panY_
                    + std::round((y - origin.y - size.y * 0.5f - panY_) / spacing) * spacing;
            }
        }

        drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), IM_COL32(210, 225, 255, 160));
        drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), IM_COL32(210, 225, 255, 160));
        drawList->AddCircleFilled(ImVec2(x, y), 2.0f, IM_COL32(230, 240, 255, 220));
    }
}

void GridViewportHost::handleInput(const ViewportInput& input)
{
    if (!input.hovered)
    {
        hasMousePos_ = false;
        return;
    }

    lastMousePos_ = input.mousePos;
    hasMousePos_ = true;

    if (input.wheelDelta != 0.0f)
    {
        const float factor = input.wheelDelta > 0.0f ? 1.25f : 0.8f;
        zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
    }

    if (input.dragging && (input.dragDelta.x != 0.0f || input.dragDelta.y != 0.0f))
    {
        float dx = input.dragDelta.x;
        float dy = input.dragDelta.y;

        if (orthoEnabled_)
        {
            if (std::abs(dx) >= std::abs(dy))
            {
                dy = 0.0f;
            }
            else
            {
                dx = 0.0f;
            }
        }

        panX_ += dx;
        panY_ += dy;
    }
}

void GridViewportHost::zoomIn()
{
    zoom_ = std::clamp(zoom_ * 1.25f, kMinZoom, kMaxZoom);
}

void GridViewportHost::zoomOut()
{
    zoom_ = std::clamp(zoom_ * 0.8f, kMinZoom, kMaxZoom);
}

void GridViewportHost::zoomExtents()
{
    panX_ = 0.0f;
    panY_ = 0.0f;
    zoom_ = 1.0f;
}

void GridViewportHost::reset()
{
    zoomExtents();
}

void GridViewportHost::setGridEnabled(bool enabled)
{
    gridEnabled_ = enabled;
}

void GridViewportHost::setSnapEnabled(bool enabled)
{
    snapEnabled_ = enabled;
}

void GridViewportHost::setOrthoEnabled(bool enabled)
{
    orthoEnabled_ = enabled;
}

std::string GridViewportHost::statusText() const
{
    std::ostringstream stream;
    stream << "Zoom " << zoom_ << "  Pan (" << panX_ << ", " << panY_ << ")";
    return stream.str();
}
