#pragma once

// Bézier / B-spline / NURBS curve and surface evaluation for the AcGe
// layer, following "The NURBS Book" 2nd edition algorithm numbering
// (A3.1 CurvePoint, A3.2 CurveDerivs via eq. 3.8 difference curves,
// A3.5 SurfacePoint, A4.1 rational point, A4.2/A4.3 rational derivatives
// via the binomial quotient rule).  Reimplemented from the book's
// pseudocode on top of genurbscore.h; the LNLib reference in external/
// (LGPL) is only consulted for structure.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gepoint.h"
#include "genurbscore.h"

namespace ge
{

// Homogeneous coordinate point for rational curves/surfaces.
struct AcGePoint4d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;

    AcGePoint4d() = default;
    AcGePoint4d(double px, double py, double pz, double pw = 1.0)
        : x(px), y(py), z(pz), w(pw)
    {
    }

    AcGePoint4d &operator+=(const AcGePoint4d &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        w += o.w;
        return *this;
    }
    AcGePoint4d operator*(double s) const
    {
        return {x * s, y * s, z * s, w * s};
    }
};

// Drop the first/last knot of a clamped knot vector (NURBS Book eq. 3.8:
// the derivative curve's knot vector).
inline std::vector<double> innerKnotVector(const std::vector<double> &knots)
{
    return std::vector<double>(knots.begin() + 1, knots.end() - 1);
}

// ---------------------------------------------------------------------------
// Bézier curve (de Casteljau).
// ---------------------------------------------------------------------------
class AcGeBezierCurve3d
{
public:
    std::vector<AcGePoint3d> controlPoints;

    AcGeBezierCurve3d() = default;
    explicit AcGeBezierCurve3d(const std::vector<AcGePoint3d> &points)
        : controlPoints(points)
    {
        if (points.empty())
            throw std::runtime_error("Bezier curve needs at least one point.");
    }

    size_t degree() const { return controlPoints.size() - 1; }

    AcGePoint3d pointAt(double t) const
    {
        std::vector<AcGePoint3d> work = controlPoints;
        const size_t n = work.size();
        for (size_t level = 1; level < n; ++level)
            for (size_t i = 0; i + level < n; ++i)
                work[i] = work[i] * (1.0 - t) + work[i + 1] * t;
        return work[0];
    }

    // First derivative via the degree-(p-1) Bézier of control-point
    // differences (NURBS Book eq. 1.7 / 1.8).
    AcGeVector3d derivativeAt(double t) const
    {
        const size_t p = degree();
        if (p == 0)
            return {0.0, 0.0, 0.0};
        std::vector<AcGePoint3d> differences(p);
        for (size_t i = 0; i < p; ++i)
        {
            const AcGeVector3d scaled =
                (controlPoints[i + 1] - controlPoints[i]) *
                static_cast<double>(p);
            differences[i] = AcGePoint3d(scaled.x, scaled.y, scaled.z);
        }
        AcGeBezierCurve3d derivative(differences);
        return derivative.pointAt(t).asVector();
    }
};

