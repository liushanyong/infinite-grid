#include "acge/AcGeCubicSpline.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{

constexpr double kDefaultKnotTolerance = 1.0e-10;

bool finiteCoord(double v)
{
    return std::isfinite(v);
}

// Thomas algorithm for the clamped C2 tangent system.  Row i (interior
// knot) is: h_i d_{i-1} + 2(h_{i-1}+h_i) d_i + h_{i-1} d_{i+1}
//          = 3(h_{i-1} * delta_i + h_i * delta_{i-1}).
std::vector<double> solveClampedComponent(const std::vector<double> &t,
                                          const std::vector<double> &p,
                                          double d0, double dn)
{
    const size_t pointCount = p.size();
    std::vector<double> d(pointCount, 0.0);
    if (pointCount == 2)
    {
        d[0] = d0;
        d[1] = dn;
        return d;
    }

    const size_t unknownCount = pointCount - 2; // d_1 .. d_{n-2}
    std::vector<double> a(unknownCount, 0.0), b(unknownCount, 0.0),
        c(unknownCount, 0.0), r(unknownCount, 0.0);
    for (size_t row = 0; row < unknownCount; ++row)
    {
        const size_t i = row + 1; // knot index
        const double hLeft = t[i] - t[i - 1];
        const double hRight = t[i + 1] - t[i];
        const double deltaLeft = (p[i] - p[i - 1]) / hLeft;
        const double deltaRight = (p[i + 1] - p[i]) / hRight;
        a[row] = hRight;
        b[row] = 2.0 * (hLeft + hRight);
        c[row] = hLeft;
        r[row] = 3.0 * (hLeft * deltaRight + hRight * deltaLeft);
    }
    // Known end tangents move to the right-hand side: row 1 multiplies
    // d_0 by h_1 = t[2]-t[1]; the last row multiplies d_{n-1} by
    // h_{n-2} = t[n-2]-t[n-3].
    r.front() -= (t[2] - t[1]) * d0;
    r.back() -= (t[pointCount - 2] - t[pointCount - 3]) * dn;

    // Forward sweep.
    std::vector<double> cp(unknownCount, 0.0), rp(unknownCount, 0.0);
    cp[0] = c[0] / b[0];
    rp[0] = r[0] / b[0];
    for (size_t i = 1; i < unknownCount; ++i)
    {
        const double denom = b[i] - a[i] * cp[i - 1];
        cp[i] = c[i] / denom;
        rp[i] = (r[i] - a[i] * rp[i - 1]) / denom;
    }
    d[pointCount - 2] = rp.back();
    for (size_t i = unknownCount - 1; i-- > 0;)
        d[i + 1] = rp[i] - cp[i] * d[i + 2];
    d[0] = d0;
    d[pointCount - 1] = dn;
    return d;
}

