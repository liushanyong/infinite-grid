#include "acge/AcGeIntersect3d.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace ge
{
namespace detail
{

std::vector<AcGePoint3d> intersectParametricPlane(
    const AcGePoint3d &base, const AcGeVector3d &direction,
    double minParameter, double maxParameter, const AcGePlane &plane,
    const AcGeTol &tol)
{
    std::vector<AcGePoint3d> result;
    const double directionLength = direction.length();
    const double normalLength = plane.normal.length();
    if (directionLength <= std::abs(tol.equalVector()) ||
        normalLength <= std::abs(tol.equalVector()))
        return result;

    const double denominator = plane.normal.dotProduct(direction);
    if (std::abs(denominator) <=
        std::abs(tol.equalVector()) * normalLength * directionLength)
        return result;

    double parameter =
        plane.normal.dotProduct(plane.origin - base) / denominator;
    const double parameterTolerance =
        std::abs(tol.equalPoint()) / directionLength;
    if (!parameterInRange(parameter, minParameter, maxParameter,
                          parameterTolerance))
        return result;
    parameter = std::clamp(parameter, minParameter, maxParameter);
    result.push_back(base + direction * parameter);
    return result;
}

std::vector<AcGePoint3d> intersectParametricSphere(
    const AcGePoint3d &base, const AcGeVector3d &direction,
    double minParameter, double maxParameter, const AcGeSphere &sphere,
    const AcGeTol &tol)
{
    std::vector<AcGePoint3d> result;
    const double a = direction.lengthSq();
    if (a <= std::pow(std::abs(tol.equalVector()), 2.0))
        return result;

    const AcGeVector3d offset = base - sphere.center();
    const double b = 2.0 * offset.dotProduct(direction);
    const double c = offset.lengthSq() - sphere.radius() * sphere.radius();
    double discriminant = b * b - 4.0 * a * c;
    const double discriminantTolerance =
        std::abs(tol.equalPoint()) *
        std::max({1.0, b * b, std::abs(4.0 * a * c)});
    if (discriminant < -discriminantTolerance)
        return result;

    if (discriminant < 0.0)
        discriminant = 0.0;
    const double root = std::sqrt(discriminant);
    const double parameterTolerance =
        std::abs(tol.equalPoint()) / std::sqrt(a);
    const double parameters[2] = {(-b - root) / (2.0 * a),
                                  (-b + root) / (2.0 * a)};
    for (int i = 0; i < (root == 0.0 ? 1 : 2); ++i)
    {
        double parameter = parameters[i];
        if (!parameterInRange(parameter, minParameter, maxParameter,
                              parameterTolerance))
            continue;
        parameter = std::clamp(parameter, minParameter, maxParameter);
        const AcGePoint3d point = base + direction * parameter;
        if (result.empty() || !point.isEqualTo(result.back(), tol))
            result.push_back(point);
    }
    return result;
}

} // namespace detail

namespace
{

struct ParametricLine3d
{
    AcGePoint3d origin;
    AcGeVector3d direction;
    double minimum;
    double maximum;
};

AcGeClosestPoints3d closestPointsParametric(
    const ParametricLine3d &first, const ParametricLine3d &second,
    const AcGeTol &tol)
{
    const AcGeVector3d offset = first.origin - second.origin;
    const double a = first.direction.lengthSq();
    const double b = first.direction.dotProduct(second.direction);
    const double c = second.direction.lengthSq();
    const double d = first.direction.dotProduct(offset);
    const double e = second.direction.dotProduct(offset);
    const double parameterTolerance =
        std::abs(tol.equalPoint()) /
        std::max({first.direction.length(), second.direction.length(),
                  std::abs(tol.equalPoint())});
    const double directionToleranceSquared =
        std::pow(std::abs(tol.equalVector()), 2.0);

    AcGeClosestPoints3d best;
    best.distance = std::numeric_limits<double>::infinity();
    const auto consider = [&](double parameterA, double parameterB)
    {
        if (parameterA < first.minimum - parameterTolerance ||
            parameterA > first.maximum + parameterTolerance ||
            parameterB < second.minimum - parameterTolerance ||
            parameterB > second.maximum + parameterTolerance)
            return;

        parameterA = std::clamp(parameterA, first.minimum, first.maximum);
        parameterB = std::clamp(parameterB, second.minimum, second.maximum);
        const AcGePoint3d pointA =
            first.origin + first.direction * parameterA;
        const AcGePoint3d pointB =
            second.origin + second.direction * parameterB;
        const double distance = pointA.distanceTo(pointB);
        if (distance < best.distance)
            best = {parameterA, parameterB, pointA, pointB, distance};
    };

    const auto optimizeSecond = [&](double parameterA)
    {
        const double parameterB =
            c <= directionToleranceSquared
                ? 0.0
                : std::clamp((e + b * parameterA) / c, second.minimum,
                             second.maximum);
        consider(parameterA, parameterB);
    };
    const auto optimizeFirst = [&](double parameterB)
    {
        const double parameterA =
            a <= directionToleranceSquared
                ? 0.0
                : std::clamp((b * parameterB - d) / a, first.minimum,
                             first.maximum);
        consider(parameterA, parameterB);
    };

    // The unconstrained minimizer handles skew lines and interior ray hits.
    const double denominator = a * c - b * b;
    const double parallelTolerance =
        directionToleranceSquared * a * c;
    if (a > directionToleranceSquared && c > directionToleranceSquared &&
        denominator > parallelTolerance)
    {
        const double parameterA = (b * e - c * d) / denominator;
        const double parameterB = (a * e - b * d) / denominator;
        consider(parameterA, parameterB);
    }

    // Zero and finite interval endpoints cover constrained minima, including
    // parallel rays and zero-direction entities.
    optimizeSecond(0.0);
    optimizeFirst(0.0);
    if (std::isfinite(first.minimum))
        optimizeSecond(first.minimum);
    if (std::isfinite(first.maximum) && first.maximum != first.minimum)
        optimizeSecond(first.maximum);
    if (std::isfinite(second.minimum))
        optimizeFirst(second.minimum);
    if (std::isfinite(second.maximum) && second.maximum != second.minimum)
        optimizeFirst(second.maximum);

    return best;
}

ParametricLine3d asParametricLine(const AcGeLine3d &line)
{
    return {line.basePoint, line.direction,
            -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()};
}

ParametricLine3d asParametricRay(const AcGeRay3d &ray)
{
    return {ray.basePoint, ray.direction, 0.0,
            std::numeric_limits<double>::infinity()};
}

} // namespace

std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                   const AcGePlane &plane, const AcGeTol &tol)
{
    return detail::intersectParametricPlane(
        line.basePoint, line.direction, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), plane, tol);
}

