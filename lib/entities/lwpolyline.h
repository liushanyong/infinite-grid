#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"

namespace entities
{

struct LwPolyline
{
    EntityCommon common;
    std::vector<AcGePoint2d> vertices;
    std::vector<double> bulges;
    double elevation = 0.0;
    double thickness = 0.0;
    bool closed = false;
    AcGeVector3d normal{0.0, 0.0, 1.0};
};

} // namespace entities
