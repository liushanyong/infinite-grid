#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

// The construction line extends without bound from the point in both
// directions.
struct XLine
{
    EntityCommon common;
    glm::dvec3 point{0.0};
    glm::dvec3 direction{1.0, 0.0, 0.0};
};

} // namespace entities
