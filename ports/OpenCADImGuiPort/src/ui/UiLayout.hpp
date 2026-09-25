#pragma once

namespace ui
{
    // Shared metrics for the fixed, CAD-style application shell.
    inline constexpr float kRibbonHeight = 164.0f;
    inline constexpr float kDocumentTabHeight = 38.0f;
    inline constexpr float kContentTop = kRibbonHeight + kDocumentTabHeight;
    inline constexpr float kLeftPanelWidth = 312.0f;
    inline constexpr float kStatusBarHeight = 40.0f;
}

namespace ui
{
    // Reset requests live outside UiHost so shell-level toolbar buttons can
    // rebuild the same deterministic DockBuilder tree.
    inline bool& dockLayoutResetRequest()
    {
        static bool requested = false;
        return requested;
    }
}
