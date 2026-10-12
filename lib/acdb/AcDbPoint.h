#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"
#include "ge/PointDisplayStyle.h"

namespace acdb
{

struct AcDbPoint
{
    AcDbEntity common;
    AcGePoint3d location{0.0, 0.0, 0.0};
    acge::PointDisplayStyle displayStyle{acge::PointDisplayStyle::Circle};
};

} // namespace acdb
