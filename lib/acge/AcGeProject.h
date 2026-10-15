#pragma once

#include "AcGePort.h"

// Point-to-curve and point-to-surface projection for the AcGe layer,
// following "The NURBS Book" 2nd edition chapter 6 (Newton iteration on
// the distance function with multiple seeds against local minima).
// Reimplemented from the book's methods; the LNLib reference in external/
// (LGPL) is only consulted for structure.

#include <cmath>
#include <optional>
#include <vector>

#include "AcGeBspline.h"
#include "AcGePoint.h"

namespace ge
{

struct CurveProjection
{
    double parameter = 0.0;
    AcGePoint3d point;
    double distance = 0.0;
};

struct SurfaceProjection
{
    double parameterU = 0.0;
    double parameterV = 0.0;
    AcGePoint3d point;
    double distance = 0.0;
};

namespace detail
{

// Newton iteration for the closest point on a curve (NURBS Book eq. 6.1):
//   t <- t - C'(t) . (C(t) - P) / (C''(t) . (C(t) - P) + C'(t) . C'(t))
template <typename CurveT>
std::optional<double> projectOnCurveNewton(const CurveT &curve,
                                           const AcGePoint3d &point,
                                           double seed)
{
    double t = seed;
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        const auto derivatives = curve.derivativesAt(t, 2);
        const AcGeVector3d first = derivatives[1];
        const AcGeVector3d second = derivatives[2];
        const AcGeVector3d difference = curve.pointAt(t) - point;
        const double numerator = first.dotProduct(difference);
        const double denominator =
            second.dotProduct(difference) + first.dotProduct(first);
        if (std::abs(denominator) < 1.0e-14)
            break;
        const double delta = numerator / denominator;
        t -= delta;
        t = std::clamp(t, curve.minParameter(), curve.maxParameter());
        if (std::abs(delta) < 1.0e-12)
            break;
    }
    return t;
}

} // namespace detail

// Closest point on a B-spline curve.  Multi-seed Newton against the local
// minima that curved segments produce.
template <typename CurveT>
std::optional<CurveProjection> projectPointOnCurve(
    const CurveT &curve, const AcGePoint3d &point,
    double tolerance = 1.0e-9)
{
    constexpr int kSeedCount = 33;
    const double span = curve.maxParameter() - curve.minParameter();
    std::optional<CurveProjection> best;

    for (int i = 0; i < kSeedCount; ++i)
    {
        const double seed =
            curve.minParameter() + span * double(i) / (kSeedCount - 1);
        const std::optional<double> t =
            detail::projectOnCurveNewton(curve, point, seed);
        if (!t)
            continue;
        const AcGePoint3d closest = curve.pointAt(*t);
        const double distance = (closest - point).length();
        if (distance <= tolerance)
            return CurveProjection{*t, closest, distance};
        if (!best || distance < best->distance)
            best = CurveProjection{*t, closest, distance};
    }
    return best;
}

// Closest point on a NURBS surface (NURBS Book eq. 6.2-6.3): Newton on the
// 2x2 system
//   [S_u.S_u  S_u.S_v] [du]   [-S_u.r]
//   [S_v.S_u  S_v.S_v] [dv] = [-S_v.r]
// with r = S(u, v) - P, clamped to the parameter domain each step.
std::optional<SurfaceProjection> projectPointOnSurface(
    const AcGeNurbsSurface3d &surface, const AcGePoint3d &point,
    double tolerance = 1.0e-9);

} // namespace ge
