#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Solid
{
    EntityCommon common;
    glm::dvec3 firstCorner{0.0};
    glm::dvec3 secondCorner{0.0};
    glm::dvec3 thirdCorner{0.0};
    glm::dvec3 fourthCorner{0.0};
};

} // namespace entities
