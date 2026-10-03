#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "../ge/ge.h"
#include "entity_common.h"
#include "render_class.h"

namespace entities
{

struct Solid3d
{
    EntityCommon common;
    std::vector<AcGePoint3d> vertices;
    std::vector<std::uint32_t> indices;
    RenderClass renderClass = RenderClass::Cad;
};

} // namespace entities
