#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbEllipse
{
    AcDbEntity common;
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d majorAxis{0.0, 0.0, 0.0};
    double radiusRatio = 1.0;
    double startParameter = 0.0;
    double endParameter = 6.283185307179586;
    AcGeVector3d normal{0.0, 0.0, 1.0};
};

} // namespace acdb