// Periodic C2 tangents via the Sherman-Morrison cyclic tridiagonal solve.
// Row i (fit knot i, indices mod n) is:
//   h_i d_{i-1} + 2(h_{i-1}+h_i) d_i + h_{i-1} d_{i+1}
//     = 3(h_{i-1} * delta_i + h_i * delta_{i-1})
// with h_{-1} = h_{n-1} (the wrap interval p_{n-1} -> p_0).
std::vector<double> solvePeriodicComponent(const std::vector<double> &t,
                                           const std::vector<double> &pLoop)
{
    const size_t n = pLoop.size() - 1; // wrap point repeats the first point
    std::vector<double> h(n), delta(n);
    for (size_t i = 0; i < n; ++i)
    {
        h[i] = t[i + 1] - t[i];
        delta[i] = (pLoop[i + 1] - pLoop[i]) / h[i];
    }

    std::vector<double> a(n, 0.0), b(n, 0.0), c(n, 0.0), r(n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const size_t left = (i + n - 1) % n;
        a[i] = h[i];         // coefficient of d_{i-1}
        b[i] = 2.0 * (h[left] + h[i]);
        c[i] = h[left];      // coefficient of d_{i+1}
        r[i] = 3.0 * (h[left] * delta[i] + h[i] * delta[left]);
    }
    // Corner terms of the cyclic system.
    const double alpha = h[0];     // row 0 coefficient of d_{n-1}
    const double beta = h[n - 2];  // row n-1 coefficient of d_0

    // Sherman-Morrison: solve (T + u v^T) x = r for a tridiagonal T.
    const double gamma = -b[0];
    std::vector<double> bb = b;
    bb[0] = b[0] - gamma;
    bb[n - 1] = b[n - 1] - alpha * beta / gamma;

    auto thomas = [&](const std::vector<double> &rhs) {
        std::vector<double> cp(n, 0.0), x(n, 0.0);
        cp[0] = c[0] / bb[0];
        x[0] = rhs[0] / bb[0];
        for (size_t i = 1; i < n; ++i)
        {
            const double denom = bb[i] - a[i] * cp[i - 1];
            cp[i] = c[i] / denom;
            x[i] = (rhs[i] - a[i] * x[i - 1]) / denom;
        }
        for (size_t i = n - 1; i-- > 0;)
            x[i] -= cp[i] * x[i + 1];
        return x;
    };

    std::vector<double> u(n, 0.0), v(n, 0.0);
    u[0] = gamma;
    u[n - 1] = alpha;
    v[0] = 1.0;
    v[n - 1] = beta / gamma;

    const std::vector<double> x = thomas(r);
    const std::vector<double> z = thomas(u);
    const double factor =
        (v[0] * x[0] + v[n - 1] * x[n - 1]) /
        (1.0 + v[0] * z[0] + v[n - 1] * z[n - 1]);
    std::vector<double> d(n);
    for (size_t i = 0; i < n; ++i)
        d[i] = x[i] - factor * z[i];
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// AcGeCubicSplineCurve2d
// ---------------------------------------------------------------------------

void AcGeCubicSplineCurve2d::checkFitIndex(int idx) const
{
    if (idx < 0 || static_cast<size_t>(idx) >= m_fitPoints.size())
        throw std::out_of_range(
            "AcGeCubicSplineCurve2d fit point index out of range.");
}

void AcGeCubicSplineCurve2d::wrapParameter(double &param) const
{
    const double t0 = m_knots.startParam();
    const double t1 = m_knots.endParam();
    if (!m_periodic)
    {
        param = std::clamp(param, t0, t1);
        return;
    }
    const double span = t1 - t0;
    if (span <= 0.0)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d knot span must be positive.");
    param = t0 + std::fmod(param - t0, span);
    if (param < t0)
        param += span;
}

void AcGeCubicSplineCurve2d::solveTangents()
{
    const size_t n = m_fitPoints.size();
    std::vector<double> t(static_cast<size_t>(m_knots.length()));
    for (int i = 0; i < m_knots.length(); ++i)
        t[static_cast<size_t>(i)] = m_knots[i];

    std::vector<double> px(n), py(n);
    for (size_t i = 0; i < n; ++i)
    {
        px[i] = m_fitPoints[i].x;
        py[i] = m_fitPoints[i].y;
    }

    if (m_mode == FitMode::Periodic)
    {
        std::vector<double> pxLoop = px, pyLoop = py;
        pxLoop.push_back(px.front());
        pyLoop.push_back(py.front());
        const auto dx = solvePeriodicComponent(t, pxLoop);
        const auto dy = solvePeriodicComponent(t, pyLoop);
        m_firstDerivs.resize(n);
        for (size_t i = 0; i < n; ++i)
            m_firstDerivs[i] = {dx[i], dy[i]};
        return;
    }

    const auto dx = solveClampedComponent(t, px, m_startDeriv.x, m_endDeriv.x);
    const auto dy = solveClampedComponent(t, py, m_startDeriv.y, m_endDeriv.y);
    m_firstDerivs.resize(n);
    for (size_t i = 0; i < n; ++i)
        m_firstDerivs[i] = {dx[i], dy[i]};
}

AcGeCubicSplineCurve2d::AcGeCubicSplineCurve2d(const AcGePoint2dArray &fitPnts,
                                               const AcGeTol &tol)
{
    if (fitPnts.size() < 3)
        throw std::invalid_argument(
            "A closed periodic cubic spline needs at least three fit points.");
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d fit points must be finite.");
    }

    // Closed chord-length parameterization including the wrap segment.
    const double eps = tol.equalPoint();
    const size_t n = fitPnts.size();
    std::vector<double> t(n + 1, 0.0);
    for (size_t i = 1; i <= n; ++i)
    {
        const AcGePoint2d &a = fitPnts[i - 1];
        const AcGePoint2d &b = fitPnts[i % n];
        const double span = a.distanceTo(b);
        if (span <= eps)
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d fit points must be distinct.");
        t[i] = t[i - 1] + span;
    }

    m_fitPoints = fitPnts;
    m_mode = FitMode::Periodic;
    m_periodic = true;
    m_knots = AcGeKnotVector(static_cast<int>(t.size()), t.data(),
                             kDefaultKnotTolerance);
    solveTangents();
}

