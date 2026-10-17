#include "acge/AcGeNurbCurve.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "AcGeNurbsCore.h"
#include "AcGeNurbsSpline.h"

namespace
{

constexpr int kMinDegree = 1;
constexpr int kMaxDegree = 31; // A2.3 fixed tables support up to 32 spans
constexpr size_t kArcLengthQuadratureOrder = 24;

bool isFinite(double v)
{
    return std::isfinite(v);
}

double nurbsCurveLength(const ge::detail::NurbsCurveCore &curve,
                        double fromParam, double toParam)
{
    if (curve.degree < 1 || curve.evalKnots.empty() ||
        !(curve.u0 < curve.u1))
        return 0.0;

    if (fromParam > toParam)
        std::swap(fromParam, toParam);
    double start = std::max(fromParam, curve.u0);
    const double end = std::min(toParam, curve.u1);
    if (!(start < end))
        return 0.0;

    const auto quadrature = ge::gaussLegendreNodes(kArcLengthQuadratureOrder);
    double length = 0.0;
    while (start < end)
    {
        const auto nextKnot =
            std::upper_bound(curve.evalKnots.begin(), curve.evalKnots.end(),
                             start);
        const double spanEnd =
            nextKnot == curve.evalKnots.end()
                ? end
                : std::min(end, *nextKnot);
        if (!(start < spanEnd))
            break;

        const double halfSpan = (spanEnd - start) * 0.5;
        for (size_t i = 0; i < quadrature.first.size(); ++i)
        {
            const double param =
                start + halfSpan * (quadrature.first[i] + 1.0);
            std::vector<std::array<double, 4>> derivatives;
            curve.homogeneousDerivatives(param, 1, derivatives);
            const double dx = derivatives[1][0];
            const double dy = derivatives[1][1];
            const double dz = derivatives[1][2];
            length += halfSpan * quadrature.second[i] *
                      std::hypot(dx, dy, dz);
        }
        start = spanEnd;
    }
    return length;
}

size_t wrappedIndex(long long raw, size_t n)
{
    const long long m = static_cast<long long>(n);
    const long long wrapped = ((raw % m) + m) % m;
    return static_cast<size_t>(wrapped);
}

} // namespace

