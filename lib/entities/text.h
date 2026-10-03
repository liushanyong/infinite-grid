#pragma once

#include <glm/glm.hpp>

#include <string>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

struct Text
{
    EntityCommon common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    double height = 1.0;
    double rotation = 0.0;
    std::string text;
};

} // namespace entities
