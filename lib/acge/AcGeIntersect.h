#pragma once

#include "AcGePort.h"

// Intersection algorithms for the AcGe layer, following "The NURBS Book"
// 2nd edition chapter 7 (curve-curve via control-polygon subdivision with
// Newton refinement, §7.1; curve-surface via Newton iteration on the
// distance function, §7.4 idea).  Reimplemented from the book's methods;
// the LNLib reference in external/ (LGPL) is only consulted for structure.

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "AcGeBspline.h"
#include "AcGePoint.h"

namespace ge
{

struct CurveCurveIntersection
{
    double parameterA = 0.0; // parameter on the first curve
    double parameterB = 0.0; // parameter on the second curve
    AcGePoint3d point;
};

struct CurveSurfaceIntersection
{
    double curveParameter = 0.0;
    double surfaceParameterU = 0.0;
    double surfaceParameterV = 0.0;
    AcGePoint3d point;
};

namespace detail
{

inline bool boundsOverlap(const AcGePoint3d &minA, const AcGePoint3d &maxA,
                          const AcGePoint3d &minB, const AcGePoint3d &maxB,
                          double tolerance)
{
    return minA.x <= maxB.x + tolerance &&
           maxA.x >= minB.x - tolerance && minA.y <= maxB.y + tolerance &&
           maxA.y >= minB.y - tolerance && minA.z <= maxB.z + tolerance &&
           maxA.z >= minB.z - tolerance;
}

inline double curveBoundsLength(const AcGePoint3d &min, const AcGePoint3d &max)
{
    return AcGeVector3d(max.x - min.x, max.y - min.y, max.z - min.z)
        .length();
}

// Newton refinement for two curves: solve for (dA, dB) minimizing the
// distance between C_A(tA + dA) and C_B(tB + dB) (NURBS Book eq. 7.1-7.3).
template <typename CurveA, typename CurveB>
bool refineCurveCurve(const CurveA &a, const CurveB &b,
                      double &parameterA, double &parameterB,
                      AcGePoint3d &point, double tolerance)
{
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        const AcGePoint3d pa = a.pointAt(parameterA);
        const AcGePoint3d pb = b.pointAt(parameterB);
        const AcGeVector3d difference = pa - pb;
        if (difference.length() <= tolerance)
        {
            point = pa;
            return true;
        }
        const auto da = a.derivativesAt(parameterA, 2);
        const auto db = b.derivativesAt(parameterB, 2);
        // Linearize r + k1*dA + k2*dB = 0 with
        // k1 = -Ca', k2 = Cb', and the second-order terms folded into r.
        const double k11 = da[1].dotProduct(da[1]);
        const double k12 = -da[1].dotProduct(db[1]);
        const double k22 = db[1].dotProduct(db[1]);
        const double determinant = k11 * k22 - k12 * k12;
        if (std::abs(determinant) < 1.0e-14)
            return false;
        const double k1r = da[1].dotProduct(difference);
        const double k2r = db[1].dotProduct(difference);
        double deltaA = (k22 * k1r - k12 * k2r) / determinant;
        double deltaB = (k11 * k2r - k12 * k1r) / determinant;
        // Second-order correction (NURBS Book eq. 7.3).
        const double fA = difference.dotProduct(da[2]) / 2.0;
        const double fB = -difference.dotProduct(db[2]) / 2.0;
        deltaA += (k22 * fA - k12 * fB) / determinant;
        deltaB += (k11 * fB - k12 * fA) / determinant;

        parameterA = std::clamp(parameterA - deltaA, a.minParameter(),
                                a.maxParameter());
        parameterB = std::clamp(parameterB - deltaB, b.minParameter(),
                                b.maxParameter());
    }
    point = a.pointAt(parameterA);
    return (a.pointAt(parameterA) - b.pointAt(parameterB)).length() <=
           tolerance;
}

} // namespace detail

// Curve-curve intersection: control-polygon overlap subdivision down to the
// parameter tolerance, then Newton refinement (NURBS Book §7.1).
std::vector<CurveCurveIntersection> intersectCurveCurve(
    const AcGeBsplineCurve3d &a, const AcGeBsplineCurve3d &b,
    double tolerance = 1.0e-7);

// Curve-surface intersection (NURBS Book §7.4 idea): seed from curve
// samples that project near the surface, then Newton-iterate the distance
// function over (curve t, surface u, v).
std::vector<CurveSurfaceIntersection> intersectCurveSurface(
    const AcGeBsplineCurve3d &curve, const AcGeNurbsSurface3d &surface,
    double tolerance = 1.0e-7);

} // namespace ge
