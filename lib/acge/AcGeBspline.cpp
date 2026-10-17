#include "acge/AcGeBspline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ge
{

// ---------------------------------------------------------------------------
// AcGeBezierCurve3d
// ---------------------------------------------------------------------------

AcGeBezierCurve3d::AcGeBezierCurve3d(const std::vector<AcGePoint3d> &points)
    : controlPoints(points)
{
    if (points.empty())
        throw std::runtime_error("Bezier curve needs at least one point.");
}

AcGePoint3d AcGeBezierCurve3d::pointAt(double t) const
{
    std::vector<AcGePoint3d> work = controlPoints;
    const size_t n = work.size();
    for (size_t level = 1; level < n; ++level)
        for (size_t i = 0; i + level < n; ++i)
            work[i] = work[i] * (1.0 - t) + work[i + 1] * t;
    return work[0];
}

AcGeVector3d AcGeBezierCurve3d::derivativeAt(double t) const
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

// ---------------------------------------------------------------------------
// AcGeBsplineCurve3d
// ---------------------------------------------------------------------------

AcGeBsplineCurve3d::AcGeBsplineCurve3d(std::vector<double> knotVector,
                                       std::vector<AcGePoint3d> points,
                                       size_t curveDegree)
    : degree(curveDegree), knots(std::move(knotVector)),
      controlPoints(std::move(points))
{
    validate();
}

void AcGeBsplineCurve3d::validate() const
{
    // degree 0 is legal: it is the constant derivative curve of a
    // degree-1 spline (differentiated() produces it).
    if (!isValidKnotVector(knots))
        throw std::runtime_error("Knot vector must be non-decreasing.");
    if (!isValidBspline(degree, knots.size(), controlPoints.size()))
        throw std::runtime_error(
            "B-spline invalid: knot count must be n + p + 1.");
}

AcGePoint3d AcGeBsplineCurve3d::pointAt(double u) const
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

AcGeBsplineCurve3d AcGeBsplineCurve3d::differentiated() const
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

std::vector<AcGeVector3d> AcGeBsplineCurve3d::derivativesAt(
    double u, size_t derivativeCount) const
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

AcGeBsplineCurve3d AcGeBsplineCurve3d::interpolate(
    const std::vector<AcGePoint3d> &fitPoints, size_t curveDegree, double alpha)
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

// ---------------------------------------------------------------------------
// AcGeNurbsCurve3d
// ---------------------------------------------------------------------------

AcGeNurbsCurve3d::AcGeNurbsCurve3d(std::vector<double> knotVector,
                                   std::vector<AcGePoint4d> points,
                                   size_t curveDegree)
    : degree(curveDegree), knots(std::move(knotVector)),
      controlPoints(std::move(points))
{
    validate();
}

void AcGeNurbsCurve3d::validate() const
{
    // degree 0 is legal (constant derivative curve).
    if (!isValidKnotVector(knots))
        throw std::runtime_error("Knot vector must be non-decreasing.");
    if (!isValidBspline(degree, knots.size(), controlPoints.size()))
        throw std::runtime_error(
            "NURBS invalid: knot count must be n + p + 1.");
}

AcGePoint3d AcGeNurbsCurve3d::pointAt(double u) const
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

std::vector<AcGeVector3d> AcGeNurbsCurve3d::derivativesAt(
    double u, size_t derivativeCount) const
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

double AcGeNurbsCurve3d::curvatureAt(double u) const
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

// ---------------------------------------------------------------------------
// AcGeBsplineSurface3d
// ---------------------------------------------------------------------------

void AcGeBsplineSurface3d::validate() const
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

AcGePoint3d AcGeBsplineSurface3d::pointAt(double u, double v) const
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

std::vector<std::vector<AcGeVector3d>> AcGeBsplineSurface3d::derivativesAt(
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

AcGeVector3d AcGeBsplineSurface3d::normalAt(double u, double v) const
{
    const auto derivatives = derivativesAt(u, v, 1, 1);
    const AcGeVector3d normal =
        derivatives[1][0].crossProduct(derivatives[0][1]);
    const double length = normal.length();
    if (length <= 64.0 * std::numeric_limits<double>::epsilon() *
                      derivatives[1][0].length() *
                      derivatives[0][1].length())
        throw std::domain_error("B-spline surface normal is singular.");
    return normal / length;
}

// ---------------------------------------------------------------------------
// AcGeNurbsSurface3d
// ---------------------------------------------------------------------------

void AcGeNurbsSurface3d::validate() const
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

AcGePoint3d AcGeNurbsSurface3d::pointAt(double u, double v) const
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

std::vector<std::vector<AcGeVector3d>> AcGeNurbsSurface3d::derivativesAt(
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
                        // Homogeneous differences: d(x*w) and dw.
                        differences[i * countV + j] = AcGePoint4d((b.x * b.w - a.x * a.w) * scale,
                            (b.y * b.w - a.y * a.w) * scale,
                            (b.z * b.w - a.z * a.w) * scale,
                            (b.w - a.w) * scale);
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
                        // Homogeneous differences: d(x*w) and dw.
                        differences[i * (countV - 1) + j] = AcGePoint4d(
                            (b.x * b.w - a.x * a.w) * scale,
                            (b.y * b.w - a.y * a.w) * scale,
                            (b.z * b.w - a.z * a.w) * scale,
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
                    const double ax =
                        (k == 0 && l == 0) ? p.x * p.w : p.x;
                    const double ay =
                        (k == 0 && l == 0) ? p.y * p.w : p.y;
                    const double az =
                        (k == 0 && l == 0) ? p.z * p.w : p.z;
                    sum += AcGePoint4d(ax * weight, ay * weight,
                                       az * weight, p.w * weight);
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

double AcGeNurbsSurface3d::binomialCoefficient(size_t n, size_t k)
{
    if (k == 0 || k == n)
        return 1.0;
    double result = 1.0;
    for (size_t i = 1; i <= k; ++i)
        result = result * double(n - k + i) / double(i);
    return result;
}

AcGeVector3d AcGeNurbsSurface3d::normalAt(double u, double v) const
{
    const auto derivatives = derivativesAt(u, v, 1, 1);
    const AcGeVector3d normal =
        derivatives[1][0].crossProduct(derivatives[0][1]);
    const double length = normal.length();
    if (length <= 64.0 * std::numeric_limits<double>::epsilon() *
                      derivatives[1][0].length() *
                      derivatives[0][1].length())
        throw std::domain_error("NURBS surface normal is singular.");
    return normal / length;
}

namespace
{
int nurbsSurfaceSingularity(const AcGeNurbsSurface3d &surface, bool inU,
                            const AcGeTol &tol)
{
    surface.validate();
    const double lower =
        inU ? surface.minParameterU() : surface.minParameterV();
    const double upper =
        inU ? surface.maxParameterU() : surface.maxParameterV();
    const double otherLower =
        inU ? surface.minParameterV() : surface.minParameterU();
    const double otherUpper =
        inU ? surface.maxParameterV() : surface.maxParameterU();
    if (!(lower < upper) || !(otherLower < otherUpper) ||
        !std::isfinite(lower) || !std::isfinite(upper) ||
        !std::isfinite(otherLower) || !std::isfinite(otherUpper))
        return AcGe::kNoPoles;

    const auto isPole = [&](double parameter) {
        const double samples[] = {
            otherLower, otherLower + (otherUpper - otherLower) * 0.25,
            otherLower + (otherUpper - otherLower) * 0.5,
            otherLower + (otherUpper - otherLower) * 0.75, otherUpper};
        const auto point = [&](double other) {
            return inU ? surface.pointAt(parameter, other)
                       : surface.pointAt(other, parameter);
        };
        const AcGePoint3d reference = point(samples[0]);
        return std::all_of(std::begin(samples) + 1, std::end(samples),
                           [&](double sample) {
                               return point(sample).distanceTo(reference) <=
                                      tol.equalPoint();
                           });
    };

    const bool atMin = isPole(lower);
    const bool atMax = isPole(upper);
    if (atMin && atMax)
        return AcGe::kPoleAtBoth;
    if (atMin)
        return AcGe::kPoleAtMin;
    if (atMax)
        return AcGe::kPoleAtMax;
    return AcGe::kNoPoles;
}
} // namespace

int AcGeNurbsSurface3d::singularityInU(const AcGeTol &tol) const
{
    return nurbsSurfaceSingularity(*this, true, tol);
}

int AcGeNurbsSurface3d::singularityInV(const AcGeTol &tol) const
{
    return nurbsSurfaceSingularity(*this, false, tol);
}

void AcGeNurbsSurface3d::getControlPoints(AcGePoint3dArray &points) const
{
    validate();
    points.clear();
    points.reserve(controlPoints.size());
    for (const AcGePoint4d &point : controlPoints)
        points.emplace_back(point.x, point.y, point.z);
}

void AcGeNurbsSurface3d::getWeights(AcGeDoubleArray &weights) const
{
    validate();
    weights.clear();
    if (!isRational())
        return;
    weights.reserve(controlPoints.size());
    for (const AcGePoint4d &point : controlPoints)
        weights.push_back(point.w);
}

void AcGeNurbsSurface3d::getUKnots(AcGeKnotVector &knots) const
{
    validate();
    knots = AcGeKnotVector(AcGeDoubleArray(knotsU.begin(), knotsU.end()));
}

void AcGeNurbsSurface3d::getVKnots(AcGeKnotVector &knots) const
{
    validate();
    knots = AcGeKnotVector(AcGeDoubleArray(knotsV.begin(), knotsV.end()));
}

void AcGeNurbsSurface3d::getDefinition(
    int &degreeInU, int &degreeInV, int &propertiesInU, int &propertiesInV,
    int &numControlPointsInU, int &numControlPointsInV,
    AcGePoint3dArray &points, AcGeDoubleArray &weights,
    AcGeKnotVector &knotsInU, AcGeKnotVector &knotsInV) const
{
    validate();
    if (degreeU > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        degreeV > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        controlPointCountU >
            static_cast<size_t>(std::numeric_limits<int>::max()) ||
        controlPointCountV >
            static_cast<size_t>(std::numeric_limits<int>::max()))
        throw std::overflow_error("NURBS surface definition exceeds int range.");

    degreeInU = static_cast<int>(degreeU);
    degreeInV = static_cast<int>(degreeV);
    numControlPointsInU = static_cast<int>(controlPointCountU);
    numControlPointsInV = static_cast<int>(controlPointCountV);
    const auto properties = [&](bool inU) {
        int result = inU ? singularityInU() : singularityInV();
        const bool closed = inU ? isClosedInU() : isClosedInV();
        double period = 0.0;
        const bool periodic = inU ? isPeriodicInU(period) : isPeriodicInV(period);
        if (closed)
            result |= AcGe::kClosed;
        else
            result |= AcGe::kOpen;
        if (periodic)
            result |= AcGe::kPeriodic;
        if (inU ? isRationalInU() : isRationalInV())
            result |= AcGe::kRational;
        return result;
    };
    propertiesInU = properties(true);
    propertiesInV = properties(false);
    getControlPoints(points);
    getWeights(weights);
    getUKnots(knotsInU);
    getVKnots(knotsInV);
}

AcGeNurbsSurface3d &AcGeNurbsSurface3d::set(
    int degreeInU, int degreeInV, int propertiesInU, int propertiesInV,
    int numControlPointsInU, int numControlPointsInV,
    const AcGePoint3d controlPoints[], const double weights[],
    const AcGeKnotVector &knotsInU, const AcGeKnotVector &knotsInV,
    const AcGeTol &tol)
{
    (void)tol;
    if (degreeInU < 0 || degreeInV < 0 || numControlPointsInU <= 0 ||
        numControlPointsInV <= 0 || controlPoints == nullptr)
        throw std::invalid_argument("Invalid NURBS surface definition.");
    const size_t countU = static_cast<size_t>(numControlPointsInU);
    const size_t countV = static_cast<size_t>(numControlPointsInV);
    if (countU > std::numeric_limits<size_t>::max() / countV)
        throw std::length_error("NURBS surface control grid is too large.");
    const size_t count = countU * countV;
    const bool rational =
        (propertiesInU & AcGe::kRational) != 0 ||
        (propertiesInV & AcGe::kRational) != 0;
    if (rational && weights == nullptr)
        throw std::invalid_argument(
            "Rational NURBS surface requires a weight array.");

    AcGeNurbsSurface3d candidate;
    candidate.degreeU = static_cast<size_t>(degreeInU);
    candidate.degreeV = static_cast<size_t>(degreeInV);
    candidate.controlPointCountU = countU;
    candidate.controlPointCountV = countV;
    if (knotsInU.length() > 0)
        candidate.knotsU.assign(knotsInU.asArrayPtr(),
                                knotsInU.asArrayPtr() + knotsInU.length());
    if (knotsInV.length() > 0)
        candidate.knotsV.assign(knotsInV.asArrayPtr(),
                                knotsInV.asArrayPtr() + knotsInV.length());
    candidate.controlPoints.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        const double weight = rational ? weights[i] : 1.0;
        candidate.controlPoints.emplace_back(controlPoints[i].x,
                                             controlPoints[i].y,
                                             controlPoints[i].z, weight);
    }
    candidate.validate();

    degreeU = candidate.degreeU;
    degreeV = candidate.degreeV;
    controlPointCountU = candidate.controlPointCountU;
    controlPointCountV = candidate.controlPointCountV;
    knotsU = std::move(candidate.knotsU);
    knotsV = std::move(candidate.knotsV);
    controlPoints = std::move(candidate.controlPoints);
    return *this;
}

AcGeNurbsSurface3d &AcGeNurbsSurface3d::set(
    size_t degreeInU, size_t degreeInV, size_t numControlPointsInU,
    size_t numControlPointsInV, const AcGePoint3dArray &points,
    const AcGeDoubleArray &weights, const AcGeKnotVector &knotsInU,
    const AcGeKnotVector &knotsInV)
{
    if (numControlPointsInV != 0 &&
        numControlPointsInU >
            std::numeric_limits<size_t>::max() / numControlPointsInV)
        throw std::length_error("NURBS surface control grid is too large.");
    if (degreeInU > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        degreeInV > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        numControlPointsInU >
            static_cast<size_t>(std::numeric_limits<int>::max()) ||
        numControlPointsInV >
            static_cast<size_t>(std::numeric_limits<int>::max()) ||
        numControlPointsInU * numControlPointsInV != points.size())
        throw std::invalid_argument("Invalid NURBS surface definition.");
    if (!weights.empty() && weights.size() != points.size())
        throw std::invalid_argument(
            "NURBS surface weights must match control points.");
    std::vector<double> weightData;
    if (!weights.empty())
        weightData.assign(weights.begin(), weights.end());
    return set(static_cast<int>(degreeInU), static_cast<int>(degreeInV),
               weights.empty() ? 0 : AcGe::kRational,
               weights.empty() ? 0 : AcGe::kRational,
               static_cast<int>(numControlPointsInU),
               static_cast<int>(numControlPointsInV), points.data(),
               weightData.empty() ? nullptr : weightData.data(), knotsInU,
               knotsInV);
}

bool AcGeNurbsSurface3d::isRational() const
{
    validate();
    for (const AcGePoint4d &point : controlPoints)
        if (std::abs(point.w - 1.0) >
            64.0 * std::numeric_limits<double>::epsilon())
            return true;
    return false;
}

bool AcGeNurbsSurface3d::isRationalInU() const
{
    validate();
    constexpr double relativeTolerance =
        64.0 * std::numeric_limits<double>::epsilon();
    for (size_t v = 0; v < controlPointCountV; ++v)
    {
        const double reference =
            controlPoints[v].w;
        for (size_t u = 1; u < controlPointCountU; ++u)
        {
            const double weight = controlPoints[u * controlPointCountV + v].w;
            if (std::abs(weight - reference) >
                relativeTolerance *
                    std::max({1.0, std::abs(weight), std::abs(reference)}))
                return true;
        }
    }
    return false;
}

bool AcGeNurbsSurface3d::isRationalInV() const
{
    validate();
    constexpr double relativeTolerance =
        64.0 * std::numeric_limits<double>::epsilon();
    for (size_t u = 0; u < controlPointCountU; ++u)
    {
        const size_t row = u * controlPointCountV;
        const double reference = controlPoints[row].w;
        for (size_t v = 1; v < controlPointCountV; ++v)
        {
            const double weight = controlPoints[row + v].w;
            if (std::abs(weight - reference) >
                relativeTolerance *
                    std::max({1.0, std::abs(weight), std::abs(reference)}))
                return true;
        }
    }
    return false;
}

namespace
{
bool nurbsSurfaceIsPeriodic(const AcGeNurbsSurface3d &surface, bool inU,
                            double &period, const AcGeTol &tol)
{
    surface.validate();
    const double lower =
        inU ? surface.minParameterU() : surface.minParameterV();
    const double upper =
        inU ? surface.maxParameterU() : surface.maxParameterV();
    if (!(lower < upper))
        return false;

    const double otherLower =
        inU ? surface.minParameterV() : surface.minParameterU();
    const double otherUpper =
        inU ? surface.maxParameterV() : surface.maxParameterU();
    const double otherSpan = otherUpper - otherLower;
    const double samples[] = {otherLower, otherLower + otherSpan * 0.25,
                              otherLower + otherSpan * 0.5,
                              otherLower + otherSpan * 0.75, otherUpper};
    for (double other : samples)
    {
        const double u0 = inU ? lower : other;
        const double v0 = inU ? other : lower;
        const double u1 = inU ? upper : other;
        const double v1 = inU ? other : upper;
        const auto start = surface.derivativesAt(u0, v0, 1, 1);
        const auto end = surface.derivativesAt(u1, v1, 1, 1);
        const AcGeVector3d &tangentStart = inU ? start[1][0] : start[0][1];
        const AcGeVector3d &tangentEnd = inU ? end[1][0] : end[0][1];
        if ((start[0][0] - end[0][0]).length() > tol.equalPoint() ||
            !tangentStart.isEqualTo(tangentEnd, tol))
            return false;
    }
    period = upper - lower;
    return true;
}
} // namespace

bool AcGeNurbsSurface3d::isPeriodicInU(double &period,
                                      const AcGeTol &tol) const
{
    return nurbsSurfaceIsPeriodic(*this, true, period, tol);
}

bool AcGeNurbsSurface3d::isPeriodicInV(double &period,
                                      const AcGeTol &tol) const
{
    return nurbsSurfaceIsPeriodic(*this, false, period, tol);
}

void AcGeNurbsSurface3d::getDefinitionData(
    size_t &degreeInU, size_t &degreeInV, size_t &controlPointCountInU,
    size_t &controlPointCountInV, bool &rationalInU, bool &rationalInV,
    AcGePoint3dArray &points, AcGeDoubleArray &weights,
    AcGeKnotVector &knotsInU, AcGeKnotVector &knotsInV) const
{
    validate();
    degreeInU = degreeU;
    degreeInV = degreeV;
    controlPointCountInU = this->controlPointCountU;
    controlPointCountInV = this->controlPointCountV;
    rationalInU = isRationalInU();
    rationalInV = isRationalInV();
    const bool rational = isRational();
    points.clear();
    weights.clear();
    points.reserve(controlPoints.size());
    if (rational)
        weights.reserve(controlPoints.size());
    for (const AcGePoint4d &point : controlPoints)
    {
        points.emplace_back(point.x, point.y, point.z);
        if (rational)
            weights.push_back(point.w);
    }
    knotsInU = AcGeKnotVector(AcGeDoubleArray(knotsU.begin(), knotsU.end()));
    knotsInV = AcGeKnotVector(AcGeDoubleArray(knotsV.begin(), knotsV.end()));
}

} // namespace ge
