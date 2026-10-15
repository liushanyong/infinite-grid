#pragma once

// NURBS core for the AcGe layer: knot-vector utilities and B-spline basis
// functions, following "The NURBS Book" 2nd edition algorithm numbering
// (A2.1 FindSpan, A2.2 BasisFuns, A2.3 DersBasisFuns, A9.1 averaging).
// All algorithms are reimplemented from the book's pseudocode (the LNLib
// reference in external/ is LGPL and is only consulted for structure).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ge
{

// A2.1: determine the knot span index i such that U[i] <= u < U[i+1].
inline size_t findKnotSpan(size_t degree, const std::vector<double> &knots,
                           double u)
{
    const size_t n = knots.size() - degree - 2; // last control-point index
    if (u >= knots[n + 1])
        return n;
    if (u <= knots[degree])
        return degree;

    size_t low = degree;
    size_t high = n + 1;
    size_t mid = (low + high) / 2;
    while (u < knots[mid] || u >= knots[mid + 1])
    {
        if (u < knots[mid])
            high = mid;
        else
            low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

// A2.2: compute the non-vanishing basis functions N[span-degree..span].
inline void basisFunctions(size_t span, size_t degree,
                           const std::vector<double> &knots, double u,
                           double *outBasis)
{
    double left[32];
    double right[32];
    if (degree >= 32)
        throw std::runtime_error("B-spline degree too large.");
    outBasis[0] = 1.0;
    for (size_t j = 1; j <= degree; ++j)
    {
        left[j] = u - knots[span + 1 - j];
        right[j] = knots[span + j] - u;
        double saved = 0.0;
        for (size_t r = 0; r < j; ++r)
        {
            const double temp = outBasis[r] / (right[r + 1] + left[j - r]);
            outBasis[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        outBasis[j] = saved;
    }
}

// A2.3: basis functions and their derivatives up to the n-th derivative.
// ders[k][i] is the k-th derivative of the i-th basis function.
inline void basisFunctionsDers(size_t span, size_t degree,
                               const std::vector<double> &knots, double u,
                               size_t derivativeCount,
                               std::vector<std::vector<double>> &ders)
{
    const size_t p = degree;
    double ndu[33][33];
    double left[33];
    double right[33];
    if (p >= 32)
        throw std::runtime_error("B-spline degree too large.");

    ndu[0][0] = 1.0;
    for (size_t j = 1; j <= p; ++j)
    {
        left[j] = u - knots[span + 1 - j];
        right[j] = knots[span + j] - u;
        double saved = 0.0;
        for (size_t r = 0; r < j; ++r)
        {
            ndu[j][r] = right[r + 1] + left[j - r];
            const double temp = ndu[r + 1][j - 1] / ndu[j][r];
            ndu[r + 1][j] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        ndu[j][j] = saved;
    }
    for (size_t j = 0; j <= p; ++j)
        ders[0][j] = ndu[j][p];

    // Derivatives via the product rule on the triangular table.
    double a[2][33];
    for (size_t r = 0; r <= p; ++r)
    {
        size_t s1 = 0;
        size_t s2 = 1;
        a[0][0] = 1.0;
        for (size_t k = 1; k <= derivativeCount; ++k)
        {
            double d = 0.0;
            const size_t rk = r - k;
            const size_t pk = p - k;
            if (r >= k)
            {
                a[s2][0] = a[s1][0] / ndu[pk + 1][rk];
                d = a[s2][0] * ndu[pk][rk];
            }
            const size_t j1 = rk >= 1 ? 1 : -rk;
            const size_t j2 = static_cast<size_t>(
                static_cast<long long>(r) - 1 <= pk
                    ? r - 1
                    : static_cast<long long>(pk));
            for (size_t j = j1; j <= j2; ++j)
            {
                a[s2][j] = (a[s1][j] - a[s1][j - 1]) / ndu[pk + 1][rk + j];
                d += a[s2][j] * ndu[pk][rk + j];
            }
            if (r <= pk)
            {
                a[s2][r] = -a[s1][r - 1] / ndu[pk + 1][r];
                d += a[s2][r] * ndu[pk][r];
            }
            ders[k][r] = d;
            std::swap(s1, s2);
        }
    }
    // Multiply by the degree factors.
    size_t r = p;
    for (size_t k = 1; k <= derivativeCount; ++k)
    {
        for (size_t j = 0; j <= p; ++j)
            ders[k][j] *= static_cast<double>(r);
        r *= p - k;
    }
}

// Knot-vector validation: non-decreasing sequence.
inline bool isValidKnotVector(const std::vector<double> &knots)
{
    for (size_t i = 1; i < knots.size(); ++i)
        if (knots[i] < knots[i - 1])
            return false;
    return knots.size() >= 2;
}

// B-spline validity: m = n + p + 1, where m is the last knot index.
inline bool isValidBspline(size_t degree, size_t knotCount,
                           size_t controlPointCount)
{
    return knotCount == controlPointCount + degree + 1;
}

// Clamped uniform knot vector for n+1 control points of the given degree.
inline std::vector<double> uniformClampedKnotVector(size_t controlPointCount,
                                                    size_t degree)
{
    if (controlPointCount < degree + 1)
        throw std::runtime_error("Control point count below degree+1.");
    const size_t knotCount = controlPointCount + degree + 1;
    std::vector<double> knots(knotCount, 0.0);
    const size_t internalCount = controlPointCount - degree - 1;
    for (size_t i = 0; i < internalCount; ++i)
        knots[degree + 1 + i] = static_cast<double>(i + 1);
    for (size_t i = 0; i <= degree; ++i)
        knots[knotCount - 1 - i] = static_cast<double>(internalCount + 1);
    return knots;
}

// A9.1 averaging: knot vector from chord/parametric distances.  The input
// is the cumulative parameter per point (bar[d] in the book); the result
// matches the clamped averaging construction.
inline std::vector<double> averagedClampedKnotVector(
    const std::vector<double> &bar, size_t controlPointCount, size_t degree)
{
    const size_t n = controlPointCount - 1; // last control-point index
    const size_t knotCount = controlPointCount + degree + 1;
    std::vector<double> knots(knotCount, 0.0);
    for (size_t i = 0; i <= degree; ++i)
        knots[knotCount - 1 - i] = 1.0;

    for (size_t j = 1; j <= n - degree; ++j)
    {
        double sum = 0.0;
        for (size_t i = j; i <= j + degree - 1; ++i)
            sum += bar[i] - bar[i - 1];
        knots[j + degree] = sum / static_cast<double>(degree);
    }
    return knots;
}

// Distance metric for parameter generation: chord length (alpha=0) or
// centripetal (alpha=0.5).  Returns the normalized cumulative parameter
// bar[0..n] with bar[0]=0 and bar[n]=1 (NURBS Book eq. 9.4/9.5).
// PointT must expose .x/.y/.z (AcGePoint3d).
template <typename PointT>
inline std::vector<double> normalizedCumulativeParameters(
    const std::vector<PointT> &points, double alpha)
{
    const size_t n = points.size() - 1;
    std::vector<double> bar(points.size(), 0.0);
    double total = 0.0;
    for (size_t k = 1; k <= n; ++k)
    {
        const double dx = points[k].x - points[k - 1].x;
        const double dy = points[k].y - points[k - 1].y;
        const double dz = points[k].z - points[k - 1].z;
        total += std::pow(dx * dx + dy * dy + dz * dz, alpha * 0.5);
        bar[k] = total;
    }
    if (total > 0.0)
        for (size_t k = 1; k <= n; ++k)
            bar[k] /= total;
    return bar;
}

} // namespace ge