namespace ge
{
namespace detail
{

void NurbsCurveCore::rebuildEvalData()
{
    const size_t n = cx.size();
    const int p = degree;

    if (periodic)
    {
        // Unroll the wrapped format: unclamped uniform knots padded by p on
        // each side; control point j maps to P[(j - p) mod N].
        const double uStart = knots.front();
        const double uEnd = knots.back();
        const double span = uEnd - uStart;
        const double step = span / static_cast<double>(n);
        const size_t paddedCtrlCount = n + static_cast<size_t>(p);
        evalKnots.resize(paddedCtrlCount + static_cast<size_t>(p) + 1);
        for (size_t k = 0; k < evalKnots.size(); ++k)
        {
            evalKnots[k] = uStart + (static_cast<double>(k) - static_cast<double>(p)) * step;
        }
        ex.resize(paddedCtrlCount);
        ey.resize(paddedCtrlCount);
        ez.resize(paddedCtrlCount);
        ew.resize(paddedCtrlCount);
        for (size_t j = 0; j < paddedCtrlCount; ++j)
        {
            const size_t src = wrappedIndex(
                static_cast<long long>(j) - p, n);
            ex[j] = cx[src];
            ey[j] = cy[src];
            ez[j] = cz[src];
            ew[j] = rational ? weights[src] : 1.0;
        }
        u0 = uStart;
        u1 = uEnd;
        return;
    }

    evalKnots = knots;
    const size_t m = n;
    ex.assign(cx.begin(), cx.end());
    ey.assign(cy.begin(), cy.end());
    ez.assign(cz.begin(), cz.end());
    ew.resize(m);
    for (size_t i = 0; i < m; ++i)
        ew[i] = rational ? weights[i] : 1.0;
    u0 = evalKnots[static_cast<size_t>(degree)];
    u1 = evalKnots[evalKnots.size() - 1 - static_cast<size_t>(degree)];
}

void NurbsCurveCore::build(int degreeIn, const std::vector<double> &knotsIn,
                           const std::vector<double> &px,
                           const std::vector<double> &py,
                           const std::vector<double> &pz,
                           const std::vector<double> &wIn,
                           bool periodicIn, bool rationalIn)
{
    if (degreeIn < kMinDegree || degreeIn > kMaxDegree)
        throw std::invalid_argument(
            "AcGeNurbCurve degree must be in [1, 31].");
    const int p = degreeIn;
    const size_t n = px.size();
    if (n == 0 || py.size() != n || pz.size() != n)
        throw std::invalid_argument(
            "AcGeNurbCurve control point arrays must be nonempty and match.");
    if (rationalIn && (wIn.size() != n || n == 0))
        throw std::invalid_argument(
            "AcGeNurbCurve weights must match control points.");
    for (size_t i = 0; i < n; ++i)
    {
        if (!isFinite(px[i]) || !isFinite(py[i]) || !isFinite(pz[i]))
            throw std::invalid_argument(
                "AcGeNurbCurve control points must be finite.");
    }
    if (rationalIn)
    {
        for (size_t i = 0; i < n; ++i)
        {
            if (!isFinite(wIn[i]) || wIn[i] <= 0.0)
                throw std::invalid_argument(
                    "AcGeNurbCurve weights must be finite and positive.");
        }
    }
    for (size_t i = 1; i < knotsIn.size(); ++i)
    {
        if (!isFinite(knotsIn[i]) || knotsIn[i] < knotsIn[i - 1])
            throw std::invalid_argument(
                "AcGeNurbCurve knots must be nondecreasing finite values.");
    }

    std::vector<double> knots = knotsIn;
    std::vector<double> pxn = px, pyn = py, pzn = pz;
    std::vector<double> wNormalized = wIn;
    bool periodic = periodicIn;

    if (periodic)
    {
        if (knots.size() == n && n >= 2)
        {
            // Format 2: duplicated endpoint control point; normalize to the
            // wrapped format by dropping the duplicate.
            const bool identical = std::abs(px.front() - px.back()) <= 1.0e-12 &&
                                   std::abs(pyn.front() - pyn.back()) <= 1.0e-12 &&
                                   std::abs(pzn.front() - pzn.back()) <= 1.0e-12;
            if (!identical)
                throw std::invalid_argument(
                    "AcGeNurbCurve periodic format 2 requires identical "
                    "first/last control points.");
            pxn.pop_back();
            pyn.pop_back();
            pzn.pop_back();
            if (rationalIn)
            {
                if (std::abs(wIn.front() - wIn.back()) > 1.0e-12)
                    throw std::invalid_argument(
                        "AcGeNurbCurve periodic format 2 requires identical "
                        "first/last weights.");
                wNormalized = wIn;
                wNormalized.pop_back();
            }
            // Knot count now equals (control point count - 1) + 1.
        }
        const size_t wrappedCount = pxn.size();
        if (knots.size() != wrappedCount + 1)
            throw std::invalid_argument(
                "AcGeNurbCurve periodic format 1 requires knot count == "
                "control point count + 1.");
        if (wrappedCount < 2 ||
            wrappedCount < static_cast<size_t>(p) + 1)
            throw std::invalid_argument(
                "AcGeNurbCurve periodic curves need more control points.");
        const bool duplicatedEnds =
            std::abs(pxn.front() - pxn.back()) <= 1.0e-12 &&
            std::abs(pyn.front() - pyn.back()) <= 1.0e-12 &&
            std::abs(pzn.front() - pzn.back()) <= 1.0e-12;
        if (duplicatedEnds)
            throw std::invalid_argument(
                "AcGeNurbCurve periodic format 1 requires distinct "
                "first/last control points.");
        // The wrapped format is unrolled to an unclamped uniform curve, so
        // the knot span must be uniform (ARX documents distinct knots; the
        // documented example is uniform, matching LNLib/OCC conventions).
        const double span = knots.back() - knots.front();
        if (!(span > 0.0))
            throw std::invalid_argument(
                "AcGeNurbCurve periodic knot span must be positive.");
        const double step = span / static_cast<double>(wrappedCount);
        for (size_t i = 1; i < knots.size(); ++i)
        {
            if (std::abs((knots[i] - knots[i - 1]) - step) >
                1.0e-9 * std::max(1.0, std::abs(step)))
                throw std::invalid_argument(
                    "AcGeNurbCurve periodic knots must be uniform.");
        }
    }
    else
    {
        if (knots.size() != pxn.size() + static_cast<size_t>(p) + 1)
            throw std::invalid_argument(
                "AcGeNurbCurve non-periodic knot count must be control "
                "point count + degree + 1.");
    }

    degree = p;
    this->periodic = periodicIn;
    rational = rationalIn;
    this->knots = knots;
    cx = pxn;
    cy = pyn;
    cz = pzn;
    weights = rationalIn ? wNormalized : std::vector<double>{};
    rebuildEvalData();


    if (!ge::isValidBspline(
            static_cast<size_t>(degree), evalKnots.size(), ex.size()))
        throw std::invalid_argument(
            "AcGeNurbCurve knot/control data does not form a valid "
            "B-spline.");
}

void NurbsCurveCore::setControlPoint(size_t i, double x, double y, double z)
{
    if (i >= cx.size())
        throw std::out_of_range("AcGeNurbCurve control point index out of range.");
    cx[i] = x;
    cy[i] = y;
    cz[i] = z;
    rebuildEvalData();
}

void NurbsCurveCore::setWeight(size_t i, double w)
{
    if (!rational)
        throw std::logic_error(
            "AcGeNurbCurve::setWeightAt requires a rational curve.");
    if (i >= weights.size())
        throw std::out_of_range("AcGeNurbCurve weight index out of range.");
    if (!isFinite(w) || w <= 0.0)
        throw std::invalid_argument(
            "AcGeNurbCurve weights must be finite and positive.");
    weights[i] = w;
    rebuildEvalData();
}

void NurbsCurveCore::homogeneousDerivatives(
    double u, size_t derivativeCount,
    std::vector<std::array<double, 4>> &out) const
{
    if (derivativeCount > static_cast<size_t>(degree))
        throw std::invalid_argument(
            "AcGeNurbCurve derivatives are only defined up to the degree.");
    const double param = std::clamp(u, u0, u1);
    const size_t span =
        ge::findKnotSpan(static_cast<size_t>(degree), evalKnots, param);
    std::vector<std::vector<double>> ders(
        derivativeCount + 1, std::vector<double>(
                                  static_cast<size_t>(degree) + 1, 0.0));
    ge::basisFunctionsDers(span, static_cast<size_t>(degree), evalKnots,
                           param, derivativeCount, ders);

    const size_t p = static_cast<size_t>(degree);
    // A_k = d^k/du^k of the homogeneous coordinate polynomials.
    std::array<std::vector<double>, 4> a;
    for (size_t k = 0; k <= derivativeCount; ++k)
    {
        a[0].push_back(0.0);
        a[1].push_back(0.0);
        a[2].push_back(0.0);
        a[3].push_back(0.0);
        for (size_t i = 0; i <= p; ++i)
        {
            const size_t ctrl = span - p + i;
            const double basis = ders[k][i];
            // Homogeneous control points: P_i * w_i (A4.2 works on the
            // weighted coordinates; the weight column carries w_i itself).
            a[0][k] += basis * ex[ctrl] * ew[ctrl];
            a[1][k] += basis * ey[ctrl] * ew[ctrl];
            a[2][k] += basis * ez[ctrl] * ew[ctrl];
            a[3][k] += basis * ew[ctrl];
        }
    }

    // A4.2 quotient rule: C^(k) = (A^(k) - sum binom(k,i) W^(i) C^(k-i)) / W.
    out.assign(derivativeCount + 1, {0.0, 0.0, 0.0, 0.0});
    const double w0 = a[3][0];
    if (!(std::abs(w0) > 0.0))
        throw std::runtime_error("AcGeNurbCurve homogeneous weight is zero.");
    for (size_t k = 0; k <= derivativeCount; ++k)
    {
        std::array<double, 4> c = {a[0][k], a[1][k], a[2][k], a[3][k]};
        double binomial = 1.0;
        for (size_t i = 1; i <= k; ++i)
        {
            binomial = binomial * static_cast<double>(k - i + 1) /
                       static_cast<double>(i);
            // A4.2: subtract binom(k,i) * W^(i) * C^(k-i), where W is the
            // homogeneous weight component (index 3), not A itself.
            for (size_t comp = 0; comp < 3; ++comp)
                c[comp] -= binomial * a[3][i] * out[k - i][comp];
        }
        for (size_t comp = 0; comp < 4; ++comp)
            out[k][comp] = c[comp] / w0;
    }
}

double removalTolerance(const NurbsCurveCore &core)
{
    double scale = 1.0;
    for (size_t i = 0; i < core.cx.size(); ++i)
    {
        scale = std::max(scale, std::abs(core.cx[i]));
        scale = std::max(scale, std::abs(core.cy[i]));
        scale = std::max(scale, std::abs(core.cz[i]));
    }
    return 1.0e-9 * scale;
}

size_t NurbsCurveCore::knotMultiplicity(double u) const
{
    size_t count = 0;
    for (double knot : knots)
    {
        if (knot == u)
            ++count;
    }
    return count;
}

void NurbsCurveCore::insertKnot(double u)
{
    if (periodic)
        throw std::logic_error(
            "AcGeNurbCurve structural editing requires a non-periodic curve.");
    if (!isFinite(u) || u < u0 || u > u1)
        throw std::invalid_argument(
            "AcGeNurbCurve insertKnot parameter is outside the curve domain.");
    const size_t p = static_cast<size_t>(degree);
    const size_t s = knotMultiplicity(u);
    if (s >= p + 1)
        throw std::invalid_argument(
            "AcGeNurbCurve insertKnot parameter already has full knot "
            "multiplicity.");
    const size_t k = ge::findKnotSpan(p, knots, u);
    const size_t n = cx.size();

    // Boehm knot insertion (A5.1) on homogeneous control points (w*P, w):
    // a convex combination there, so the curve geometry is unchanged.
    std::vector<double> hx(n), hy(n), hz(n), hw(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double w = rational ? weights[i] : 1.0;
        hx[i] = cx[i] * w;
        hy[i] = cy[i] * w;
        hz[i] = cz[i] * w;
        hw[i] = w;
    }
    std::vector<double> qx(n + 1), qy(n + 1), qz(n + 1), qw(n + 1);
    for (size_t i = 0; i <= k - p; ++i)
    {
        qx[i] = hx[i];
        qy[i] = hy[i];
        qz[i] = hz[i];
        qw[i] = hw[i];
    }
    for (size_t i = k - p + 1; i <= k - s; ++i)
    {
        const double alpha = (u - knots[i]) / (knots[i + p] - knots[i]);
        const double beta = 1.0 - alpha;
        qx[i] = beta * hx[i - 1] + alpha * hx[i];
        qy[i] = beta * hy[i - 1] + alpha * hy[i];
        qz[i] = beta * hz[i - 1] + alpha * hz[i];
        qw[i] = beta * hw[i - 1] + alpha * hw[i];
    }
    for (size_t i = k - s; i < n; ++i)
    {
        qx[i + 1] = hx[i];
        qy[i + 1] = hy[i];
        qz[i + 1] = hz[i];
        qw[i + 1] = hw[i];
    }

    knots.insert(knots.begin() + static_cast<std::ptrdiff_t>(k) + 1, u);
    cx.resize(n + 1);
    cy.resize(n + 1);
    cz.resize(n + 1);
    for (size_t i = 0; i <= n; ++i)
    {
        cx[i] = qx[i] / qw[i];
        cy[i] = qy[i] / qw[i];
        cz[i] = qz[i] / qw[i];
    }
    if (rational)
    {
        weights.resize(n + 1);
        for (size_t i = 0; i <= n; ++i)
            weights[i] = qw[i];
    }
    rebuildEvalData();
}

bool NurbsCurveCore::removeKnot(double u, double tol)
{
    if (periodic)
        throw std::logic_error(
            "AcGeNurbCurve structural editing requires a non-periodic curve.");
    const size_t p = static_cast<size_t>(degree);
    const size_t n = cx.size();
    const size_t s = knotMultiplicity(u);
    if (s == 0 || s > p + 1)
        return false;
    size_t first = n + p + 1;
    for (size_t i = 0; i < knots.size(); ++i)
    {
        if (knots[i] == u)
        {
            first = i;
            break;
        }
    }
    const size_t last = first + s - 1; // the occurrence to remove
    if (first < p + 1 || last > n - 1)
        return false; // clamped endpoint knot blocks cannot be removed

    std::vector<double> hx(n), hy(n), hz(n), hw(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double w = rational ? weights[i] : 1.0;
        hx[i] = cx[i] * w;
        hy[i] = cy[i] * w;
        hz[i] = cz[i] * w;
        hw[i] = w;
    }
    // Knot vector with the removed occurrence deleted; the insertion
    // alphas of the inverse operation are evaluated against it (A5.2).
    std::vector<double> reducedKnots = knots;
    reducedKnots.erase(reducedKnots.begin() +
                       static_cast<std::ptrdiff_t>(last));

    std::vector<double> qx(n - 1), qy(n - 1), qz(n - 1), qw(n - 1);
    // Points before the affected window are unchanged.
    for (size_t j = 0; j <= last - p - 1; ++j)
    {
        qx[j] = hx[j];
        qy[j] = hy[j];
        qz[j] = hz[j];
        qw[j] = hw[j];
    }
    // Forward recovery chain: invert C_i = beta*Q_{i-1} + alpha*Q_i.
    for (size_t i = last - p; i <= last - s; ++i)
    {
        const double alpha = (u - knots[i]) /
                             (reducedKnots[i + p] - knots[i]);
        const double beta = 1.0 - alpha;
        qx[i] = (hx[i] - beta * qx[i - 1]) / alpha;
        qy[i] = (hy[i] - beta * qy[i - 1]) / alpha;
        qz[i] = (hz[i] - beta * qz[i - 1]) / alpha;
        qw[i] = (hw[i] - beta * qw[i - 1]) / alpha;
    }
    // Points after the affected window shift left by one.
    for (size_t j = last - s + 1; j + 1 < n; ++j)
    {
        qx[j] = hx[j + 1];
        qy[j] = hy[j + 1];
        qz[j] = hz[j + 1];
        qw[j] = hw[j + 1];
    }

    // Closing consistency check: the recovered point must match the
    // original point that follows it; otherwise the knot is not removable
    // exactly and the curve is left untouched.
    const size_t seam = last - s;
    const double ddx = qx[seam] / qw[seam] - hx[seam + 1] / hw[seam + 1];
    const double ddy = qy[seam] / qw[seam] - hy[seam + 1] / hw[seam + 1];
    const double ddz = qz[seam] / qw[seam] - hz[seam + 1] / hw[seam + 1];
    if (!(std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz) <= tol) ||
        !(std::abs(qw[seam] - hw[seam + 1]) <= tol))
        return false;

