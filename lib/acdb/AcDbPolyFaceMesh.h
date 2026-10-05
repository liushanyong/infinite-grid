#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "acdb/AcDbCore.h"
#include "acdb/AcDbMaterial.h"

namespace acdb
{

struct AcDbPolyFaceMeshGeometry
{
    // CPU authoring geometry remains double precision so tessellated CAD
    // picking and large-coordinate fills use the same world positions.
    std::vector<glm::dvec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<std::uint32_t> indices;
};

struct AcDbPolyFaceMesh
{
    AcDbEntity common;
    AcDbPolyFaceMeshGeometry geometry;
    MeshStyle style = MeshStyle::Cad;
    AcDbMaterial material;
};

} // namespace acdb
