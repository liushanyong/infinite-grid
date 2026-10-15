#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbSpline : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    int degree = 3;
    bool closed = false;
    std::vector<AcGePoint3d> controlPoints;
    std::vector<AcGePoint3d> fitPoints;
    std::vector<double> knots;
    // Rational weights over |controlPoints| (empty = non-rational).  A
    // control-point spline with no fit points drives the GPU curve channel
    // through the Gs replay instead of the stroke tessellation.
    std::vector<double> weights;
};

} // namespace acdb
