#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"

namespace ui
{
    void drawPropertiesPanel(IAppController& controller, const AppSnapshot& snapshot);
}
