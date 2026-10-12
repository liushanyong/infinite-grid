#include "GripOverlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cadui
{
    namespace
    {
        constexpr float kDropdownOffsetX = 24.0f;
        constexpr float kDropdownOffsetY = 27.0f;
        constexpr float kAdjacentDropdownOffsetX = 12.0f;

        bool isGizmo(GripShape shape)
        {
            return shape == GripShape::GizmoAxis ||
                   shape == GripShape::GizmoPlane;
        }

        ImVec2 toImVec2(const glm::vec2 &point, const ImVec2 &offset)
        {
            return { offset.x + point.x, offset.y + point.y };
        }

        glm::vec2 normalizedOr(const glm::vec2 &value,
                               const glm::vec2 &fallback)
        {
            const float lengthSquared = glm::dot(value, value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f)
            {
                return fallback;
            }
            return value / std::sqrt(lengthSquared);
        }

        std::array<std::optional<glm::vec2>, 3> gizmoScreenAxes(
            const glm::dvec3 &world, const glm::vec2 &center,
            const GripProjector &project)
        {
            std::array<std::optional<glm::vec2>, 3> axes{};
            std::array<std::optional<glm::vec2>, 3> spans{};
            float longest = 0.0f;
            for (std::uint8_t axis = 0; axis < 3; ++axis)
            {
                const auto projected = project(world + gripGizmoAxis(axis));
                if (projected)
                {
                    spans[axis] = *projected - center;
                    longest = std::max(longest, glm::length(*spans[axis]));
                }
            }

            for (std::size_t axis = 0; axis < spans.size(); ++axis)
            {
                if (spans[axis] && longest > 0.0f &&
                    glm::length(*spans[axis]) > 0.15f * longest)
                {
                    axes[axis] = glm::normalize(*spans[axis]);
                }
            }
            return axes;
        }

        std::optional<std::pair<glm::vec2, std::optional<glm::vec2>>>
        placeMarker(const GripPoint &grip, const glm::vec2 &projected,
                    const GripProjector &project)
        {
            if (grip.shape == GripShape::GizmoAxis)
            {
                const auto axes = gizmoScreenAxes(grip.world, projected, project);
                const auto &axis = axes[grip.shapeIndex % axes.size()];
                if (!axis)
                {
                    return std::nullopt;
                }
                return std::pair{
                    projected + *axis * kGripGizmoAxisPixels,
                    std::optional<glm::vec2>{
                        glm::vec2(axis->x, -axis->y) }};
            }
            if (grip.shape == GripShape::GizmoPlane)
            {
                const auto axes = gizmoScreenAxes(grip.world, projected, project);
                const glm::u8vec2 pair = gripGizmoPlaneAxes(grip.shapeIndex);
                if (!axes[pair.x] || !axes[pair.y])
                {
                    return std::nullopt;
                }
                const glm::vec2 a = *axes[pair.x];
                const glm::vec2 b = *axes[pair.y];
                return std::pair{
                    projected + (a + b) * (0.3f * kGripGizmoAxisPixels),
                    std::optional<glm::vec2>{}};
            }

            std::optional<glm::vec2> direction;
            if (grip.direction)
            {
                const auto projectedDirection = project(grip.world + *grip.direction);
                if (projectedDirection)
                {
                    const glm::vec2 delta = *projectedDirection - projected;
                    if (glm::dot(delta, delta) > 1.0e-12f)
                    {
                        direction = glm::vec2(delta.x, -delta.y);
                    }
                }
            }

            glm::vec2 position = projected;
            if (grip.shape == GripShape::Dropdown)
            {
                position += glm::vec2{ kDropdownOffsetX, kDropdownOffsetY };
            }
            else if (grip.shape == GripShape::DropdownAdjacent)
            {
                position.x += kAdjacentDropdownOffsetX;
            }
            return std::pair{ position, direction };
        }

        float pointSegmentDistance(const glm::vec2 &point,
                                   const glm::vec2 &start,
                                   const glm::vec2 &end)
        {
            const glm::vec2 segment = end - start;
            const float lengthSquared = glm::dot(segment, segment);
            const float t = lengthSquared > 1.0e-12f
                                ? std::clamp(glm::dot(point - start, segment) /
                                                 lengthSquared,
                                             0.0f, 1.0f)
                                : 0.0f;
            return glm::length(point - (start + segment * t));
        }

        void drawDashedSegment(ImDrawList *drawList, ImVec2 a, ImVec2 b,
                               ImU32 color)
        {
            const ImVec2 delta{ b.x - a.x, b.y - a.y };
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (!std::isfinite(length) || length <= 0.0f)
            {
                return;
            }
            constexpr float dashLength = 4.0f;
            constexpr float period = 8.0f;
            for (float start = 0.0f; start < length; start += period)
            {
                const float end = std::min(start + dashLength, length);
                const float t0 = start / length;
                const float t1 = end / length;
                drawList->AddLine(
                    { a.x + delta.x * t0, a.y + delta.y * t0 },
                    { a.x + delta.x * t1, a.y + delta.y * t1 }, color, 1.0f);
            }
        }

        void drawControlPolygon(ImDrawList *drawList, const ImVec2 &origin,
                                std::span<const glm::vec2> points,
                                bool closed, ImU32 color)
        {
            if (points.size() < 2)
            {
                return;
            }
            for (std::size_t i = 1; i < points.size(); ++i)
            {
                drawDashedSegment(drawList, toImVec2(points[i - 1], origin),
                                 toImVec2(points[i], origin), color);
            }
            if (closed)
            {
                drawDashedSegment(drawList, toImVec2(points.back(), origin),
                                 toImVec2(points.front(), origin), color);
            }
        }

        void drawMoveGizmo(ImDrawList *drawList, const ImVec2 &origin,
                           std::span<const GripMarker> markers)
        {
            constexpr std::array<ImU32, 3> axisColors{
                IM_COL32(230, 56, 51, 255),
                IM_COL32(69, 179, 74, 255),
                IM_COL32(46, 133, 237, 255)
            };
            constexpr ImU32 activeColor = IM_COL32(255, 214, 0, 255);
            std::array<std::optional<std::pair<ImVec2, ImVec2>>, 3> axes{};

            for (const GripMarker &marker : markers)
            {
                if (marker.shape != GripShape::GizmoAxis || !marker.direction)
                {
                    continue;
                }
                const ImVec2 tip = toImVec2(marker.position, origin);
                const glm::vec2 direction =
                    normalizedOr({ marker.direction->x, -marker.direction->y },
                                 { 0.0f, -1.0f });
                axes[marker.shapeIndex % 3] =
                    std::pair{ tip, ImVec2{ direction.x, direction.y } };
            }

            std::optional<ImVec2> center;
            for (const auto &axis : axes)
            {
                if (axis)
                {
                    center = ImVec2{
                        axis->first.x - axis->second.x * kGripGizmoAxisPixels,
                        axis->first.y - axis->second.y * kGripGizmoAxisPixels
                    };
                    break;
                }
            }
            if (!center)
            {
                return;
            }

            for (const GripMarker &marker : markers)
            {
                if (marker.shape != GripShape::GizmoPlane)
                {
                    continue;
                }
                const glm::u8vec2 pair = gripGizmoPlaneAxes(marker.shapeIndex);
                if (!axes[pair.x] || !axes[pair.y])
                {
                    continue;
                }
                const ImVec2 a = axes[pair.x]->second;
                const ImVec2 b = axes[pair.y]->second;
                const auto corner = [&](float u, float v)
                {
                    return ImVec2{
                        center->x + (a.x * u + b.x * v) * kGripGizmoAxisPixels,
                        center->y + (a.y * u + b.y * v) * kGripGizmoAxisPixels
                    };
                };
                const std::array<ImVec2, 4> quad{
                    corner(0.18f, 0.18f), corner(0.42f, 0.18f),
                    corner(0.42f, 0.42f), corner(0.18f, 0.42f)
                };
                const bool active = marker.hot || marker.hovered;
                const ImU32 color = active ? activeColor : IM_COL32(217, 217, 217, 255);
                const ImU32 fill = active ? color : IM_COL32(217, 217, 217, 64);
                drawList->AddConvexPolyFilled(quad.data(),
                                              static_cast<int>(quad.size()), fill);
                drawList->AddPolyline(quad.data(), static_cast<int>(quad.size()),
                                      color, ImDrawFlags_Closed, 1.0f);
            }

            for (const GripMarker &marker : markers)
            {
                if (marker.shape != GripShape::GizmoAxis || !marker.direction)
                {
                    continue;
                }
                const std::size_t axis = marker.shapeIndex % 3;
                const ImVec2 tip = toImVec2(marker.position, origin);
                const ImVec2 direction = axes[axis]->second;
                const ImVec2 base{
                    tip.x - direction.x * 13.0f,
                    tip.y - direction.y * 13.0f
                };
                const ImU32 color =
                    marker.hot || marker.hovered ? activeColor : axisColors[axis];
                drawList->AddLine(*center, base, color, 2.5f);
                const std::array<ImVec2, 3> head{
                    tip,
                    ImVec2{ base.x - direction.y * 6.0f,
                            base.y + direction.x * 6.0f },
                    ImVec2{ base.x + direction.y * 6.0f,
                            base.y - direction.x * 6.0f }
                };
                drawList->AddConvexPolyFilled(head.data(),
                                              static_cast<int>(head.size()), color);
            }
        }

        void drawGripMarker(ImDrawList *drawList, const ImVec2 &origin,
                            const GripMarker &marker,
                            const GripOverlayStyle &style)
        {
            if (isGizmo(marker.shape))
            {
                return;
            }

            const float half = std::isfinite(style.size)
                                   ? std::clamp(style.size, 1.0f, 25.0f)
                                   : 5.0f;
            const float outlineWidth = std::isfinite(style.outlineWidth)
                                           ? std::max(style.outlineWidth, 0.0f)
                                           : 0.0f;
            const ImVec2 center = toImVec2(marker.position, origin);
            const bool highlighted = marker.hot || marker.hovered;
            const ImU32 fill = marker.hot
                                   ? style.hotColor
                                   : marker.hovered ? style.hoverColor
                                                    : (marker.shape == GripShape::Dropdown ||
                                                               marker.shape == GripShape::DropdownAdjacent
                                                           ? style.outlineColor
                                                           : style.fillColor);
            const ImU32 outline = marker.hovered && !marker.hot
                                      ? style.hoverOutlineColor
                                      : marker.hot ? style.hotColor
                                                   : style.outlineColor;

            std::array<ImVec2, 4> vertices{};
            int vertexCount = 0;
            switch (marker.shape)
            {
            case GripShape::Square:
                vertices = {{
                    { center.x - half, center.y - half },
                    { center.x + half, center.y - half },
                    { center.x + half, center.y + half },
                    { center.x - half, center.y + half }
                }};
                vertexCount = 4;
                break;
            case GripShape::Rectangle:
            {
                const float longHalf = half * 1.4f;
                const float shortHalf = half * 0.7f;
                const glm::vec2 direction = marker.direction
                                                ? normalizedOr(
                                                      { marker.direction->x,
                                                        -marker.direction->y },
                                                      { 1.0f, 0.0f })
                                                : glm::vec2{ 1.0f, 0.0f };
                const glm::vec2 side{ -direction.y, direction.x };
                const glm::vec2 a = direction * longHalf;
                const glm::vec2 b = side * shortHalf;
                vertices = {{
                    { center.x + a.x + b.x, center.y + a.y + b.y },
                    { center.x + a.x - b.x, center.y + a.y - b.y },
                    { center.x - a.x - b.x, center.y - a.y - b.y },
                    { center.x - a.x + b.x, center.y - a.y + b.y }
                }};
                vertexCount = 4;
                break;
            }
            case GripShape::Triangle:
            {
                const glm::vec2 direction = marker.direction
                                                ? normalizedOr(
                                                      { marker.direction->x,
                                                        -marker.direction->y },
                                                      { 0.0f, -1.0f })
                                                : glm::vec2{ 0.0f, -1.0f };
                const glm::vec2 side{ -direction.y, direction.x };
                const glm::vec2 tip{ center.x + direction.x * half,
                                     center.y + direction.y * half };
                const glm::vec2 base{ center.x - direction.x * half,
                                      center.y - direction.y * half };
                vertices[0] = { tip.x, tip.y };
                vertices[1] = { base.x + side.x * half,
                                base.y + side.y * half };
                vertices[2] = { base.x - side.x * half,
                                base.y - side.y * half };
                vertexCount = 3;
                break;
            }
            case GripShape::Circle:
                drawList->AddCircleFilled(center, half, fill, 20);
                drawList->AddCircle(center, half, outline, 0, outlineWidth);
                return;
            case GripShape::Dropdown:
            case GripShape::DropdownAdjacent:
                vertices = {{
                    { center.x - half, center.y - half * 0.5f },
                    { center.x + half, center.y - half * 0.5f },
                    { center.x, center.y + half }
                }};
                vertexCount = 3;
                break;
            case GripShape::GizmoAxis:
            case GripShape::GizmoPlane:
                return;
            }

            drawList->AddConvexPolyFilled(vertices.data(), vertexCount, fill);
            drawList->AddPolyline(vertices.data(), vertexCount, outline,
                                  ImDrawFlags_Closed,
                                  highlighted ? std::max(outlineWidth, 1.5f)
                                              : outlineWidth);
        }
    } // namespace

    glm::dvec3 gripGizmoAxis(std::uint8_t axis)
    {
        constexpr std::array<glm::dvec3, 3> axes{
            glm::dvec3{ 1.0, 0.0, 0.0 },
            glm::dvec3{ 0.0, 1.0, 0.0 },
            glm::dvec3{ 0.0, 0.0, 1.0 }
        };
        return axes[axis % axes.size()];
    }

    glm::u8vec2 gripGizmoPlaneAxes(std::uint8_t plane)
    {
        constexpr std::array<glm::u8vec2, 3> axes{
            glm::u8vec2{ 0, 1 }, glm::u8vec2{ 1, 2 }, glm::u8vec2{ 2, 0 }
        };
        return axes[plane % axes.size()];
    }

    std::vector<GripMarker> projectGripMarkers(
        std::span<const GripPoint> grips, const GripProjector &project)
    {
        std::vector<GripMarker> markers;
        markers.reserve(grips.size());
        for (std::size_t index = 0; index < grips.size(); ++index)
        {
            const GripPoint &grip = grips[index];
            const auto projected = project(grip.world);
            if (!projected || !std::isfinite(projected->x) ||
                !std::isfinite(projected->y))
            {
                continue;
            }
            const auto placed = placeMarker(grip, *projected, project);
            if (!placed || !std::isfinite(placed->first.x) ||
                !std::isfinite(placed->first.y))
            {
                continue;
            }
            markers.push_back({
                index,
                grip.id,
                grip.world,
                placed->first,
                grip.shape,
                grip.shapeIndex,
                grip.midpoint,
                false,
                false,
                placed->second
            });
        }
        return markers;
    }

    std::optional<std::size_t> hitTestGripMarkers(
        glm::vec2 cursor, std::span<const GripMarker> markers, float hitRadius)
    {
        if (!std::isfinite(cursor.x) || !std::isfinite(cursor.y) ||
            !std::isfinite(hitRadius) || hitRadius <= 0.0f)
        {
            return std::nullopt;
        }
        float bestDistance = hitRadius;
        std::optional<std::size_t> best;
        for (const GripMarker &marker : markers)
        {
            if (!std::isfinite(marker.position.x) ||
                !std::isfinite(marker.position.y))
            {
                continue;
            }
            float distance = glm::length(cursor - marker.position);
            if (marker.shape == GripShape::GizmoAxis && marker.direction)
            {
                const glm::vec2 direction =
                    normalizedOr({ marker.direction->x, -marker.direction->y },
                                 { 0.0f, -1.0f });
                const glm::vec2 start =
                    marker.position - direction * (kGripGizmoAxisPixels - 14.0f);
                distance = pointSegmentDistance(cursor, start, marker.position);
            }
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = marker.index;
            }
        }
        return best;
    }

    void drawGripOverlay(
        ImDrawList *drawList, const ImVec2 &viewportMin,
        const ImVec2 &viewportSize, std::span<const GripMarker> markers,
        const GripOverlayStyle &style, std::span<const glm::vec2> controlPolygon,
        bool polygonClosed)
    {
        if (drawList == nullptr || viewportSize.x <= 0.0f ||
            viewportSize.y <= 0.0f || !std::isfinite(viewportMin.x) ||
            !std::isfinite(viewportMin.y) || !std::isfinite(viewportSize.x) ||
            !std::isfinite(viewportSize.y))
        {
            return;
        }
        const ImVec2 viewportMax{ viewportMin.x + viewportSize.x,
                                  viewportMin.y + viewportSize.y };
        drawList->PushClipRect(viewportMin, viewportMax, true);
        drawControlPolygon(drawList, viewportMin, controlPolygon,
                           polygonClosed, style.controlPolygonColor);
        drawMoveGizmo(drawList, viewportMin, markers);
        for (const GripMarker &marker : markers)
        {
            drawGripMarker(drawList, viewportMin, marker, style);
        }
        drawList->PopClipRect();
    }
} // namespace cadui