    knots.erase(knots.begin() + static_cast<std::ptrdiff_t>(last));
    cx.resize(n - 1);
    cy.resize(n - 1);
    cz.resize(n - 1);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        cx[i] = qx[i] / qw[i];
        cy[i] = qy[i] / qw[i];
        cz[i] = qz[i] / qw[i];
    }
    if (rational)
    {
        weights.resize(n - 1);
        for (size_t i = 0; i + 1 < n; ++i)
            weights[i] = qw[i];
    }
    rebuildEvalData();
    return true;
}

void NurbsCurveCore::elevateDegree(int times)
{
    if (periodic)
        throw std::logic_error(
            "AcGeNurbCurve structural editing requires a non-periodic curve.");
    if (times < 0)
        throw std::invalid_argument(
            "AcGeNurbCurve elevateDegree requires a non-negative increment.");
    if (times == 0)
        return;
    const size_t p = static_cast<size_t>(degree);
    const size_t n = cx.size();

    // Interior knot values with multiplicities (the Bezier decomposition
    // below requires multiplicity <= degree).
    std::vector<std::pair<double, size_t>> interior;
    for (size_t i = p + 1; i < n;)
    {
        size_t mult = 0;
        size_t j = i;
        while (j < n && knots[j] == knots[i])
        {
            ++mult;
            ++j;
        }
        if (mult > p)
            throw std::invalid_argument(
                "AcGeNurbCurve elevateDegree requires interior knot "
                "multiplicity <= degree.");
        interior.emplace_back(knots[i], mult);
        i = j;
    }

    // 1) Bezier decomposition: insert every interior knot to multiplicity
    // p. Segment i then uses control points [i*p, i*p + p], and adjacent
    // segments share their junction control point.
    for (const auto &entry : interior)
    {
        while (knotMultiplicity(entry.first) < p)
            insertKnot(entry.first);
    }

    // 2) Bernstein degree elevation applied to each segment; exact on
    // homogeneous control points (A5.6-style elevation, times steps).
    const size_t segCount = interior.size() + 1;
    const size_t q = p + static_cast<size_t>(times);
    std::vector<std::vector<std::array<double, 4>>> segs(segCount);
    for (size_t i = 0; i < segCount; ++i)
    {
        auto &pts = segs[i];
        pts.reserve(q + 1);
        for (size_t j = 0; j <= p; ++j)
        {
            const size_t idx = i * p + j;
            const double w = rational ? weights[idx] : 1.0;
            pts.push_back({cx[idx] * w, cy[idx] * w, cz[idx] * w, w});
        }
        for (int t = 0; t < times; ++t)
        {
            const size_t d = pts.size() - 1;
            std::vector<std::array<double, 4>> next(d + 2);
            for (size_t j = 0; j <= d + 1; ++j)
            {
                std::array<double, 4> v{};
                if (j > 0)
                {
                    const double a = static_cast<double>(j) /
                                     static_cast<double>(d + 1);
                    for (size_t c = 0; c < 4; ++c)
                        v[c] += a * pts[j - 1][c];
                }
                if (j <= d)
                {
                    const double b = 1.0 - static_cast<double>(j) /
                                               static_cast<double>(d + 1);
                    for (size_t c = 0; c < 4; ++c)
                        v[c] += b * pts[j][c];
                }
                next[j] = v;
            }
            pts = std::move(next);
        }
    }

    // 3) Merge segments: clamped ends at multiplicity q + 1, junction
    // knots at multiplicity q (C0), dropping duplicated junction points.
    std::vector<double> mergedKnots;
    mergedKnots.reserve(2 * (q + 1) + interior.size() * q);
    mergedKnots.insert(mergedKnots.end(), q + 1, knots.front());
    for (const auto &entry : interior)
        mergedKnots.insert(mergedKnots.end(), q, entry.first);
    mergedKnots.insert(mergedKnots.end(), q + 1, knots.back());

    std::vector<double> nx, ny, nz, nw;
    const size_t ctrlCount = (q + 1) + (segCount - 1) * q;
    nx.reserve(ctrlCount);
    ny.reserve(ctrlCount);
    nz.reserve(ctrlCount);
    nw.reserve(ctrlCount);
    for (size_t i = 0; i < segCount; ++i)
    {
        for (size_t j = (i == 0 ? 0 : 1); j <= q; ++j)
        {
            nx.push_back(segs[i][j][0]);
            ny.push_back(segs[i][j][1]);
            nz.push_back(segs[i][j][2]);
            nw.push_back(segs[i][j][3]);
        }
    }

    // The segment points are homogeneous (w*P, w); store the Cartesian
    // coordinates and the weight column separately.
    for (size_t i = 0; i < ctrlCount; ++i)
    {
        nx[i] /= nw[i];
        ny[i] /= nw[i];
        nz[i] /= nw[i];
    }
    degree = static_cast<int>(q);
    knots = std::move(mergedKnots);
    cx = std::move(nx);
    cy = std::move(ny);
    cz = std::move(nz);
    if (rational)
        weights = std::move(nw);
    rebuildEvalData();

    // 4) Reduce junction multiplicities to (original multiplicity + times).
    // Degree elevation preserves each junction continuity class, so every
    // removal step is mathematically exact.
    const double tol = removalTolerance(*this);
    for (const auto &entry : interior)
    {
        const size_t target = entry.second + static_cast<size_t>(times);
        while (knotMultiplicity(entry.first) > target)
        {
            if (!removeKnot(entry.first, tol))
                throw std::runtime_error(
                    "AcGeNurbCurve elevateDegree knot reduction failed.");
        }
    }
}

