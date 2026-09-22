#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

#include <optional>

namespace cadui
{
    enum class UcsAxis : unsigned char
    {
        None,
        X,
        Y,
        Z
    };

    enum class UcsGripKind : unsigned char
    {
        None,
        Origin,
        XAxis,
        YAxis
    };

    struct UcsIconState
    {
        // World-to-eye rotation (the upper 3x3 part of the camera view matrix).
        glm::mat3 viewRotation{ 1.0f };

        // Active UCS axis directions in world space.
        glm::vec3 xAxis{ 1.0f, 0.0f, 0.0f };
        glm::vec3 yAxis{ 0.0f, 1.0f, 0.0f };
        glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };

        // Draws CAD-style grips and highlights the tripod when true.
        bool selected{ false };

        // Dark backgrounds get a brighter Y green and a light origin dot.
        float backgroundLuminance{ 0.12f };

        // Optional projected UCS origin (UCSICON ORIGIN mode).
        std::optional<ImVec2> originScreen;
    };

    struct UcsIconHit
    {
        ImVec2 origin{ 0.0f, 0.0f };
        ImVec2 tips[3]{};
        bool valid{ false };
    };

    struct UcsIconResult
    {
        UcsAxis hoveredAxis{ UcsAxis::None };
        UcsAxis clickedAxis{ UcsAxis::None };
        UcsGripKind hoveredGrip{ UcsGripKind::None };
        UcsGripKind clickedGrip{ UcsGripKind::None };
        UcsIconHit hit{};
    };

    // An ImGui DrawList implementation of the OpenCADStudio UCS tripod.
    class UcsIconWidget
    {
    public:
        UcsIconResult render(const char* strId, const ImVec2& size, const UcsIconState& state);
    };
} // namespace cadui
