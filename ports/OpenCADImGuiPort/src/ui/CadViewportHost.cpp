#include "CadViewportHost.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    constexpr float kMinZoom = 0.02f;
    constexpr float kMaxZoom = 50.0f;
    constexpr float kGridStep = 32.0f;
}

ImVec2 CadViewportHost::worldToScreen(float worldX, float worldY, const ImVec2& origin, const ImVec2& size) const
{
    const float screenX = origin.x + size.x * 0.5f + panX_ + worldX * zoom_;
    const float screenY = origin.y + size.y * 0.5f + panY_ - worldY * zoom_;
    return ImVec2(screenX, screenY);
}

ImVec2 CadViewportHost::screenToWorld(const ImVec2& screenPos, const ImVec2& origin, const ImVec2& size) const
{
    const float worldX = (screenPos.x - origin.x - size.x * 0.5f - panX_) / zoom_;
    const float worldY = -(screenPos.y - origin.y - size.y * 0.5f - panY_) / zoom_;
    return ImVec2(worldX, worldY);
}

ImVec2 CadViewportHost::snappedScreenPos(const ImVec2& screenPos, const ImVec2& origin, const ImVec2& size) const
{
    if (!snapEnabled_)
    {
        return screenPos;
    }

    const float spacing = kGridStep * zoom_;
    if (spacing <= 0.0f)
    {
        return screenPos;
    }

    const float centerX = origin.x + size.x * 0.5f + panX_;
    const float centerY = origin.y + size.y * 0.5f + panY_;
    const float snappedX = centerX + std::round((screenPos.x - centerX) / spacing) * spacing;
    const float snappedY = centerY + std::round((screenPos.y - centerY) / spacing) * spacing;
    return ImVec2(snappedX, snappedY);
}

ImVec2 CadViewportHost::worldAt(const ImVec2& screenPos) const
{
    if (lastCanvasSize_.x <= 0.0f || lastCanvasSize_.y <= 0.0f)
    {
        return ImVec2(0.0f, 0.0f);
    }

    return screenToWorld(snappedScreenPos(screenPos, lastCanvasOrigin_, lastCanvasSize_), lastCanvasOrigin_, lastCanvasSize_);
}

bool CadViewportHost::layerVisible(const std::string& layerName) const
{
    const auto it = std::find_if(layers_.begin(), layers_.end(), [&](const LayerInfo& layer) {
        return layer.name == layerName;
    });

    return it == layers_.end() || it->visible;
}

ImVec2 CadViewportHost::cursorWorld() const
{
    if (!hasMousePos_ || lastCanvasSize_.x <= 0.0f || lastCanvasSize_.y <= 0.0f)
    {
        return ImVec2(0.0f, 0.0f);
    }

    const ImVec2 snappedScreen = snappedScreenPos(lastMousePos_, lastCanvasOrigin_, lastCanvasSize_);
    return screenToWorld(snappedScreen, lastCanvasOrigin_, lastCanvasSize_);
}

void CadViewportHost::draw(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size)
{
    if (drawList == nullptr || size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    lastCanvasOrigin_ = origin;
    lastCanvasSize_ = size;

    const ImVec2 min = origin;
    const ImVec2 max = ImVec2(origin.x + size.x, origin.y + size.y);
    drawList->AddRectFilled(min, max, IM_COL32(18, 22, 28, 255));
    drawList->PushClipRect(min, max, true);

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

    for (const SceneObject& object : objects_)
    {
        if (!layerVisible(object.layer))
        {
            continue;
        }

        const bool primarySelected = object.id == selectedObjectId_;
        const bool selected = primarySelected
            || std::find(selectedObjectIds_.begin(), selectedObjectIds_.end(), object.id) != selectedObjectIds_.end();
        const ImU32 color = primarySelected
            ? IM_COL32(255, 210, 90, 255)
            : (selected ? IM_COL32(220, 185, 95, 255) : IM_COL32(215, 225, 240, 255));
        const float thickness = primarySelected ? 3.0f : (selected ? 2.2f : 1.5f);

        if (object.type == "Line")
        {
            const ImVec2 start = worldToScreen(object.x1, object.y1, origin, size);
            const ImVec2 end = worldToScreen(object.x2, object.y2, origin, size);
            drawList->AddLine(start, end, color, thickness);
        }
        else if (object.type == "Rectangle")
        {
            const ImVec2 cornerA = worldToScreen(object.x1, object.y1, origin, size);
            const ImVec2 cornerB = worldToScreen(object.x2, object.y2, origin, size);
            drawList->AddRect(cornerA, cornerB, color, 0.0f, 0, thickness);
        }
        else if (object.type == "Circle")
        {
            const ImVec2 center = worldToScreen(object.x1, object.y1, origin, size);
            drawList->AddCircle(center, std::max(1.0f, object.radius * zoom_), color, 0, thickness);
        }
    }

    if (hasPendingDrawPoint_)
    {
        const ImVec2 pending = worldToScreen(pendingDrawX_, pendingDrawY_, origin, size);
        drawList->AddCircleFilled(pending, 4.0f, IM_COL32(90, 210, 255, 230));
        drawList->AddCircle(pending, 7.0f, IM_COL32(90, 210, 255, 140));
    }

    const float axisX = origin.x + size.x * 0.5f + panX_;
    const float axisY = origin.y + size.y * 0.5f + panY_;
    drawList->AddLine(ImVec2(min.x, axisY), ImVec2(max.x, axisY), IM_COL32(90, 120, 170, 140), 1.2f);
    drawList->AddLine(ImVec2(axisX, min.y), ImVec2(axisX, max.y), IM_COL32(90, 170, 120, 140), 1.2f);

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

        drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), IM_COL32(210, 225, 255, 130));
        drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), IM_COL32(210, 225, 255, 130));
        drawList->AddCircleFilled(ImVec2(x, y), 2.0f, IM_COL32(230, 240, 255, 220));
    }

    drawList->PopClipRect();
}

