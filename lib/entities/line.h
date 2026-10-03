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
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
