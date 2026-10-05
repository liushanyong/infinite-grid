#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbRay
{
    AcDbEntity common;
    AcGePoint3d start{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
};

} // namespace acdb