// ---------------------------------------------------------------------------
// B-spline curve (A3.1 / A3.2).
// ---------------------------------------------------------------------------
class AcGeBsplineCurve3d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint3d> controlPoints;

    AcGeBsplineCurve3d() = default;
    AcGeBsplineCurve3d(std::vector<double> knotVector,
                       std::vector<AcGePoint3d> points, size_t curveDegree)
        : degree(curveDegree), knots(std::move(knotVector)),
          controlPoints(std::move(points))
    {
        validate();
    }

    void validate() const
    {
        // degree 0 is legal: it is the constant derivative curve of a
        // degree-1 spline (differentiated() produces it).
        if (!isValidKnotVector(knots))
            throw std::runtime_error("Knot vector must be non-decreasing.");
        if (!isValidBspline(degree, knots.size(), controlPoints.size()))
            throw std::runtime_error(
                "B-spline invalid: knot count must be n + p + 1.");
    }

    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }

    // A3.1
    AcGePoint3d pointAt(double u) const
    {
        const size_t span = findKnotSpan(degree, knots, u);
        std::vector<double> basis(degree + 1, 0.0);
        basisFunctions(span, degree, knots, u, basis.data());
        AcGeVector3d accumulation;
        for (size_t i = 0; i <= degree; ++i)
        {
            const AcGePoint3d &p = controlPoints[span - degree + i];
            accumulation += AcGeVector3d(p.x, p.y, p.z) * basis[i];
        }
        return AcGePoint3d(accumulation.x, accumulation.y, accumulation.z);
    }

    // A3.2 via eq. 3.8: the k-th derivative is a degree-(p-k) B-spline
    // whose knots drop the first/last pair and whose control points are
    // scaled differences.  Evaluated with the same A3.1 machinery, which
    // avoids the delicate A2.3 derivative-basis table entirely.
    AcGeBsplineCurve3d differentiated() const
    {
        if (degree < 1 || controlPoints.size() < 2)
            throw std::runtime_error(
                "Cannot differentiate a constant B-spline curve.");
        std::vector<double> inner = innerKnotVector(knots);
        std::vector<AcGePoint3d> differences(controlPoints.size() - 1);
        for (size_t i = 0; i + 1 < controlPoints.size(); ++i)
        {
            const AcGeVector3d scaled =
                (controlPoints[i + 1] - controlPoints[i]) *
                static_cast<double>(degree);
            differences[i] = AcGePoint3d(scaled.x, scaled.y, scaled.z);
        }
        return AcGeBsplineCurve3d(std::move(inner), std::move(differences),
                                  degree - 1);
    }

    // A3.2: derivatives 0..derivativeCount at u.
    std::vector<AcGeVector3d> derivativesAt(double u,
                                            size_t derivativeCount) const
    {
        std::vector<AcGeVector3d> result(derivativeCount + 1);
        result[0] = pointAt(u).asVector();
        AcGeBsplineCurve3d derivative = *this;
        for (size_t k = 1; k <= derivativeCount; ++k)
        {
            if (derivative.degree < 1 || derivative.controlPoints.size() < 2)
            {
                for (size_t rest = k; rest <= derivativeCount; ++rest)
                    result[rest] = AcGeVector3d();
                break;
            }
            derivative = derivative.differentiated();
            result[k] = derivative.pointAt(u).asVector();
        }
        return result;
    }

    // Global interpolation through fit points (A9.1): constructs the
    // control points so the curve passes through every fit point.
    static AcGeBsplineCurve3d interpolate(
        const std::vector<AcGePoint3d> &fitPoints, size_t curveDegree,
        double alpha = 0.0)
    {
        if (fitPoints.size() < curveDegree + 1)
            throw std::runtime_error(
                "Interpolation needs at least degree+1 fit points.");
        const std::vector<double> bar =
            normalizedCumulativeParameters(fitPoints, alpha);
        const size_t controlPointCount = fitPoints.size();
        const std::vector<double> knots =
            averagedClampedKnotVector(bar, controlPointCount, curveDegree);

        // A9.1: build the banded interpolation matrix from the fit-point
        // parameters and solve it (dense Gaussian elimination with partial
        // pivoting; control point counts in CAD are modest).
        std::vector<AcGePoint3d> controlPoints(controlPointCount);
        std::vector<double> basisRow(curveDegree + 1, 0.0);
        const size_t size = controlPointCount;
        std::vector<std::vector<double>> A(
            size, std::vector<double>(size, 0.0));
        std::vector<double> bx(size, 0.0), by(size, 0.0), bz(size, 0.0);
        for (size_t i = 0; i < size; ++i)
        {
            // Every row is a basis-function collocation row: the clamped
            // knot vector makes row 0 and row n exactly e_0 and e_n.
            const size_t span = findKnotSpan(curveDegree, knots, bar[i]);
            basisFunctions(span, curveDegree, knots, bar[i],
                           basisRow.data());
            for (size_t l = 0; l <= curveDegree; ++l)
                A[i][span - curveDegree + l] = basisRow[l];
            bx[i] = fitPoints[i].x;
            by[i] = fitPoints[i].y;
            bz[i] = fitPoints[i].z;
        }
        for (size_t col = 0; col < size; ++col)
        {
            size_t pivot = col;
            for (size_t row = col + 1; row < size; ++row)
                if (std::abs(A[row][col]) > std::abs(A[pivot][col]))
                    pivot = row;
            std::swap(A[col], A[pivot]);
            std::swap(bx[col], bx[pivot]);
            std::swap(by[col], by[pivot]);
            std::swap(bz[col], bz[pivot]);
            const double diagonal = A[col][col];
            if (std::abs(diagonal) < 1.0e-14)
                throw std::runtime_error("Singular interpolation system.");
            for (size_t row = col + 1; row < size; ++row)
            {
                const double factor = A[row][col] / diagonal;
                if (factor == 0.0)
                    continue;
                for (size_t k = col; k < size; ++k)
                    A[row][k] -= factor * A[col][k];
                bx[row] -= factor * bx[col];
                by[row] -= factor * by[col];
                bz[row] -= factor * bz[col];
            }
        }
        for (size_t row = size; row-- > 0;)
        {
            double sx = bx[row], sy = by[row], sz = bz[row];
            for (size_t k = row + 1; k < size; ++k)
            {
                sx -= A[row][k] * controlPoints[k].x;
                sy -= A[row][k] * controlPoints[k].y;
                sz -= A[row][k] * controlPoints[k].z;
            }
            controlPoints[row] =
                AcGePoint3d(sx / A[row][row], sy / A[row][row],
                            sz / A[row][row]);
        }
        return AcGeBsplineCurve3d(knots, controlPoints, curveDegree);
    }
};

