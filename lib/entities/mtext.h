#pragma once

#include <glm/glm.hpp>

#include <string>

#include "entity_common.h"

namespace entities
{

struct MText
{
    EntityCommon common;
    glm::dvec3 insertion{0.0};
    glm::dvec3 direction{1.0, 0.0, 0.0};
    double width = 0.0;
    double height = 1.0;
    std::string text;
};

} // namespace entities
