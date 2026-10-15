#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gepoint.h"
#include "geoctree.h"

struct AcGeTriangle2d
{
    AcGePoint2d a{};
    AcGePoint2d b{};
    AcGePoint2d c{};

    AcGeTriangle2d() = default;
    AcGeTriangle2d(const AcGePoint2d &first, const AcGePoint2d &second,
                   const AcGePoint2d &third)
        : a(first), b(second), c(third)
    {
    }

    double signedArea() const
    {
        const AcGeVector2d ab = b - a;
        const AcGeVector2d ac = c - a;
        return (ab.x * ac.y - ab.y * ac.x) * 0.5;
    }
    double area() const { return std::abs(signedArea()); }
    AcGePoint2d centroid() const
    {
        return {(a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0};
    }

    bool barycentricCoordinates(const AcGePoint2d &point, double &u,
                                double &v, double &w,
                                const AcGeTol &tol = AcGeTol{}) const
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

    bool contains(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const
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

    AcGePoint2d closestPointTo(const AcGePoint2d &point) const
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
};

struct AcGeTriangle3d
{
    AcGePoint3d a{};
    AcGePoint3d b{};
    AcGePoint3d c{};

    AcGeTriangle3d() = default;
    AcGeTriangle3d(const AcGePoint3d &first, const AcGePoint3d &second,
                   const AcGePoint3d &third)
        : a(first), b(second), c(third)
    {
    }

    AcGeVector3d areaVector() const
    {
        return (b - a).crossProduct(c - a);
    }
    AcGeVector3d normal() const { return areaVector().normal(); }
    double area() const { return areaVector().length() * 0.5; }
    AcGePoint3d centroid() const
    {
        return { (a.x + b.x + c.x) / 3.0,
                 (a.y + b.y + c.y) / 3.0,
                 (a.z + b.z + c.z) / 3.0 };
    }

    bool barycentricCoordinates(const AcGePoint3d &point, double &u,
                                double &v, double &w,
                                const AcGeTol &tol = AcGeTol{}) const
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

    bool contains(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const
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

    AcGePoint3d closestPointTo(const AcGePoint3d &point) const
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
};

class AcGeMesh3d
{
public:
    using Triangle = std::array<std::uint32_t, 3>;

private:
    std::vector<AcGePoint3d> m_vertices;
    std::vector<Triangle> m_triangles;

    static bool finite(const AcGePoint3d &point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) &&
               std::isfinite(point.z);
    }
    void validateTriangle(const Triangle &triangle) const
    {
        for (std::uint32_t index : triangle)
            if (index >= m_vertices.size())
                throw std::out_of_range("AcGeMesh3d triangle index is out of range.");
    }

public:
    AcGeMesh3d() = default;
    AcGeMesh3d(std::vector<AcGePoint3d> vertices,
               std::vector<Triangle> triangles)
        : m_vertices(std::move(vertices)), m_triangles(std::move(triangles))
    {
        for (const AcGePoint3d &point : m_vertices)
            if (!finite(point))
                throw std::invalid_argument("AcGeMesh3d vertices must be finite.");
        for (const Triangle &triangle : m_triangles)
            validateTriangle(triangle);
    }

    std::size_t numVertices() const { return m_vertices.size(); }
    std::size_t numTriangles() const { return m_triangles.size(); }
    bool isEmpty() const { return m_triangles.empty(); }
    const std::vector<AcGePoint3d> &vertices() const { return m_vertices; }
    const std::vector<Triangle> &triangles() const { return m_triangles; }

    AcGeMesh3d &setVertices(std::vector<AcGePoint3d> vertices)
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
    AcGeMesh3d &addVertex(const AcGePoint3d &point)
    {
        if (!finite(point))
            throw std::invalid_argument("AcGeMesh3d vertices must be finite.");
        if (m_vertices.size() >=
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
            throw std::length_error("AcGeMesh3d has too many vertices.");
        m_vertices.push_back(point);
        return *this;
    }
    AcGeMesh3d &addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
    {
        Triangle triangle{a, b, c};
        validateTriangle(triangle);
        m_triangles.push_back(triangle);
        return *this;
    }
    AcGeTriangle3d triangleAt(std::size_t index) const
    {
        const Triangle &triangle = m_triangles.at(index);
        return {m_vertices[triangle[0]], m_vertices[triangle[1]],
                m_vertices[triangle[2]]};
    }
    std::optional<ge::AcGeBoundBox3d> bounds() const
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
    std::vector<AcGeVector3d> faceNormals() const
    {
        std::vector<AcGeVector3d> result;
        result.reserve(m_triangles.size());
        for (std::size_t i = 0; i < m_triangles.size(); ++i)
            result.push_back(triangleAt(i).normal());
        return result;
    }
    std::vector<AcGeVector3d> vertexNormals() const
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
    AcGeMesh3d &clear()
    {
        m_vertices.clear();
        m_triangles.clear();
        return *this;
    }
};