AcGeCubicSplineCurve2d::AcGeCubicSplineCurve2d(
    const AcGePoint2dArray &fitPnts, const AcGeVector2d &startDeriv,
    const AcGeVector2d &endDeriv, const AcGeTol &tol)
{
    if (fitPnts.size() < 2)
        throw std::invalid_argument(
            "A clamped cubic spline needs at least two fit points.");
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d fit points must be finite.");
    }
    if (!(startDeriv.lengthSqrd() > 0.0) || !(endDeriv.lengthSqrd() > 0.0))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d end derivatives must be nonzero.");

    const double eps = tol.equalPoint();
    const size_t n = fitPnts.size();
    std::vector<double> t(n, 0.0);
    for (size_t i = 1; i < n; ++i)
    {
        const double span = fitPnts[i - 1].distanceTo(fitPnts[i]);
        if (span <= eps)
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d fit points must be distinct.");
        t[i] = t[i - 1] + span;
    }

    m_fitPoints = fitPnts;
    m_startDeriv = startDeriv;
    m_endDeriv = endDeriv;
    m_mode = FitMode::Clamped;
    m_periodic = false;
    m_knots = AcGeKnotVector(static_cast<int>(t.size()), t.data(),
                             kDefaultKnotTolerance);
    solveTangents();
}

AcGeCubicSplineCurve2d::AcGeCubicSplineCurve2d(
    const AcGeKnotVector &knots, const AcGePoint2dArray &fitPnts,
    const AcGeVector2dArray &firstDerivs, bool isPeriodic)
{
    const int knotCount = knots.length();
    if (knotCount < 2)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d needs at least two knots.");
    if (knotCount != static_cast<int>(fitPnts.size()) + 1)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d knot count must be fit points + 1.");
    if (firstDerivs.size() != fitPnts.size())
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d derivative count must match fit points.");
    for (int i = 1; i < knotCount; ++i)
    {
        if (knots[i] < knots[i - 1])
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d knots must be nondecreasing.");
    }
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d fit points must be finite.");
    }
    for (const auto &v : firstDerivs)
    {
        if (!finiteCoord(v.x) || !finiteCoord(v.y))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve2d derivatives must be finite.");
    }

    m_knots = knots;
    m_fitPoints = fitPnts;
    m_firstDerivs = firstDerivs;
    m_mode = FitMode::Explicit;
    m_periodic = isPeriodic;
}

int AcGeCubicSplineCurve2d::numFitPoints() const
{
    return static_cast<int>(m_fitPoints.size());
}

AcGePoint2d AcGeCubicSplineCurve2d::fitPointAt(int idx) const
{
    checkFitIndex(idx);
    return m_fitPoints[static_cast<size_t>(idx)];
}

