#include "acge/AcGeOffset.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace ge::detail
{

std::vector<AcGePoint2d> offsetPolylineVertices(
    const std::vector<AcGePoint2d> &vertices, bool closed, double distance,
    const AcGeTol &tol)
{
    const double pointTolerance = std::abs(tol.equalPoint());
    const double vectorTolerance = std::abs(tol.equalVector());
    if (!std::isfinite(distance) || !std::isfinite(pointTolerance) ||
        !std::isfinite(vectorTolerance))
        throw std::invalid_argument(
            "AcGe offset distance and tolerances must be finite.");

    if (vertices.empty())
        return {};
    if (vertices.size() < 2)
        throw std::invalid_argument(
            "An offset polyline requires at least two vertices.");

    const std::size_t segmentCount = closed ? vertices.size()
                                             : vertices.size() - 1;
    std::vector<AcGeVector2d> directions;
    directions.reserve(segmentCount);
    for (std::size_t i = 0; i < segmentCount; ++i)
    {
        const AcGePoint2d &start = vertices[i];
        const AcGePoint2d &end = vertices[(i + 1) % vertices.size()];
        if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
            !std::isfinite(end.x) || !std::isfinite(end.y))
            throw std::invalid_argument(
                "AcGe offset polyline vertices must be finite.");

        const AcGeVector2d direction = end - start;
        const double directionLength =
            std::hypot(direction.x, direction.y);
        if (!std::isfinite(directionLength) ||
            directionLength <= vectorTolerance)
            throw std::invalid_argument(
                "AcGe offset polyline cannot contain zero-length segments.");
        directions.push_back(direction / directionLength);
    }

    std::vector<AcGePoint2d> result;
    result.reserve(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        const bool hasPrevious = closed || i > 0;
        const bool hasNext = closed || i + 1 < vertices.size();
        const AcGeVector2d previous =
            hasPrevious ? directions[(i + segmentCount - 1) % segmentCount]
                        : AcGeVector2d{};
        const AcGeVector2d next =
            hasNext ? directions[i % segmentCount] : AcGeVector2d{};
        const AcGeVector2d previousNormal{-previous.y, previous.x};
        const AcGeVector2d nextNormal{-next.y, next.x};

        if (!hasPrevious)
        {
            result.push_back(vertices[i] + nextNormal * distance);
            continue;
        }
        if (!hasNext)
        {
            result.push_back(vertices[i] + previousNormal * distance);
            continue;
        }

        const double denominator =
            previous.x * next.y - previous.y * next.x;
        const double alignment = previous.dotProduct(next);
        if (std::abs(denominator) <= vectorTolerance)
        {
            if (alignment < 0.0)
                throw std::invalid_argument(
                    "A reversing polyline vertex has no unique miter offset.");
            const AcGeVector2d averageNormal =
                (previousNormal + nextNormal).normal();
            result.push_back(vertices[i] + averageNormal * distance);
            continue;
        }

        const AcGePoint2d previousOffset =
            vertices[i] + previousNormal * distance;
        const AcGePoint2d nextOffset =
            vertices[i] + nextNormal * distance;
        const AcGeVector2d between = nextOffset - previousOffset;
        const double parameter =
            (between.x * next.y - between.y * next.x) / denominator;
        const AcGePoint2d miter = previousOffset + previous * parameter;
        if (!std::isfinite(miter.x) || !std::isfinite(miter.y))
            throw std::overflow_error("AcGe offset miter is not finite.");
        result.push_back(miter);
    }
    return result;
}

} // namespace ge::detail

AcGeOffsetPolyline2d &AcGeOffsetPolyline2d::set(
    const AcGePolyline2d &polyline, double distance, const AcGeTol &tol)
{
    const auto vertices = ge::detail::offsetPolylineVertices(
        polyline.vertices(), polyline.isClosed(), distance, tol);
    m_polyline = AcGePolyline2d(vertices, polyline.isClosed());
    m_distance = distance;
    return *this;
}

AcGeOffsetPolyline3d &AcGeOffsetPolyline3d::set(
    const AcGePolyline3d &polyline, const AcGeVector3d &normal,
    double distance, const AcGeTol &tol)
{
    const double pointTolerance = std::abs(tol.equalPoint());
    const double vectorTolerance = std::abs(tol.equalVector());
    if (!std::isfinite(pointTolerance) ||
        !std::isfinite(vectorTolerance) || !std::isfinite(distance))
        throw std::invalid_argument(
            "AcGe offset distance and tolerances must be finite.");
    const double normalLength =
        std::hypot(normal.x, normal.y, normal.z);
    if (!std::isfinite(normalLength) || normalLength <= vectorTolerance)
        throw std::invalid_argument(
            "AcGe 3D offset polyline requires a nonzero plane normal.");
    if (polyline.isEmpty())
    {
        m_polyline.clear();
        m_distance = distance;
        m_normal = normal / normalLength;
        return *this;
    }
    if (polyline.numVerts() < 2)
        throw std::invalid_argument(
            "An offset polyline requires at least two vertices.");

    const AcGeVector3d unitNormal = normal / normalLength;
    const AcGePoint3d origin = polyline.vertexAt(0);
    const AcGeVector3d u = unitNormal.perpVector().normal();
    const AcGeVector3d v = unitNormal.crossProduct(u);
    std::vector<AcGePoint2d> projected;
    projected.reserve(polyline.numVerts());
    for (const AcGePoint3d &point : polyline.vertices())
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z))
            throw std::invalid_argument(
                "AcGe offset polyline vertices must be finite.");
        const AcGeVector3d relative = point - origin;
        if (std::abs(relative.dotProduct(unitNormal)) > pointTolerance)
            throw std::invalid_argument(
                "AcGe 3D offset polyline vertices must be coplanar.");
        projected.emplace_back(relative.dotProduct(u),
                               relative.dotProduct(v));
    }

    const auto offset = ge::detail::offsetPolylineVertices(
        projected, polyline.isClosed(), distance, tol);
    std::vector<AcGePoint3d> result;
    result.reserve(offset.size());
    for (const AcGePoint2d &point : offset)
        result.push_back(origin + u * point.x + v * point.y);

    m_polyline = AcGePolyline3d(result, polyline.isClosed());
    m_distance = distance;
    m_normal = unitNormal;
    return *this;
}