// ---------------------------------------------------------------------------
// NURBS curve (A4.1 / A4.2).
// ---------------------------------------------------------------------------
class AcGeNurbsCurve3d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint4d> controlPoints; // homogeneous (w = weight)

    AcGeNurbsCurve3d() = default;
    AcGeNurbsCurve3d(std::vector<double> knotVector,
                     std::vector<AcGePoint4d> points, size_t curveDegree)
        : degree(curveDegree), knots(std::move(knotVector)),
          controlPoints(std::move(points))
    {
        validate();
    }

    void validate() const
    {
        // degree 0 is legal (constant derivative curve).
        if (!isValidKnotVector(knots))
            throw std::runtime_error("Knot vector must be non-decreasing.");
        if (!isValidBspline(degree, knots.size(), controlPoints.size()))
            throw std::runtime_error(
                "NURBS invalid: knot count must be n + p + 1.");
    }

    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }

    // A4.1: projective evaluation of the rational curve.
    AcGePoint3d pointAt(double u) const
    {
        const size_t span = findKnotSpan(degree, knots, u);
        std::vector<double> basis(degree + 1, 0.0);
        basisFunctions(span, degree, knots, u, basis.data());
        AcGePoint4d homogeneous(0.0, 0.0, 0.0, 0.0);
        for (size_t i = 0; i <= degree; ++i)
        {
            const AcGePoint4d &p = controlPoints[span - degree + i];
            // Homogeneous control point is (w*x, w*y, w*z, w).
            homogeneous += AcGePoint4d(p.x * p.w * basis[i],
                                       p.y * p.w * basis[i],
                                       p.z * p.w * basis[i],
                                       p.w * basis[i]);
        }
        if (std::abs(homogeneous.w) < 1.0e-14)
            throw std::runtime_error("Rational evaluation at w=0.");
        return AcGePoint3d(homogeneous.x / homogeneous.w,
                           homogeneous.y / homogeneous.w,
                           homogeneous.z / homogeneous.w);
    }

    // A4.2 via scalar difference splines: differentiate the weight and the
    // three homogeneous component splines k times each (degree drops per
    // step, NURBS Book eq. 3.8), evaluate all parts at u, then apply the
    // binomial quotient rule in ascending order.
    std::vector<AcGeVector3d> derivativesAt(double u,
                                            size_t derivativeCount) const
    {
        std::vector<double> wx(controlPoints.size());
        std::vector<double> xx(controlPoints.size());
        std::vector<double> yy(controlPoints.size());
        std::vector<double> zz(controlPoints.size());
        for (size_t i = 0; i < controlPoints.size(); ++i)
        {
            wx[i] = controlPoints[i].w;
            // The homogeneous components are (w*x, w*y, w*z).
            xx[i] = controlPoints[i].x * controlPoints[i].w;
            yy[i] = controlPoints[i].y * controlPoints[i].w;
            zz[i] = controlPoints[i].z * controlPoints[i].w;
        }
        std::vector<double> wKnots = knots;
        std::vector<double> xKnots = knots;
        std::vector<double> yKnots = knots;
        std::vector<double> zKnots = knots;
        size_t scalarDegree = degree;

        auto scalarDerivative = [](std::vector<double> scalarKnots,
                                   std::vector<double> values,
                                   size_t curveDegree) {
            std::vector<double> inner(scalarKnots.begin() + 1,
                                      scalarKnots.end() - 1);
            std::vector<double> diff(values.size() - 1);
            for (size_t i = 0; i + 1 < values.size(); ++i)
                diff[i] = (values[i + 1] - values[i]) *
                          static_cast<double>(curveDegree);
            return std::make_pair(std::move(inner), std::move(diff));
        };
        auto scalarPoint = [](const std::vector<double> &scalarKnots,
                              const std::vector<double> &values,
                              size_t scalarDeg, double parameter) {
            const size_t span =
                findKnotSpan(scalarDeg, scalarKnots, parameter);
            std::vector<double> basis(scalarDeg + 1, 0.0);
            basisFunctions(span, scalarDeg, scalarKnots, parameter,
                           basis.data());
            double sum = 0.0;
            for (size_t i = 0; i <= scalarDeg; ++i)
                sum += values[span - scalarDeg + i] * basis[i];
            return sum;
        };

        std::vector<double> wders(derivativeCount + 1, 0.0);
        std::vector<double> hx(derivativeCount + 1, 0.0);
        std::vector<double> hy(derivativeCount + 1, 0.0);
        std::vector<double> hz(derivativeCount + 1, 0.0);
        wders[0] = scalarPoint(wKnots, wx, scalarDegree, u);
        hx[0] = scalarPoint(xKnots, xx, scalarDegree, u);
        hy[0] = scalarPoint(yKnots, yy, scalarDegree, u);
        hz[0] = scalarPoint(zKnots, zz, scalarDegree, u);
        for (size_t k = 1; k <= derivativeCount; ++k)
        {
            if (scalarDegree < 1 || wx.size() < 2)
            {
                for (size_t rest = k; rest <= derivativeCount; ++rest)
                {
                    wders[rest] = 0.0;
                    hx[rest] = hy[rest] = hz[rest] = 0.0;
                }
                break;
            }
            auto dw = scalarDerivative(wKnots, wx, scalarDegree);
            wKnots = std::move(dw.first);
            wx = std::move(dw.second);
            auto dxv = scalarDerivative(xKnots, xx, scalarDegree);
            xKnots = std::move(dxv.first);
            xx = std::move(dxv.second);
            auto dyv = scalarDerivative(yKnots, yy, scalarDegree);
            yKnots = std::move(dyv.first);
            yy = std::move(dyv.second);
            auto dzv = scalarDerivative(zKnots, zz, scalarDegree);
            zKnots = std::move(dzv.first);
            zz = std::move(dzv.second);
            --scalarDegree;
            wders[k] = scalarPoint(wKnots, wx, scalarDegree, u);
            hx[k] = scalarPoint(xKnots, xx, scalarDegree, u);
            hy[k] = scalarPoint(yKnots, yy, scalarDegree, u);
            hz[k] = scalarPoint(zKnots, zz, scalarDegree, u);
        }

        std::vector<AcGeVector3d> result(derivativeCount + 1);
        const double w0 = wders[0];
        if (std::abs(w0) < 1.0e-14)
            throw std::runtime_error("Rational derivative at w=0.");
        result[0] = AcGeVector3d(hx[0] / w0, hy[0] / w0, hz[0] / w0);
        for (size_t k = 1; k <= derivativeCount; ++k)
        {
            double ax = hx[k], ay = hy[k], az = hz[k];
            double binomial = 1.0;
            for (size_t i = 1; i <= k; ++i)
            {
                binomial = binomial * double(k - i + 1) / double(i);
                ax -= binomial * wders[i] * result[k - i].x;
                ay -= binomial * wders[i] * result[k - i].y;
                az -= binomial * wders[i] * result[k - i].z;
            }
            result[k] = AcGeVector3d(ax / w0, ay / w0, az / w0);
        }
        return result;
    }

    double curvatureAt(double u) const
    {
        const auto d = derivativesAt(u, 2);
        const AcGeVector3d first = d[1];
        const AcGeVector3d second = d[2];
        const AcGeVector3d cross = first.crossProduct(second);
        const double speed = first.length();
        if (speed < 1.0e-14)
            return 0.0;
        return cross.length() / (speed * speed * speed);
    }
};

