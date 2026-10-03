#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "entity_common.h"

namespace entities
{

enum class LightType
{
    Point,
    Directional,
    Spot,
};

struct Light
{
    EntityCommon common;
    LightType type = LightType::Point;
    AcGePoint3d position{0.0, 0.0, 0.0};
    AcGeVector3d target{0.0, 0.0, -1.0};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float innerConeAngle = 0.0f;
    float outerConeAngle = 0.0f;
};

} // namespace entities
