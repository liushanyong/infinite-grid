#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"
#include "acdb/AcDbRenderClass.h"

namespace acdb
{

struct AcDb3dSolid
{
    AcDbEntity common;
    std::vector<AcGePoint3d> vertices;
    std::vector<std::uint32_t> indices;
    RenderClass renderClass = RenderClass::Cad;
};

} // namespace acdb