// ---------------------------------------------------------------------------
// B-spline surface (A3.5 / A3.6 difference surfaces).
// ---------------------------------------------------------------------------
class AcGeBsplineSurface3d
{
public:
    size_t degreeU = 3;
    size_t degreeV = 3;
    std::vector<double> knotsU;
    std::vector<double> knotsV;
    // Row-major over V: controlPoints[uIndex * vCount + vIndex].
    std::vector<AcGePoint3d> controlPoints;
    size_t controlPointCountU = 0;
    size_t controlPointCountV = 0;

    void validate() const
    {
        if (!isValidKnotVector(knotsU) || !isValidKnotVector(knotsV))
            throw std::runtime_error("Knot vectors must be non-decreasing.");
        if (controlPoints.size() != controlPointCountU * controlPointCountV)
            throw std::runtime_error("Control point grid size mismatch.");
        if (!isValidBspline(degreeU, knotsU.size(), controlPointCountU) ||
            !isValidBspline(degreeV, knotsV.size(), controlPointCountV))
            throw std::runtime_error(
                "Surface invalid: knot count must be n + p + 1.");
    }

    double minParameterU() const { return knotsU.front(); }
    double maxParameterU() const { return knotsU.back(); }
    double minParameterV() const { return knotsV.front(); }
    double maxParameterV() const { return knotsV.back(); }

