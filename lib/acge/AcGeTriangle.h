#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGePoint.h"
#include "AcGeOctree.h"

struct ACGE_PORT AcGeTriangle2d
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
                                const AcGeTol &tol = AcGeTol{}) const;
    bool contains(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
    AcGePoint2d closestPointTo(const AcGePoint2d &point) const;
};

struct ACGE_PORT AcGeTriangle3d
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
                                const AcGeTol &tol = AcGeTol{}) const;
    bool contains(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
    AcGePoint3d closestPointTo(const AcGePoint3d &point) const;
};

class ACGE_PORT AcGeMesh3d
{
public:
    using Triangle = std::array<std::uint32_t, 3>;
    using Edge = std::array<std::uint32_t, 2>;

private:
    std::vector<AcGePoint3d> m_vertices;
    std::vector<Triangle> m_triangles;

    static bool finite(const AcGePoint3d &point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) &&
               std::isfinite(point.z);
    }
    void validateTriangle(const Triangle &triangle) const;
    std::map<Edge, std::vector<std::size_t>> edgeIncidence() const;

public:
    AcGeMesh3d() = default;
    AcGeMesh3d(std::vector<AcGePoint3d> vertices,
               std::vector<Triangle> triangles);

    std::size_t numVertices() const { return m_vertices.size(); }
    std::size_t numTriangles() const { return m_triangles.size(); }
    bool isEmpty() const { return m_triangles.empty(); }
    const std::vector<AcGePoint3d> &vertices() const { return m_vertices; }
    const std::vector<Triangle> &triangles() const { return m_triangles; }

    AcGeMesh3d &setVertices(std::vector<AcGePoint3d> vertices);
    AcGeMesh3d &addVertex(const AcGePoint3d &point);
    AcGeMesh3d &addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
    AcGeTriangle3d triangleAt(std::size_t index) const;
    std::optional<ge::AcGeBoundBox3d> bounds() const;
    std::vector<AcGeVector3d> faceNormals() const;
    double surfaceArea() const;
    double signedVolume() const;
    double volume() const { return std::abs(signedVolume()); }
    AcGePoint3d centroid(const AcGeTol &tol = AcGeTol{}) const;
    std::vector<AcGeVector3d> vertexNormals() const;
    std::vector<std::uint32_t> vertexNeighbors(std::uint32_t vertex) const;
    std::vector<std::size_t> faceNeighbors(std::size_t face) const;
    std::vector<Edge> boundaryEdges() const;
    std::vector<Edge> nonManifoldEdges() const;
    bool isEdgeManifold() const { return nonManifoldEdges().empty(); }
    bool isClosedEdgeManifold() const;
    AcGeMesh3d &clear();
};
