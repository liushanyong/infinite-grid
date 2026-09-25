#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"

namespace entities
{

struct Spline
{
    EntityCommon common;
    int degree = 3;
    bool closed = false;
    std::vector<glm::dvec3> controlPoints;
    std::vector<glm::dvec3> fitPoints;
    std::vector<double> knots;
};

} // namespace entities
