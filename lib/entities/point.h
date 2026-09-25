#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Point
{
    EntityCommon common;
    glm::dvec3 location{0.0};
};

} // namespace entities
