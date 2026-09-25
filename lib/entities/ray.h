#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Ray
{
    EntityCommon common;
    glm::dvec3 start{0.0};
    glm::dvec3 direction{1.0, 0.0, 0.0};
};

} // namespace entities
