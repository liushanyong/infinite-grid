#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"
#include "acge/AcGePointDisplayStyle.h"

namespace acdb
{

struct ACDB_PORT AcDbPoint
{
    AcDbEntity common;
    AcGePoint3d location{0.0, 0.0, 0.0};
    acge::PointDisplayStyle displayStyle{acge::PointDisplayStyle::Circle};
};

} // namespace acdb
