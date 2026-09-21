#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"

namespace ui
{
    void drawLayersPanel(IAppController& controller, const AppSnapshot& snapshot);
}
