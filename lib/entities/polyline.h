#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Polyline
{
    EntityCommon common;
    std::vector<AcGePoint3d> vertices;
    std::vector<double> bulges;
    bool closed = false;
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
