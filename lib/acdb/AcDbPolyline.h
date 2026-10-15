#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "arx/ArxTypes.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbPolyline : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    AcArray<AcGePoint2d> vertices;
    AcArray<double> bulges;
    double elevation = 0.0;
    double thickness = 0.0;
    bool closed = false;
    AcGeVector3d normal{0.0, 0.0, 1.0};
};

} // namespace acdb
