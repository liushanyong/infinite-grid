#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbMline
{
    AcDbEntity common;
    std::vector<AcGePoint3d> vertices;
    AcGeVector3d scale{1.0, 1.0, 1.0};
    bool closed = false;
};

} // namespace acdb
