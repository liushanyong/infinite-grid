#pragma once

#include "../app/AppSnapshot.hpp"
#include "../app/IAppController.hpp"

namespace ui
{
    void drawDocumentPanel(IAppController& controller, const AppSnapshot& snapshot);
}
