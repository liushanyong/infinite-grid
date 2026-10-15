#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbLine : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    AcGePoint3d start{0.0, 0.0, 0.0};
    AcGePoint3d end{0.0, 0.0, 0.0};
    // DWG LINE carries an OCS extrusion and a wall thickness.
    double thickness = 0.0;
    AcGeVector3d extrusion{0.0, 0.0, 1.0};
};

} // namespace acdb
