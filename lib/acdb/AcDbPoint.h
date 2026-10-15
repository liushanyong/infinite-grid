#pragma once

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"
#include "acge/AcGePointDisplayStyle.h"

namespace acdb
{

struct AcDbPoint
{
    AcDbEntity common;
    AcGePoint3d location{0.0, 0.0, 0.0};
    acge::PointDisplayStyle displayStyle{acge::PointDisplayStyle::Circle};
};

} // namespace acdb
