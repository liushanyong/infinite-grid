#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"
#include "../platform/InputRouter.hpp"
#include "ViewportHost.hpp"

namespace ui
{
    void drawViewportPanel(
        IAppController& controller,
        ViewportHost& viewportHost,
        const AppSnapshot& snapshot,
        const InputRouter& router
    );
}
