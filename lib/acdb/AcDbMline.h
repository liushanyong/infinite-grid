#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbMline
{
    AcDbEntity common;
    std::vector<AcGePoint3d> vertices;
    AcGeVector3d scale{1.0, 1.0, 1.0};
    bool closed = false;
};

} // namespace acdb
