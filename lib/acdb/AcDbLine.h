#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbLine
{
    AcDbEntity common;
    AcGePoint3d start{0.0, 0.0, 0.0};
    AcGePoint3d end{0.0, 0.0, 0.0};
    // DWG LINE carries an OCS extrusion and a wall thickness.
    double thickness = 0.0;
    AcGeVector3d extrusion{0.0, 0.0, 1.0};
};

} // namespace acdb
