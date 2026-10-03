#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Point
{
    EntityCommon common;
    AcGePoint3d location{0.0, 0.0, 0.0};
};

} // namespace entities
