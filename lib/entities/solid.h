#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Solid
{
    EntityCommon common;
    AcGePoint3d firstCorner{0.0, 0.0, 0.0};
    AcGePoint3d secondCorner{0.0, 0.0, 0.0};
    AcGePoint3d thirdCorner{0.0, 0.0, 0.0};
    AcGePoint3d fourthCorner{0.0, 0.0, 0.0};
};

} // namespace entities