AcGeCubicSplineCurve2d &AcGeCubicSplineCurve2d::setFitPointAt(
    int idx, const AcGePoint2d &point)
{
    checkFitIndex(idx);
    if (!finiteCoord(point.x) || !finiteCoord(point.y))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d fit points must be finite.");
    m_fitPoints[static_cast<size_t>(idx)] = point;
    if (m_mode == FitMode::Periodic || m_mode == FitMode::Clamped)
        solveTangents(); // re-solve through the new fit point
    return *this;
}

AcGeVector2d AcGeCubicSplineCurve2d::firstDerivAt(int idx) const
{
    checkFitIndex(idx);
    return m_firstDerivs[static_cast<size_t>(idx)];
}

AcGeCubicSplineCurve2d &AcGeCubicSplineCurve2d::setFirstDerivAt(
    int idx, const AcGeVector2d &deriv)
{
    checkFitIndex(idx);
    if (!finiteCoord(deriv.x) || !finiteCoord(deriv.y))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve2d derivatives must be finite.");
    m_firstDerivs[static_cast<size_t>(idx)] = deriv;
    m_mode = FitMode::Explicit; // tangent edits pin the derivative data
    return *this;
}

double AcGeCubicSplineCurve2d::startParam() const
{
    return m_knots.startParam();
}

double AcGeCubicSplineCurve2d::endParam() const
{
    return m_knots.endParam();
}

AcGePoint2d AcGeCubicSplineCurve2d::pointAt(double param) const
{
    if (m_fitPoints.empty())
        throw std::logic_error("AcGeCubicSplineCurve2d is empty.");
    wrapParameter(param);

    const int knotCount = m_knots.length();
    int low = 0;
    int high = knotCount - 2;
    while (low < high)
    {
        const int mid = low + (high - low + 1) / 2;
        if (m_knots[mid] <= param)
            low = mid;
        else
            high = mid - 1;
    }
    const size_t i = static_cast<size_t>(low);
    const double h = m_knots[i + 1] - m_knots[i];
    const double m = (param - m_knots[i]) / h;

    const AcGePoint2d &p0 = m_fitPoints[i];
    const AcGePoint2d &p1 = m_fitPoints[i + 1];
    const AcGeVector2d &d0 = m_firstDerivs[i];
    const AcGeVector2d &d1 = m_firstDerivs[i + 1];

    const double h00 = 2.0 * m * m * m - 3.0 * m * m + 1.0;
    const double h10 = m * m * m - 2.0 * m * m + m;
    const double h01 = -2.0 * m * m * m + 3.0 * m * m;
    const double h11 = m * m * m - m * m;
    return {h00 * p0.x + h01 * p1.x + h * (h10 * d0.x + h11 * d1.x),
            h00 * p0.y + h01 * p1.y + h * (h10 * d0.y + h11 * d1.y)};
}

AcGeVector2d AcGeCubicSplineCurve2d::firstDeriv(double param) const
{
    if (m_fitPoints.empty())
        throw std::logic_error("AcGeCubicSplineCurve2d is empty.");
    wrapParameter(param);

    const int knotCount = m_knots.length();
    int low = 0;
    int high = knotCount - 2;
    while (low < high)
    {
        const int mid = low + (high - low + 1) / 2;
        if (m_knots[mid] <= param)
            low = mid;
        else
            high = mid - 1;
    }
    const size_t i = static_cast<size_t>(low);
    const double h = m_knots[i + 1] - m_knots[i];
    const double m = (param - m_knots[i]) / h;

    const AcGePoint2d &p0 = m_fitPoints[i];
    const AcGePoint2d &p1 = m_fitPoints[i + 1];
    const AcGeVector2d &d0 = m_firstDerivs[i];
    const AcGeVector2d &d1 = m_firstDerivs[i + 1];

    const double h00 = 6.0 * m * m - 6.0 * m;
    const double h10 = 3.0 * m * m - 4.0 * m + 1.0;
    const double h01 = -6.0 * m * m + 6.0 * m;
    const double h11 = 3.0 * m * m - 2.0 * m;
    // p'(t) = [h00' p0 + h10' h d0 + h01' p1 + h11' h d1] / h.
    return {(h00 * (p0.x - p1.x)) / h + h10 * d0.x + h11 * d1.x,
            (h00 * (p0.y - p1.y)) / h + h10 * d0.y + h11 * d1.y};
}

