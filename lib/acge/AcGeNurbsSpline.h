#pragma once

#include "AcGePort.h"

// AcGe-compatible fit-point interpolation (the AcGeNurbCurve3d fit
// construction subset): a natural cubic spline through the points, C2
// continuous.  The system layout matches the simpline reference verified
// against AutoCAD spline fitting:
//   * open curves  — second-derivative tridiagonal system with natural
//                    end conditions, solved with the Thomas algorithm (O(n));
//   * closed rings — the same system made cyclic, solved via
//                    Sherman-Morrison with the corner entries factored out
//                    so the identical Thomas solver is reused;
//   * arc length   — 24-point Gauss-Legendre quadrature of the gradient
//                    magnitude per span; the nodes/weights are computed by
//                    Newton iteration on the Legendre polynomial instead of
//                    transcribed tables.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGePoint.h"

namespace ge
{

// Thomas algorithm for the tridiagonal system
//   b[0]x[0] + c[0]x[1] = d[0]
//   a[i]x[i-1] + b[i]x[i] + c[i]x[i+1] = d[i]
//   a[n-1]x[n-2] + b[n-1]x[n-1] = d[n-1]
std::vector<double> solveTridiagonal(const std::vector<double> &a,
                                     const std::vector<double> &b,
                                     const std::vector<double> &c,
                                     const std::vector<double> &d);

// Cyclic tridiagonal solve: the system additionally couples x[n-1] into row
// 0 with weight alpha and x[0] into row n-1 with weight beta.  The corners
// are factored out as a rank-one update A = T + u v^T with
// u = (alpha, 0, ..., 0, beta), v = (1, 0, ..., 0, 1) and resolved with
// Sherman-Morrison, reusing the scalar Thomas solver per component.
std::vector<double> solveCyclicTridiagonal(
    const std::vector<double> &a, const std::vector<double> &b,
    const std::vector<double> &c, double alpha, double beta,
    const std::vector<double> &d);

// Gauss-Legendre nodes/weights on [-1, 1], computed by Newton iteration on
// the Legendre polynomial (exact to double precision, no transcribed
// tables).
std::pair<std::vector<double>, std::vector<double>>
gaussLegendreNodes(size_t order);

} // namespace ge

// Interpolating cubic spline through fit points.  Open curves use natural
// end conditions; a periodic curve treats the point list as a closed ring
// (chord-length parametrization of the ring) and stays C2 across the seam.
class ACGE_PORT AcGeFitSpline3d
{
public:
    AcGeFitSpline3d() = default;

    // params must be strictly increasing, one per point (open curves).
    AcGeFitSpline3d(const std::vector<double> &params,
                    const std::vector<AcGePoint3d> &points,
                    bool periodic = false);

    // Chord-length parametrization: parameters are the cumulative chord
    // lengths through the point sequence.
    static AcGeFitSpline3d fromChordLength(
        const std::vector<AcGePoint3d> &points, bool periodic = false);

    bool isEmpty() const { return points_.empty(); }
    double minParameter() const { return 0.0; }
    double maxParameter() const { return span_; }

    AcGePoint3d pointAt(double u) const;
    AcGeVector3d gradientAt(double u) const;

    // Gauss-Legendre arc length of the parameter interval.
    double arcLength(double from, double to) const;
    double arcLength() const
    {
        return arcLength(minParameter(), maxParameter());
    }

private:
    static constexpr size_t kArcLengthQuadratureOrder = 24;

    size_t segmentCount() const { return h_.size(); }

    // Segment containing u; wraps periodic parameters and clamps open ones
    // (guarding the end-of-span ulp).
    size_t segmentAt(double &u) const;

    static AcGeVector3d segmentSlope(const AcGePoint3d &a,
                                     const AcGePoint3d &b, double h)
    {
        return (b - a) / h;
    }

    void build(const std::vector<double> &params,
               const std::vector<AcGePoint3d> &points, bool periodic);

    // Per-segment cubic coefficients from the second derivatives.
    void finishSegments();

    std::vector<AcGePoint3d> points_;
    std::vector<double> knots_;   // segment start parameters
    std::vector<double> h_;       // segment parameter lengths
    std::vector<AcGeVector3d> slopes_;
    std::vector<AcGeVector3d> secondDerivatives_; // per knot
    std::vector<AcGeVector3d> firstDerivatives_;  // per segment
    std::vector<AcGeVector3d> thirdDerivatives_;  // per segment
    bool periodic_ = false;
    double span_ = 0.0;
};
