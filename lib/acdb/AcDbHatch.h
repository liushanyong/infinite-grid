#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbHatch
{
    AcDbEntity common;
    std::vector<AcGePoint3d> outerLoop;
    std::vector<std::vector<AcGePoint3d>> innerLoops;
    bool solidFill = true;
    std::string patternName = "SOLID";
    double patternScale = 1.0;
    double patternAngle = 0.0;
};

} // namespace acdb
