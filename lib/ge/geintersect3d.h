#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "gecurve3d.h"
#include "gematrix.h"
#include "gelinear.h"
#include "gesurface3d.h"

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
