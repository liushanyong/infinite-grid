#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "acge/AcGe.h"

namespace
{

constexpr double kEps = 1.0e-9;

bool near(double a, double b, double eps = kEps)
{
    return std::abs(a - b) <= eps;
}

void testClampedSplineInterpolatesAndIsSmooth()
{
    // Collinear uniform fit points: tangents must degenerate to the
    // chord direction (C2 clamped system sanity check).
    AcGePoint2dArray line = {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0},
                             {3.0, 0.0}};
    AcGeCubicSplineCurve2d straight(line, {1.0, 0.0}, {1.0, 0.0});
    assert(straight.numFitPoints() == 4);
    assert(near(straight.startParam(), 0.0));
    assert(near(straight.endParam(), 3.0));
    for (int i = 0; i < 4; ++i)
    {
        const AcGePoint2d p = straight.pointAt(static_cast<double>(i));
        assert(near(p.x, static_cast<double>(i)));
        assert(near(p.y, 0.0));
    }
    for (int i = 0; i < 4; ++i)
    {
        const AcGeVector2d d = straight.firstDerivAt(i);
        assert(near(d.x, 1.0));
        assert(near(d.y, 0.0));
    }
    // Midpoints stay on the line and the derivative stays (1, 0).
    const AcGePoint2d mid = straight.pointAt(0.5);
    assert(near(mid.x, 0.5) && near(mid.y, 0.0));
    const AcGeVector2d dmid = straight.firstDeriv(0.5);
    assert(near(dmid.x, 1.0) && near(dmid.y, 0.0));
}

void testClampedSplineKnownTangents()
{
    // Parabola y = x^2 sampled at x = 0, 1, 2 with unit chord
    // parameterization.  The spline passes through the fit points and its
    // end tangents match the requested derivatives.
    AcGePoint2dArray parabola = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}};
    const AcGeVector2d d0(1.0, 0.0);
    const AcGeVector2d dn(1.0, 4.0);
    AcGeCubicSplineCurve2d curve(parabola, d0, dn);
    // Chord-length parameterization: fit points sit at the knots.
    for (int i = 0; i < 3; ++i)
    {
        const AcGePoint2d p = curve.pointAt(curve.knots()[i]);
        assert(near(p.x, parabola[i].x));
        assert(near(p.y, parabola[i].y));
    }
    const AcGeVector2d start = curve.firstDeriv(curve.startParam());
    assert(near(start.x, d0.x) && near(start.y, d0.y));
    const AcGeVector2d end = curve.firstDeriv(curve.endParam());
    assert(near(end.x, dn.x) && near(end.y, dn.y));
}

void testPeriodicSplineClosesAndWraps()
{
    // Four corners of a square, chord-length closed parameterization.
    AcGePoint2dArray square = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0},
                               {0.0, 1.0}};
    AcGeCubicSplineCurve2d loop(square);
    assert(loop.isPeriodic());
    assert(loop.numFitPoints() == 4);
    const double span = loop.endParam();
    assert(near(span, 4.0));

    // Every fit point is interpolated; wrapping returns the start point.
    for (int i = 0; i < 4; ++i)
    {
        const AcGePoint2d p = loop.pointAt(static_cast<double>(i));
        assert(near(p.x, square[i].x));
        assert(near(p.y, square[i].y));
    }
    const AcGePoint2d wrapped = loop.pointAt(span + 0.5);
    const AcGePoint2d direct = loop.pointAt(0.5);
    assert(near(wrapped.x, direct.x));
    assert(near(wrapped.y, direct.y));
    const AcGePoint2d closed = loop.pointAt(0.0);
    const AcGePoint2d atEnd = loop.pointAt(span);
    assert(near(closed.x, atEnd.x) && near(closed.y, atEnd.y));
}

void testSetFitPointResolvesThroughNewPoint()
{
    AcGePoint2dArray line = {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0},
                             {3.0, 0.0}};
    AcGeCubicSplineCurve2d curve(line, {1.0, 0.0}, {1.0, 0.0});
    curve.setFitPointAt(2, {2.0, 3.0});
    const AcGePoint2d moved = curve.pointAt(curve.knots()[2]);
    assert(near(moved.x, 2.0));
    assert(near(moved.y, 3.0));
}

void testExplicitDerivativeData()
{
    AcGeKnotVector knots;
    knots.set(6, 0.0, 5.0);
    AcGePoint2dArray pts = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0},
                            {3.0, 1.0}, {4.0, 0.0}};
    AcGeVector2dArray derivs(5, {1.0, 0.0});
    AcGeCubicSplineCurve2d curve(knots, pts, derivs);
    assert(curve.fitMode() ==
           AcGeCubicSplineCurve2d::FitMode::Explicit);
    for (int i = 0; i < 5; ++i)
    {
        const AcGePoint2d p = curve.pointAt(static_cast<double>(i));
        assert(near(p.x, pts[i].x));
        assert(near(p.y, pts[i].y));
    }
    curve.setFirstDerivAt(0, {2.0, 0.0});
    assert(near(curve.firstDerivAt(0).x, 2.0));
}


void test3dSplineInterpolates()
{
    AcGePoint3dArray helixPts = {{0.0, 0.0, 0.0}, {1.0, 0.0, 1.0},
                                 {0.0, 1.0, 2.0}, {-1.0, 0.0, 3.0},
                                 {0.0, -1.0, 4.0}};
    AcGeCubicSplineCurve3d curve(helixPts, {1.0, 0.0, 1.0},
                                 {1.0, 0.0, 1.0});
    for (int i = 0; i < 5; ++i)
    {
        const AcGePoint3d p = curve.pointAt(curve.knots()[i]);
        assert(near(p.x, helixPts[i].x));
        assert(near(p.y, helixPts[i].y));
        assert(near(p.z, helixPts[i].z));
    }
    const AcGeVector3d d = curve.firstDeriv(curve.startParam());
    assert(near(d.x, 1.0) && near(d.y, 0.0) && near(d.z, 1.0));

    AcGeCubicSplineCurve3d closed(helixPts);
    assert(closed.isPeriodic());
    assert(closed.numFitPoints() == 5);
    const double span = closed.endParam();
    const AcGePoint3d wrapped = closed.pointAt(span + 1.0);
    const AcGePoint3d direct = closed.pointAt(1.0);
    assert(near(wrapped.x, direct.x));
    assert(near(wrapped.y, direct.y));
    assert(near(wrapped.z, direct.z));
}

void testInvalidInputRejected()
{
    bool threw = false;
    try
    {
        const AcGeCubicSplineCurve2d short_({{0.0, 0.0}, {1.0, 0.0}});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        const AcGeCubicSplineCurve2d dup(
            {{0.0, 0.0}, {1.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        const AcGeCubicSplineCurve2d inf(
            {{0.0, 0.0}, {1.0, 0.0},
             {std::numeric_limits<double>::infinity(), 0.0}});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        AcGeKnotVector knots;
        knots.set(3, 0.0, 2.0); // 3 knots for 3 fit points -> mismatch
        const AcGeCubicSplineCurve2d mismatched(
            knots, {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}},
            {{1.0, 0.0}, {1.0, 0.0}});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main()
{
    testClampedSplineInterpolatesAndIsSmooth();
    testClampedSplineKnownTangents();
    testPeriodicSplineClosesAndWraps();
    testSetFitPointResolvesThroughNewPoint();
    testExplicitDerivativeData();
    test3dSplineInterpolates();
    testInvalidInputRejected();
    return 0;
}
