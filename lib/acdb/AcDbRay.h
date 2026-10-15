#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbRay
{
    AcDbEntity common;
    AcGePoint3d start{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
};

} // namespace acdb
