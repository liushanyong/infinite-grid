#include "AcGeDelaunay2d.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGePredicates.h"

namespace ge
{
namespace
{

using Triangle = std::array<std::uint32_t, 3>;
using Edge = std::pair<std::uint32_t, std::uint32_t>;

struct Vertex
{
    AcGePoint2d point;
    std::uint32_t inputIndex;
};

Edge orderedEdge(std::uint32_t a, std::uint32_t b)
{
    return std::minmax(a, b);
}

Triangle counterClockwise(std::uint32_t a, std::uint32_t b,
                          std::uint32_t c,
                          const std::vector<Vertex> &vertices)
{
    if (orient2d(vertices[a].point, vertices[b].point,
                 vertices[c].point) < 0.0)
        std::swap(b, c);
    return {a, b, c};
}

bool pointOnSegment(const AcGePoint2d &point, const AcGePoint2d &a,
                    const AcGePoint2d &b)
{
    return point.x >= std::min(a.x, b.x) &&
           point.x <= std::max(a.x, b.x) &&
           point.y >= std::min(a.y, b.y) &&
           point.y <= std::max(a.y, b.y);
}

std::vector<std::uint32_t> convexHull(const std::vector<Vertex> &vertices)
{
    std::vector<std::uint32_t> lower;
    std::vector<std::uint32_t> upper;
    lower.reserve(vertices.size());
    upper.reserve(vertices.size());

    for (std::uint32_t i = 0; i < vertices.size(); ++i)
    {
        while (lower.size() >= 2 &&
               orient2d(vertices[lower[lower.size() - 2]].point,
                        vertices[lower.back()].point, vertices[i].point) <=
                   0.0)
            lower.pop_back();
        lower.push_back(i);
    }

    for (std::size_t i = vertices.size(); i-- > 0;)
    {
        while (upper.size() >= 2 &&
               orient2d(vertices[upper[upper.size() - 2]].point,
                        vertices[upper.back()].point, vertices[i].point) <=
                   0.0)
            upper.pop_back();
        upper.push_back(static_cast<std::uint32_t>(i));
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

bool isConvexQuadrilateral(std::uint32_t a, std::uint32_t b,
                           std::uint32_t c, std::uint32_t d,
                           const std::vector<Vertex> &vertices)
{
    const double sideC =
        orient2d(vertices[a].point, vertices[b].point, vertices[c].point);
    const double sideD =
        orient2d(vertices[a].point, vertices[b].point, vertices[d].point);
    const double sideA =
        orient2d(vertices[c].point, vertices[d].point, vertices[a].point);
    const double sideB =
        orient2d(vertices[c].point, vertices[d].point, vertices[b].point);
    return sideC != 0.0 && sideD != 0.0 && sideA != 0.0 && sideB != 0.0 &&
           ((sideC < 0.0) != (sideD < 0.0)) &&
           ((sideA < 0.0) != (sideB < 0.0));
}

} // namespace

std::vector<std::array<std::uint32_t, 3>> delaunayTriangulate2d(
    const std::vector<AcGePoint2d> &points)
{
    if (points.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Delaunay input has too many points.");

    std::vector<Vertex> vertices;
    vertices.reserve(points.size());
    for (std::uint32_t i = 0; i < points.size(); ++i)
    {
        const AcGePoint2d &point = points[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            throw std::invalid_argument(
                "Delaunay input coordinates must be finite.");
        vertices.push_back({point, i});
    }

    std::sort(vertices.begin(), vertices.end(),
              [](const Vertex &a, const Vertex &b) {
                  if (a.point.x != b.point.x)
                      return a.point.x < b.point.x;
                  if (a.point.y != b.point.y)
                      return a.point.y < b.point.y;
                  return a.inputIndex < b.inputIndex;
              });

    vertices.erase(
        std::unique(vertices.begin(), vertices.end(),
                    [](const Vertex &a, const Vertex &b) {
                        return a.point.x == b.point.x &&
                               a.point.y == b.point.y;
                    }),
        vertices.end());
    if (vertices.size() < 3)
        return {};

    const std::vector<std::uint32_t> hull = convexHull(vertices);
    if (hull.size() < 3)
        return {};

    std::vector<Triangle> triangles;
    triangles.reserve(vertices.size() * 2);
    for (std::size_t i = 1; i + 1 < hull.size(); ++i)
        triangles.push_back({hull[0], hull[i], hull[i + 1]});

    std::vector<bool> onHull(vertices.size(), false);
    for (std::uint32_t vertex : hull)
        onHull[vertex] = true;

    for (std::uint32_t pointIndex = 0; pointIndex < vertices.size();
         ++pointIndex)
    {
        if (onHull[pointIndex])
            continue;

        std::vector<std::size_t> containing;
        std::vector<Edge> splitEdges;
        for (std::size_t i = 0; i < triangles.size(); ++i)
        {
            const Triangle &triangle = triangles[i];
            bool inside = true;
            for (int edge = 0; edge < 3; ++edge)
            {
                const std::uint32_t a = triangle[edge];
                const std::uint32_t b = triangle[(edge + 1) % 3];
                const double side = orient2d(vertices[a].point,
                                             vertices[b].point,
                                             vertices[pointIndex].point);
                if (side < 0.0)
                {
                    inside = false;
                    break;
                }
                if (side == 0.0 &&
                    pointOnSegment(vertices[pointIndex].point,
                                   vertices[a].point, vertices[b].point))
                    splitEdges.push_back(orderedEdge(a, b));
            }
            if (inside)
                containing.push_back(i);
        }

        if (containing.empty())
            continue;

        if (!splitEdges.empty())
        {
            std::sort(splitEdges.begin(), splitEdges.end());
            splitEdges.erase(std::unique(splitEdges.begin(), splitEdges.end()),
                             splitEdges.end());
            const Edge splitEdge = splitEdges.front();
            std::vector<Triangle> updated;
            updated.reserve(triangles.size() + 2);
            for (const Triangle &triangle : triangles)
            {
                int edgeIndex = -1;
                for (int edge = 0; edge < 3; ++edge)
                {
                    if (orderedEdge(triangle[edge],
                                    triangle[(edge + 1) % 3]) == splitEdge)
                    {
                        edgeIndex = edge;
                        break;
                    }
                }
                if (edgeIndex < 0)
                {
                    updated.push_back(triangle);
                    continue;
                }

                const std::uint32_t a = triangle[edgeIndex];
                const std::uint32_t b = triangle[(edgeIndex + 1) % 3];
                const std::uint32_t opposite = triangle[(edgeIndex + 2) % 3];
                updated.push_back(counterClockwise(
                    a, pointIndex, opposite, vertices));
                updated.push_back(counterClockwise(
                    pointIndex, b, opposite, vertices));
            }
            triangles = std::move(updated);
        }
        else
        {
            const std::size_t containingTriangle = containing.front();
            const Triangle old = triangles[containingTriangle];
            triangles[containingTriangle] =
                counterClockwise(old[0], old[1], pointIndex, vertices);
            triangles.push_back(
                counterClockwise(old[1], old[2], pointIndex, vertices));
            triangles.push_back(
                counterClockwise(old[2], old[0], pointIndex, vertices));
        }

        while (true)
        {
            std::map<Edge, std::vector<std::size_t>> incidence;
            for (std::size_t i = 0; i < triangles.size(); ++i)
            {
                const Triangle &triangle = triangles[i];
                for (int edge = 0; edge < 3; ++edge)
                    incidence[orderedEdge(triangle[edge],
                                           triangle[(edge + 1) % 3])]
                        .push_back(i);
            }

            bool flipped = false;
            for (const auto &[edge, adjacent] : incidence)
            {
                if (adjacent.size() != 2)
                    continue;
                const Triangle &first = triangles[adjacent[0]];
                const Triangle &second = triangles[adjacent[1]];
                std::uint32_t oppositeFirst = first[0];
                std::uint32_t oppositeSecond = second[0];
                for (std::uint32_t vertex : first)
                    if (vertex != edge.first && vertex != edge.second)
                        oppositeFirst = vertex;
                for (std::uint32_t vertex : second)
                    if (vertex != edge.first && vertex != edge.second)
                        oppositeSecond = vertex;

                if (!isConvexQuadrilateral(edge.first, edge.second,
                                           oppositeFirst, oppositeSecond,
                                           vertices))
                    continue;

                const double orientation =
                    orient2d(vertices[edge.first].point,
                             vertices[edge.second].point,
                             vertices[oppositeFirst].point);
                const double circleSide =
                    orientation > 0.0
                        ? incircle(vertices[edge.first].point,
                                   vertices[edge.second].point,
                                   vertices[oppositeFirst].point,
                                   vertices[oppositeSecond].point)
                        : incircle(vertices[edge.second].point,
                                   vertices[edge.first].point,
                                   vertices[oppositeFirst].point,
                                   vertices[oppositeSecond].point);
                if (circleSide <= 0.0)
                    continue;

                triangles[adjacent[0]] = counterClockwise(
                    oppositeFirst, oppositeSecond, edge.first, vertices);
                triangles[adjacent[1]] = counterClockwise(
                    oppositeSecond, oppositeFirst, edge.second, vertices);
                flipped = true;
                break;
            }
            if (!flipped)
                break;
        }
    }

    std::vector<Triangle> result;
    result.reserve(triangles.size());
    for (const Triangle &triangle : triangles)
        result.push_back({vertices[triangle[0]].inputIndex,
                          vertices[triangle[1]].inputIndex,
                          vertices[triangle[2]].inputIndex});
    return result;
}

} // namespace ge
