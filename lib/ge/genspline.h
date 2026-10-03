#pragma once

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

#include "gepoint.h"

namespace ge
{

// Thomas algorithm for the tridiagonal system
//   b[0]x[0] + c[0]x[1] = d[0]
//   a[i]x[i-1] + b[i]x[i] + c[i]x[i+1] = d[i]
//   a[n-1]x[n-2] + b[n-1]x[n-1] = d[n-1]
inline std::vector<double> solveTridiagonal(const std::vector<double> &a,
                                            const std::vector<double> &b,
                                            const std::vector<double> &c,
                                            const std::vector<double> &d)
{
    const size_t n = b.size();
    if (n == 0)
        return {};
    if (n == 1)
        return {d[0] / b[0]};

    std::vector<double> scratchC = c;
    std::vector<double> scratchD = d;
    scratchC[0] /= b[0];
    scratchD[0] /= b[0];
    for (size_t i = 1; i < n; ++i)
    {
        const double denominator = b[i] - a[i] * scratchC[i - 1];
        scratchC[i] = i + 1 < n ? scratchC[i] / denominator : 0.0;
        scratchD[i] = (scratchD[i] - a[i] * scratchD[i - 1]) / denominator;
    }
    for (size_t i = n - 1; i-- > 0;)
        scratchD[i] -= scratchC[i] * scratchD[i + 1];
    return scratchD;
}

// Cyclic tridiagonal solve: the system additionally couples x[n-1] into row
// 0 with weight alpha and x[0] into row n-1 with weight beta.  The corners
// are factored out as a rank-one update A = T + u v^T with
// u = (alpha, 0, ..., 0, beta), v = (1, 0, ..., 0, 1) and resolved with
// Sherman-Morrison, reusing the scalar Thomas solver per component.
inline std::vector<double> solveCyclicTridiagonal(
    const std::vector<double> &a, const std::vector<double> &b,
    const std::vector<double> &c, double alpha, double beta,
    const std::vector<double> &d)
{
    const size_t n = b.size();
    std::vector<double> tb = b;
    // T keeps the tridiagonal part; the corner entries move into the
    // rank-one update, which also adjusts the two corner-adjacent diagonal
    // entries (row 0 gains alpha on its diagonal, row n-1 gains beta).
    tb[0] -= alpha;
    tb[n - 1] -= beta;

    std::vector<double> update(n, 0.0);
    update[0] = alpha;
    update[n - 1] = beta;

    const std::vector<double> y = solveTridiagonal(a, tb, c, d);
    const std::vector<double> z = solveTridiagonal(a, tb, c, update);
    const double vDotY = y[0] + y[n - 1];
    const double vDotZ = z[0] + z[n - 1];
    const double factor = vDotY / (1.0 + vDotZ);

    std::vector<double> x(n);
    for (size_t i = 0; i < n; ++i)
        x[i] = y[i] - z[i] * factor;
    return x;
}

// Gauss-Legendre nodes/weights on [-1, 1], computed by Newton iteration on
// the Legendre polynomial (exact to double precision, no transcribed
// tables).
inline std::pair<std::vector<double>, std::vector<double>>
gaussLegendreNodes(size_t order)
{
    static const size_t kReservedOrder = 0; // documentation anchor
    (void)kReservedOrder;
    const double pi = std::acos(-1.0);
    std::vector<double> nodes(order);
    std::vector<double> weights(order);
    for (size_t i = 0; i < order; ++i)
    {
        double x = std::cos(pi * (double(i) + 0.75) /
                            (double(order) + 0.5));
        double derivative = 0.0;
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            double p0 = 1.0;
            double p1 = x;
            for (size_t k = 2; k <= order; ++k)
            {
                const double p2 = ((2.0 * double(k) - 1.0) * x * p1 -
                                   (double(k) - 1.0) * p0) /
                                  double(k);
                p0 = p1;
                p1 = p2;
            }
            derivative = double(order) * (x * p1 - p0) / (x * x - 1.0);
            const double delta = p1 / derivative;
            x -= delta;
            if (std::abs(delta) < 1.0e-15)
                break;
        }
        nodes[i] = x;
        weights[i] = 2.0 / ((1.0 - x * x) * derivative * derivative);
    }
    return {nodes, weights};
}

} // namespace ge

// Interpolating cubic spline through fit points.  Open curves use natural
// end conditions; a periodic curve treats the point list as a closed ring
// (chord-length parametrization of the ring) and stays C2 across the seam.
class AcGeFitSpline3d
{
public:
    AcGeFitSpline3d() = default;

