#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDb3dPolyline : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    std::vector<AcGePoint3d> vertices;
    std::vector<double> bulges;
    bool closed = false;
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace acdb
