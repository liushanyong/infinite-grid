#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Arc
{
    EntityCommon common;
    AcGePoint3d center{0.0, 0.0, 0.0};
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double thickness = 0.0;
};

} // namespace entities
