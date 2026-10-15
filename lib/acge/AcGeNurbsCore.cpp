#include "acge/AcGeNurbsCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ge
{

size_t findKnotSpan(size_t degree, const std::vector<double> &knots,
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

void basisFunctions(size_t span, size_t degree,
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

void basisFunctionsDers(size_t span, size_t degree,
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

bool isValidKnotVector(const std::vector<double> &knots)
{
    for (size_t i = 1; i < knots.size(); ++i)
        if (knots[i] < knots[i - 1])
            return false;
    return knots.size() >= 2;
}

bool isValidBspline(size_t degree, size_t knotCount,
                    size_t controlPointCount)
{
    return knotCount == controlPointCount + degree + 1;
}

std::vector<double> uniformClampedKnotVector(size_t controlPointCount,
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

std::vector<double> averagedClampedKnotVector(
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

} // namespace ge
