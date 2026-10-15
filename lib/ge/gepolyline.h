#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#include "gepredicates.h"
#include "gepoint.h"

// Source-level AcGe polyline subset. Parameters are segment indices with a
// fractional position along each segment; closed polylines wrap parameters.
template <typename Point, typename Vector>
class AcGePolylineBase
{
protected:
    std::vector<Point> m_vertices;
    bool m_closed = false;

    std::size_t segmentCount() const
    {
        if (m_vertices.size() < 2)
            return 0;
        return m_closed ? m_vertices.size() : m_vertices.size() - 1;
    }

public:
    AcGePolylineBase() = default;
    explicit AcGePolylineBase(const std::vector<Point> &vertices,
                              bool closed = false)
        : m_vertices(vertices), m_closed(closed)
    {
    }
    AcGePolylineBase(std::initializer_list<Point> vertices,
                     bool closed = false)
        : m_vertices(vertices), m_closed(closed)
    {
    }

    std::size_t numVerts() const { return m_vertices.size(); }
    bool isClosed() const { return m_closed; }
    bool isEmpty() const { return m_vertices.empty(); }
    const std::vector<Point> &vertices() const { return m_vertices; }

    AcGePolylineBase &setClosed(bool closed)
    {
        m_closed = closed;
        return *this;
    }
    AcGePolylineBase &setVertexAt(std::size_t index, const Point &point)
    {
        m_vertices.at(index) = point;
        return *this;
    }
    Point vertexAt(std::size_t index) const { return m_vertices.at(index); }
    AcGePolylineBase &appendVertex(const Point &point)
    {
        m_vertices.push_back(point);
        return *this;
    }
    AcGePolylineBase &insertVertexAt(std::size_t index, const Point &point)
    {
        if (index > m_vertices.size())
            throw std::out_of_range("AcGePolyline vertex index is out of range.");
        m_vertices.insert(m_vertices.begin() + index, point);
        return *this;
    }
    AcGePolylineBase &removeVertexAt(std::size_t index)
    {
        if (index >= m_vertices.size())
            throw std::out_of_range("AcGePolyline vertex index is out of range.");
        m_vertices.erase(m_vertices.begin() + index);
        return *this;
    }
    AcGePolylineBase &reverse()
    {
        std::reverse(m_vertices.begin(), m_vertices.end());
        return *this;
    }
    AcGePolylineBase &clear()
    {
        m_vertices.clear();
        m_closed = false;
        return *this;
    }

    double length() const
    {
        double result = 0.0;
        for (std::size_t i = 0; i < segmentCount(); ++i)
        {
            const std::size_t next = (i + 1) % m_vertices.size();
            result += m_vertices[i].distanceTo(m_vertices[next]);
        }
        return result;
    }

    Point pointAt(double parameter) const
    {
        if (m_vertices.empty())
            return {};
        if (m_vertices.size() == 1)
            return m_vertices.front();
        if (!std::isfinite(parameter))
            return m_vertices.front();

        const std::size_t segments = segmentCount();
        if (segments == 0)
            return m_vertices.back();
        if (m_closed)
        {
            parameter = std::fmod(parameter, double(segments));
            if (parameter < 0.0)
                parameter += double(segments);
        }
        else
        {
            parameter = std::clamp(parameter, 0.0, double(segments));
            if (parameter >= double(segments))
                return m_vertices.back();
        }

        const auto index = static_cast<std::size_t>(std::floor(parameter));
        const double fraction = parameter - double(index);
        const std::size_t next = (index + 1) % m_vertices.size();
        return m_vertices[index] + (m_vertices[next] - m_vertices[index]) *
                                        fraction;
    }