// ---------------------------------------------------------------------------
// AcGeCubicSplineCurve3d
// ---------------------------------------------------------------------------

void AcGeCubicSplineCurve3d::checkFitIndex(int idx) const
{
    if (idx < 0 || static_cast<size_t>(idx) >= m_fitPoints.size())
        throw std::out_of_range(
            "AcGeCubicSplineCurve3d fit point index out of range.");
}

void AcGeCubicSplineCurve3d::wrapParameter(double &param) const
{
    const double t0 = m_knots.startParam();
    const double t1 = m_knots.endParam();
    if (!m_periodic)
    {
        param = std::clamp(param, t0, t1);
        return;
    }
    const double span = t1 - t0;
    if (span <= 0.0)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d knot span must be positive.");
    param = t0 + std::fmod(param - t0, span);
    if (param < t0)
        param += span;
}

void AcGeCubicSplineCurve3d::solveTangents()
{
    const size_t n = m_fitPoints.size();
    std::vector<double> t(static_cast<size_t>(m_knots.length()));
    for (int i = 0; i < m_knots.length(); ++i)
        t[static_cast<size_t>(i)] = m_knots[i];

    std::vector<double> px(n), py(n), pz(n);
    for (size_t i = 0; i < n; ++i)
    {
        px[i] = m_fitPoints[i].x;
        py[i] = m_fitPoints[i].y;
        pz[i] = m_fitPoints[i].z;
    }

    if (m_mode == FitMode::Periodic)
    {
        std::vector<double> pxLoop = px, pyLoop = py, pzLoop = pz;
        pxLoop.push_back(px.front());
        pyLoop.push_back(py.front());
        pzLoop.push_back(pz.front());
        const auto dx = solvePeriodicComponent(t, pxLoop);
        const auto dy = solvePeriodicComponent(t, pyLoop);
        const auto dz = solvePeriodicComponent(t, pzLoop);
        m_firstDerivs.resize(n);
        for (size_t i = 0; i < n; ++i)
            m_firstDerivs[i] = {dx[i], dy[i], dz[i]};
        return;
    }

    const auto dx = solveClampedComponent(t, px, m_startDeriv.x, m_endDeriv.x);
    const auto dy = solveClampedComponent(t, py, m_startDeriv.y, m_endDeriv.y);
    const auto dz = solveClampedComponent(t, pz, m_startDeriv.z, m_endDeriv.z);
    m_firstDerivs.resize(n);
    for (size_t i = 0; i < n; ++i)
        m_firstDerivs[i] = {dx[i], dy[i], dz[i]};
}

AcGeCubicSplineCurve3d::AcGeCubicSplineCurve3d(const AcGePoint3dArray &fitPnts,
                                               const AcGeTol &tol)
{
    if (fitPnts.size() < 3)
        throw std::invalid_argument(
            "A closed periodic cubic spline needs at least three fit points.");
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y) || !finiteCoord(p.z))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d fit points must be finite.");
    }

    const double eps = tol.equalPoint();
    const size_t n = fitPnts.size();
    std::vector<double> t(n + 1, 0.0);
    for (size_t i = 1; i <= n; ++i)
    {
        const AcGePoint3d &a = fitPnts[i - 1];
        const AcGePoint3d &b = fitPnts[i % n];
        const double span = a.distanceTo(b);
        if (span <= eps)
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d fit points must be distinct.");
        t[i] = t[i - 1] + span;
    }

    m_fitPoints = fitPnts;
    m_mode = FitMode::Periodic;
    m_periodic = true;
    m_knots = AcGeKnotVector(static_cast<int>(t.size()), t.data(),
                             kDefaultKnotTolerance);
    solveTangents();
}

