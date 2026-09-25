#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"

namespace entities
{

struct MLine
{
    EntityCommon common;
    std::vector<glm::dvec3> vertices;
    glm::dvec3 scale{1.0, 1.0, 1.0};
    bool closed = false;
};

} // namespace entities