    Point closestPointTo(const Point &point, double &parameter) const
    {
        if (m_vertices.empty())
        {
            parameter = 0.0;
            return {};
        }
        if (m_vertices.size() == 1 || segmentCount() == 0)
        {
            parameter = 0.0;
            return m_vertices.front();
        }

        Point closest = m_vertices.front();
        double bestDistanceSquared = (point - closest).lengthSq();
        parameter = 0.0;
        for (std::size_t i = 0; i < segmentCount(); ++i)
        {
            const std::size_t next = (i + 1) % m_vertices.size();
            const Vector direction = m_vertices[next] - m_vertices[i];
            const double lengthSquared = direction.lengthSq();
            const double fraction =
                lengthSquared == 0.0
                    ? 0.0
                    : std::clamp((point - m_vertices[i]).dotProduct(direction) /
                                     lengthSquared,
                                 0.0, 1.0);
            const Point candidate = m_vertices[i] + direction * fraction;
            const double distanceSquared = (point - candidate).lengthSq();
            if (distanceSquared < bestDistanceSquared)
            {
                closest = candidate;
                bestDistanceSquared = distanceSquared;
                parameter = double(i) + fraction;
            }
        }
        return closest;
    }

    Point closestPointTo(const Point &point) const
    {
        double ignoredParameter = 0.0;
        return closestPointTo(point, ignoredParameter);
    }

    double distanceTo(const Point &point) const
    {
        return point.distanceTo(closestPointTo(point));
    }

    bool isOn(const Point &point, double &parameter,
              double tolerance = 1.0e-9) const
    {
        if (m_vertices.empty())
        {
            parameter = 0.0;
            return false;
        }
        const Point closest = closestPointTo(point, parameter);
        return closest.distanceTo(point) <= std::abs(tolerance);
    }
};

struct AcGePolyline2d : AcGePolylineBase<AcGePoint2d, AcGeVector2d>
{
    using AcGePolylineBase::AcGePolylineBase;

    double signedArea() const
    {
        if (!m_closed || m_vertices.size() < 3)
            return 0.0;
        double twiceArea = 0.0;
        const std::size_t count = m_vertices.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            const AcGePoint2d &a = m_vertices[i];
            const AcGePoint2d &b = m_vertices[(i + 1) % count];
            twiceArea += a.x * b.y - b.x * a.y;
        }
        return twiceArea * 0.5;
    }

    double area() const { return std::abs(signedArea()); }
};

struct AcGePolygon2d
{
private:
    AcGePolyline2d m_boundary;

public:
    AcGePolygon2d() { m_boundary.setClosed(true); }
    explicit AcGePolygon2d(const std::vector<AcGePoint2d> &vertices)
        : m_boundary(vertices, true)
    {
    }
    AcGePolygon2d(std::initializer_list<AcGePoint2d> vertices)
        : m_boundary(vertices, true)
    {
    }

    AcGePolygon2d &setVertices(const std::vector<AcGePoint2d> &vertices)
    {
        m_boundary = AcGePolyline2d(vertices, true);
        return *this;
    }
    AcGePolygon2d &appendVertex(const AcGePoint2d &point)
    {
        m_boundary.appendVertex(point);
        return *this;
    }
    std::size_t numVerts() const { return m_boundary.numVerts(); }
    const std::vector<AcGePoint2d> &vertices() const
    {
        return m_boundary.vertices();
    }

    double perimeter() const { return m_boundary.length(); }
    double signedArea() const { return m_boundary.signedArea(); }
    double area() const { return m_boundary.area(); }