void NurbsCurveCore::hardTrim(double newStart, double newEnd)
{
    if (periodic)
        throw std::logic_error(
            "AcGeNurbCurve structural editing requires a non-periodic curve.");
    if (!isFinite(newStart) || !isFinite(newEnd))
        throw std::invalid_argument(
            "AcGeNurbCurve hardTrim parameters must be finite.");
    if (newStart < u0 || newEnd > u1)
        throw std::invalid_argument(
            "AcGeNurbCurve hardTrim range is outside the curve domain.");
    if (newStart >= newEnd)
        throw std::invalid_argument(
            "AcGeNurbCurve hardTrim requires newStart < newEnd.");
    const size_t p = static_cast<size_t>(degree);
    while (knotMultiplicity(newStart) < p + 1)
        insertKnot(newStart);
    while (knotMultiplicity(newEnd) < p + 1)
        insertKnot(newEnd);

    // With both cut parameters at multiplicity p + 1 the retained control
    // points are [firstStart, firstEnd], and the new knots are the two
    // clamped blocks plus whatever lay strictly between them.
    size_t firstStart = 0;
    while (firstStart < knots.size() && knots[firstStart] != newStart)
        ++firstStart;
    size_t firstEnd = 0;
    while (firstEnd < knots.size() && knots[firstEnd] != newEnd)
        ++firstEnd;
    const size_t last = firstEnd - 1;

    std::vector<double> newKnots;
    newKnots.reserve(2 * (p + 1) + (knots.size() - 2 * (p + 1)));
    newKnots.insert(newKnots.end(), p + 1, newStart);
    for (size_t i = 0; i < knots.size(); ++i)
    {
        if (knots[i] > newStart && knots[i] < newEnd)
            newKnots.push_back(knots[i]);
    }
    newKnots.insert(newKnots.end(), p + 1, newEnd);

    std::vector<double> ncx, ncy, ncz, nw;
    ncx.reserve(last - firstStart + 1);
    ncy.reserve(last - firstStart + 1);
    ncz.reserve(last - firstStart + 1);
    nw.reserve(last - firstStart + 1);
    for (size_t i = firstStart; i <= last; ++i)
    {
        ncx.push_back(cx[i]);
        ncy.push_back(cy[i]);
        ncz.push_back(cz[i]);
        nw.push_back(rational ? weights[i] : 1.0);
    }

    knots = std::move(newKnots);
    cx = std::move(ncx);
    cy = std::move(ncy);
    cz = std::move(ncz);
    if (rational)
        weights = std::move(nw);
    rebuildEvalData();
}