void CadViewportHost::handleInput(const ViewportInput& input)
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

void CadViewportHost::zoomIn()
{
    zoom_ = std::clamp(zoom_ * 1.25f, kMinZoom, kMaxZoom);
}

void CadViewportHost::zoomOut()
{
    zoom_ = std::clamp(zoom_ * 0.8f, kMinZoom, kMaxZoom);
}

void CadViewportHost::zoomExtents()
{
    if (objects_.empty() || lastCanvasSize_.x <= 0.0f || lastCanvasSize_.y <= 0.0f)
    {
        panX_ = 0.0f;
        panY_ = 0.0f;
        zoom_ = 1.0f;
        return;
    }

    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    bool first = true;

    for (const SceneObject& object : objects_)
    {
        float left = object.x1;
        float bottom = object.y1;
        float right = object.x2;
        float top = object.y2;

        if (object.type == "Circle")
        {
            left = object.x1 - object.radius;
            bottom = object.y1 - object.radius;
            right = object.x1 + object.radius;
            top = object.y1 + object.radius;
        }

        if (left > right)
        {
            std::swap(left, right);
        }
        if (bottom > top)
        {
            std::swap(bottom, top);
        }

        if (first)
        {
            minX = left;
            minY = bottom;
            maxX = right;
            maxY = top;
            first = false;
        }
        else
        {
            minX = std::min(minX, left);
            minY = std::min(minY, bottom);
            maxX = std::max(maxX, right);
            maxY = std::max(maxY, top);
        }
    }

    const float width = std::max(1.0f, maxX - minX);
    const float height = std::max(1.0f, maxY - minY);
    const float fitZoomX = lastCanvasSize_.x / width;
    const float fitZoomY = lastCanvasSize_.y / height;
    zoom_ = std::clamp(std::min(fitZoomX, fitZoomY) * 0.85f, kMinZoom, kMaxZoom);

    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    panX_ = -centerX * zoom_;
    panY_ = centerY * zoom_;
}

void CadViewportHost::reset()
{
    zoomExtents();
}

void CadViewportHost::setGridEnabled(bool enabled)
{
    gridEnabled_ = enabled;
}

void CadViewportHost::setSnapEnabled(bool enabled)
{
    snapEnabled_ = enabled;
}

void CadViewportHost::setOrthoEnabled(bool enabled)
{
    orthoEnabled_ = enabled;
}

void CadViewportHost::setScene(const AppSnapshot& snapshot)
{
    objects_ = snapshot.objects;
    layers_ = snapshot.layers;
    selectedObjectId_ = snapshot.selectedObjectId;
    selectedObjectIds_ = snapshot.selectedObjectIds;
    hasPendingDrawPoint_ = snapshot.hasPendingDrawPoint;
    pendingDrawX_ = snapshot.pendingDrawX;
    pendingDrawY_ = snapshot.pendingDrawY;
}

