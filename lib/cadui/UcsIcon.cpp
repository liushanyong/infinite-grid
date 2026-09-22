#include "UcsIcon.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cadui
{
    namespace
    {
        constexpr float kIconMargin = 50.0f;
        constexpr float kIconLength = 38.0f;
        constexpr float kIconTip = 7.0f;
        constexpr float kGripBox = 7.0f;
        constexpr float kGripHit = 9.0f;
        constexpr float kIconPick = 10.0f;
        constexpr float kArmPick = 6.0f;

        struct IconAxis
        {
            glm::vec2 delta{ 0.0f };
            float length{ 0.0f };
            float depth{ 0.0f };
            UcsAxis axis{ UcsAxis::None };
            const char* label{ "" };
            ImU32 color{ IM_COL32_WHITE };
        };

        float pointDistanceSq(const ImVec2& a, const ImVec2& b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return dx * dx + dy * dy;
        }

        float segmentDistanceSq(const ImVec2& point, const ImVec2& a, const ImVec2& b)
        {
            const float vx = b.x - a.x;
            const float vy = b.y - a.y;
            const float lengthSq = vx * vx + vy * vy;
            float t = 0.0f;
            if (lengthSq > 1.0e-6f)
            {
                t = ((point.x - a.x) * vx + (point.y - a.y) * vy) / lengthSq;
                t = std::clamp(t, 0.0f, 1.0f);
            }
            return pointDistanceSq(point, { a.x + t * vx, a.y + t * vy });
        }

        bool pointNearPoint(const ImVec2& point, const ImVec2& target, float radius)
        {
            return pointDistanceSq(point, target) <= radius * radius;
        }

        bool pointNearArm(const ImVec2& point, const ImVec2& origin, const ImVec2& tip)
        {
            return segmentDistanceSq(point, origin, tip) <= kArmPick * kArmPick;
        }

        bool overIcon(const ImVec2& point, const UcsIconHit& hit)
        {
            if (!hit.valid)
            {
                return false;
            }
            if (pointNearPoint(point, hit.origin, kIconPick) ||
                pointNearPoint(point, hit.tips[0], kIconPick) ||
                pointNearPoint(point, hit.tips[1], kIconPick) ||
                pointNearPoint(point, hit.tips[2], kIconPick))
            {
                return true;
            }
            return pointNearArm(point, hit.origin, hit.tips[0]) ||
                   pointNearArm(point, hit.origin, hit.tips[1]) ||
                   pointNearArm(point, hit.origin, hit.tips[2]);
        }

        UcsGripKind gripUnder(const ImVec2& point, const UcsIconHit& hit)
        {
            if (!hit.valid)
            {
                return UcsGripKind::None;
            }
            if (pointNearPoint(point, hit.origin, kGripHit))
            {
                return UcsGripKind::Origin;
            }
            if (pointNearPoint(point, hit.tips[0], kGripHit))
            {
                return UcsGripKind::XAxis;
            }
            if (pointNearPoint(point, hit.tips[1], kGripHit))
            {
                return UcsGripKind::YAxis;
            }
            return UcsGripKind::None;
        }

        UcsIconHit buildHit(const ImVec2& origin, const std::array<IconAxis, 3>& axes)
        {
            UcsIconHit hit;
            hit.origin = origin;
            for (unsigned i = 0; i < 3; ++i)
            {
                hit.tips[i] = { origin.x + axes[i].delta.x, origin.y + axes[i].delta.y };
            }
            hit.valid = true;
            return hit;
        }

        ImU32 lighten(ImU32 color, float amount)
        {
            amount = std::clamp(amount, 0.0f, 1.0f);
            const auto mix = [amount](uint32_t component)
            {
                const float value = static_cast<float>(component) / 255.0f;
                const float mixed = value + (1.0f - value) * amount;
                return static_cast<uint32_t>(std::clamp(mixed, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            const uint32_t r = mix(color & 0xffu);
            const uint32_t g = mix((color >> 8u) & 0xffu);
            const uint32_t b = mix((color >> 16u) & 0xffu);
            const uint32_t a = (color >> 24u) & 0xffu;
            return r | (g << 8u) | (b << 16u) | (a << 24u);
        }

        void drawGrip(ImDrawList* drawList, const ImVec2& center)
        {
            const float half = kGripBox * 0.5f;
            const ImVec2 min{ center.x - half, center.y - half };
            const ImVec2 max{ center.x + half, center.y + half };
            drawList->AddRectFilled(min, max, IM_COL32(51, 217, 242, 255));
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 230));
        }
    } // namespace

    UcsIconResult UcsIconWidget::render(const char* strId, const ImVec2& size, const UcsIconState& state)
    {
        UcsIconResult result;

        ImGui::InvisibleButton(strId, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const ImVec2 rectMin = ImGui::GetItemRectMin();
        const ImVec2 rectMax = ImGui::GetItemRectMax();
        const ImVec2 mouse = ImGui::GetIO().MousePos;

        if (size.x < 10.0f || size.y < 10.0f)
        {
            return result;
        }

        ImVec2 corner{
            rectMin.x + kIconMargin,
            rectMin.y + std::max(size.y - kIconMargin, kIconMargin)
        };

        bool originInside = false;
        if (state.originScreen)
        {
            const ImVec2& point = *state.originScreen;
            originInside = point.x >= rectMin.x && point.x <= rectMax.x &&
                           point.y >= rectMin.y && point.y <= rectMax.y;
        }
        const ImVec2 origin = originInside ? *state.originScreen : corner;
        (void)corner;

        std::array<IconAxis, 3> axes{};
        const std::array<glm::vec3, 3> directions{ state.xAxis, state.yAxis, state.zAxis };
        float longest = 0.0f;
        for (unsigned i = 0; i < 3; ++i)
        {
            const glm::vec3 viewDirection = state.viewRotation * glm::normalize(directions[i]);
            const glm::vec2 screen{ viewDirection.x, -viewDirection.y };
            const float projectedLength = std::max(glm::length(screen), 1.0e-4f);
            longest = std::max(longest, projectedLength);
            axes[i].delta = screen;
            axes[i].length = projectedLength;
            axes[i].depth = viewDirection.z;
            axes[i].axis = static_cast<UcsAxis>(i + 1);
        }
        const float scale = kIconLength / longest;
        for (IconAxis& axis : axes)
        {
            axis.delta *= scale;
            axis.length *= scale;
        }

        const bool lightBackground = state.backgroundLuminance > 0.5f;
        const float yGreen = lightBackground ? 0.60f : 0.85f;
        axes[0].label = "X";
        axes[1].label = "Y";
        axes[2].label = "Z";
        axes[0].color = IM_COL32(230, 56, 56, 255);
        axes[1].color = IM_COL32(56, static_cast<int>(yGreen * 255.0f), 56, 255);
        axes[2].color = IM_COL32(56, 115, 230, 255);

        result.hit = buildHit(origin, axes);
        result.hoveredGrip = gripUnder(mouse, result.hit);
        if (!overIcon(mouse, result.hit))
        {
            result.hoveredGrip = UcsGripKind::None;
        }

        float bestDistance = std::numeric_limits<float>::max();
        UcsAxis bestAxis = UcsAxis::None;
        if (hovered && overIcon(mouse, result.hit))
        {
            for (const IconAxis& axis : axes)
            {
                const ImVec2 tip{ origin.x + axis.delta.x, origin.y + axis.delta.y };
                const float tipDistance = pointDistanceSq(mouse, tip);
                const float armDistance = segmentDistanceSq(mouse, origin, tip);
                const float distance = std::min(tipDistance, armDistance);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestAxis = axis.axis;
                }
            }
        }
        result.hoveredAxis = bestAxis;
        if (clicked && bestAxis != UcsAxis::None)
        {
            result.clickedAxis = bestAxis;
        }
        if (clicked)
        {
            result.clickedGrip = gripUnder(mouse, result.hit);
        }

        const ImVec2 xAxisTip{ origin.x + axes[0].delta.x, origin.y + axes[0].delta.y };
        const ImVec2 yAxisTip{ origin.x + axes[1].delta.x, origin.y + axes[1].delta.y };

        // Farther axes are drawn first. Positive view-space Z points away.
        std::sort(axes.begin(), axes.end(), [](const IconAxis& lhs, const IconAxis& rhs)
        {
            return lhs.depth > rhs.depth;
        });

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(rectMin, rectMax, true);
        const bool highlight = hovered || state.selected;
        for (const IconAxis& axis : axes)
        {
            const bool axisHighlight = highlight && (axis.axis == bestAxis || state.selected);
            const ImU32 color = axisHighlight ? lighten(axis.color, 0.45f) : axis.color;
            const ImVec2 tip{ origin.x + axis.delta.x, origin.y + axis.delta.y };

            if (axis.length > 1.0f)
            {
                drawList->AddLine(origin, tip, color, axisHighlight ? 3.0f : 2.0f);
            }

            if (axis.length > 3.0f)
            {
                const glm::vec2 n = axis.delta / axis.length;
                const glm::vec2 p{ -n.y, n.x };
                const ImVec2 left{
                    tip.x - n.x * kIconTip + p.x * (kIconTip * 0.45f),
                    tip.y - n.y * kIconTip + p.y * (kIconTip * 0.45f)
                };
                const ImVec2 right{
                    tip.x - n.x * kIconTip - p.x * (kIconTip * 0.45f),
                    tip.y - n.y * kIconTip - p.y * (kIconTip * 0.45f)
                };
                drawList->AddTriangleFilled(tip, left, right, color);
            }

            if (axis.length > 4.0f)
            {
                const glm::vec2 n = axis.delta / std::max(axis.length, 1.0f);
                const ImVec2 labelPosition{
                    tip.x + n.x * 8.0f - 3.5f,
                    tip.y + n.y * 8.0f - 5.0f
                };
                drawList->AddText(labelPosition, color, axis.label);
            }
        }

        const ImU32 dotColor = lightBackground ? IM_COL32(38, 38, 38, 242) : IM_COL32(230, 230, 230, 242);
        drawList->AddCircleFilled(origin, 3.5f, dotColor, 12);

        if (state.selected)
        {
            drawGrip(drawList, origin);
            drawGrip(drawList, xAxisTip);
            drawGrip(drawList, yAxisTip);
        }
        drawList->PopClipRect();

        if (result.hoveredAxis != UcsAxis::None)
        {
            const char* name =
                result.hoveredAxis == UcsAxis::X ? "UCS X axis" :
                result.hoveredAxis == UcsAxis::Y ? "UCS Y axis" : "UCS Z axis";
            ImGui::SetTooltip("%s", name);
        }
        else if (result.hoveredGrip == UcsGripKind::Origin)
        {
            ImGui::SetTooltip("UCS origin");
        }
        return result;
    }
} // namespace cadui
