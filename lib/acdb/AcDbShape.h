#pragma once

#include "AcDbPort.h"

// AcDbShape (namespace acdb) — the SHAPE entity, aligned with ObjectARX
// AcDbShape: a reference to a shape glyph inside a compiled shape/font
// file (STYLE table record whose file is a .shx shape set).  The stroke
// text engine carries text glyphs only, so until a shape-glyph lookup
// exists worldDraw renders the placement as a marker cross of the
// shape's size — the file still round-trips losslessly.

#include <glm/glm.hpp>

#include <string>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbShape : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    double scale = 1.0;
    double rotation = 0.0;
    double widthFactor = 1.0;
    double obliqueAngle = 0.0;
    double thickness = 0.0;
    // STYLE table record whose file holds the shape definition.
    std::string shapeName;
};

} // namespace acdb
