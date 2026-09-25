#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"
#include "render_class.h"

namespace entities
{

struct Solid3d
{
    EntityCommon common;
    std::vector<glm::dvec3> vertices;
    std::vector<std::uint32_t> indices;
    RenderClass renderClass = RenderClass::Cad;
};

} // namespace entities
