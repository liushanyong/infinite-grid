#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "gecurve3d.h"
#include "gematrix.h"
#include "gelinear.h"
#include "gesurface3d.h"
#include "getriangle.h"

namespace ge
{
namespace detail
{

inline bool parameterInRange(double parameter, double minimum, double maximum,
                             double tolerance)
{
    return parameter >= minimum - tolerance &&
           parameter <= maximum + tolerance;
}

inline std::vector<AcGePoint3d> intersectParametricPlane(
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

inline std::vector<AcGePoint3d> intersectParametricSphere(
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

inline std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                         const AcGePlane &plane,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricPlane(
        line.basePoint, line.direction, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), plane, tol);
}

inline std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                         const AcGePlane &plane,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricPlane(
        ray.basePoint, ray.direction, 0.0,
        std::numeric_limits<double>::infinity(), plane, tol);
}

inline std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                         const AcGePlane &plane,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricPlane(
        segment.startPoint(), segment.direction(), 0.0, 1.0, plane, tol);
}

inline std::optional<AcGeLine3d> intersect(const AcGePlane &first,
                                          const AcGePlane &second,
                                          const AcGeTol &tol = AcGeTol{})
{
    const AcGeVector3d direction =
        first.normal.crossProduct(second.normal);
    const double directionLengthSquared = direction.lengthSq();
    const double parallelTolerance = std::abs(tol.equalVector()) *
                                     first.normal.length() *
                                     second.normal.length();
    if (directionLengthSquared <= parallelTolerance * parallelTolerance)
        return std::nullopt;

    const double firstOffset = first.normal.dotProduct(first.origin);
    const double secondOffset = second.normal.dotProduct(second.origin);
    const AcGeVector3d pointVector =
        (second.normal.crossProduct(direction) * firstOffset +
         direction.crossProduct(first.normal) * secondOffset) /
        directionLengthSquared;
    const AcGePoint3d point{pointVector.x, pointVector.y, pointVector.z};
    return AcGeLine3d{point, direction};
}

inline std::optional<AcGePoint3d> intersect(const AcGeLine3d &first,
                                          const AcGeLine3d &second,
                                          const AcGeTol &tol = AcGeTol{})
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

inline std::optional<AcGePoint3d> intersect(
    const AcGeLineSeg3d &segment, const AcGeTriangle3d &triangle,
    const AcGeTol &tol = AcGeTol{})
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
    const double parameterTolerance =
        pointTolerance / std::max(direction.length(), pointTolerance);
    if (u < -parameterTolerance || v < -parameterTolerance ||
        u + v > 1.0 + parameterTolerance ||
        parameter < -parameterTolerance ||
        parameter > 1.0 + parameterTolerance)
        return std::nullopt;

    return segment.pointAt(std::clamp(parameter, 0.0, 1.0));
}

inline std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                         const AcGeSphere &sphere,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricSphere(
        line.basePoint, line.direction,
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), sphere, tol);
}

inline std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                         const AcGeSphere &sphere,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricSphere(
        ray.basePoint, ray.direction, 0.0,
        std::numeric_limits<double>::infinity(), sphere, tol);
}

inline std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                         const AcGeSphere &sphere,
                                         const AcGeTol &tol = AcGeTol{})
{
    return detail::intersectParametricSphere(
        segment.startPoint(), segment.direction(), 0.0, 1.0, sphere, tol);
}

} // namespace ge
