#include "acgs/AcGsPicking.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace acgs
{

AcGsPickRay makePickRay(double ndcX, double ndcY, const AcGsPickView &view)
{
    const double width = std::max(1, view.viewportWidth);
    const double height = std::max(1, view.viewportHeight);
    const double aspect = width / height;
    if (view.orthographic)
    {
        const double halfHeight = std::max(0.0, view.orthoHalfHeight);
        const double halfWidth = halfHeight * aspect;
        return {
            view.position +
                view.right * (ndcX * halfWidth) +
                view.up * (ndcY * halfHeight),
            view.front.normal()};
    }

    const double tangent = std::tan(view.verticalFovRadians * 0.5);
    return {
        view.position,
        (view.front + view.right * (ndcX * tangent * aspect) +
         view.up * (ndcY * tangent)).normal()};
}

double pickMinDepth(const AcGsPickView &view)
{
    return view.orthographic ? view.nearDepth : 0.0;
}

double pickWorldPerPixel(const AcGsPickRay &ray,
                         const AcGePoint3d &worldPoint,
                         const AcGsPickView &view)
{
    const double height = double(std::max(1, view.viewportHeight));
    if (view.orthographic)
        return 2.0 * view.orthoHalfHeight / height;

    const double depth = std::max(
        0.0, (worldPoint - ray.origin).dotProduct(view.front));
    return 2.0 * depth * std::tan(view.verticalFovRadians * 0.5) / height;
}

double pickCursorTolerance(const AcGsPickRay &ray,
                           const AcGePoint3d &worldPoint,
                           const AcGsPickView &view,
                           double pixelRadius)
{
    return pickWorldPerPixel(ray, worldPoint, view) * pixelRadius;
}

bool rayIntersectsAabb(const AcGsPickRay &ray,
                       const AcGePoint3d &minimum,
                       const AcGePoint3d &maximum,
                       double nearDepth, double farDepth,
                       double &hitDepth)
{
    const double origin[] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const double direction[] = {
        ray.direction.x, ray.direction.y, ray.direction.z};
    const double minimumValues[] = {minimum.x, minimum.y, minimum.z};
    const double maximumValues[] = {maximum.x, maximum.y, maximum.z};
    double enter = -std::numeric_limits<double>::infinity();
    double exit = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(direction[axis]) < 1.0e-20)
        {
            if (origin[axis] < minimumValues[axis] ||
                origin[axis] > maximumValues[axis])
                return false;
            continue;
        }
        const double inverse = 1.0 / direction[axis];
        const double first = (minimumValues[axis] - origin[axis]) * inverse;
        const double second = (maximumValues[axis] - origin[axis]) * inverse;
        enter = std::max(enter, std::min(first, second));
        exit = std::min(exit, std::max(first, second));
    }

    const double clippedEnter = std::max(enter, nearDepth);
    if (exit < clippedEnter || clippedEnter > farDepth)
        return false;
    hitDepth = clippedEnter;
    return true;
}

namespace
{

double raySegmentClosestRayParameter(const AcGsPickRay &ray,
                                     const AcGePoint3d &start,
                                     const AcGePoint3d &end,
                                     double &segmentParameter,
                                     double depthNear, double depthFar,
                                     bool semiInfiniteRay,
                                     bool orthographic)
{
    const AcGeVector3d segment = end - start;
    const double segmentLengthSquared = segment.lengthSq();
    if (segmentLengthSquared < 1.0e-24)
    {
        segmentParameter = 0.0;
        return std::numeric_limits<double>::quiet_NaN();
    }

    const AcGeVector3d originToStart = ray.origin - start;
    const double uu = ray.direction.lengthSq();
    if (uu < 1.0e-24)
        return std::numeric_limits<double>::quiet_NaN();
    const double uv = ray.direction.dotProduct(segment);
    const double vv = segmentLengthSquared;
    const double wd = originToStart.dotProduct(ray.direction);
    const double we = originToStart.dotProduct(segment);
    const double denominator = uu * vv - uv * uv;
    const auto clampSegmentParameter = [semiInfiniteRay](double value) {
        return semiInfiniteRay ? std::max(0.0, value)
                               : std::clamp(value, 0.0, 1.0);
    };

    double segmentParameterValue;
    if (denominator > std::max(1.0e-24, vv * 1.0e-14))
        segmentParameterValue = (uu * we - uv * wd) / denominator;
    else
        segmentParameterValue = we / vv;
    segmentParameterValue = clampSegmentParameter(segmentParameterValue);

    double rayParameter = (uv * segmentParameterValue - wd) / uu;
    if (!semiInfiniteRay || !orthographic)
        rayParameter = std::clamp(rayParameter, depthNear, depthFar);
    segmentParameterValue =
        denominator > std::max(1.0e-24, vv * 1.0e-14)
            ? clampSegmentParameter((uv * rayParameter + we) / vv)
            : clampSegmentParameter(we / vv);
    segmentParameter = segmentParameterValue;
    return rayParameter;
}

} // namespace