void NurbsCurveCore::makeRational(double weight)
{
    if (!isFinite(weight) || weight <= 0.0)
        throw std::invalid_argument(
            "AcGeNurbCurve makeRational weight must be finite and positive.");
    if (!rational)
    {
        weights.assign(cx.size(), weight);
        rational = true;
    }
    else
    {
        for (double &w : weights)
            w *= weight;
    }
    rebuildEvalData();
}

void NurbsCurveCore::join(const NurbsCurveCore &other, double tol)
{
    if (periodic || other.periodic)
        throw std::logic_error(
            "AcGeNurbCurve structural editing requires non-periodic curves.");
    if (degree != other.degree)
        throw std::invalid_argument(
            "AcGeNurbCurve joinWith requires curves of equal degree.");

    const size_t n1 = cx.size();
    const size_t n2 = other.cx.size();
    auto cornerDistance = [](const NurbsCurveCore &c, size_t a, size_t b) {
        const double dx = c.cx[a] - c.cx[b];
        const double dy = c.cy[a] - c.cy[b];
        const double dz = c.cz[a] - c.cz[b];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    if (cornerDistance(*this, 0, n1 - 1) <= tol ||
        cornerDistance(other, 0, n2 - 1) <= tol)
        throw std::invalid_argument(
            "AcGeNurbCurve joinWith requires open (non-closed) curves.");
    const double dx = cx[n1 - 1] - other.cx[0];
    const double dy = cy[n1 - 1] - other.cy[0];
    const double dz = cz[n1 - 1] - other.cz[0];
    if (!(std::sqrt(dx * dx + dy * dy + dz * dz) <= tol))
        throw std::invalid_argument(
            "AcGeNurbCurve joinWith requires coincident endpoints.");

    const size_t p = static_cast<size_t>(degree);
    // Drop the last junction occurrence of u1 from our knots and the whole
    // clamped start block of the other curve; the junction keeps
    // multiplicity p (C0 join), sharing the junction control point.
    std::vector<double> newKnots(knots.begin(), knots.end() - 1);
    const double shift = u1 - other.u0;
    for (size_t i = p + 1; i < other.knots.size(); ++i)
        newKnots.push_back(other.knots[i] + shift);

    cx.insert(cx.end(), other.cx.begin() + 1, other.cx.end());
    cy.insert(cy.end(), other.cy.begin() + 1, other.cy.end());
    cz.insert(cz.end(), other.cz.begin() + 1, other.cz.end());

    const bool rat1 = rational;
    const bool rat2 = other.rational;
    rational = rat1 || rat2;
    if (rational)
    {
        std::vector<double> newWeights;
        newWeights.reserve(n1 + n2 - 1);
        for (size_t i = 0; i < n1; ++i)
            newWeights.push_back(rat1 ? weights[i] : 1.0);
        for (size_t i = 1; i < n2; ++i)
            newWeights.push_back(rat2 ? other.weights[i] : 1.0);
        weights = std::move(newWeights);
    }
    knots = std::move(newKnots);
    rebuildEvalData();
}
} // namespace detail
} // namespace ge