AcGeCubicSplineCurve3d::AcGeCubicSplineCurve3d(
    const AcGePoint3dArray &fitPnts, const AcGeVector3d &startDeriv,
    const AcGeVector3d &endDeriv, const AcGeTol &tol)
{
    if (fitPnts.size() < 2)
        throw std::invalid_argument(
            "A clamped cubic spline needs at least two fit points.");
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y) || !finiteCoord(p.z))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d fit points must be finite.");
    }
    if (!(startDeriv.lengthSqrd() > 0.0) || !(endDeriv.lengthSqrd() > 0.0))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d end derivatives must be nonzero.");

    const double eps = tol.equalPoint();
    const size_t n = fitPnts.size();
    std::vector<double> t(n, 0.0);
    for (size_t i = 1; i < n; ++i)
    {
        const double span = fitPnts[i - 1].distanceTo(fitPnts[i]);
        if (span <= eps)
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d fit points must be distinct.");
        t[i] = t[i - 1] + span;
    }

    m_fitPoints = fitPnts;
    m_startDeriv = startDeriv;
    m_endDeriv = endDeriv;
    m_mode = FitMode::Clamped;
    m_periodic = false;
    m_knots = AcGeKnotVector(static_cast<int>(t.size()), t.data(),
                             kDefaultKnotTolerance);
    solveTangents();
}

AcGeCubicSplineCurve3d::AcGeCubicSplineCurve3d(
    const AcGeKnotVector &knots, const AcGePoint3dArray &fitPnts,
    const AcGeVector3dArray &firstDerivs, bool isPeriodic)
{
    const int knotCount = knots.length();
    if (knotCount < 2)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d needs at least two knots.");
    if (knotCount != static_cast<int>(fitPnts.size()) + 1)
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d knot count must be fit points + 1.");
    if (firstDerivs.size() != fitPnts.size())
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d derivative count must match fit points.");
    for (int i = 1; i < knotCount; ++i)
    {
        if (knots[i] < knots[i - 1])
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d knots must be nondecreasing.");
    }
    for (const auto &p : fitPnts)
    {
        if (!finiteCoord(p.x) || !finiteCoord(p.y) || !finiteCoord(p.z))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d fit points must be finite.");
    }
    for (const auto &v : firstDerivs)
    {
        if (!finiteCoord(v.x) || !finiteCoord(v.y) || !finiteCoord(v.z))
            throw std::invalid_argument(
                "AcGeCubicSplineCurve3d derivatives must be finite.");
    }

    m_knots = knots;
    m_fitPoints = fitPnts;
    m_firstDerivs = firstDerivs;
    m_mode = FitMode::Explicit;
    m_periodic = isPeriodic;
}

int AcGeCubicSplineCurve3d::numFitPoints() const
{
    return static_cast<int>(m_fitPoints.size());
}

AcGePoint3d AcGeCubicSplineCurve3d::fitPointAt(int idx) const
{
    checkFitIndex(idx);
    return m_fitPoints[static_cast<size_t>(idx)];
}

AcGeCubicSplineCurve3d &AcGeCubicSplineCurve3d::setFitPointAt(
    int idx, const AcGePoint3d &point)
{
    checkFitIndex(idx);
    if (!finiteCoord(point.x) || !finiteCoord(point.y) ||
        !finiteCoord(point.z))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d fit points must be finite.");
    m_fitPoints[static_cast<size_t>(idx)] = point;
    if (m_mode == FitMode::Periodic || m_mode == FitMode::Clamped)
        solveTangents();
    return *this;
}

AcGeVector3d AcGeCubicSplineCurve3d::firstDerivAt(int idx) const
{
    checkFitIndex(idx);
    return m_firstDerivs[static_cast<size_t>(idx)];
}

