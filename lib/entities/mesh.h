#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "entity_common.h"
#include "material.h"

namespace entities
{

struct MeshGeometry
{
    // CPU authoring geometry remains double precision so tessellated CAD
    // picking and large-coordinate fills use the same world positions.
    std::vector<glm::dvec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<std::uint32_t> indices;
};

struct Mesh
{
    EntityCommon common;
    MeshGeometry geometry;
    MeshStyle style = MeshStyle::Cad;
    PbrMaterial material;
};

} // namespace entities
