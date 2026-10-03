#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Spline
{
    EntityCommon common;
    int degree = 3;
    bool closed = false;
    std::vector<AcGePoint3d> controlPoints;
    std::vector<AcGePoint3d> fitPoints;
    std::vector<double> knots;
};

} // namespace entities
