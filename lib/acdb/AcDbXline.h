#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

// The construction line extends without bound from the point in both
// directions.
struct ACDB_PORT AcDbXline
{
    AcDbEntity common;
    AcGePoint3d point{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
};

} // namespace acdb
