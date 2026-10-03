#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

// The construction line extends without bound from the point in both
// directions.
struct XLine
{
    EntityCommon common;
    AcGePoint3d point{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
};

} // namespace entities