// ---------------------------------------------------------------------------
// AcGeNurbCurve2d
// ---------------------------------------------------------------------------

AcGeNurbCurve2d::AcGeNurbCurve2d(int degree, const AcGeKnotVector &knots,
                                 const AcGePoint2dArray &controlPoints,
                                 bool isPeriodic)
{
    std::vector<double> k(static_cast<size_t>(knots.length()));
    for (int i = 0; i < knots.length(); ++i)
        k[static_cast<size_t>(i)] = knots[i];
    std::vector<double> px, py, pz;
    px.reserve(controlPoints.size());
    py.reserve(controlPoints.size());
    pz.assign(controlPoints.size(), 0.0);
    for (const auto &pt : controlPoints)
    {
        px.push_back(pt.x);
        py.push_back(pt.y);
    }
    m_core.build(degree, k, px, py, pz, {}, isPeriodic, false);
}

AcGeNurbCurve2d::AcGeNurbCurve2d(int degree, const AcGeKnotVector &knots,
                                 const AcGePoint2dArray &controlPoints,
                                 const AcGeDoubleArray &weights,
                                 bool isPeriodic)
{
    std::vector<double> k(static_cast<size_t>(knots.length()));
    for (int i = 0; i < knots.length(); ++i)
        k[static_cast<size_t>(i)] = knots[i];
    std::vector<double> px, py, pz;
    px.reserve(controlPoints.size());
    py.reserve(controlPoints.size());
    pz.assign(controlPoints.size(), 0.0);
    for (const auto &pt : controlPoints)
    {
        px.push_back(pt.x);
        py.push_back(pt.y);
    }
    m_core.build(degree, k, px, py, pz, weights, isPeriodic, true);
}

int AcGeNurbCurve2d::numKnots() const
{
    return static_cast<int>(m_core.knots.size());
}

double AcGeNurbCurve2d::knotAt(int i) const
{
    if (i < 0 || static_cast<size_t>(i) >= m_core.knots.size())
        throw std::out_of_range("AcGeNurbCurve2d knot index out of range.");
    return m_core.knots[static_cast<size_t>(i)];
}

int AcGeNurbCurve2d::numControlPoints() const
{
    return static_cast<int>(m_core.cx.size());
}

AcGePoint2d AcGeNurbCurve2d::controlPointAt(int i) const
{
    if (i < 0 || static_cast<size_t>(i) >= m_core.cx.size())
        throw std::out_of_range(
            "AcGeNurbCurve2d control point index out of range.");
    const size_t idx = static_cast<size_t>(i);
    return {m_core.cx[idx], m_core.cy[idx]};
}

AcGeNurbCurve2d &AcGeNurbCurve2d::setControlPointAt(int i,
                                                    const AcGePoint2d &point)
{
    if (!isFinite(point.x) || !isFinite(point.y))
        throw std::invalid_argument(
            "AcGeNurbCurve2d control points must be finite.");
    m_core.setControlPoint(static_cast<size_t>(i), point.x, point.y, 0.0);
    return *this;
}

int AcGeNurbCurve2d::numWeights() const
{
    return m_core.rational ? static_cast<int>(m_core.weights.size()) : 0;
}

double AcGeNurbCurve2d::weightAt(int i) const
{
    if (!m_core.rational)
        throw std::logic_error("AcGeNurbCurve2d is not rational.");
    if (i < 0 || static_cast<size_t>(i) >= m_core.weights.size())
        throw std::out_of_range("AcGeNurbCurve2d weight index out of range.");
    return m_core.weights[static_cast<size_t>(i)];
}

AcGeNurbCurve2d &AcGeNurbCurve2d::setWeightAt(int i, double weight)
{
    m_core.setWeight(static_cast<size_t>(i), weight);
    return *this;
}

void AcGeNurbCurve2d::getDefinitionData(int &degree, bool &rational,
                                        bool &periodic,
                                        AcGeKnotVector &knots,
                                        AcGePoint2dArray &controlPoints,
                                        AcGeDoubleArray &weights) const
{
    degree = m_core.degree;
    rational = m_core.rational;
    periodic = m_core.periodic;
    knots = AcGeKnotVector(
        static_cast<int>(m_core.knots.size()),
        m_core.knots.empty() ? nullptr : m_core.knots.data());
    controlPoints.clear();
    for (size_t i = 0; i < m_core.cx.size(); ++i)
        controlPoints.push_back({m_core.cx[i], m_core.cy[i]});
    weights = m_core.weights;
}

