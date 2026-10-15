#include "acge/AcGeTriangle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// AcGeTriangle2d
// ---------------------------------------------------------------------------

bool AcGeTriangle2d::barycentricCoordinates(const AcGePoint2d &point, double &u,
                                            double &v, double &w,
                                            const AcGeTol &tol) const
{
    const AcGeVector2d ab = b - a;
    const AcGeVector2d ac = c - a;
    const double determinant = ab.x * ac.y - ab.y * ac.x;
    const double scale =
        std::max({ab.length(), ac.length(), (c - b).length()});
    if (std::abs(determinant) <= std::abs(tol.equalPoint()) * scale)
        return false;

    const AcGeVector2d ap = point - a;
    v = (ap.x * ac.y - ap.y * ac.x) / determinant;
    w = (ab.x * ap.y - ab.y * ap.x) / determinant;
    u = 1.0 - v - w;
    return true;
}

bool AcGeTriangle2d::contains(const AcGePoint2d &point,
                              const AcGeTol &tol) const
{
    double u = 0.0, v = 0.0, w = 0.0;
    if (!barycentricCoordinates(point, u, v, w, tol))
        return closestPointTo(point).distanceTo(point) <=
               std::abs(tol.equalPoint());
    const double edgeScale =
        std::max({(b - a).length(), (c - b).length(), (a - c).length(),
                  std::abs(tol.equalPoint())});
    const double parameterTolerance =
        std::abs(tol.equalPoint()) / edgeScale;
    return u >= -parameterTolerance && v >= -parameterTolerance &&
           w >= -parameterTolerance;
}

AcGePoint2d AcGeTriangle2d::closestPointTo(const AcGePoint2d &point) const
{
    double u = 0.0, v = 0.0, w = 0.0;
    if (barycentricCoordinates(point, u, v, w) &&
        u >= 0.0 && v >= 0.0 && w >= 0.0)
        return point;

    const auto closestOnSegment = [](const AcGePoint2d &query,
                                     const AcGePoint2d &start,
                                     const AcGePoint2d &end) {
        const AcGeVector2d direction = end - start;
        const double lengthSquared = direction.lengthSq();
        const double parameter =
            lengthSquared == 0.0
                ? 0.0
                : std::clamp((query - start).dotProduct(direction) /
                                 lengthSquared,
                             0.0, 1.0);
        return start + direction * parameter;
    };
    AcGePoint2d closest = closestOnSegment(point, a, b);
    double bestDistance = closest.distanceTo(point);
    for (const auto &edge :
         {std::pair<AcGePoint2d, AcGePoint2d>{b, c}, {c, a}})
    {
        const AcGePoint2d candidate =
            closestOnSegment(point, edge.first, edge.second);
        const double distance = candidate.distanceTo(point);
        if (distance < bestDistance)
        {
            closest = candidate;
            bestDistance = distance;
        }
    }
    return closest;
}

// ---------------------------------------------------------------------------
// AcGeTriangle3d
// ---------------------------------------------------------------------------

bool AcGeTriangle3d::barycentricCoordinates(const AcGePoint3d &point, double &u,
                                            double &v, double &w,
                                            const AcGeTol &tol) const
{
    const AcGeVector3d ab = b - a;
    const AcGeVector3d ac = c - a;
    const AcGeVector3d ap = point - a;
    const double d00 = ab.dotProduct(ab);
    const double d01 = ab.dotProduct(ac);
    const double d11 = ac.dotProduct(ac);
    const double d20 = ap.dotProduct(ab);
    const double d21 = ap.dotProduct(ac);
    const double denominator = d00 * d11 - d01 * d01;
    const double scale =
        std::max({ab.length(), ac.length(), (c - b).length()});
    const double pointTolerance = std::abs(tol.equalPoint());
    if (denominator <= pointTolerance * pointTolerance * scale * scale)
        return false;

    v = (d11 * d20 - d01 * d21) / denominator;
    w = (d00 * d21 - d01 * d20) / denominator;
    u = 1.0 - v - w;
    return true;
}

