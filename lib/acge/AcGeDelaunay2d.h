#pragma once

#include "AcGePort.h"

#include <array>
#include <cstdint>
#include <vector>

#include "AcGePoint.h"

namespace ge
{

// Triangulate a 2D point set. Each returned counter-clockwise triangle
// contains indices into |points|. Exact duplicate points are represented by
// their first input index; collinear point sets return no triangles.
ACGE_PORT std::vector<std::array<std::uint32_t, 3>> delaunayTriangulate2d(
    const std::vector<AcGePoint2d> &points);

} // namespace ge