    // params must be strictly increasing, one per point (open curves).
    AcGeFitSpline3d(const std::vector<double> &params,
                    const std::vector<AcGePoint3d> &points,
                    bool periodic = false)
    {
        build(params, points, periodic);
    }

    // Chord-length parametrization: parameters are the cumulative chord
    // lengths through the point sequence.
    static AcGeFitSpline3d fromChordLength(
        const std::vector<AcGePoint3d> &points, bool periodic = false)
    {
        const size_t count = points.size();
        const size_t segmentCount = periodic ? count : count - 1;
        std::vector<double> params(count, 0.0);
        for (size_t i = 0; i < segmentCount; ++i)
        {
            const AcGePoint3d &a = points[i];
            const AcGePoint3d &b = points[(i + 1) % count];
            // A periodic ring has one more segment than knots: the closing
            // segment's length is recomputed from geometry in build(), so
            // it contributes no knot parameter here.
            if (i + 1 < count)
                params[i + 1] = params[i] + a.distanceTo(b);
        }
        return AcGeFitSpline3d(params, points, periodic);
    }

    bool isEmpty() const { return points_.empty(); }
    double minParameter() const { return 0.0; }
    double maxParameter() const { return span_; }

    AcGePoint3d pointAt(double u) const
    {
        const size_t segment = segmentAt(u);
        const double t = u - knots_[segment];
        return points_[segment] + firstDerivatives_[segment] * t +
               (secondDerivatives_[segment] / 2.0) * (t * t) +
               (thirdDerivatives_[segment] / 6.0) * (t * t * t);
    }

    AcGeVector3d gradientAt(double u) const
    {
        const size_t segment = segmentAt(u);
        const double t = u - knots_[segment];
        return firstDerivatives_[segment] +
               secondDerivatives_[segment] * t +
               (thirdDerivatives_[segment] / 2.0) * (t * t);
    }

    // Gauss-Legendre arc length of the parameter interval.
    double arcLength(double from, double to) const
    {
        if (points_.empty() || to <= from)
            return 0.0;
        from = std::max(from, minParameter());
        to = std::min(to, maxParameter());

        const auto quadrature =
            ge::gaussLegendreNodes(kArcLengthQuadratureOrder);
        double length = 0.0;
        double start = from;
        while (start < to)
        {
            // Integrate knot-aligned pieces so the gradient (C2 inside a
            // piece) is smooth under the quadrature.
            double end = to;
            for (const double knot : knots_)
            {
                if (knot > start)
                {
                    end = std::min(end, knot);
                    break;
                }
            }
            const double halfPiece = (end - start) * 0.5;
            for (size_t j = 0; j < quadrature.first.size(); ++j)
            {
                const double t = start + halfPiece *
                                                (quadrature.first[j] + 1.0);
                length += halfPiece * quadrature.second[j] *
                          gradientAt(t).length();
            }
            start = end;
        }
        return length;
    }

    double arcLength() const
    {
        return arcLength(minParameter(), maxParameter());
    }

private:
    static constexpr size_t kArcLengthQuadratureOrder = 24;

    size_t segmentCount() const { return h_.size(); }

    // Segment containing u; wraps periodic parameters and clamps open ones
    // (guarding the end-of-span ulp).
    size_t segmentAt(double &u) const
    {
        u = std::max(u, minParameter());
        u = periodic_ ? std::fmod(u, span_) : std::min(u, span_);
        size_t segment = 0;
        while (segment + 1 < segmentCount() &&
               knots_[segment + 1] <= u)
            ++segment;
        return segment;
    }

    static AcGeVector3d segmentSlope(const AcGePoint3d &a,
                                     const AcGePoint3d &b, double h)
    {
        return (b - a) / h;
    }

