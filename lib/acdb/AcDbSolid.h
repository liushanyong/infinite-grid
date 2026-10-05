#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbSolid
{
    AcDbEntity common;
    AcGePoint3d firstCorner{0.0, 0.0, 0.0};
    AcGePoint3d secondCorner{0.0, 0.0, 0.0};
    AcGePoint3d thirdCorner{0.0, 0.0, 0.0};
    AcGePoint3d fourthCorner{0.0, 0.0, 0.0};
};

} // namespace acdb