bool rayIntersectsSegment(const AcGsPickRay &ray,
                          const AcGePoint3d &start,
                          const AcGePoint3d &end, double tolerance,
                          double depthNear, double depthFar,
                          bool semiInfiniteRay, bool orthographic,
                          double &hitDepth)
{
    return rayIntersectsSegmentWithTolerance(
        ray, start, end, [tolerance](double) { return tolerance; },
        hitDepth, depthNear, depthFar, semiInfiniteRay, orthographic);
}

bool rayIntersectsSegmentWithTolerance(
    const AcGsPickRay &ray, const AcGePoint3d &start,
    const AcGePoint3d &end,
    const std::function<double(double rayDepth)> &toleranceAtDepth,
    double &hitDepth, double depthNear, double depthFar,
    bool semiInfiniteRay, bool orthographic)
{
    double segmentParameter = 0.0;
    const double rayParameter = raySegmentClosestRayParameter(
        ray, start, end, segmentParameter, depthNear, depthFar,
        semiInfiniteRay, orthographic);
    if (!std::isfinite(rayParameter))
        return false;

    const AcGePoint3d rayPoint = ray.origin + ray.direction * rayParameter;
    const AcGePoint3d segmentPoint =
        start + (end - start) * segmentParameter;
    if (rayPoint.distanceTo(segmentPoint) > toleranceAtDepth(rayParameter))
        return false;

    hitDepth = rayParameter;
    return true;
}

bool rayIntersectsTriangle(const AcGsPickRay &ray,
                           const AcGePoint3d &a, const AcGePoint3d &b,
                           const AcGePoint3d &c, double depthNear,
                           double depthFar, double &hitDepth)
{
    const AcGeVector3d edge1 = b - a;
    const AcGeVector3d edge2 = c - a;
    const AcGeVector3d pvec = ray.direction.crossProduct(edge2);
    const double determinant = edge1.dotProduct(pvec);
    if (std::abs(determinant) < 1.0e-24)
        return false;

    const double inverseDeterminant = 1.0 / determinant;
    const AcGeVector3d tvec = ray.origin - a;
    const double u = tvec.dotProduct(pvec) * inverseDeterminant;
    if (u < 0.0 || u > 1.0)
        return false;

    const AcGeVector3d qvec = tvec.crossProduct(edge1);
    const double v = ray.direction.dotProduct(qvec) * inverseDeterminant;
    if (v < 0.0 || u + v > 1.0)
        return false;

    const double depth = edge2.dotProduct(qvec) * inverseDeterminant;
    if (depth < depthNear || depth > depthFar)
        return false;
    hitDepth = depth;
    return true;
}

bool rayIntersectsPoint(const AcGsPickRay &ray,
                        const AcGePoint3d &location, double tolerance,
                        double depthNear, double depthFar,
                        double &hitDepth)
{
    const double depth =
        (location - ray.origin).dotProduct(ray.direction);
    if (depth < depthNear || depth > depthFar)
        return false;
    const AcGePoint3d rayPoint = ray.origin + ray.direction * depth;
    if (rayPoint.distanceTo(location) > tolerance)
        return false;
    hitDepth = depth;
    return true;
}

} // namespace acgs
