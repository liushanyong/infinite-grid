#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Line
{
    EntityCommon common;
    AcGePoint3d start{0.0, 0.0, 0.0};
    AcGePoint3d end{0.0, 0.0, 0.0};
    // DWG LINE carries an OCS extrusion and a wall thickness.
    double thickness = 0.0;
    AcGeVector3d extrusion{0.0, 0.0, 1.0};
};

} // namespace entities
