#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbPoint
{
    AcDbEntity common;
    AcGePoint3d location{0.0, 0.0, 0.0};
};

} // namespace acdb
