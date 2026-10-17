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
    // Derivatives of the B-spline basis via the standard identity
    //   D(k, q, i) = q/(U[i+q]-U[i]) * D(k-1, q-1, i)
    //              - q/(U[i+q+1]-U[i+1]) * D(k-1, q-1, i+1)
    // with D(0, q, i) the Cox-de Boor basis value N_{i,q}(u) and the
    // 0/0 -> 0 convention for repeated knots.  D(k, q, i) is the k-th
    // derivative of N_{i,q}; the caller receives ders[k][j] =
    // D(k, p, span - p + j).  Validated against exact Bezier derivative
    // answers in AcGeNurbCurveTests.
    const size_t p = degree;
    if (derivativeCount > p)
        throw std::runtime_error(
            "B-spline basis derivatives beyond the degree are zero.");
    if (p >= 32)
        throw std::runtime_error("B-spline degree too large.");

    // table[k][q] holds a window over global control indices
    // [span - q, span]; entry access outside that window is zero.
    std::vector<std::vector<double>> table(
        (derivativeCount + 1) * (p + 1),
        std::vector<double>(2 * p + 1, 0.0));
    auto at = [&](size_t k, size_t q, long long g) -> double & {
        const long long low = static_cast<long long>(span) -
                              static_cast<long long>(q);
        return table[k * (p + 1) + q]
                    [static_cast<size_t>(g - low)];
    };
    auto get = [&](size_t k, size_t q, long long g) -> double {
        const long long low = static_cast<long long>(span) -
                              static_cast<long long>(q);
        if (g < low || g > static_cast<long long>(span))
            return 0.0;
        return table[k * (p + 1) + q]
                    [static_cast<size_t>(g - low)];
    };

    // Degree 0: N_{span,0}(u) = 1.
    at(0, 0, static_cast<long long>(span)) = 1.0;
    // Cox-de Boor recursion for every degree level q.
    for (size_t q = 1; q <= p; ++q)
    {
        for (long long g = static_cast<long long>(span) -
                           static_cast<long long>(q);
             g <= static_cast<long long>(span); ++g)
        {
            const size_t i = static_cast<size_t>(g);
            double acc = 0.0;
            const double left = knots[i + q] - knots[i];
            if (left > 0.0)
                acc += (u - knots[i]) / left * get(0, q - 1, g);
            const double right = knots[i + q + 1] - knots[i + 1];
            if (right > 0.0)
                acc += (knots[i + q + 1] - u) / right *
                       get(0, q - 1, g + 1);
            at(0, q, g) = acc;
        }
    }

    // Derivative recursion.
    for (size_t k = 1; k <= derivativeCount; ++k)
    {
        for (size_t q = k; q <= p; ++q)
        {
            for (long long g = static_cast<long long>(span) -
                               static_cast<long long>(q);
                 g <= static_cast<long long>(span); ++g)
            {
                const size_t i = static_cast<size_t>(g);
                double acc = 0.0;
                const double left = knots[i + q] - knots[i];
                if (left > 0.0)
                    acc += static_cast<double>(q) / left *
                           get(k - 1, q - 1, g);
                const double right = knots[i + q + 1] - knots[i + 1];
                if (right > 0.0)
                    acc -= static_cast<double>(q) / right *
                           get(k - 1, q - 1, g + 1);
                at(k, q, g) = acc;
            }
        }
    }

    ders.assign(derivativeCount + 1,
                std::vector<double>(p + 1, 0.0));
    for (size_t k = 0; k <= derivativeCount; ++k)
    {
        for (size_t j = 0; j <= p; ++j)
        {
            const long long g = static_cast<long long>(span) -
                                static_cast<long long>(p) +
                                static_cast<long long>(j);
            ders[k][j] = get(k, p, g);
        }
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