AcGePoint2d AcGeNurbCurve2d::pointAt(double param) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, 0, ders);
    return {ders[0][0], ders[0][1]};
}

AcGeVector2d AcGeNurbCurve2d::firstDeriv(double param) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, 1, ders);
    return {ders[1][0], ders[1][1]};
}

double AcGeNurbCurve2d::length(double fromParam, double toParam,
                               double tol) const
{
    if (!isFinite(fromParam) || !isFinite(toParam) || !isFinite(tol))
        throw std::invalid_argument(
            "AcGeNurbCurve2d length parameters must be finite.");
    const double result = nurbsCurveLength(m_core, fromParam, toParam);
    return result <= std::abs(tol) ? 0.0 : result;
}

std::vector<AcGeVector2d> AcGeNurbCurve2d::derivativesAt(
    double param, size_t derivativeCount) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, derivativeCount, ders);
    std::vector<AcGeVector2d> result;
    result.reserve(derivativeCount + 1);
    for (size_t k = 0; k <= derivativeCount; ++k)
        result.push_back({ders[k][0], ders[k][1]});
    return result;
}

double AcGeNurbCurve2d::curvatureAt(double param) const
{
    const auto d = derivativesAt(param, 2);
    const double dx = d[1].x, dy = d[1].y;
    const double ddx = d[2].x, ddy = d[2].y;
    const double speedSq = dx * dx + dy * dy;
    if (!(speedSq > 0.0))
        return 0.0;
    const double numerator = std::abs(dx * ddy - dy * ddx);
    return numerator / (speedSq * std::sqrt(speedSq));
}

bool AcGeNurbCurve2d::isClosed(const AcGeTol &tol) const
{
    return startPoint().distanceTo(endPoint()) <= tol.equalPoint();
}
// ---------------------------------------------------------------------------
// AcGeNurbCurve2d structural editing (ObjectARX genurb2d API)
// ---------------------------------------------------------------------------

AcGeNurbCurve2d &AcGeNurbCurve2d::insertKnot(double param)
{
    m_core.insertKnot(param);
    return *this;
}

AcGeNurbCurve2d &AcGeNurbCurve2d::addKnot(double param)
{
    if (m_core.knotMultiplicity(param) == 0)
        m_core.insertKnot(param);
    return *this;
}

bool AcGeNurbCurve2d::deleteControlPointAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_core.cx.size())
        throw std::out_of_range(
            "AcGeNurbCurve2d control point index out of range.");
    const double knot = m_core.knots[static_cast<size_t>(index) + 1];
    return m_core.removeKnot(knot, ge::detail::removalTolerance(m_core));
}

AcGeNurbCurve2d &AcGeNurbCurve2d::elevateDegree(int plusDegree)
{
    m_core.elevateDegree(plusDegree);
    return *this;
}

AcGeNurbCurve2d &AcGeNurbCurve2d::makeRational(double weight)
{
    m_core.makeRational(weight);
    return *this;
}

AcGeNurbCurve2d &AcGeNurbCurve2d::hardTrimByParams(double newStartParam,
                                                   double newEndParam)
{
    m_core.hardTrim(newStartParam, newEndParam);
    return *this;
}

AcGeNurbCurve2d &AcGeNurbCurve2d::joinWith(const AcGeNurbCurve2d &curve)
{
    m_core.join(curve.m_core, AcGeTol{}.equalPoint());
    return *this;
}

// ---------------------------------------------------------------------------
// AcGeNurbCurve3d
// ---------------------------------------------------------------------------

AcGeNurbCurve3d::AcGeNurbCurve3d(int degree, const AcGeKnotVector &knots,
                                 const AcGePoint3dArray &controlPoints,
                                 bool isPeriodic)
{
    std::vector<double> k(static_cast<size_t>(knots.length()));
    for (int i = 0; i < knots.length(); ++i)
        k[static_cast<size_t>(i)] = knots[i];
    std::vector<double> px, py, pz;
    px.reserve(controlPoints.size());
    py.reserve(controlPoints.size());
    pz.reserve(controlPoints.size());
    for (const auto &pt : controlPoints)
    {
        px.push_back(pt.x);
        py.push_back(pt.y);
        pz.push_back(pt.z);
    }
    m_core.build(degree, k, px, py, pz, {}, isPeriodic, false);
}

AcGeNurbCurve3d::AcGeNurbCurve3d(int degree, const AcGeKnotVector &knots,
                                 const AcGePoint3dArray &controlPoints,
                                 const AcGeDoubleArray &weights,
                                 bool isPeriodic)
{
    std::vector<double> k(static_cast<size_t>(knots.length()));
    for (int i = 0; i < knots.length(); ++i)
        k[static_cast<size_t>(i)] = knots[i];
    std::vector<double> px, py, pz;
    px.reserve(controlPoints.size());
    py.reserve(controlPoints.size());
    pz.reserve(controlPoints.size());
    for (const auto &pt : controlPoints)
    {
        px.push_back(pt.x);
        py.push_back(pt.y);
        pz.push_back(pt.z);
    }
    m_core.build(degree, k, px, py, pz, weights, isPeriodic, true);
}

int AcGeNurbCurve3d::numKnots() const
{
    return static_cast<int>(m_core.knots.size());
}

double AcGeNurbCurve3d::knotAt(int i) const
{
    if (i < 0 || static_cast<size_t>(i) >= m_core.knots.size())
        throw std::out_of_range("AcGeNurbCurve3d knot index out of range.");
    return m_core.knots[static_cast<size_t>(i)];
}

int AcGeNurbCurve3d::numControlPoints() const
{
    return static_cast<int>(m_core.cx.size());
}

