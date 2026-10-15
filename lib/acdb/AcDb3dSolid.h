#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"
#include "acdb/AcDbRenderClass.h"

namespace acdb
{

struct ACDB_PORT AcDb3dSolid
{
    AcDbEntity common;
    std::vector<AcGePoint3d> vertices;
    std::vector<std::uint32_t> indices;
    RenderClass renderClass = RenderClass::Cad;
};

} // namespace acdb
