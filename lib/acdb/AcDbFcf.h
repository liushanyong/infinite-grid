#pragma once

// AcDbFcf (namespace acdb) — the TOLERANCE entity, aligned with
// ObjectARX AcDbFcf ("feature control frame"): a boxed GD&T callout
// whose text carries the AutoCAD formatting codes ({\f...;} font runs,
// \A...; alignment).  worldDraw strips the frame codes and draws the
// plain glyph run.

#include <glm/glm.hpp>

#include <string>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbFcf
{
    AcDbEntity common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    // Frame's reading direction (DXF 11 x_direction).
    AcGeVector3d direction{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    std::string text;
    double height = 1.0;
};

} // namespace acdb