std::vector<int> CadViewportHost::pickInRect(const ImVec2& firstCorner, const ImVec2& secondCorner) const
{
    std::vector<int> pickedIds;
    if (lastCanvasSize_.x <= 0.0f || lastCanvasSize_.y <= 0.0f)
    {
        return pickedIds;
    }

    const float rectMinX = std::min(firstCorner.x, secondCorner.x);
    const float rectMaxX = std::max(firstCorner.x, secondCorner.x);
    const float rectMinY = std::min(firstCorner.y, secondCorner.y);
    const float rectMaxY = std::max(firstCorner.y, secondCorner.y);

    for (auto it = objects_.rbegin(); it != objects_.rend(); ++it)
    {
        const SceneObject& object = *it;
        if (!layerVisible(object.layer))
        {
            continue;
        }

        float objectMinX = 0.0f;
        float objectMaxX = 0.0f;
        float objectMinY = 0.0f;
        float objectMaxY = 0.0f;

        if (object.type == "Circle")
        {
            const ImVec2 center = worldToScreen(object.x1, object.y1, lastCanvasOrigin_, lastCanvasSize_);
            const float radius = std::max(1.0f, object.radius * zoom_);
            objectMinX = center.x - radius;
            objectMaxX = center.x + radius;
            objectMinY = center.y - radius;
            objectMaxY = center.y + radius;
        }
        else
        {
            const ImVec2 first = worldToScreen(object.x1, object.y1, lastCanvasOrigin_, lastCanvasSize_);
            const ImVec2 second = worldToScreen(object.x2, object.y2, lastCanvasOrigin_, lastCanvasSize_);
            objectMinX = std::min(first.x, second.x);
            objectMaxX = std::max(first.x, second.x);
            objectMinY = std::min(first.y, second.y);
            objectMaxY = std::max(first.y, second.y);
        }

        const bool intersects = objectMinX <= rectMaxX
            && objectMaxX >= rectMinX
            && objectMinY <= rectMaxY
            && objectMaxY >= rectMinY;
        if (intersects)
        {
            pickedIds.push_back(object.id);
        }
    }

    return pickedIds;
}

int CadViewportHost::pickAt(const ImVec2& screenPos) const
{
    if (lastCanvasSize_.x <= 0.0f || lastCanvasSize_.y <= 0.0f)
    {
        return -1;
    }

    const ImVec2 world = screenToWorld(screenPos, lastCanvasOrigin_, lastCanvasSize_);
    const float pickRadius = 6.0f / std::max(0.001f, zoom_);

    for (auto it = objects_.rbegin(); it != objects_.rend(); ++it)
    {
        const SceneObject& object = *it;
        if (!layerVisible(object.layer))
        {
            continue;
        }

        if (object.type == "Circle")
        {
            const float dx = world.x - object.x1;
            const float dy = world.y - object.y1;
            if (std::sqrt(dx * dx + dy * dy) <= object.radius + pickRadius)
            {
                return object.id;
            }
        }
        else if (object.type == "Rectangle")
        {
            const float left = std::min(object.x1, object.x2) - pickRadius;
            const float right = std::max(object.x1, object.x2) + pickRadius;
            const float bottom = std::min(object.y1, object.y2) - pickRadius;
            const float top = std::max(object.y1, object.y2) + pickRadius;

            if (world.x >= left && world.x <= right && world.y >= bottom && world.y <= top)
            {
                return object.id;
            }
        }
        else if (object.type == "Line")
        {
            const float ax = object.x2 - object.x1;
            const float ay = object.y2 - object.y1;
            const float bx = world.x - object.x1;
            const float by = world.y - object.y1;
            const float lengthSquared = ax * ax + ay * ay;

            if (lengthSquared <= 0.000001f)
            {
                const float dx = world.x - object.x1;
                const float dy = world.y - object.y1;
                if (std::sqrt(dx * dx + dy * dy) <= pickRadius)
                {
                    return object.id;
                }
            }
            else
            {
                const float t = std::clamp((bx * ax + by * ay) / lengthSquared, 0.0f, 1.0f);
                const float projectedX = object.x1 + t * ax;
                const float projectedY = object.y1 + t * ay;
                const float dx = world.x - projectedX;
                const float dy = world.y - projectedY;
                if (std::sqrt(dx * dx + dy * dy) <= pickRadius)
                {
                    return object.id;
                }
            }
        }
    }

    return -1;
}

std::string CadViewportHost::statusText() const
{
    std::ostringstream stream;
    stream << "Zoom " << zoom_ << "  Objects " << objects_.size()
           << "  Selected " << selectedObjectIds_.size();
    return stream.str();
}
