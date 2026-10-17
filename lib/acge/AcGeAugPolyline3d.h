#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "AcGeArrays.h"
#include "AcGeKnotVector.h"
#include "AcGePoint.h"
#include "AcGePointOnCurve.h"

// Source-level AcGeAugPolyline3d aligned with the ObjectARX reference
// (geapln3d.h): an augmented polyline storing, at every vertex, the point,
// the first-derivative vector, and the second-derivative vector. The knot
// vector holds the vertex parameters.
//
// Direct construction takes matching knot/point/vector bundles (missing
// second-derivative data defaults to zero vectors). The curve-fitting
// constructor type-erases any curve providing const pointAt(double) -- the
// same protocol as the point-on-curve wrappers -- and adaptively samples
// the curve so no vertex-to-chord deviation exceeds approxTol (midpoint
// deviation criterion, ARX documents the same contract in terms of the
// maximum distance from any segment to the curve).

class ACGE_PORT AcGeAugPolyline3d
{
public:
    AcGeAugPolyline3d() = default;

    // Knots + per-vertex point / first-derivative / second-derivative data.
    AcGeAugPolyline3d(const AcGeKnotVector &knots,
                      const AcGePoint3dArray &points,
                      const AcGeVector3dArray &d1Vectors,
                      const AcGeVector3dArray &d2Vectors = {})
    {
        set(knots, points, d1Vectors, d2Vectors);
    }

    // Approximates the curve over [fromParam, toParam] with vertices on the
    // curve; every chord stays within approxTol of the curve.
    template <class Curve>
    AcGeAugPolyline3d(const Curve &curve, double fromParam, double toParam,
                      double approxTol)
    {
        sampleCurve(curve, fromParam, toParam, approxTol);
    }

    AcGeAugPolyline3d &set(const AcGeKnotVector &knots,
                           const AcGePoint3dArray &points,
                           const AcGeVector3dArray &d1Vectors,
                           const AcGeVector3dArray &d2Vectors = {})
    {
        const int knotCount = knots.length();
        if (knotCount <= 0 || points.size() != static_cast<size_t>(knotCount) ||
            d1Vectors.size() != static_cast<size_t>(knotCount) ||
            (!d2Vectors.empty() &&
             d2Vectors.size() != static_cast<size_t>(knotCount)))
            throw std::invalid_argument(
                "AugPolyline knot, point, and vector arrays must be "
                "non-empty and of equal length.");
        m_knots = knots;
        m_points = points;
        m_d1Vectors = d1Vectors;
        m_d2Vectors = d2Vectors.empty()
                          ? AcGeVector3dArray(static_cast<size_t>(knotCount),
                                              AcGeVector3d{0.0, 0.0, 0.0})
                          : d2Vectors;
        return *this;
    }

    template <class Curve>
    AcGeAugPolyline3d &setFromCurve(const Curve &curve, double fromParam,
                                    double toParam, double approxTol)
    {
        sampleCurve(curve, fromParam, toParam, approxTol);
        return *this;
    }

    int numPoints() const { return static_cast<int>(m_points.size()); }
    double approxTol() const { return m_approxTol; }
    AcGeAugPolyline3d &setApproxTol(double tol)
    {
        m_approxTol = tol;
        return *this;
    }

    const AcGeKnotVector &knots() const { return m_knots; }

    AcGePoint3d getPoint(int idx) const { return m_points.at(checkIndex(idx)); }
    AcGeVector3d getVector(int idx) const
    {
        return m_d1Vectors.at(checkIndex(idx));
    }
    AcGeVector3d getD2Vector(int idx) const
    {
        return m_d2Vectors.at(checkIndex(idx));
    }
    void getPoints(AcGePoint3dArray &points) const { points = m_points; }
    void getD1Vectors(AcGeVector3dArray &vectors) const
    {
        vectors = m_d1Vectors;
    }
    void getD2Vectors(AcGeVector3dArray &vectors) const
    {
        vectors = m_d2Vectors;
    }