    // A3.5: tensor-product evaluation.
    AcGePoint3d pointAt(double u, double v) const
    {
        const size_t spanU = findKnotSpan(degreeU, knotsU, u);
        const size_t spanV = findKnotSpan(degreeV, knotsV, v);
        std::vector<double> basisU(degreeU + 1, 0.0);
        std::vector<double> basisV(degreeV + 1, 0.0);
        basisFunctions(spanU, degreeU, knotsU, u, basisU.data());
        basisFunctions(spanV, degreeV, knotsV, v, basisV.data());

        std::vector<AcGePoint3d> temp(degreeV + 1);
        for (size_t l = 0; l <= degreeV; ++l)
        {
            AcGeVector3d accumulation;
            for (size_t k = 0; k <= degreeU; ++k)
            {
                const AcGePoint3d &p =
                    controlPoints[(spanU - degreeU + k) * controlPointCountV +
                                  spanV - degreeV + l];
                accumulation += AcGeVector3d(p.x, p.y, p.z) * basisU[k];
            }
            temp[l] = AcGePoint3d(accumulation.x, accumulation.y,
                                  accumulation.z);
        }
        AcGeVector3d pointVector;
        for (size_t l = 0; l <= degreeV; ++l)
        {
            const AcGePoint3d &p = temp[l];
            pointVector += AcGeVector3d(p.x, p.y, p.z) * basisV[l];
        }
        return AcGePoint3d(pointVector.x, pointVector.y, pointVector.z);
    }

