#include "acge/AcGePolyline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// AcGePolyline2d
// ---------------------------------------------------------------------------

double AcGePolyline2d::signedArea() const
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

// ---------------------------------------------------------------------------
// AcGePolygon2d
// ---------------------------------------------------------------------------

AcGePoint2d AcGePolygon2d::centroid(const AcGeTol &tol) const
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

bool AcGePolygon2d::contains(const AcGePoint2d &point,
                             const AcGeTol &tol) const
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

// ---------------------------------------------------------------------------
// AcGeConvexHull2d
// ---------------------------------------------------------------------------

AcGeConvexHull2d &AcGeConvexHull2d::setPoints(
    const std::vector<AcGePoint2d> &points, const AcGeTol &tol)
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

// ---------------------------------------------------------------------------
// AcGePolygon3d
// ---------------------------------------------------------------------------

AcGeVector3d AcGePolygon3d::areaVector() const
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

AcGePolygon2d AcGePolygon3d::projected(const AcGeVector3d &unitNormal) const
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

double AcGePolygon3d::perimeter() const
{
    if (m_vertices.size() < 2)
        return 0.0;
    double result = 0.0;
    for (std::size_t i = 0; i < m_vertices.size(); ++i)
        result += m_vertices[i].distanceTo(
            m_vertices[(i + 1) % m_vertices.size()]);
    return result;
}

bool AcGePolygon3d::isPlanar(const AcGeTol &tol) const
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

AcGePoint3d AcGePolygon3d::centroid(const AcGeTol &tol) const
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

bool AcGePolygon3d::contains(const AcGePoint3d &point,
                             const AcGeTol &tol) const
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
