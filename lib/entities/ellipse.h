#pragma once

#include <glm/glm.hpp>

#include "entity_common.h"

namespace entities
{

struct Ellipse
{
    EntityCommon common;
    glm::dvec3 center{0.0};
    glm::dvec3 majorAxis{0.0};
    double radiusRatio = 1.0;
    double startParameter = 0.0;
    double endParameter = 6.283185307179586;
    glm::dvec3 normal{0.0, 0.0, 1.0};
};

} // namespace entities
