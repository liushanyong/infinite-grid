#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

// The construction line extends without bound from the point in both
// directions.
struct AcDbXline
{
    AcDbEntity common;
    AcGePoint3d point{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
};

} // namespace acdb
