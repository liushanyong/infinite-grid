#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbSolid : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    AcGePoint3d firstCorner{0.0, 0.0, 0.0};
    AcGePoint3d secondCorner{0.0, 0.0, 0.0};
    AcGePoint3d thirdCorner{0.0, 0.0, 0.0};
    AcGePoint3d fourthCorner{0.0, 0.0, 0.0};
};

} // namespace acdb
