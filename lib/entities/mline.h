#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct MLine
{
    EntityCommon common;
    std::vector<AcGePoint3d> vertices;
    AcGeVector3d scale{1.0, 1.0, 1.0};
    bool closed = false;
};

} // namespace entities
