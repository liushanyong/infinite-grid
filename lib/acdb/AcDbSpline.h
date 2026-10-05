#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbSpline
{
    AcDbEntity common;
    int degree = 3;
    bool closed = false;
    std::vector<AcGePoint3d> controlPoints;
    std::vector<AcGePoint3d> fitPoints;
    std::vector<double> knots;
};

} // namespace acdb
