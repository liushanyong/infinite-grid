#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbCircle
{
    AcDbEntity common;
    AcGePoint3d center{0.0, 0.0, 0.0};
    double radius = 0.0;
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace acdb
