#pragma once

#include <imgui.h>

#include <cstdint>
#include <functional>

namespace cadui
{
    struct ViewportPanelFrame
    {
        ImVec2 origin{ 0.0f, 0.0f };
        ImVec2 size{ 0.0f, 0.0f };
        bool hovered{ false };
    };

    using ViewportPanelOverlay =
        std::function<void(ImDrawList *, const ImVec2 &, const ImVec2 &)>;

    // Draw a viewport image panel and an optional viewport-relative overlay.
    // The returned screen-space bounds and hover state can drive app input.
    ViewportPanelFrame drawViewportPanel(
        const char *title, const ImVec2 &position, const ImVec2 &size,
        std::uint32_t textureId,
        const ViewportPanelOverlay &drawOverlay = {});
} // namespace cadui