    // Partial derivatives up to dU in u and dV in v via difference surfaces:
    // result[k][l] = d^(k+l) S / du^k dv^l (eq. 3.9, the operations commute).
    std::vector<std::vector<AcGeVector3d>> derivativesAt(
        double u, double v, size_t derivativeCountU,
        size_t derivativeCountV) const
    {
        std::vector<std::vector<AcGeVector3d>> result(
            derivativeCountU + 1,
            std::vector<AcGeVector3d>(derivativeCountV + 1));

        for (size_t k = 0; k <= derivativeCountU; ++k)
        {
            for (size_t l = 0; l <= derivativeCountV; ++l)
            {
                std::vector<double> wKnotsU = knotsU;
                std::vector<double> wKnotsV = knotsV;
                std::vector<AcGePoint3d> wPoints = controlPoints;
                size_t degU = degreeU;
                size_t degV = degreeV;
                size_t countU = controlPointCountU;
                size_t countV = controlPointCountV;
                bool valid = true;

                auto diffU = [&]() {
                    std::vector<double> inner = innerKnotVector(wKnotsU);
                    std::vector<AcGePoint3d> differences(
                        (countU - 1) * countV);
                    for (size_t i = 0; i + 1 < countU; ++i)
                        for (size_t j = 0; j < countV; ++j)
                        {
                            const AcGePoint3d &a =
                                wPoints[i * countV + j];
                            const AcGePoint3d &b =
                                wPoints[(i + 1) * countV + j];
                            const AcGeVector3d scaled =
                                (b - a) * static_cast<double>(degU);
                            differences[i * countV + j] =
                                AcGePoint3d(scaled.x, scaled.y, scaled.z);
                        }
                    wKnotsU = std::move(inner);
                    wPoints = std::move(differences);
                    --degU;
                    --countU;
                };
                auto diffV = [&]() {
                    std::vector<double> inner = innerKnotVector(wKnotsV);
                    std::vector<AcGePoint3d> differences(
                        countU * (countV - 1));
                    for (size_t i = 0; i < countU; ++i)
                        for (size_t j = 0; j + 1 < countV; ++j)
                        {
                            const AcGePoint3d &a =
                                wPoints[i * countV + j];
                            const AcGePoint3d &b =
                                wPoints[i * countV + j + 1];
                            const AcGeVector3d scaled =
                                (b - a) * static_cast<double>(degV);
                            differences[i * (countV - 1) + j] =
                                AcGePoint3d(scaled.x, scaled.y, scaled.z);
                        }
                    wKnotsV = std::move(inner);
                    wPoints = std::move(differences);
                    --degV;
                    --countV;
                };
                for (size_t step = 0; step < k && valid; ++step)
                {
                    if (degU < 1 || countU < 2) { valid = false; break; }
                    diffU();
                }
                for (size_t step = 0; step < l && valid; ++step)
                {
                    if (degV < 1 || countV < 2) { valid = false; break; }
                    diffV();
                }
                if (!valid)
                {
                    result[k][l] = AcGeVector3d();
                    continue;
                }

                const size_t spanU = findKnotSpan(degU, wKnotsU, u);
                const size_t spanV = findKnotSpan(degV, wKnotsV, v);
                std::vector<double> basisU(degU + 1, 0.0);
                std::vector<double> basisV(degV + 1, 0.0);
                basisFunctions(spanU, degU, wKnotsU, u, basisU.data());
                basisFunctions(spanV, degV, wKnotsV, v, basisV.data());
                std::vector<AcGePoint3d> temp(degV + 1);
                for (size_t l2 = 0; l2 <= degV; ++l2)
                {
                    AcGeVector3d accumulation;
                    for (size_t k2 = 0; k2 <= degU; ++k2)
                    {
                        const AcGePoint3d &p =
                            wPoints[(spanU - degU + k2) * countV +
                                    spanV - degV + l2];
                        accumulation +=
                            AcGeVector3d(p.x, p.y, p.z) * basisU[k2];
                    }
                    temp[l2] = AcGePoint3d(accumulation.x, accumulation.y,
                                           accumulation.z);
                }
                AcGeVector3d pointVector;
                for (size_t l2 = 0; l2 <= degV; ++l2)
                {
                    const AcGePoint3d &p = temp[l2];
                    pointVector +=
                        AcGeVector3d(p.x, p.y, p.z) * basisV[l2];
                }
                result[k][l] = pointVector;
            }
        }
        return result;
    }
};

// ---------------------------------------------------------------------------
// NURBS surface (A4.3 rational tensor product).
// ---------------------------------------------------------------------------
class AcGeNurbsSurface3d
{
public:
    size_t degreeU = 3;
    size_t degreeV = 3;
    std::vector<double> knotsU;
    std::vector<double> knotsV;
    std::vector<AcGePoint4d> controlPoints; // homogeneous grid, row-major U
    size_t controlPointCountU = 0;
    size_t controlPointCountV = 0;

