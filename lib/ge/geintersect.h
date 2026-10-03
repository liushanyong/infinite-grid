#pragma once

// Intersection algorithms for the AcGe layer, following "The NURBS Book"
// 2nd edition chapter 7 (curve-curve via control-polygon subdivision with
// Newton refinement, §7.1; curve-surface via Newton iteration on the
// distance function, §7.4 idea).  Reimplemented from the book's methods;
// the LNLib reference in external/ (LGPL) is only consulted for structure.

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "gebspline.h"
#include "gepoint.h"

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

inline void curveBounds(const AcGeBsplineCurve3d &curve, AcGePoint3d &min,
                        AcGePoint3d &max)
{
    min = max = curve.controlPoints.front();
    for (const AcGePoint3d &p : curve.controlPoints)
    {
        min = AcGePoint3d(std::min(min.x, p.x), std::min(min.y, p.y),
                          std::min(min.z, p.z));
        max = AcGePoint3d(std::max(max.x, p.x), std::max(max.y, p.y),
                          std::max(max.z, p.z));
    }
}

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

// Invert a 3x3 matrix by Cramer's rule; false when singular.
inline bool invert3x3(const double m[3][3], double out[3][3])
{
    const double c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    const double c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
    const double c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    const double determinant =
        m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
    if (std::abs(determinant) < 1.0e-14)
        return false;
    const double inverse = 1.0 / determinant;
    out[0][0] = c00 * inverse;
    out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inverse;
    out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inverse;
    out[1][0] = c01 * inverse;
    out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inverse;
    out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inverse;
    out[2][0] = c02 * inverse;
    out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inverse;
    out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inverse;
    return true;
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
inline std::vector<CurveCurveIntersection> intersectCurveCurve(
    const AcGeBsplineCurve3d &a, const AcGeBsplineCurve3d &b,
    double tolerance = 1.0e-7)
{
    std::vector<CurveCurveIntersection> intersections;
    AcGePoint3d minA, maxA, minB, maxB;
    detail::curveBounds(a, minA, maxA);
    detail::curveBounds(b, minB, maxB);
    if (!detail::boundsOverlap(minA, maxA, minB, maxB, tolerance))
        return intersections;

    // Seed parameters: sample both curves, keep pairs whose points are
    // within the coarse seed tolerance, then refine each uniquely.
    constexpr int kSeedSamples = 64;
    const double spanA = a.maxParameter() - a.minParameter();
    const double spanB = b.maxParameter() - b.minParameter();
    const double seedTolerance = std::max(
        tolerance,
        2.0 * (detail::curveBoundsLength(minA, maxA) +
               detail::curveBoundsLength(minB, maxB)) /
            double(kSeedSamples));

    std::vector<CurveCurveIntersection> seeds;
    for (int i = 0; i <= kSeedSamples; ++i)
    {
        const double ta = a.minParameter() + spanA * double(i) / kSeedSamples;
        const AcGePoint3d pa = a.pointAt(ta);
        for (int j = 0; j <= kSeedSamples; ++j)
        {
            const double tb =
                b.minParameter() + spanB * double(j) / kSeedSamples;
            if ((pa - b.pointAt(tb)).length() <= seedTolerance)
                seeds.push_back({ta, tb, pa});
        }
    }

    for (const CurveCurveIntersection &seed : seeds)
    {
        double parameterA = seed.parameterA;
        double parameterB = seed.parameterB;
        AcGePoint3d point;
        if (!detail::refineCurveCurve(a, b, parameterA, parameterB, point,
                                      tolerance))
            continue;

        bool duplicate = false;
        for (const CurveCurveIntersection &existing : intersections)
        {
            if (std::abs(existing.parameterA - parameterA) <
                    (a.maxParameter() - a.minParameter()) * 1.0e-6 &&
                std::abs(existing.parameterB - parameterB) <
                    (b.maxParameter() - b.minParameter()) * 1.0e-6)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            intersections.push_back({parameterA, parameterB, point});
    }
    return intersections;
}

// Curve-surface intersection (NURBS Book §7.4 idea): seed from curve
// samples that project near the surface, then Newton-iterate the distance
// function over (curve t, surface u, v).
inline std::vector<CurveSurfaceIntersection> intersectCurveSurface(
    const AcGeBsplineCurve3d &curve, const AcGeNurbsSurface3d &surface,
    double tolerance = 1.0e-7)
{
    std::vector<CurveSurfaceIntersection> intersections;
    constexpr int kSeedSamples = 48;
    const double spanC = curve.maxParameter() - curve.minParameter();
    const double spanU = surface.maxParameterU() - surface.minParameterU();
    const double spanV = surface.maxParameterV() - surface.minParameterV();

    for (int i = 0; i <= kSeedSamples; ++i)
    {
        const double t = curve.minParameter() + spanC * double(i) / kSeedSamples;
        const AcGePoint3d pointOnCurve = curve.pointAt(t);
        // Coarse surface scan for the closest surface point seed.
        double bestDistance = 1.0e300;
        double seedU = surface.minParameterU();
        double seedV = surface.minParameterV();
        for (int su = 0; su <= kSeedSamples; ++su)
        {
            for (int sv = 0; sv <= kSeedSamples; ++sv)
            {
                const double uu =
                    surface.minParameterU() + spanU * double(su) / kSeedSamples;
                const double vv =
                    surface.minParameterV() + spanV * double(sv) / kSeedSamples;
                const double distance =
                    (pointOnCurve - surface.pointAt(uu, vv)).length();
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    seedU = uu;
                    seedV = vv;
                }
            }
        }

        // Newton over (t, u, v): minimize f = C(t) - S(u, v).
        double ct = t;
        double cu = seedU;
        double cv = seedV;
        bool converged = false;
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            const AcGePoint3d pc = curve.pointAt(ct);
            const AcGePoint3d ps = surface.pointAt(cu, cv);
            const AcGeVector3d difference = pc - ps;
            if (difference.length() <= tolerance)
            {
                converged = true;
                break;
            }
            const auto dc = curve.derivativesAt(ct, 2);
            const auto ds = surface.derivativesAt(cu, cv, 2, 2);
            // Unknowns: (dt, du, dv); linearize
            //   [ -C'  S_u  S_v ] [dt du dv]^T = -r
            // normal equations via the 3x3 system J^T J x = -J^T r.
            const AcGeVector3d columns[3] = {-dc[1], ds[1][0], ds[0][1]};
            double matrix[3][3];
            double rhs[3];
            for (int r = 0; r < 3; ++r)
            {
                rhs[r] = -columns[r].dotProduct(difference);
                for (int c = 0; c < 3; ++c)
                    matrix[r][c] = columns[r].dotProduct(columns[c]);
            }
            double inverse[3][3];
            if (!detail::invert3x3(matrix, inverse))
                break;
            const double deltas[3] = {
                inverse[0][0] * rhs[0] + inverse[0][1] * rhs[1] +
                    inverse[0][2] * rhs[2],
                inverse[1][0] * rhs[0] + inverse[1][1] * rhs[1] +
                    inverse[1][2] * rhs[2],
                inverse[2][0] * rhs[0] + inverse[2][1] * rhs[1] +
                    inverse[2][2] * rhs[2],
            };
            ct = std::clamp(ct + deltas[0], curve.minParameter(),
                            curve.maxParameter());
            cu = std::clamp(cu + deltas[1], surface.minParameterU(),
                            surface.maxParameterU());
            cv = std::clamp(cv + deltas[2], surface.minParameterV(),
                            surface.maxParameterV());
        }
        if (!converged)
            continue;

        bool duplicate = false;
        for (const CurveSurfaceIntersection &existing : intersections)
        {
            if (std::abs(existing.curveParameter - ct) <
                    spanC * 1.0e-6 &&
                std::abs(existing.surfaceParameterU - cu) <
                    spanU * 1.0e-6)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            intersections.push_back({ct, cu, cv, curve.pointAt(ct)});
    }
    return intersections;
}

} // namespace ge
