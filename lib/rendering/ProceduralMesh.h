#pragma once

#include "RendererBackend.h"

#include <cstddef>
#include <vector>

namespace rendering
{

inline constexpr size_t kProceduralMeshFloatStride = 8;

// Shared CPU/GPU triangle soup. Three vertices form one triangle; each vertex
// is position.xyz, normal.xyz, then uv. Keeping picking on this exact data
// prevents analytic hit points from diverging from the rendered tessellation.
const std::vector<float> &proceduralMeshVertices(MeshType mesh);

} // namespace rendering