    void validate() const
    {
        if (!isValidKnotVector(knotsU) || !isValidKnotVector(knotsV))
            throw std::runtime_error("Knot vectors must be non-decreasing.");
        if (controlPoints.size() != controlPointCountU * controlPointCountV)
            throw std::runtime_error("Control point grid size mismatch.");
        if (!isValidBspline(degreeU, knotsU.size(), controlPointCountU) ||
            !isValidBspline(degreeV, knotsV.size(), controlPointCountV))
            throw std::runtime_error(
                "Surface invalid: knot count must be n + p + 1.");
    }

    double minParameterU() const { return knotsU.front(); }
    double maxParameterU() const { return knotsU.back(); }
    double minParameterV() const { return knotsV.front(); }
    double maxParameterV() const { return knotsV.back(); }

    // A4.3: rational tensor-product evaluation.
    AcGePoint3d pointAt(double u, double v) const
    {
        const size_t spanU = findKnotSpan(degreeU, knotsU, u);
        const size_t spanV = findKnotSpan(degreeV, knotsV, v);
        std::vector<double> basisU(degreeU + 1, 0.0);
        std::vector<double> basisV(degreeV + 1, 0.0);
        basisFunctions(spanU, degreeU, knotsU, u, basisU.data());
        basisFunctions(spanV, degreeV, knotsV, v, basisV.data());

        AcGePoint4d homogeneous(0.0, 0.0, 0.0, 0.0);
        for (size_t i = 0; i <= degreeU; ++i)
        {
            for (size_t j = 0; j <= degreeV; ++j)
            {
                const AcGePoint4d &p =
                    controlPoints[(spanU - degreeU + i) *
                                      controlPointCountV +
                                  spanV - degreeV + j];
                const double weight = basisU[i] * basisV[j];
                homogeneous += AcGePoint4d(p.x * p.w * weight,
                                           p.y * p.w * weight,
                                           p.z * p.w * weight,
                                           p.w * weight);
            }
        }
        if (std::abs(homogeneous.w) < 1.0e-14)
            throw std::runtime_error("Rational evaluation at w=0.");
        return AcGePoint3d(homogeneous.x / homogeneous.w,
                           homogeneous.y / homogeneous.w,
                           homogeneous.z / homogeneous.w);
    }

