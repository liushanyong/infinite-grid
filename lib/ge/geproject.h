#pragma once

// Point-to-curve and point-to-surface projection for the AcGe layer,
// following "The NURBS Book" 2nd edition chapter 6 (Newton iteration on
// the distance function with multiple seeds against local minima).
// Reimplemented from the book's methods; the LNLib reference in external/
// (LGPL) is only consulted for structure.

#include <cmath>
#include <optional>
#include <vector>

#include "gebspline.h"
#include "gepoint.h"

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
inline std::optional<SurfaceProjection> projectPointOnSurface(
    const AcGeNurbsSurface3d &surface, const AcGePoint3d &point,
    double tolerance = 1.0e-9)
{
    constexpr int kSeedCount = 9; // 9x9 grid of seeds against local minima
    const double spanU = surface.maxParameterU() - surface.minParameterU();
    const double spanV = surface.maxParameterV() - surface.minParameterV();
    std::optional<SurfaceProjection> best;

    for (int su = 0; su < kSeedCount; ++su)
    {
        for (int sv = 0; sv < kSeedCount; ++sv)
        {
            double u = surface.minParameterU() +
                       spanU * double(su) / (kSeedCount - 1);
            double v = surface.minParameterV() +
                       spanV * double(sv) / (kSeedCount - 1);
            for (int iteration = 0; iteration < 32; ++iteration)
            {
                const auto derivatives =
                    surface.derivativesAt(u, v, 2, 2);
                const AcGeVector3d &su1 = derivatives[1][0];
                const AcGeVector3d &sv1 = derivatives[0][1];
                const AcGeVector3d difference =
                    surface.pointAt(u, v) - point;
                const double k11 = su1.dotProduct(su1);
                const double k12 = su1.dotProduct(sv1);
                const double k22 = sv1.dotProduct(sv1);
                const double determinant = k11 * k22 - k12 * k12;
                if (std::abs(determinant) < 1.0e-14)
                    break;
                const double rhsU = -su1.dotProduct(difference);
                const double rhsV = -sv1.dotProduct(difference);
                const double deltaU = (k22 * rhsU - k12 * rhsV) / determinant;
                const double deltaV = (k11 * rhsV - k12 * rhsU) / determinant;
                u = std::clamp(u + deltaU, surface.minParameterU(),
                               surface.maxParameterU());
                v = std::clamp(v + deltaV, surface.minParameterV(),
                               surface.maxParameterV());
                if (std::abs(deltaU) * spanU < 1.0e-12 &&
                    std::abs(deltaV) * spanV < 1.0e-12)
                    break;
            }
            const AcGePoint3d closest = surface.pointAt(u, v);
            const double distance = (closest - point).length();
            if (distance <= tolerance)
                return SurfaceProjection{u, v, closest, distance};
            if (!best || distance < best->distance)
                best = SurfaceProjection{u, v, closest, distance};
        }
    }
    return best;
}

} // namespace ge
