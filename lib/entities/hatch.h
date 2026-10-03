#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Hatch
{
    EntityCommon common;
    std::vector<AcGePoint3d> outerLoop;
    std::vector<std::vector<AcGePoint3d>> innerLoops;
    bool solidFill = true;
    std::string patternName = "SOLID";
    double patternScale = 1.0;
    double patternAngle = 0.0;
};

} // namespace entities
