#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Line
{
    EntityCommon common;
    glm::dvec3 start{0.0};
    glm::dvec3 end{0.0};
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
