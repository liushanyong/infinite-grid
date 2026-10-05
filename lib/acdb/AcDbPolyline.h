#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbPolyline
{
    AcDbEntity common;
    std::vector<AcGePoint2d> vertices;
    std::vector<double> bulges;
    double elevation = 0.0;
    double thickness = 0.0;
    bool closed = false;
    AcGeVector3d normal{0.0, 0.0, 1.0};
};

} // namespace acdb