    // Partial derivatives up to dU in u and dV in v via homogeneous
    // difference surfaces: for each (k, l) re-derive a working surface
    // differentiated k times in u and l times in v (the operations
    // commute), evaluate its homogeneous parts, then apply the binomial
    // quotient rule in ascending order.
    std::vector<std::vector<AcGeVector3d>> derivativesAt(
        double u, double v, size_t derivativeCountU,
        size_t derivativeCountV) const
    {
        std::vector<std::vector<AcGeVector3d>> result(
            derivativeCountU + 1,
            std::vector<AcGeVector3d>(derivativeCountV + 1));

        std::vector<std::vector<double>> hx(
            derivativeCountU + 1,
            std::vector<double>(derivativeCountV + 1, 0.0));
        std::vector<std::vector<double>> hy = hx;
        std::vector<std::vector<double>> hz = hx;
        std::vector<std::vector<double>> wders = hx;

        for (size_t k = 0; k <= derivativeCountU; ++k)
        {
            for (size_t l = 0; l <= derivativeCountV; ++l)
            {
                std::vector<double> wKnotsU = knotsU;
                std::vector<double> wKnotsV = knotsV;
                std::vector<AcGePoint4d> wPoints = controlPoints;
                size_t degU = degreeU;
                size_t degV = degreeV;
                size_t countU = controlPointCountU;
                size_t countV = controlPointCountV;
                bool valid = true;

                auto diffU = [&]() {
                    std::vector<double> inner = innerKnotVector(wKnotsU);
                    std::vector<AcGePoint4d> differences(
                        (countU - 1) * countV);
                    for (size_t i = 0; i + 1 < countU; ++i)
                        for (size_t j = 0; j < countV; ++j)
                        {
                            const AcGePoint4d &a =
                                wPoints[i * countV + j];
                            const AcGePoint4d &b =
                                wPoints[(i + 1) * countV + j];
                            const double scale =
                                static_cast<double>(degU);
                            differences[i * countV + j] = AcGePoint4d(
                                (b.x - a.x) * scale, (b.y - a.y) * scale,
                                (b.z - a.z) * scale, (b.w - a.w) * scale);
                        }
                    wKnotsU = std::move(inner);
                    wPoints = std::move(differences);
                    --degU;
                    --countU;
                };
                auto diffV = [&]() {
                    std::vector<double> inner = innerKnotVector(wKnotsV);
                    std::vector<AcGePoint4d> differences(
                        countU * (countV - 1));
                    for (size_t i = 0; i < countU; ++i)
                        for (size_t j = 0; j + 1 < countV; ++j)
                        {
                            const AcGePoint4d &a =
                                wPoints[i * countV + j];
                            const AcGePoint4d &b =
                                wPoints[i * countV + j + 1];
                            const double scale =
                                static_cast<double>(degV);
                            differences[i * (countV - 1) + j] =
                                AcGePoint4d((b.x - a.x) * scale,
                                            (b.y - a.y) * scale,
                                            (b.z - a.z) * scale,
                                            (b.w - a.w) * scale);
                        }
                    wKnotsV = std::move(inner);
                    wPoints = std::move(differences);
                    --degV;
                    --countV;
                };
                for (size_t step = 0; step < k && valid; ++step)
                {
                    if (degU < 1 || countU < 2) { valid = false; break; }
                    diffU();
                }
                for (size_t step = 0; step < l && valid; ++step)
                {
                    if (degV < 1 || countV < 2) { valid = false; break; }
                    diffV();
                }
                if (!valid)
                {
                    hx[k][l] = hy[k][l] = hz[k][l] = 0.0;
                    wders[k][l] = 0.0;
                    continue;
                }

                const size_t spanU = findKnotSpan(degU, wKnotsU, u);
                const size_t spanV = findKnotSpan(degV, wKnotsV, v);
                std::vector<double> basisU(degU + 1, 0.0);
                std::vector<double> basisV(degV + 1, 0.0);
                basisFunctions(spanU, degU, wKnotsU, u, basisU.data());
                basisFunctions(spanV, degV, wKnotsV, v, basisV.data());
                AcGePoint4d sum(0.0, 0.0, 0.0, 0.0);
                for (size_t i = 0; i <= degU; ++i)
                    for (size_t j = 0; j <= degV; ++j)
                    {
                        const AcGePoint4d &p =
                            wPoints[(spanU - degU + i) * countV +
                                    spanV - degV + j];
                        const double weight = basisU[i] * basisV[j];
                        sum += AcGePoint4d(p.x * p.w * weight,
                                           p.y * p.w * weight,
                                           p.z * p.w * weight,
                                           p.w * weight);
                    }
                hx[k][l] = sum.x;
                hy[k][l] = sum.y;
                hz[k][l] = sum.z;
                wders[k][l] = sum.w;
            }
        }

        const double w00 = wders[0][0];
        if (std::abs(w00) < 1.0e-14)
            throw std::runtime_error("Rational derivative at w=0.");
        result[0][0] = AcGeVector3d(hx[0][0] / w00, hy[0][0] / w00,
                                    hz[0][0] / w00);
        for (size_t k = 0; k <= derivativeCountU; ++k)
            for (size_t l = 0; l <= derivativeCountV; ++l)
            {
                if (k == 0 && l == 0)
                    continue;
                double ax = hx[k][l], ay = hy[k][l], az = hz[k][l];
                for (size_t i = 0; i <= k; ++i)
                    for (size_t j = 0; j <= l; ++j)
                    {
                        if (i == 0 && j == 0)
                            continue;
                        const double binomial =
                            binomialCoefficient(k, i) *
                            binomialCoefficient(l, j);
                        ax -= binomial * wders[i][j] *
                              result[k - i][l - j].x;
                        ay -= binomial * wders[i][j] *
                              result[k - i][l - j].y;
                        az -= binomial * wders[i][j] *
                              result[k - i][l - j].z;
                    }
                result[k][l] = AcGeVector3d(ax / w00, ay / w00, az / w00);
            }
        return result;
    }

    static double binomialCoefficient(size_t n, size_t k)
    {
        if (k == 0 || k == n)
            return 1.0;
        double result = 1.0;
        for (size_t i = 1; i <= k; ++i)
            result = result * double(n - k + i) / double(i);
        return result;
    }
};

} // namespace ge