AcGeCubicSplineCurve3d &AcGeCubicSplineCurve3d::setFirstDerivAt(
    int idx, const AcGeVector3d &deriv)
{
    checkFitIndex(idx);
    if (!finiteCoord(deriv.x) || !finiteCoord(deriv.y) ||
        !finiteCoord(deriv.z))
        throw std::invalid_argument(
            "AcGeCubicSplineCurve3d derivatives must be finite.");
    m_firstDerivs[static_cast<size_t>(idx)] = deriv;
    m_mode = FitMode::Explicit;
    return *this;
}

double AcGeCubicSplineCurve3d::startParam() const
{
    return m_knots.startParam();
}

double AcGeCubicSplineCurve3d::endParam() const
{
    return m_knots.endParam();
}

AcGePoint3d AcGeCubicSplineCurve3d::pointAt(double param) const
{
    if (m_fitPoints.empty())
        throw std::logic_error("AcGeCubicSplineCurve3d is empty.");
    wrapParameter(param);

    const int knotCount = m_knots.length();
    int low = 0;
    int high = knotCount - 2;
    while (low < high)
    {
        const int mid = low + (high - low + 1) / 2;
        if (m_knots[mid] <= param)
            low = mid;
        else
            high = mid - 1;
    }
    const size_t i = static_cast<size_t>(low);
    const double h = m_knots[i + 1] - m_knots[i];
    const double m = (param - m_knots[i]) / h;

    const AcGePoint3d &p0 = m_fitPoints[i];
    const AcGePoint3d &p1 = m_fitPoints[i + 1];
    const AcGeVector3d &d0 = m_firstDerivs[i];
    const AcGeVector3d &d1 = m_firstDerivs[i + 1];

    const double h00 = 2.0 * m * m * m - 3.0 * m * m + 1.0;
    const double h10 = m * m * m - 2.0 * m * m + m;
    const double h01 = -2.0 * m * m * m + 3.0 * m * m;
    const double h11 = m * m * m - m * m;
    return {h00 * p0.x + h01 * p1.x + h * (h10 * d0.x + h11 * d1.x),
            h00 * p0.y + h01 * p1.y + h * (h10 * d0.y + h11 * d1.y),
            h00 * p0.z + h01 * p1.z + h * (h10 * d0.z + h11 * d1.z)};
}

AcGeVector3d AcGeCubicSplineCurve3d::firstDeriv(double param) const
{
    if (m_fitPoints.empty())
        throw std::logic_error("AcGeCubicSplineCurve3d is empty.");
    wrapParameter(param);

    const int knotCount = m_knots.length();
    int low = 0;
    int high = knotCount - 2;
    while (low < high)
    {
        const int mid = low + (high - low + 1) / 2;
        if (m_knots[mid] <= param)
            low = mid;
        else
            high = mid - 1;
    }
    const size_t i = static_cast<size_t>(low);
    const double h = m_knots[i + 1] - m_knots[i];
    const double m = (param - m_knots[i]) / h;

    const AcGePoint3d &p0 = m_fitPoints[i];
    const AcGePoint3d &p1 = m_fitPoints[i + 1];
    const AcGeVector3d &d0 = m_firstDerivs[i];
    const AcGeVector3d &d1 = m_firstDerivs[i + 1];

    const double h00 = 6.0 * m * m - 6.0 * m;
    const double h10 = 3.0 * m * m - 4.0 * m + 1.0;
    const double h01 = -6.0 * m * m + 6.0 * m;
    const double h11 = 3.0 * m * m - 2.0 * m;
    return {(h00 * (p0.x - p1.x)) / h + h10 * d0.x + h11 * d1.x,
            (h00 * (p0.y - p1.y)) / h + h10 * d0.y + h11 * d1.y,
            (h00 * (p0.z - p1.z)) / h + h10 * d0.z + h11 * d1.z};
}