    AcGeAugPolyline3d &setPoint(int idx, const AcGePoint3d &point)
    {
        m_points.at(checkIndex(idx)) = point;
        return *this;
    }
    AcGeAugPolyline3d &setVector(int idx, const AcGeVector3d &vector)
    {
        m_d1Vectors.at(checkIndex(idx)) = vector;
        return *this;
    }
    AcGeAugPolyline3d &setD2Vector(int idx, const AcGeVector3d &vector)
    {
        m_d2Vectors.at(checkIndex(idx)) = vector;
        return *this;
    }

private:
    AcGeKnotVector m_knots;
    AcGePoint3dArray m_points;
    AcGeVector3dArray m_d1Vectors;
    AcGeVector3dArray m_d2Vectors;
    double m_approxTol = 0.0;

    size_t checkIndex(int idx) const
    {
        if (idx < 0 || static_cast<size_t>(idx) >= m_points.size())
            throw std::out_of_range(
                "AugPolyline vertex index is out of range.");
        return static_cast<size_t>(idx);
    }

    static double pointSegmentDistance(const AcGePoint3d &point,
                                       const AcGePoint3d &segmentStart,
                                       const AcGePoint3d &segmentEnd)
    {
        const AcGeVector3d segment = segmentEnd - segmentStart;
        const double lengthSq = segment.dotProduct(segment);
        if (lengthSq == 0.0)
            return point.distanceTo(segmentStart);
        const double t = std::clamp(
            (point - segmentStart).dotProduct(segment) / lengthSq, 0.0, 1.0);
        return point.distanceTo(segmentStart + segment * t);
    }

    template <class Curve>
    void sampleCurve(const Curve &curve, double fromParam, double toParam,
                     double approxTol)
    {
        if (!std::isfinite(fromParam) || !std::isfinite(toParam) ||
            !(fromParam < toParam))
            throw std::invalid_argument(
                "AugPolyline requires finite fromParam < toParam.");
        if (!std::isfinite(approxTol) || approxTol <= 0.0)
            throw std::invalid_argument(
                "AugPolyline approximation tolerance must be positive.");

        ge::detail::PointOnCurveCore<AcGePoint3d, AcGeVector3d> core;
        core.assign(curve);

        int segments = 4;
        for (;;)
        {
            bool converged = true;
            for (int i = 0; i < segments; ++i)
            {
                const double a = fromParam +
                                 (toParam - fromParam) * i / segments;
                const double b = fromParam +
                                 (toParam - fromParam) * (i + 1) / segments;
                const double mid = 0.5 * (a + b);
                const double deviation = pointSegmentDistance(
                    core.point(mid), core.point(a), core.point(b));
                if (deviation > approxTol)
                {
                    converged = false;
                    break;
                }
            }
            if (converged)
                break;
            segments *= 2;
            if (segments > 65536)
                throw std::runtime_error(
                    "AugPolyline approximation did not converge within "
                    "the segment budget.");
        }

        m_points.clear();
        m_d1Vectors.clear();
        m_d2Vectors.clear();
        for (int i = 0; i <= segments; ++i)
        {
            const double u =
                fromParam + (toParam - fromParam) * i / segments;
            m_points.push_back(core.point(u));
            if (core.firstDeriv)
            {
                m_d1Vectors.push_back(core.firstDeriv(u));
            }
            else
            {
                const double h =
                    ge::detail::PointOnCurveCore<AcGePoint3d, AcGeVector3d>::step1(u);
                m_d1Vectors.push_back(
                    (core.point(u + h) - core.point(u - h)) * (0.5 / h));
            }
            const double h2 =
                ge::detail::PointOnCurveCore<AcGePoint3d, AcGeVector3d>::step2(u);
            if (core.firstDeriv)
            {
                m_d2Vectors.push_back(
                    (core.firstDeriv(u + h2) - core.firstDeriv(u - h2)) *
                    (0.5 / h2));
            }
            else
            {
                const AcGeVector3d forward = core.point(u + h2) - core.point(u);
                const AcGeVector3d backward = core.point(u) - core.point(u - h2);
                m_d2Vectors.push_back(
                    (forward - backward) * (1.0 / (h2 * h2)));
            }
        }
        m_knots.set(segments + 1, fromParam, toParam);
        m_approxTol = approxTol;
    }
};