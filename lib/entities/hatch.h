#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "entity_common.h"

namespace entities
{

struct Hatch
{
    EntityCommon common;
    std::vector<glm::dvec3> outerLoop;
    std::vector<std::vector<glm::dvec3>> innerLoops;
    bool solidFill = true;
    std::string patternName = "SOLID";
    double patternScale = 1.0;
    double patternAngle = 0.0;
};

} // namespace entities