AcGePoint3d AcGeNurbCurve3d::controlPointAt(int i) const
{
    if (i < 0 || static_cast<size_t>(i) >= m_core.cx.size())
        throw std::out_of_range(
            "AcGeNurbCurve3d control point index out of range.");
    const size_t idx = static_cast<size_t>(i);
    return {m_core.cx[idx], m_core.cy[idx], m_core.cz[idx]};
}

AcGeNurbCurve3d &AcGeNurbCurve3d::setControlPointAt(int i,
                                                    const AcGePoint3d &point)
{
    if (!isFinite(point.x) || !isFinite(point.y) || !isFinite(point.z))
        throw std::invalid_argument(
            "AcGeNurbCurve3d control points must be finite.");
    m_core.setControlPoint(static_cast<size_t>(i), point.x, point.y, point.z);
    return *this;
}

int AcGeNurbCurve3d::numWeights() const
{
    return m_core.rational ? static_cast<int>(m_core.weights.size()) : 0;
}

double AcGeNurbCurve3d::weightAt(int i) const
{
    if (!m_core.rational)
        throw std::logic_error("AcGeNurbCurve3d is not rational.");
    if (i < 0 || static_cast<size_t>(i) >= m_core.weights.size())
        throw std::out_of_range("AcGeNurbCurve3d weight index out of range.");
    return m_core.weights[static_cast<size_t>(i)];
}

AcGeNurbCurve3d &AcGeNurbCurve3d::setWeightAt(int i, double weight)
{
    m_core.setWeight(static_cast<size_t>(i), weight);
    return *this;
}

void AcGeNurbCurve3d::getDefinitionData(int &degree, bool &rational,
                                        bool &periodic,
                                        AcGeKnotVector &knots,
                                        AcGePoint3dArray &controlPoints,
                                        AcGeDoubleArray &weights) const
{
    degree = m_core.degree;
    rational = m_core.rational;
    periodic = m_core.periodic;
    knots = AcGeKnotVector(
        static_cast<int>(m_core.knots.size()),
        m_core.knots.empty() ? nullptr : m_core.knots.data());
    controlPoints.clear();
    for (size_t i = 0; i < m_core.cx.size(); ++i)
        controlPoints.push_back(
            {m_core.cx[i], m_core.cy[i], m_core.cz[i]});
    weights = m_core.weights;
}

AcGePoint3d AcGeNurbCurve3d::pointAt(double param) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, 0, ders);
    return {ders[0][0], ders[0][1], ders[0][2]};
}

AcGeVector3d AcGeNurbCurve3d::firstDeriv(double param) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, 1, ders);
    return {ders[1][0], ders[1][1], ders[1][2]};
}

double AcGeNurbCurve3d::length(double fromParam, double toParam,
                               double tol) const
{
    if (!isFinite(fromParam) || !isFinite(toParam) || !isFinite(tol))
        throw std::invalid_argument(
            "AcGeNurbCurve3d length parameters must be finite.");
    const double result = nurbsCurveLength(m_core, fromParam, toParam);
    return result <= std::abs(tol) ? 0.0 : result;
}

std::vector<AcGeVector3d> AcGeNurbCurve3d::derivativesAt(
    double param, size_t derivativeCount) const
{
    std::vector<std::array<double, 4>> ders;
    m_core.homogeneousDerivatives(param, derivativeCount, ders);
    std::vector<AcGeVector3d> result;
    result.reserve(derivativeCount + 1);
    for (size_t k = 0; k <= derivativeCount; ++k)
        result.push_back({ders[k][0], ders[k][1], ders[k][2]});
    return result;
}

double AcGeNurbCurve3d::curvatureAt(double param) const
{
    const auto d = derivativesAt(param, 2);
    const AcGeVector3d cross{
        d[1].y * d[2].z - d[1].z * d[2].y,
        d[1].z * d[2].x - d[1].x * d[2].z,
        d[1].x * d[2].y - d[1].y * d[2].x};
    const double speed = d[1].length();
    if (!(speed > 0.0))
        return 0.0;
    return cross.length() / (speed * speed * speed);
}

bool AcGeNurbCurve3d::isClosed(const AcGeTol &tol) const
{
    return startPoint().distanceTo(endPoint()) <= tol.equalPoint();
}

// ---------------------------------------------------------------------------
// AcGeNurbCurve3d structural editing (ObjectARX genurb3d API)
// ---------------------------------------------------------------------------

AcGeNurbCurve3d &AcGeNurbCurve3d::insertKnot(double param)
{
    m_core.insertKnot(param);
    return *this;
}

AcGeNurbCurve3d &AcGeNurbCurve3d::addKnot(double param)
{
    if (m_core.knotMultiplicity(param) == 0)
        m_core.insertKnot(param);
    return *this;
}

bool AcGeNurbCurve3d::deleteControlPointAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_core.cx.size())
        throw std::out_of_range(
            "AcGeNurbCurve3d control point index out of range.");
    const double knot = m_core.knots[static_cast<size_t>(index) + 1];
    return m_core.removeKnot(knot, ge::detail::removalTolerance(m_core));
}

AcGeNurbCurve3d &AcGeNurbCurve3d::elevateDegree(int plusDegree)
{
    m_core.elevateDegree(plusDegree);
    return *this;
}

AcGeNurbCurve3d &AcGeNurbCurve3d::makeRational(double weight)
{
    m_core.makeRational(weight);
    return *this;
}

AcGeNurbCurve3d &AcGeNurbCurve3d::hardTrimByParams(double newStartParam,
                                                   double newEndParam)
{
    m_core.hardTrim(newStartParam, newEndParam);
    return *this;
}

AcGeNurbCurve3d &AcGeNurbCurve3d::joinWith(const AcGeNurbCurve3d &curve)
{
    m_core.join(curve.m_core, AcGeTol{}.equalPoint());
    return *this;
}