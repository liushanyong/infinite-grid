#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace cadui
{
    class ViewCubeBgfxRenderer;

    enum class RegionKind : unsigned char
    {
        Face,
        Edge,
        Corner
    };

    struct ViewCubeRegion
    {
        RegionKind kind{ RegionKind::Face };
        unsigned char index{ 0 };
        unsigned char id{ 0 };
        const char* label{ "" };
    };

    enum class ViewCubeActionKind : unsigned char
    {
        None,
        Region,
        Cardinal,
        Home,
        RollLeft,
        RollRight,
        NudgeUp,
        NudgeDown,
        NudgeLeft,
        NudgeRight,
        UcsChanged
    };

    struct ViewCubeAction
    {
        ViewCubeActionKind kind{ ViewCubeActionKind::None };
        ViewCubeRegion region{};
        int ucsIndex{ -1 };
        std::string ucs;
    };

    struct ViewCubeResult
    {
        bool clicked{ false };
        ViewCubeRegion clickedRegion{};
        bool hasHover{ false };
        ViewCubeRegion hoveredRegion{};
        ViewCubeAction action{};
    };

    struct ViewCubeOptions
    {
        // Localized face captions in region order: top/bottom/front/back/right/left.
        std::array<const char*, 6> faceLabels{ "TOP", "BOTTOM", "FRONT", "BACK", "RIGHT", "LEFT" };
        bool showControls{ true };
        bool showUcsPicker{ true };
        const char* activeUcs{ "WCS" };
        const std::vector<std::string>* ucsNames{ nullptr };
    };

    // Interactive navigation cube widget. The offscreen bgfx renderer is
    // optional; when it is unavailable the same geometry is drawn by ImGui.
    class ViewCubeWidget
    {
    public:
        void setBgfxRenderer(ViewCubeBgfxRenderer* renderer);
        ViewCubeResult render(const char* strId,
                              const ImVec2& size,
                              const glm::mat3& viewRotation,
                              const glm::mat3& ucsRotation = glm::mat3{ 1.0f },
                              const ViewCubeOptions& options = {});
        static ViewCubeRegion regionById(int id);
        static glm::vec3 snapDirection(const ViewCubeRegion& region);
        static ViewCubeRegion cardinalRegion(int cardinalIndex);
        static glm::vec3 cardinalDirection(int cardinalIndex);

    private:
        ViewCubeBgfxRenderer* m_renderer{ nullptr };
    };
} // namespace cadui