bool AcGeTriangle3d::contains(const AcGePoint3d &point,
                              const AcGeTol &tol) const
{
    const AcGeVector3d area = areaVector();
    const double twiceArea = area.length();
    const double pointTolerance = std::abs(tol.equalPoint());
    if (twiceArea <= pointTolerance *
                         std::max({(b - a).length(), (c - b).length(),
                                   (a - c).length(), pointTolerance}))
        return closestPointTo(point).distanceTo(point) <= pointTolerance;
    if (std::abs((point - a).dotProduct(area / twiceArea)) >
        pointTolerance)
        return false;

    double u = 0.0, v = 0.0, w = 0.0;
    if (!barycentricCoordinates(point, u, v, w, tol))
        return false;
    const double edgeScale =
        std::max({(b - a).length(), (c - b).length(), (a - c).length(),
                  pointTolerance});
    const double parameterTolerance = pointTolerance / edgeScale;
    return u >= -parameterTolerance && v >= -parameterTolerance &&
           w >= -parameterTolerance;
}

AcGePoint3d AcGeTriangle3d::closestPointTo(const AcGePoint3d &point) const
{
    const AcGeVector3d ab = b - a;
    const AcGeVector3d ac = c - a;
    const AcGeVector3d ap = point - a;
    const double d1 = ab.dotProduct(ap);
    const double d2 = ac.dotProduct(ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return a;

    const AcGeVector3d bp = point - b;
    const double d3 = ab.dotProduct(bp);
    const double d4 = ac.dotProduct(bp);
    if (d3 >= 0.0 && d4 <= d3)
        return b;

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
        return a + ab * (d1 / (d1 - d3));

    const AcGeVector3d cp = point - c;
    const double d5 = ab.dotProduct(cp);
    const double d6 = ac.dotProduct(cp);
    if (d6 >= 0.0 && d5 <= d6)
        return c;

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
        return a + ac * (d2 / (d2 - d6));

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0)
        return b + (c - b) * ((d4 - d3) /
                              ((d4 - d3) + (d5 - d6)));

    const double denominator = va + vb + vc;
    if (denominator > 0.0 && std::isfinite(denominator))
        return a + ab * (vb / denominator) + ac * (vc / denominator);

    const auto closestOnSegment = [](const AcGePoint3d &query,
                                     const AcGePoint3d &start,
                                     const AcGePoint3d &end) {
        const AcGeVector3d direction = end - start;
        const double lengthSquared = direction.lengthSq();
        const double parameter =
            lengthSquared == 0.0
                ? 0.0
                : std::clamp((query - start).dotProduct(direction) /
                                 lengthSquared,
                             0.0, 1.0);
        return start + direction * parameter;
    };
    AcGePoint3d closest = closestOnSegment(point, a, b);
    double bestDistance = closest.distanceTo(point);
    for (const auto &edge :
         {std::pair<AcGePoint3d, AcGePoint3d>{b, c}, {c, a}})
    {
        const AcGePoint3d candidate =
            closestOnSegment(point, edge.first, edge.second);
        const double distance = candidate.distanceTo(point);
        if (distance < bestDistance)
        {
            closest = candidate;
            bestDistance = distance;
        }
    }
    return closest;
}

// ---------------------------------------------------------------------------
// AcGeMesh3d
// ---------------------------------------------------------------------------

void AcGeMesh3d::validateTriangle(const Triangle &triangle) const
{
    for (std::uint32_t index : triangle)
        if (index >= m_vertices.size())
            throw std::out_of_range("AcGeMesh3d triangle index is out of range.");
}

std::map<AcGeMesh3d::Edge, std::vector<std::size_t>>
AcGeMesh3d::edgeIncidence() const
{
    std::map<Edge, std::vector<std::size_t>> incidence;
    for (std::size_t face = 0; face < m_triangles.size(); ++face)
    {
        const Triangle &triangle = m_triangles[face];
        std::set<Edge> faceEdges;
        for (std::size_t edgeIndex = 0; edgeIndex < triangle.size();
             ++edgeIndex)
        {
            std::uint32_t first = triangle[edgeIndex];
            std::uint32_t second = triangle[(edgeIndex + 1) % 3];
            if (second < first)
                std::swap(first, second);
            if (first != second)
                faceEdges.insert({first, second});
        }
        for (const Edge &edge : faceEdges)
            incidence[edge].push_back(face);
    }
    return incidence;
}

