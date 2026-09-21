#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"

namespace ui
{
    void drawToolbarPanel(IAppController& controller, const AppSnapshot& snapshot);
}
