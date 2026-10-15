#pragma once

#include "AcGePort.h"

#include <cstdint>

namespace acge
{

enum class PointDisplayStyle : std::uint8_t
{
    Circle,
    Square,
    Rectangle,
    Plus,
    X,
    Star
};

} // namespace acge
