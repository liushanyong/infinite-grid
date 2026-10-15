#pragma once

#include "AcGePort.h"

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
size_t findKnotSpan(size_t degree, const std::vector<double> &knots,
                    double u);

// A2.2: compute the non-vanishing basis functions N[span-degree..span].
void basisFunctions(size_t span, size_t degree,
                    const std::vector<double> &knots, double u,
                    double *outBasis);

// A2.3: basis functions and their derivatives up to the n-th derivative.
// ders[k][i] is the k-th derivative of the i-th basis function.
void basisFunctionsDers(size_t span, size_t degree,
                        const std::vector<double> &knots, double u,
                        size_t derivativeCount,
                        std::vector<std::vector<double>> &ders);

// Knot-vector validation: non-decreasing sequence.
bool isValidKnotVector(const std::vector<double> &knots);

// B-spline validity: m = n + p + 1, where m is the last knot index.
bool isValidBspline(size_t degree, size_t knotCount,
                    size_t controlPointCount);

// Clamped uniform knot vector for n+1 control points of the given degree.
std::vector<double> uniformClampedKnotVector(size_t controlPointCount,
                                             size_t degree);

// A9.1 averaging: knot vector from chord/parametric distances.  The input
// is the cumulative parameter per point (bar[d] in the book); the result
// matches the clamped averaging construction.
std::vector<double> averagedClampedKnotVector(
    const std::vector<double> &bar, size_t controlPointCount, size_t degree);

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
