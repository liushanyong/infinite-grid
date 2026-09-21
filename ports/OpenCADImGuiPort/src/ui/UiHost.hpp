#pragma once

#include "../app/IAppController.hpp"
#include "../platform/InputRouter.hpp"
#include "ViewportHost.hpp"

#include <memory>

namespace ui
{
    class UiHost
    {
    public:
        UiHost(IAppController& controller, InputRouter& router);

        void draw(float frameMs);

    private:
        void drawDockspace();
        void drawMainMenuBar(const AppSnapshot& snapshot);
        void drawPanels(const AppSnapshot& snapshot);
        void drawToolbar(const AppSnapshot& snapshot);
        void drawGlobalShortcuts(const AppSnapshot& snapshot);

        IAppController& controller_;
        InputRouter& router_;
        std::unique_ptr<ViewportHost> viewportHost_;
        bool showDemo_{false};
    };
}