std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                   const AcGePlane &plane, const AcGeTol &tol)
{
    return detail::intersectParametricPlane(
        ray.basePoint, ray.direction, 0.0,
        std::numeric_limits<double>::infinity(), plane, tol);
}

std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                   const AcGePlane &plane, const AcGeTol &tol)
{
    return detail::intersectParametricPlane(
        segment.startPoint(), segment.direction(), 0.0, 1.0, plane, tol);
}

std::optional<AcGeLine3d> intersect(const AcGePlane &first,
                                    const AcGePlane &second, const AcGeTol &tol)
{
    const AcGeVector3d direction =
        first.normal.crossProduct(second.normal);
    const double directionLengthSquared = direction.lengthSq();
    const double parallelTolerance = std::abs(tol.equalVector()) *
                                     first.normal.length() *
                                     second.normal.length();
    if (directionLengthSquared <= parallelTolerance * parallelTolerance)
        return std::nullopt;

    const double firstOffset = first.normal.dotProduct(first.origin.asVector());
    const double secondOffset =
        second.normal.dotProduct(second.origin.asVector());
    const AcGeVector3d pointVector =
        (second.normal.crossProduct(direction) * firstOffset +
         direction.crossProduct(first.normal) * secondOffset) /
        directionLengthSquared;
    const AcGePoint3d point{pointVector.x, pointVector.y, pointVector.z};
    return AcGeLine3d{point, direction};
}

