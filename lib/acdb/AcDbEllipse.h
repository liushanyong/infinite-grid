#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbEllipse
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
