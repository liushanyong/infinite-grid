#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"
#include "ViewportHost.hpp"

namespace ui
{
    void drawCommandLinePanel(IAppController& controller, const AppSnapshot& snapshot, ViewportHost* viewportHost);
}