std::optional<AcGePoint3d> intersect(const AcGeLine3d &first,
                                     const AcGeLine3d &second,
                                     const AcGeTol &tol)
{
    const AcGeVector3d &u = first.direction;
    const AcGeVector3d &v = second.direction;
    const AcGeVector3d w = first.basePoint - second.basePoint;
    const double a = u.dotProduct(u);
    const double b = u.dotProduct(v);
    const double c = v.dotProduct(v);
    const double d = u.dotProduct(w);
    const double e = v.dotProduct(w);
    const double denominator = a * c - b * b;
    const double scale = a * c;
    if (a <= std::pow(std::abs(tol.equalVector()), 2.0) ||
        c <= std::pow(std::abs(tol.equalVector()), 2.0) ||
        denominator <= std::pow(std::abs(tol.equalVector()), 2.0) * scale)
        return std::nullopt;

    const double firstParameter = (b * e - c * d) / denominator;
    const double secondParameter = (a * e - b * d) / denominator;
    const AcGePoint3d firstPoint = first.pointAt(firstParameter);
    const AcGePoint3d secondPoint = second.pointAt(secondParameter);
    if (!firstPoint.isEqualTo(secondPoint, tol))
        return std::nullopt;
    return firstPoint + (secondPoint - firstPoint) * 0.5;
}

std::optional<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                     const AcGeTriangle3d &triangle,
                                     const AcGeTol &tol)
{
    const AcGeVector3d direction = segment.direction();
    const AcGeVector3d edge1 = triangle.b - triangle.a;
    const AcGeVector3d edge2 = triangle.c - triangle.a;
    const AcGeVector3d p = direction.crossProduct(edge2);
    const double determinant = edge1.dotProduct(p);
    const double determinantTolerance =
        std::abs(tol.equalVector()) * edge1.length() * edge2.length() *
        direction.length();
    if (std::abs(determinant) <= determinantTolerance)
        return std::nullopt;

    const double inverseDeterminant = 1.0 / determinant;
    const AcGeVector3d offset = segment.startPoint() - triangle.a;
    const double u = offset.dotProduct(p) * inverseDeterminant;
    const AcGeVector3d q = offset.crossProduct(edge1);
    const double v = direction.dotProduct(q) * inverseDeterminant;
    const double parameter = edge2.dotProduct(q) * inverseDeterminant;
    const double pointTolerance = std::abs(tol.equalPoint());
    const double barycentricTolerance =
        pointTolerance /
        std::max({edge1.length(), edge2.length(), pointTolerance});
    const double parameterTolerance =
        pointTolerance / std::max(direction.length(), pointTolerance);
    if (u < -barycentricTolerance || v < -barycentricTolerance ||
        u + v > 1.0 + barycentricTolerance ||
        parameter < -parameterTolerance ||
        parameter > 1.0 + parameterTolerance)
        return std::nullopt;

    return segment.pointAt(std::clamp(parameter, 0.0, 1.0));
}

std::optional<AcGeRayTriangleHit> intersectRayTriangle(
    const AcGeRay3d &ray, const AcGeTriangle3d &triangle, const AcGeTol &tol)
{
    const AcGeVector3d direction = ray.direction;
    const AcGeVector3d edge1 = triangle.b - triangle.a;
    const AcGeVector3d edge2 = triangle.c - triangle.a;
    const AcGeVector3d p = direction.crossProduct(edge2);
    const double determinant = edge1.dotProduct(p);
    const double directionLength = direction.length();
    const double edge1Length = edge1.length();
    const double edge2Length = edge2.length();
    const double determinantTolerance =
        std::abs(tol.equalVector()) * directionLength * edge1Length *
        edge2Length;
    if (directionLength <= std::abs(tol.equalVector()) ||
        edge1Length <= std::abs(tol.equalPoint()) ||
        edge2Length <= std::abs(tol.equalPoint()) ||
        std::abs(determinant) <= determinantTolerance)
        return std::nullopt;

    const double inverseDeterminant = 1.0 / determinant;
    const AcGeVector3d offset = ray.basePoint - triangle.a;
    const double u = offset.dotProduct(p) * inverseDeterminant;
    const AcGeVector3d q = offset.crossProduct(edge1);
    const double v = direction.dotProduct(q) * inverseDeterminant;
    const double parameter = edge2.dotProduct(q) * inverseDeterminant;
    const double pointTolerance = std::abs(tol.equalPoint());
    const double barycentricTolerance =
        pointTolerance /
        std::max({edge1Length, edge2Length, (triangle.c - triangle.b).length(),
                  pointTolerance});
    const double parameterTolerance =
        pointTolerance / std::max(directionLength, pointTolerance);
    if (u < -barycentricTolerance || v < -barycentricTolerance ||
        u + v > 1.0 + barycentricTolerance || parameter < -parameterTolerance)
        return std::nullopt;

    const double clampedParameter = std::max(0.0, parameter);
    return AcGeRayTriangleHit{
        clampedParameter, u, v, ray.pointAt(clampedParameter)};
}