    AcGePoint2d centroid(const AcGeTol &tol = AcGeTol{}) const
    {
        const auto &points = m_boundary.vertices();
        if (points.empty())
            return {};

        double twiceArea = 0.0;
        double xSum = 0.0;
        double ySum = 0.0;
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const AcGePoint2d &a = points[i];
            const AcGePoint2d &b = points[(i + 1) % points.size()];
            const double cross = a.x * b.y - b.x * a.y;
            twiceArea += cross;
            xSum += (a.x + b.x) * cross;
            ySum += (a.y + b.y) * cross;
        }
        if (std::abs(twiceArea) <=
            std::abs(tol.equalPoint()) * std::abs(tol.equalPoint()))
        {
            AcGeVector2d average;
            for (const AcGePoint2d &point : points)
                average += point.asVector();
            return {average.x / double(points.size()),
                    average.y / double(points.size())};
        }
        return {xSum / (3.0 * twiceArea), ySum / (3.0 * twiceArea)};
    }

    bool contains(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const
    {
        const auto &points = m_boundary.vertices();
        if (points.size() < 3)
            return false;
        const double pointTolerance = std::abs(tol.equalPoint());
        if (area() <= pointTolerance * pointTolerance)
            return false;
        double parameter = 0.0;
        if (m_boundary.isOn(point, parameter, pointTolerance))
            return true;

        bool inside = false;
        for (std::size_t i = 0, j = points.size() - 1; i < points.size();
             j = i++)
        {
            const AcGePoint2d &a = points[i];
            const AcGePoint2d &b = points[j];
            if ((a.y > point.y) != (b.y > point.y) &&
                point.x <
                    (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                inside = !inside;
        }
        return inside;
    }
};

struct AcGeConvexHull2d
{
private:
    std::vector<AcGePoint2d> m_vertices;

public:
    AcGeConvexHull2d() = default;
    explicit AcGeConvexHull2d(const std::vector<AcGePoint2d> &points,
                              const AcGeTol &tol = AcGeTol{})
    {
        setPoints(points, tol);
    }

    AcGeConvexHull2d &setPoints(const std::vector<AcGePoint2d> &points,
                                const AcGeTol &tol = AcGeTol{})
    {
        const double pointTolerance = std::abs(tol.equalPoint());
        if (!std::isfinite(pointTolerance))
            throw std::invalid_argument(
                "AcGeConvexHull2d tolerance must be finite.");

        std::vector<AcGePoint2d> sorted = points;
        for (const AcGePoint2d &point : sorted)
        {
            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                throw std::invalid_argument(
                    "AcGeConvexHull2d points must have finite coordinates.");
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const AcGePoint2d &a, const AcGePoint2d &b) {
                      return a.x < b.x || (a.x == b.x && a.y < b.y);
                  });

        std::vector<AcGePoint2d> unique;
        unique.reserve(sorted.size());
        for (const AcGePoint2d &point : sorted)
        {
            if (unique.empty() ||
                point.distanceTo(unique.back()) > pointTolerance)
                unique.push_back(point);
        }
        if (unique.size() <= 2)
        {
            m_vertices = std::move(unique);
            return *this;
        }

        std::vector<AcGePoint2d> lower;
        std::vector<AcGePoint2d> upper;
        lower.reserve(unique.size());
        upper.reserve(unique.size());
        for (const AcGePoint2d &point : unique)
        {
            while (lower.size() >= 2 &&
                   ge::orient2d(lower[lower.size() - 2], lower.back(),
                                point) <= 0.0)
                lower.pop_back();
            lower.push_back(point);
        }
        for (auto point = unique.rbegin(); point != unique.rend(); ++point)
        {
            while (upper.size() >= 2 &&
                   ge::orient2d(upper[upper.size() - 2], upper.back(),
                                *point) <= 0.0)
                upper.pop_back();
            upper.push_back(*point);
        }
        lower.pop_back();
        upper.pop_back();
        lower.insert(lower.end(), upper.begin(), upper.end());
        m_vertices = std::move(lower);
        return *this;
    }

    std::size_t numVerts() const { return m_vertices.size(); }
    bool isEmpty() const { return m_vertices.empty(); }
    const std::vector<AcGePoint2d> &vertices() const { return m_vertices; }

    AcGePolygon2d polygon() const { return AcGePolygon2d(m_vertices); }
    double area() const { return polygon().area(); }
    bool contains(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const
    {
        return polygon().contains(point, tol);
    }
};

struct AcGePolyline3d : AcGePolylineBase<AcGePoint3d, AcGeVector3d>
{
    using AcGePolylineBase::AcGePolylineBase;
};

struct AcGePolygon3d
{
private:
    std::vector<AcGePoint3d> m_vertices;

    AcGeVector3d areaVector() const
    {
        AcGeVector3d result;
        if (m_vertices.size() < 3)
            return result;
        const AcGePoint3d &origin = m_vertices.front();
        for (std::size_t i = 0; i < m_vertices.size(); ++i)
        {
            const AcGeVector3d a = m_vertices[i] - origin;
            const AcGeVector3d b =
                m_vertices[(i + 1) % m_vertices.size()] - origin;
            result += a.crossProduct(b);
        }
        return result;
    }

    AcGePolygon2d projected(const AcGeVector3d &unitNormal) const
    {
        AcGePolygon2d result;
        if (m_vertices.empty())
            return result;
        const AcGeVector3d u = unitNormal.perpVector().normal();
        const AcGeVector3d v = unitNormal.crossProduct(u);
        const AcGePoint3d &origin = m_vertices.front();
        for (const AcGePoint3d &point : m_vertices)
        {
            const AcGeVector3d offset = point - origin;
            result.appendVertex({offset.dotProduct(u), offset.dotProduct(v)});
        }
        return result;
    }

public:
    AcGePolygon3d() = default;
    explicit AcGePolygon3d(const std::vector<AcGePoint3d> &vertices)
        : m_vertices(vertices)
    {
    }
    AcGePolygon3d(std::initializer_list<AcGePoint3d> vertices)
        : m_vertices(vertices)
    {
    }

    AcGePolygon3d &setVertices(const std::vector<AcGePoint3d> &vertices)
    {
        m_vertices = vertices;
        return *this;
    }
    AcGePolygon3d &appendVertex(const AcGePoint3d &point)
    {
        m_vertices.push_back(point);
        return *this;
    }
    std::size_t numVerts() const { return m_vertices.size(); }
    const std::vector<AcGePoint3d> &vertices() const { return m_vertices; }

    AcGeVector3d normal() const { return areaVector().normal(); }
    double area() const { return areaVector().length() * 0.5; }

    double perimeter() const
    {
        if (m_vertices.size() < 2)
            return 0.0;
        double result = 0.0;
        for (std::size_t i = 0; i < m_vertices.size(); ++i)
            result += m_vertices[i].distanceTo(
                m_vertices[(i + 1) % m_vertices.size()]);
        return result;
    }

    bool isPlanar(const AcGeTol &tol = AcGeTol{}) const
    {
        if (m_vertices.size() < 3)
            return false;
        const AcGeVector3d unitNormal = normal();
        if (unitNormal.isZeroLength(tol))
            return false;
        const AcGePoint3d &origin = m_vertices.front();
        for (const AcGePoint3d &point : m_vertices)
            if (std::abs((point - origin).dotProduct(unitNormal)) >
                std::abs(tol.equalPoint()))
                return false;
        return true;
    }

    AcGePoint3d centroid(const AcGeTol &tol = AcGeTol{}) const
    {
        if (m_vertices.empty())
            return {};
        const AcGeVector3d unitNormal = normal();
        if (unitNormal.isZeroLength(tol) || area() <=
                                                   std::abs(tol.equalPoint()) *
                                                       std::abs(tol.equalPoint()))
        {
            AcGeVector3d average;
            for (const AcGePoint3d &point : m_vertices)
                average += point.asVector();
            return {average.x / double(m_vertices.size()),
                    average.y / double(m_vertices.size()),
                    average.z / double(m_vertices.size())};
        }
        const AcGePoint2d projectedCenter = projected(unitNormal).centroid(tol);
        const AcGeVector3d u = unitNormal.perpVector().normal();
        const AcGeVector3d v = unitNormal.crossProduct(u);
        return m_vertices.front() + u * projectedCenter.x +
               v * projectedCenter.y;
    }

    bool contains(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const
    {
        if (m_vertices.size() < 3 || !isPlanar(tol))
            return false;
        const AcGeVector3d unitNormal = normal();
        const AcGePoint3d &origin = m_vertices.front();
        const AcGeVector3d offset = point - origin;
        if (std::abs(offset.dotProduct(unitNormal)) >
            std::abs(tol.equalPoint()))
            return false;
        const AcGeVector3d u = unitNormal.perpVector().normal();
        const AcGeVector3d v = unitNormal.crossProduct(u);
        return projected(unitNormal).contains(
            {offset.dotProduct(u), offset.dotProduct(v)}, tol);
    }
};