AcGeMesh3d::AcGeMesh3d(std::vector<AcGePoint3d> vertices,
                       std::vector<Triangle> triangles)
    : m_vertices(std::move(vertices)), m_triangles(std::move(triangles))
{
    for (const AcGePoint3d &point : m_vertices)
        if (!finite(point))
            throw std::invalid_argument("AcGeMesh3d vertices must be finite.");
    for (const Triangle &triangle : m_triangles)
        validateTriangle(triangle);
}

AcGeMesh3d &AcGeMesh3d::setVertices(std::vector<AcGePoint3d> vertices)
{
    for (const AcGePoint3d &point : vertices)
        if (!finite(point))
            throw std::invalid_argument("AcGeMesh3d vertices must be finite.");
    for (const Triangle &triangle : m_triangles)
        for (std::uint32_t index : triangle)
            if (index >= vertices.size())
                throw std::out_of_range(
                    "AcGeMesh3d triangle index is out of range.");
    m_vertices = std::move(vertices);
    return *this;
}

AcGeMesh3d &AcGeMesh3d::addVertex(const AcGePoint3d &point)
{
    if (!finite(point))
        throw std::invalid_argument("AcGeMesh3d vertices must be finite.");
    if (m_vertices.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        throw std::length_error("AcGeMesh3d has too many vertices.");
    m_vertices.push_back(point);
    return *this;
}

AcGeMesh3d &AcGeMesh3d::addTriangle(std::uint32_t a, std::uint32_t b,
                                    std::uint32_t c)
{
    Triangle triangle{a, b, c};
    validateTriangle(triangle);
    m_triangles.push_back(triangle);
    return *this;
}

AcGeTriangle3d AcGeMesh3d::triangleAt(std::size_t index) const
{
    const Triangle &triangle = m_triangles.at(index);
    return {m_vertices[triangle[0]], m_vertices[triangle[1]],
            m_vertices[triangle[2]]};
}

std::optional<ge::AcGeBoundBox3d> AcGeMesh3d::bounds() const
{
    if (m_vertices.empty())
        return std::nullopt;
    AcGePoint3d minimum = m_vertices.front();
    AcGePoint3d maximum = minimum;
    for (const AcGePoint3d &point : m_vertices)
    {
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }
    return ge::AcGeBoundBox3d{minimum, maximum};
}

std::vector<AcGeVector3d> AcGeMesh3d::faceNormals() const
{
    std::vector<AcGeVector3d> result;
    result.reserve(m_triangles.size());
    for (std::size_t i = 0; i < m_triangles.size(); ++i)
        result.push_back(triangleAt(i).normal());
    return result;
}

double AcGeMesh3d::surfaceArea() const
{
    double area = 0.0;
    for (std::size_t i = 0; i < m_triangles.size(); ++i)
        area += triangleAt(i).area();
    return area;
}

double AcGeMesh3d::signedVolume() const
{
    double volume = 0.0;
    for (const Triangle &indices : m_triangles)
    {
        const AcGeVector3d a = m_vertices[indices[0]].asVector();
        const AcGeVector3d b = m_vertices[indices[1]].asVector();
        const AcGeVector3d c = m_vertices[indices[2]].asVector();
        volume += a.dotProduct(b.crossProduct(c)) / 6.0;
    }
    return volume;
}

AcGePoint3d AcGeMesh3d::centroid(const AcGeTol &tol) const
{
    if (m_vertices.empty())
        return {};

    double totalVolume = 0.0;
    AcGeVector3d weightedCentroid;
    for (const Triangle &indices : m_triangles)
    {
        const AcGePoint3d &a = m_vertices[indices[0]];
        const AcGePoint3d &b = m_vertices[indices[1]];
        const AcGePoint3d &c = m_vertices[indices[2]];
        const double tetrahedronVolume =
            a.asVector().dotProduct(
                b.asVector().crossProduct(c.asVector())) /
            6.0;
        totalVolume += tetrahedronVolume;
        weightedCentroid +=
            (a.asVector() + b.asVector() + c.asVector()) *
            (tetrahedronVolume / 4.0);
    }
    const double pointTolerance = std::abs(tol.equalPoint());
    if (std::abs(totalVolume) >
        pointTolerance * pointTolerance * pointTolerance)
        return {weightedCentroid.x / totalVolume,
                weightedCentroid.y / totalVolume,
                weightedCentroid.z / totalVolume};

    double totalArea = 0.0;
    AcGeVector3d surfaceCentroid;
    for (std::size_t i = 0; i < m_triangles.size(); ++i)
    {
        const AcGeTriangle3d triangle = triangleAt(i);
        const double area = triangle.area();
        totalArea += area;
        surfaceCentroid += triangle.centroid().asVector() * area;
    }
    if (totalArea > pointTolerance * pointTolerance)
        return {surfaceCentroid.x / totalArea,
                surfaceCentroid.y / totalArea,
                surfaceCentroid.z / totalArea};

    AcGeVector3d average;
    for (const AcGePoint3d &vertex : m_vertices)
        average += vertex.asVector();
    return {average.x / static_cast<double>(m_vertices.size()),
            average.y / static_cast<double>(m_vertices.size()),
            average.z / static_cast<double>(m_vertices.size())};
}

std::vector<AcGeVector3d> AcGeMesh3d::vertexNormals() const
{
    std::vector<AcGeVector3d> result(m_vertices.size());
    for (const Triangle &indices : m_triangles)
    {
        const AcGeVector3d area =
            (m_vertices[indices[1]] - m_vertices[indices[0]])
                .crossProduct(m_vertices[indices[2]] -
                              m_vertices[indices[0]]);
        for (std::uint32_t index : indices)
            result[index] += area;
    }
    for (AcGeVector3d &normal : result)
        normal.normalize();
    return result;
}

std::vector<std::uint32_t> AcGeMesh3d::vertexNeighbors(
    std::uint32_t vertex) const
{
    if (vertex >= m_vertices.size())
        throw std::out_of_range("AcGeMesh3d vertex index is out of range.");
    std::set<std::uint32_t> neighbors;
    for (const Triangle &triangle : m_triangles)
    {
        for (std::size_t i = 0; i < triangle.size(); ++i)
        {
            if (triangle[i] != vertex)
                continue;
            const std::uint32_t first = triangle[(i + 1) % 3];
            const std::uint32_t second = triangle[(i + 2) % 3];
            if (first != vertex)
                neighbors.insert(first);
            if (second != vertex)
                neighbors.insert(second);
        }
    }
    return {neighbors.begin(), neighbors.end()};
}

std::vector<std::size_t> AcGeMesh3d::faceNeighbors(std::size_t face) const
{
    const Triangle &triangle = m_triangles.at(face);
    const auto incidence = edgeIncidence();
    std::set<std::size_t> neighbors;
    for (std::size_t edgeIndex = 0; edgeIndex < triangle.size();
         ++edgeIndex)
    {
        std::uint32_t first = triangle[edgeIndex];
        std::uint32_t second = triangle[(edgeIndex + 1) % 3];
        if (first == second)
            continue;
        if (second < first)
            std::swap(first, second);
        for (std::size_t adjacent : incidence.at({first, second}))
            if (adjacent != face)
                neighbors.insert(adjacent);
    }
    return {neighbors.begin(), neighbors.end()};
}

std::vector<AcGeMesh3d::Edge> AcGeMesh3d::boundaryEdges() const
{
    std::vector<Edge> result;
    for (const auto &[edge, faces] : edgeIncidence())
        if (faces.size() == 1)
            result.push_back(edge);
    return result;
}

std::vector<AcGeMesh3d::Edge> AcGeMesh3d::nonManifoldEdges() const
{
    std::vector<Edge> result;
    for (const auto &[edge, faces] : edgeIncidence())
        if (faces.size() > 2)
            result.push_back(edge);
    return result;
}

bool AcGeMesh3d::isClosedEdgeManifold() const
{
    const auto incidence = edgeIncidence();
    return !incidence.empty() &&
           std::all_of(incidence.begin(), incidence.end(),
                       [](const auto &entry) {
                           return entry.second.size() == 2;
                       });
}

AcGeMesh3d &AcGeMesh3d::clear()
{
    m_vertices.clear();
    m_triangles.clear();
    return *this;
}