std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol)
{
    return detail::intersectParametricSphere(
        line.basePoint, line.direction,
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), sphere, tol);
}

std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol)
{
    return detail::intersectParametricSphere(
        ray.basePoint, ray.direction, 0.0,
        std::numeric_limits<double>::infinity(), sphere, tol);
}

std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol)
{
    return detail::intersectParametricSphere(
        segment.startPoint(), segment.direction(), 0.0, 1.0, sphere, tol);
}

AcGeClosestPoints3d closestPoints(const AcGeLineSeg3d &first,
                                  const AcGeLineSeg3d &second,
                                  const AcGeTol &tol)
{
    const AcGePoint3d startA = first.startPoint();
    const AcGePoint3d startB = second.startPoint();
    const AcGeVector3d directionA = first.direction();
    const AcGeVector3d directionB = second.direction();
    const AcGeVector3d offset = startA - startB;
    const double a = directionA.dotProduct(directionA);
    const double b = directionA.dotProduct(directionB);
    const double c = directionB.dotProduct(directionB);
    const double d = directionA.dotProduct(offset);
    const double e = directionB.dotProduct(offset);
    const double denominator = a * c - b * b;
    const double parallelTolerance =
        std::abs(tol.equalVector()) * std::sqrt(a * c);

    double parameterA = 0.0;
    double parameterB = 0.0;
    const double vectorToleranceSquared =
        std::pow(std::abs(tol.equalVector()), 2.0);
    if (a <= vectorToleranceSquared)
    {
        parameterB = c <= vectorToleranceSquared
                         ? 0.0
                         : std::clamp(e / c, 0.0, 1.0);
    }
    else if (c <= vectorToleranceSquared)
    {
        parameterA = std::clamp(-d / a, 0.0, 1.0);
    }
    else
    {
        if (denominator > parallelTolerance * parallelTolerance)
            parameterA =
                std::clamp((b * e - c * d) / denominator, 0.0, 1.0);

        parameterB = (b * parameterA + e) / c;
        if (parameterB < 0.0)
        {
            parameterB = 0.0;
            parameterA = std::clamp(-d / a, 0.0, 1.0);
        }
        else if (parameterB > 1.0)
        {
            parameterB = 1.0;
            parameterA = std::clamp((b - d) / a, 0.0, 1.0);
        }
    }

    const AcGePoint3d pointA = first.pointAt(parameterA);
    const AcGePoint3d pointB = second.pointAt(parameterB);
    return {parameterA, parameterB, pointA, pointB, pointA.distanceTo(pointB)};
}

AcGeClosestPoints3d closestPoints(const AcGeLine3d &first,
                                  const AcGeLine3d &second,
                                  const AcGeTol &tol)
{
    return closestPointsParametric(asParametricLine(first),
                                   asParametricLine(second), tol);
}

AcGeClosestPoints3d closestPoints(const AcGeRay3d &first,
                                  const AcGeRay3d &second,
                                  const AcGeTol &tol)
{
    return closestPointsParametric(asParametricRay(first),
                                   asParametricRay(second), tol);
}

AcGeClosestPoints3d closestPoints(const AcGeLine3d &line,
                                  const AcGeRay3d &ray,
                                  const AcGeTol &tol)
{
    return closestPointsParametric(asParametricLine(line),
                                   asParametricRay(ray), tol);
}

AcGeClosestPoints3d closestPoints(const AcGeRay3d &ray,
                                  const AcGeLine3d &line,
                                  const AcGeTol &tol)
{
    return closestPointsParametric(asParametricRay(ray),
                                   asParametricLine(line), tol);
}

} // namespace ge
