#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

enum class LightType
{
    AcDbPoint,
    Directional,
    Spot,
};

struct AcDbLight
{
    AcDbEntity common;
    LightType type = LightType::AcDbPoint;
    AcGePoint3d position{0.0, 0.0, 0.0};
    AcGeVector3d target{0.0, 0.0, -1.0};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float innerConeAngle = 0.0f;
    float outerConeAngle = 0.0f;
};

} // namespace acdb
