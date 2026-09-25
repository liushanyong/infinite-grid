#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"

namespace entities
{

struct Polyline
{
    EntityCommon common;
    std::vector<glm::dvec3> vertices;
    std::vector<double> bulges;
    bool closed = false;
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
