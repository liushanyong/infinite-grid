#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Arc
{
    EntityCommon common;
    glm::dvec3 center{0.0};
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
