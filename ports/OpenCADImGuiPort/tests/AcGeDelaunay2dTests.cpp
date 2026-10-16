#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "acge/AcGe.h"

namespace
{

double orientation(const std::vector<AcGePoint2d> &points,
                   const std::array<std::uint32_t, 3> &triangle)
{
    return ge::orient2d(points[triangle[0]], points[triangle[1]],
                        points[triangle[2]]);
}

void testSquare()
{
    const std::vector<AcGePoint2d> points{
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    const auto triangles = ge::delaunayTriangulate2d(points);
    assert(triangles.size() == 2);
    for (const auto &triangle : triangles)
        assert(orientation(points, triangle) > 0.0);

    const double diagonal02 =
        points[0].distanceTo(points[2]);
    const double diagonal13 =
        points[1].distanceTo(points[3]);
    assert(std::abs(diagonal02 - diagonal13) < 1.0e-12);
}

void testInteriorPointAndDuplicates()
{
    const std::vector<AcGePoint2d> points{
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0},
        {1.0, 1.0}, {1.0, 1.0}};
    const auto triangles = ge::delaunayTriangulate2d(points);
    assert(triangles.size() == 4);
    for (const auto &triangle : triangles)
    {
        assert(orientation(points, triangle) > 0.0);
        assert(triangle[0] != 5 && triangle[1] != 5 && triangle[2] != 5);
    }
}

void testDegenerateAndInvalidInputs()
{
    assert(ge::delaunayTriangulate2d({}).empty());
    assert(ge::delaunayTriangulate2d(
               {{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}})
               .empty());

    bool rejectedNonFinite = false;
    try
    {
        ge::delaunayTriangulate2d(
            {{0.0, 0.0},
             {1.0, 0.0},
             {0.0, std::numeric_limits<double>::infinity()}});
    }
    catch (const std::invalid_argument &)
    {
        rejectedNonFinite = true;
    }
    assert(rejectedNonFinite);
}

} // namespace

int main()
{
    testSquare();
    testInteriorPointAndDuplicates();
    testDegenerateAndInvalidInputs();
}
