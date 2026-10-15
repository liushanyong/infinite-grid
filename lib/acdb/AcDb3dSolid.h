#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"
#include "acdb/AcDbRenderClass.h"

namespace acdb
{

struct ACDB_PORT AcDb3dSolid : AcDbEntity
{
    std::vector<AcGePoint3d> vertices;
    std::vector<std::uint32_t> indices;
    RenderClass renderClass = RenderClass::Cad;
    // Fills render in the 3DFace surface channel (DXF 3DFace parity; the
    // demo's Solid3d opts in).  Entity data drives drawing — ARX style.
    bool fillIs3DFace = false;

    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definition lives in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
};

} // namespace acdb