    void build(const std::vector<double> &params,
               const std::vector<AcGePoint3d> &points, bool periodic)
    {
        if (points.size() < 2)
            throw std::runtime_error(
                "Fit-point spline needs at least two points.");
        if (params.size() != points.size())
            throw std::runtime_error(
                "Parameter count must match the point count.");

        points_ = points;
        periodic_ = periodic && points.size() >= 3;

        if (!periodic_)
        {
            const size_t n = points_.size();
            knots_ = params;
            knots_.pop_back(); // segment start parameters
            h_.resize(n - 1);
            slopes_.resize(n - 1);
            for (size_t i = 0; i + 1 < n; ++i)
            {
                h_[i] = params[i + 1] - params[i];
                if (!(h_[i] > 0.0))
                    throw std::runtime_error(
                        "Fit-point parameters must be strictly increasing.");
                slopes_[i] = segmentSlope(points_[i], points_[i + 1], h_[i]);
            }
            span_ = params.back();

            // Second-derivative system, natural end conditions: n unknowns
            // M[0..n-1], row 0 and row n-1 pin M to zero at the ends.
            std::vector<double> a(n, 0.0), b(n, 0.0), c(n, 0.0), dx(n, 0.0),
                dy(n, 0.0), dz(n, 0.0);
            b[0] = 1.0;
            b[n - 1] = 1.0;
            for (size_t i = 1; i + 1 < n; ++i)
            {
                const double hPrevious = h_[i - 1];
                const double hNext = h_[i];
                a[i] = hPrevious / (hPrevious + hNext);
                b[i] = 2.0;
                c[i] = hNext / (hPrevious + hNext);
                const AcGeVector3d slopeDifference =
                    slopes_[i] - slopes_[i - 1];
                const double factor = 6.0 / (hPrevious + hNext);
                dx[i] = slopeDifference.x * factor;
                dy[i] = slopeDifference.y * factor;
                dz[i] = slopeDifference.z * factor;
            }
            const std::vector<double> mx =
                ge::solveTridiagonal(a, b, c, dx);
            const std::vector<double> my =
                ge::solveTridiagonal(a, b, c, dy);
            const std::vector<double> mz =
                ge::solveTridiagonal(a, b, c, dz);
            secondDerivatives_.resize(n);
            for (size_t i = 0; i < n; ++i)
                secondDerivatives_[i] = AcGeVector3d(mx[i], my[i], mz[i]);
            finishSegments();
            return;
        }

        // Periodic ring: chord-length knots around the ring, cyclic
        // second-derivative system.
        const size_t n = points_.size();
        knots_.resize(n);
        h_.resize(n);
        slopes_.resize(n);
        for (size_t i = 0; i < n; ++i)
            h_[i] = points_[i].distanceTo(points_[(i + 1) % n]);
        knots_[0] = 0.0;
        for (size_t i = 1; i < n; ++i)
            knots_[i] = knots_[i - 1] + h_[i - 1];
        span_ = knots_[n - 1] + h_[n - 1];
        for (size_t i = 0; i < n; ++i)
            slopes_[i] = segmentSlope(points_[i], points_[(i + 1) % n], h_[i]);

        std::vector<double> a(n, 0.0), b(n, 0.0), c(n, 0.0), dx(n, 0.0),
            dy(n, 0.0), dz(n, 0.0);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t previous = (i + n - 1) % n;
            const size_t next = (i + 1) % n;
            // Normalized like the open-system interior rows.
            a[i] = h_[previous] / (h_[previous] + h_[i]);
            b[i] = 2.0;
            c[i] = h_[i] / (h_[previous] + h_[i]);
            const AcGeVector3d slopeDifference = slopes_[i] - slopes_[previous];
            const double factor = 6.0 / (h_[previous] + h_[i]);
            dx[i] = slopeDifference.x * factor;
            dy[i] = slopeDifference.y * factor;
            dz[i] = slopeDifference.z * factor;
        }
        // Row 0 couples x[n-1] with weight a[0]; row n-1 couples x[0] with
        // weight c[n-1].
        const std::vector<double> mx =
            ge::solveCyclicTridiagonal(a, b, c, a[0], c[n - 1], dx);
        const std::vector<double> my =
            ge::solveCyclicTridiagonal(a, b, c, a[0], c[n - 1], dy);
        const std::vector<double> mz =
            ge::solveCyclicTridiagonal(a, b, c, a[0], c[n - 1], dz);
        secondDerivatives_.resize(n);
        for (size_t i = 0; i < n; ++i)
            secondDerivatives_[i] = AcGeVector3d(mx[i], my[i], mz[i]);
        finishSegments();
    }

    // Per-segment cubic coefficients from the second derivatives.
    void finishSegments()
    {
        const size_t segments = segmentCount();
        firstDerivatives_.resize(segments);
        thirdDerivatives_.resize(segments);
        for (size_t i = 0; i < segments; ++i)
        {
            const size_t next = (i + 1) % points_.size();
            firstDerivatives_[i] =
                slopes_[i] - secondDerivatives_[i] * (h_[i] / 3.0) -
                secondDerivatives_[next] * (h_[i] / 6.0);
            thirdDerivatives_[i] =
                (secondDerivatives_[next] - secondDerivatives_[i]) / h_[i];
        }
    }

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
