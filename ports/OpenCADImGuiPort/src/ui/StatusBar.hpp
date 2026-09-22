#pragma once

#include "../app/AppSnapshot.hpp"

#include <imgui.h>
#include <string>
#include "../app/IAppController.hpp"

namespace ui
{
    void drawStatusBar(
        IAppController& controller,
        const AppSnapshot& snapshot,
        const ImVec2& cursorWorld,
        const std::string& viewportStatus
    );
}
