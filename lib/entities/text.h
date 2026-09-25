#pragma once

#include <glm/glm.hpp>

#include <string>

#include "entity_common.h"

namespace entities
{

struct Text
{
    EntityCommon common;
    glm::dvec3 insertion{0.0};
    double height = 1.0;
    double rotation = 0.0;
    std::string text;
};

} // namespace entities
