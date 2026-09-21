#pragma once

#include "AppSnapshot.hpp"
#include "UiAction.hpp"

class IAppController
{
public:
    virtual ~IAppController() = default;

    virtual void execute(const UiAction& action) = 0;
    virtual AppSnapshot snapshot() const = 0;
    virtual bool closeRequested() const = 0;
};
