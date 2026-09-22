#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"

#include <functional>

namespace ui
{
    void drawToolbarPanel(IAppController& controller, const AppSnapshot& snapshot);
}
