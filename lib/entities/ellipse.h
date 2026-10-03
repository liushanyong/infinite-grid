#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Ellipse
{
    EntityCommon common;
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d majorAxis{0.0, 0.0, 0.0};
    double radiusRatio = 1.0;
    double startParameter = 0.0;
    double endParameter = 6.283185307179586;
    AcGeVector3d normal{0.0, 0.0, 1.0};
};

} // namespace entities
