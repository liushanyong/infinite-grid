#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace cadui
{
    enum class GripShape : unsigned char
    {
        Square,
        Rectangle,
        Triangle,
        Circle,
        Dropdown,
        DropdownAdjacent,
        GizmoAxis,
        GizmoPlane
    };

    struct GripPoint
    {
        std::size_t id{ 0 };
        glm::dvec3 world{ 0.0 };
        GripShape shape{ GripShape::Square };
        std::uint8_t shapeIndex{ 0 };
        bool midpoint{ false };
        std::optional<glm::dvec3> direction;
    };

    struct GripMarker
    {
        std::size_t index{ 0 };
        std::size_t id{ 0 };
        glm::dvec3 world{ 0.0 };
        glm::vec2 position{ 0.0f };
        GripShape shape{ GripShape::Square };
        std::uint8_t shapeIndex{ 0 };
        bool midpoint{ false };
        bool hot{ false };
        bool hovered{ false };
        // Screen-plane direction with positive Y pointing up.
        std::optional<glm::vec2> direction;
    };

    using GripProjector =
        std::function<std::optional<glm::vec2>(const glm::dvec3 &)>;

    struct GripOverlayStyle
    {
        float size{ 5.0f };
        float outlineWidth{ 1.5f };
        ImU32 fillColor{ IM_COL32(40, 100, 220, 180) };
        ImU32 outlineColor{ IM_COL32(90, 155, 255, 255) };
        ImU32 hotColor{ IM_COL32(235, 55, 55, 255) };
        ImU32 hoverColor{ IM_COL32(255, 205, 35, 255) };
        ImU32 hoverOutlineColor{ IM_COL32(255, 255, 255, 255) };
        ImU32 controlPolygonColor{ IM_COL32(184, 82, 38, 255) };
    };

    constexpr float kGripHitRadius = 8.0f;
    constexpr float kGripGizmoAxisPixels = 72.0f;

    glm::dvec3 gripGizmoAxis(std::uint8_t axis);
    glm::u8vec2 gripGizmoPlaneAxes(std::uint8_t plane);

    // Convert world-space grip definitions into viewport-relative screen
    // markers. The projector should subtract the camera/rebase eye in double
    // precision before converting to float.
    std::vector<GripMarker> projectGripMarkers(
        std::span<const GripPoint> grips, const GripProjector &project);

    // Returns the closest marker's source index when its marker or gizmo shaft
    // is within the pixel hit radius.
    std::optional<std::size_t> hitTestGripMarkers(
        glm::vec2 cursor, std::span<const GripMarker> markers,
        float hitRadius = kGripHitRadius);

    // Draw a clipped, screen-space grip overlay. Positions and optional
    // control-polygon vertices are relative to viewportMin.
    void drawGripOverlay(
        ImDrawList *drawList, const ImVec2 &viewportMin,
        const ImVec2 &viewportSize, std::span<const GripMarker> markers,
        const GripOverlayStyle &style = {},
        std::span<const glm::vec2> controlPolygon = {},
        bool polygonClosed = false);

    void drawProjectedGripOverlay(
        ImDrawList *drawList, const ImVec2 &viewportMin,
        const ImVec2 &viewportSize, std::span<const GripPoint> grips,
        const GripProjector &project, const GripOverlayStyle &style = {},
        std::span<const glm::vec2> controlPolygon = {},
        bool polygonClosed = false);
} // namespace cadui
