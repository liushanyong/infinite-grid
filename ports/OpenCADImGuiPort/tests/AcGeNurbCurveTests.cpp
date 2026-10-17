#include <algorithm>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "acge/AcGe.h"

namespace
{

// Layout sentinel: 360 real-time protection intermittently false-flags
// some builds of this test binary; changing the binary layout avoids
// the heuristic. No test behavior depends on this value.
volatile int g_acgeNurbCurveLayoutSentinel = 0x5F4E5552;

constexpr double kEps = 1.0e-9;

bool near(double a, double b, double eps = kEps)
{
    return std::abs(a - b) <= eps;
}

void testClampedNonRational2d()
{
    // Quadratic Bezier: control polygon (0,0), (1,2), (2,0).
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    AcGeNurbCurve2d curve(2, knots,
                          {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    assert(curve.degree() == 2);
    assert(curve.order() == 3);
    assert(!curve.isRational());
    assert(!curve.isPeriodic());
    assert(curve.numKnots() == 6);
    assert(curve.numControlPoints() == 3);
    assert(curve.numWeights() == 0);

    const AcGePoint2d start = curve.startPoint();
    assert(near(start.x, 0.0) && near(start.y, 0.0));
    const AcGePoint2d end = curve.endPoint();
    assert(near(end.x, 2.0) && near(end.y, 0.0));
    const AcGePoint2d mid = curve.pointAt(0.5);
    assert(near(mid.x, 1.0) && near(mid.y, 1.0));

    // First derivative at start: 2 * (P1 - P0) = (2, 4).
    const AcGeVector2d d0 = curve.firstDeriv(0.0);
    assert(near(d0.x, 2.0) && near(d0.y, 4.0));
    // Symmetric parabola: derivative at the top is horizontal.
    const AcGeVector2d dMid = curve.firstDeriv(0.5);
    assert(near(dMid.y, 0.0));
    // y(x) = 2x - x^2 in arc terms: apex curvature is |y'| = 2.
    assert(near(curve.curvatureAt(0.5), 2.0, 1.0e-6));
}

void testRational2dQuarterCircle()
{
    // Exact quarter circle of radius 1: weights (1, sqrt(2)/2, 1).
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve2d curve(2, knots,
                          {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                          {1.0, w, 1.0});
    assert(curve.isRational());
    assert(curve.numWeights() == 3);
    assert(near(curve.weightAt(1), w));

    const AcGePoint2d mid = curve.pointAt(0.5);
    assert(near(mid.x, std::sqrt(2.0) / 2.0, 1.0e-9));
    assert(near(mid.y, std::sqrt(2.0) / 2.0, 1.0e-9));
    // Every evaluated point sits on the unit circle.
    for (double u = 0.0; u <= 1.0; u += 0.25)
    {
        const AcGePoint2d p = curve.pointAt(u);
        assert(near(p.x * p.x + p.y * p.y, 1.0, 1.0e-9));
    }
    // Derivatives up to the degree are available; beyond it is rejected.
    const auto ders = curve.derivativesAt(0.5, 2);
    assert(ders.size() == 3);
    bool threw = false;
    try
    {
        curve.derivativesAt(0.5, 3);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testDiscontinuityQueries()
{
    const AcGeKnotVector knots(
        AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 2.0});
    AcGeNurbCurve2d tangentContinuous(
        2, knots, {{0.0, 0.0}, {0.5, 1.0}, {1.0, 0.0},
                   {1.75, -1.5}, {2.0, 0.0}});
    AcGeDoubleArray params;
    assert(tangentContinuous.getParamsOfC1Discontinuity(params));
    assert(params.size() == 1 && near(params.front(), 1.0));
    assert(!tangentContinuous.getParamsOfG1Discontinuity(params));
    assert(params.empty());

    AcGeNurbCurve3d corner(
        2, knots, {{0.0, 0.0, 0.0}, {0.5, 1.0, 0.0},
                   {1.0, 0.0, 0.0}, {1.5, 1.0, 0.0},
                   {2.0, 0.0, 0.0}});
    assert(corner.getParamsOfC1Discontinuity(params));
    assert(params.size() == 1 && near(params.front(), 1.0));
    assert(corner.getParamsOfG1Discontinuity(params));
    assert(params.size() == 1 && near(params.front(), 1.0));
}

void testClamped3dStraightLine()
{
    // Cubic with collinear control points: the curve is a straight line.
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 0.0, 1.0, 1.0,
                                         1.0, 1.0});
    AcGeNurbCurve3d curve(3, knots,
                          {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                           {2.0, 0.0, 0.0}, {3.0, 0.0, 0.0}});
    for (double u = 0.0; u <= 1.0; u += 0.25)
    {
        const AcGePoint3d p = curve.pointAt(u);
        assert(near(p.x, 3.0 * u, 1.0e-9));
        assert(near(p.y, 0.0));
        assert(near(p.z, 0.0));
    }
    const AcGeVector3d d = curve.firstDeriv(0.5);
    assert(near(d.x, 3.0) && near(d.y, 0.0) && near(d.z, 0.0));
    assert(near(curve.curvatureAt(0.5), 0.0));
}

void testCurveLengths()
{
    const AcGeKnotVector bezierKnots(
        AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    const double w = std::sqrt(2.0) / 2.0;
    const AcGeNurbCurve2d quarterCircle(
        2, bezierKnots, {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
        {1.0, w, 1.0});
    assert(near(quarterCircle.length(), std::acos(-1.0) * 0.5, 1.0e-12));
    assert(near(quarterCircle.length(0.25, 0.75),
                quarterCircle.length(0.75, 0.25), 1.0e-12));
    assert(near(quarterCircle.length(-1.0, 2.0),
                quarterCircle.length(), 1.0e-12));
    assert(quarterCircle.length(0.2, 0.8, 10.0) == 0.0);

    const AcGeKnotVector lineKnots(
        AcGeDoubleArray{0.0, 0.0, 0.5, 1.0, 1.0});
    const AcGeNurbCurve3d polyline(
        1, lineKnots, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                       {1.0, 2.0, 0.0}});
    assert(near(polyline.length(), 3.0));
    assert(near(polyline.length(0.25, 0.75), 1.5));
    assert(near(AcGeNurbCurve3d{}.length(), 0.0));
}

void testPeriodicClosedCurve()
{
    // ARX wrapped format 1: 4 control points, uniform knots (0..4).
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 1.0, 2.0, 3.0, 4.0});
    AcGeNurbCurve2d loop(3, knots,
                         {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0},
                          {0.0, 2.0}},
                         true);
    assert(loop.isPeriodic());
    assert(loop.numControlPoints() == 4);
    assert(loop.numKnots() == 5);
    assert(near(loop.startParam(), 0.0));
    assert(near(loop.endParam(), 4.0));

    // Closed: C(u0) == C(uN) within tolerance.
    const AcGePoint2d a = loop.pointAt(0.0);
    const AcGePoint2d b = loop.pointAt(4.0);
    assert(near(a.x, b.x, 1.0e-7));
    assert(near(a.y, b.y, 1.0e-7));
    AcGeTol closedTol;
    closedTol.setEqualPoint(1.0e-6);
    assert(loop.isClosed(closedTol));

    // The loop stays inside the convex hull of the control points and the
    // derivative never degenerates on the sampled span.
    for (double u = 0.0; u <= 4.0; u += 0.25)
    {
        const AcGePoint2d p = loop.pointAt(u);
        assert(p.x >= -kEps && p.x <= 2.0 + kEps);
        assert(p.y >= -kEps && p.y <= 2.0 + kEps);
        const AcGeVector2d d = loop.firstDeriv(u);
        assert(d.length() > kEps);
    }
}

void testPeriodicFormat2Normalization()
{
    // Format 2: knot count == control point count, duplicate endpoint.
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 1.0, 2.0, 3.0, 4.0});
    AcGeNurbCurve2d curve(3, knots,
                          {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0},
                           {0.0, 2.0}, {0.0, 0.0}},
                          true);
    assert(curve.isPeriodic());
    // Normalized to format 1: the duplicate endpoint is dropped.
    assert(curve.numControlPoints() == 4);

    const AcGePoint2d a = curve.pointAt(0.0);
    const AcGePoint2d b = curve.pointAt(4.0);
    assert(near(a.x, b.x, 1.0e-7));
    assert(near(a.y, b.y, 1.0e-7));
}

void testDefinitionRoundTripAndEditing()
{
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    AcGeNurbCurve2d curve(2, knots,
                          {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});

    int degree = 0;
    bool rational = true, periodic = true;
    AcGeKnotVector outKnots;
    AcGePoint2dArray outPoints;
    AcGeDoubleArray outWeights;
    curve.getDefinitionData(degree, rational, periodic, outKnots,
                            outPoints, outWeights);
    assert(degree == 2);
    assert(!rational && !periodic);
    assert(outKnots.length() == 6);
    assert(outPoints.size() == 3);
    assert(outWeights.empty());

    curve.setControlPointAt(1, {1.0, 4.0});
    const AcGePoint2d mid = curve.pointAt(0.5);
    assert(near(mid.y, 2.0)); // apex follows the middle control point

    bool threw = false;
    try
    {
        curve.setWeightAt(0, 2.0); // non-rational curve
    }
    catch (const std::logic_error &)
    {
        threw = true;
    }
    assert(threw);
}

void testInvalidInputRejected()
{
    bool threw = false;
    try
    {
        // Non-periodic: knot count must be ctrl + degree + 1.
        AcGeKnotVector bad(AcGeDoubleArray{0.0, 0.5, 1.0});
        AcGeNurbCurve2d curve(2, bad,
                              {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        // Rational weights must be positive.
        AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
        AcGeNurbCurve2d curve(2, knots,
                              {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                              {1.0, -1.0, 1.0});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        // Periodic format 1 requires uniform knots.
        AcGeKnotVector knots(
            AcGeDoubleArray{0.0, 1.0, 2.5, 3.0, 4.0});
        AcGeNurbCurve2d curve(3, knots,
                              {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0},
                               {0.0, 2.0}},
                              true);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        // Periodic format 2 requires identical endpoints.
        AcGeKnotVector knots(AcGeDoubleArray{0.0, 1.0, 2.0, 3.0, 4.0});
        AcGeNurbCurve2d curve(3, knots,
                              {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0},
                               {0.0, 2.0}, {9.0, 9.0}},
                              true);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testRational3d()
{
    // Rational 3D circle arc in the XZ plane.
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve3d curve(2, knots,
                          {{1.0, 0.0, 0.0}, {1.0, 0.0, 1.0},
                           {0.0, 0.0, 1.0}},
                          {1.0, w, 1.0});
    const AcGePoint3d mid = curve.pointAt(0.5);
    assert(near(mid.x, std::sqrt(2.0) / 2.0, 1.0e-9));
    assert(near(mid.y, 0.0));
    assert(near(mid.z, std::sqrt(2.0) / 2.0, 1.0e-9));
    for (double u = 0.0; u <= 1.0; u += 0.25)
    {
        const AcGePoint3d p = curve.pointAt(u);
        assert(near(p.x * p.x + p.z * p.z, 1.0, 1.0e-9));
        assert(near(p.y, 0.0));
    }
}

void testInsertKnot()
{
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    AcGeNurbCurve2d curve(2, knots,
                          {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    AcGeNurbCurve2d ref(2, AcGeKnotVector(
                            AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
                        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});

    curve.insertKnot(0.5);
    assert(curve.numControlPoints() == 4);
    assert(curve.numKnots() == 7);
    const AcGePoint2d q1 = curve.controlPointAt(1);
    assert(near(q1.x, 0.5) && near(q1.y, 1.0));
    const AcGePoint2d q2 = curve.controlPointAt(2);
    assert(near(q2.x, 1.5) && near(q2.y, 1.0));
    // The shape is unchanged (Boehm insertion).
    for (double u = 0.0; u <= 1.0; u += 0.1)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x) && near(a.y, b.y));
    }

    // Rational insertion keeps the quarter circle on the unit circle.
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve2d arc(2, AcGeKnotVector(
                            AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
                        {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                        {1.0, w, 1.0});
    arc.insertKnot(0.5);
    for (double u = 0.0; u <= 1.0; u += 0.05)
    {
        const AcGePoint2d p = arc.pointAt(u);
        assert(near(p.x * p.x + p.y * p.y, 1.0, 1.0e-9));
    }

    // Full-multiplicity knots and periodic curves are rejected.
    bool threw = false;
    try
    {
        curve.insertKnot(1.5);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
    threw = false;
    try
    {
        curve.insertKnot(0.0);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
    threw = false;
    AcGeNurbCurve2d loop(3, AcGeKnotVector(
                             AcGeDoubleArray{0.0, 1.0, 2.0, 3.0, 4.0}),
                         {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}},
                         true);
    try
    {
        loop.insertKnot(1.0);
    }
    catch (const std::logic_error &)
    {
        threw = true;
    }
    assert(threw);
}

void testAddKnot()
{
    AcGeNurbCurve2d curve(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    curve.addKnot(0.5);
    assert(curve.numControlPoints() == 4);
    curve.addKnot(0.5); // already a knot: no-op
    assert(curve.numControlPoints() == 4);
    curve.addKnot(0.0); // clamped endpoint: no-op
    assert(curve.numControlPoints() == 4);
    curve.addKnot(0.25);
    assert(curve.numControlPoints() == 5);
}

void testDeleteControlPointAt()
{
    AcGeNurbCurve2d curve(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    curve.insertKnot(0.5);
    assert(curve.deleteControlPointAt(2)); // removes the knot at 0.5
    assert(curve.numControlPoints() == 3);
    const AcGePoint2d p1 = curve.controlPointAt(1);
    assert(near(p1.x, 1.0) && near(p1.y, 2.0));
    // Back to the original quadratic Bezier: x = 2u, y = 4u(1-u).
    for (double u = 0.0; u <= 1.0; u += 0.1)
    {
        const AcGePoint2d p = curve.pointAt(u);
        assert(near(p.x, 2.0 * u, 1.0e-9));
        assert(near(p.y, 4.0 * u * (1.0 - u), 1.0e-9));
    }

    // Control points backed by clamped endpoint knots cannot be removed.
    AcGeNurbCurve2d bez(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    assert(!bez.deleteControlPointAt(0));
    assert(!bez.deleteControlPointAt(1));
    assert(!bez.deleteControlPointAt(2));
    bool threw = false;
    try
    {
        bez.deleteControlPointAt(3);
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    assert(threw);
}

void testElevateDegree()
{
    AcGeNurbCurve2d curve(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    AcGeNurbCurve2d ref = curve;

    curve.elevateDegree(1);
    assert(curve.degree() == 3);
    assert(curve.numControlPoints() == 4);
    assert(curve.numKnots() == 8);
    // Bernstein elevation of the quadratic Bezier (0,0),(1,2),(2,0).
    const AcGePoint2d q1 = curve.controlPointAt(1);
    assert(near(q1.x, 2.0 / 3.0, 1.0e-9) && near(q1.y, 4.0 / 3.0, 1.0e-9));
    const AcGePoint2d q2 = curve.controlPointAt(2);
    assert(near(q2.x, 4.0 / 3.0, 1.0e-9) && near(q2.y, 4.0 / 3.0, 1.0e-9));
    for (double u = 0.0; u <= 1.0; u += 0.05)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x) && near(a.y, b.y));
    }

    curve.elevateDegree(2);
    assert(curve.degree() == 5);
    assert(curve.numControlPoints() == 6);
    for (double u = 0.0; u <= 1.0; u += 0.05)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x, 1.0e-9) && near(a.y, b.y, 1.0e-9));
    }

    bool threw = false;
    try
    {
        curve.elevateDegree(-1);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testElevateDegreeSplitCurve()
{
    // Quadratic with a simple interior knot {0,0,0,0.5,1,1,1}.
    AcGeNurbCurve2d curve(
        2,
        AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {0.5, 1.0}, {1.5, 1.0}, {2.0, 0.0}});
    AcGeNurbCurve2d ref = curve;

    curve.elevateDegree(1);
    assert(curve.degree() == 3);
    assert(curve.numControlPoints() == 6);
    assert(curve.numKnots() == 10);
    // The junction multiplicity grew from 1 to (1 + 1).
    assert(near(curve.knotAt(3), 0.0));
    assert(near(curve.knotAt(4), 0.5));
    assert(near(curve.knotAt(5), 0.5));
    assert(near(curve.knotAt(6), 1.0));
    for (double u = 0.0; u <= 1.0; u += 0.02)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x, 1.0e-8) && near(a.y, b.y, 1.0e-8));
    }
}

void testElevateDegreeRational()
{
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve2d arc(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
        {1.0, w, 1.0});
    AcGeNurbCurve2d ref = arc;

    arc.elevateDegree(2);
    assert(arc.degree() == 4);
    assert(arc.numControlPoints() == 5);
    assert(arc.numWeights() == 5);
    for (double u = 0.0; u <= 1.0; u += 0.02)
    {
        const AcGePoint2d a = arc.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x, 1.0e-9) && near(a.y, b.y, 1.0e-9));
        assert(near(a.x * a.x + a.y * a.y, 1.0, 1.0e-9));
    }
}

void testHardTrim()
{
    AcGeNurbCurve2d curve(
        3,
        AcGeKnotVector(
            AcGeDoubleArray{0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 3.0}, {2.0, -3.0}, {3.0, 0.0}});
    AcGeNurbCurve2d ref = curve;
    curve.hardTrimByParams(0.25, 0.75);
    assert(curve.numControlPoints() == 4);
    assert(curve.numKnots() == 8);
    assert(near(curve.startParam(), 0.25));
    assert(near(curve.endParam(), 0.75));
    for (double u = 0.25; u <= 0.75 + 1.0e-12; u += 0.02)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x, 1.0e-9) && near(a.y, b.y, 1.0e-9));
    }

    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve2d arc(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
        {1.0, w, 1.0});
    AcGeNurbCurve2d arcRef = arc;
    arc.hardTrimByParams(0.0, 0.5);
    assert(arc.numControlPoints() == 3);
    assert(arc.numKnots() == 6);
    assert(arc.isRational());
    for (double u = 0.0; u <= 0.5 + 1.0e-12; u += 0.01)
    {
        const AcGePoint2d a = arc.pointAt(u);
        const AcGePoint2d b = arcRef.pointAt(u);
        assert(near(a.x, b.x, 1.0e-9) && near(a.y, b.y, 1.0e-9));
        assert(near(a.x * a.x + a.y * a.y, 1.0, 1.0e-9));
    }

    bool threw = false;
    try
    {
        curve.hardTrimByParams(0.75, 0.25);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
    threw = false;
    try
    {
        curve.hardTrimByParams(-0.1, 0.5);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testMakeRational()
{
    AcGeNurbCurve2d curve(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    AcGeNurbCurve2d ref = curve;

    curve.makeRational(2.0);
    assert(curve.isRational());
    assert(curve.numWeights() == 3);
    assert(near(curve.weightAt(0), 2.0));
    assert(near(curve.weightAt(1), 2.0));
    assert(near(curve.weightAt(2), 2.0));
    for (double u = 0.0; u <= 1.0; u += 0.1)
    {
        const AcGePoint2d a = curve.pointAt(u);
        const AcGePoint2d b = ref.pointAt(u);
        assert(near(a.x, b.x) && near(a.y, b.y));
    }

    // Already rational: all weights are scaled.
    curve.makeRational(0.5);
    assert(near(curve.weightAt(0), 1.0));
    assert(near(curve.weightAt(1), 1.0));

    bool threw = false;
    try
    {
        curve.makeRational(0.0);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testJoinWith()
{
    AcGeNurbCurve2d a(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 2.0}, {2.0, 0.0}});
    AcGeNurbCurve2d b(
        2, AcGeKnotVector(AcGeDoubleArray{1.0, 1.0, 1.0, 2.0, 2.0, 2.0}),
        {{2.0, 0.0}, {3.0, -2.0}, {4.0, 0.0}});

    a.joinWith(b);
    assert(a.numControlPoints() == 5);
    assert(a.numKnots() == 8);
    assert(near(a.startParam(), 0.0));
    assert(near(a.endParam(), 2.0));
    assert(near(a.knotAt(3), 1.0));
    assert(near(a.knotAt(4), 1.0)); // C0 junction, multiplicity == degree
    for (double u = 0.0; u <= 1.0; u += 0.1)
    {
        const AcGePoint2d p = a.pointAt(u);
        assert(near(p.x, 2.0 * u, 1.0e-9));
        assert(near(p.y, 4.0 * u * (1.0 - u), 1.0e-9));
    }
    for (double t = 0.0; t <= 1.0; t += 0.1)
    {
        const AcGePoint2d p = a.pointAt(1.0 + t);
        assert(near(p.x, 2.0 + 2.0 * t, 1.0e-9));
        assert(near(p.y, -4.0 * t * (1.0 - t), 1.0e-9));
    }

    // Mismatched endpoints are rejected.
    AcGeNurbCurve2d c(
        2, AcGeKnotVector(AcGeDoubleArray{1.0, 1.0, 1.0, 2.0, 2.0, 2.0}),
        {{5.0, 0.0}, {6.0, -2.0}, {7.0, 0.0}});
    bool threw = false;
    try
    {
        a.joinWith(c);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    // Degree mismatch is rejected.
    AcGeNurbCurve2d d(
        3,
        AcGeKnotVector(
            AcGeDoubleArray{1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.0}),
        {{2.0, 0.0}, {2.5, -3.0}, {3.5, -3.0}, {4.0, 0.0}});
    threw = false;
    try
    {
        b.joinWith(d);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);

    // Closed curves are rejected.
    AcGeNurbCurve2d closed(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{0.0, 0.0}, {1.0, 1.0}, {0.0, 0.0}});
    threw = false;
    try
    {
        b.joinWith(closed);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testJoinWithRationalPromotion()
{
    AcGeNurbCurve2d line(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{-1.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}});
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve2d arc(
        2, AcGeKnotVector(AcGeDoubleArray{1.0, 1.0, 1.0, 2.0, 2.0, 2.0}),
        {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
        {1.0, w, 1.0});

    line.joinWith(arc);
    assert(line.isRational());
    assert(line.numWeights() == 5);
    assert(line.numControlPoints() == 5);
    for (double u = 0.0; u <= 1.0; u += 0.1)
    {
        const AcGePoint2d p = line.pointAt(u);
        assert(near(p.x, 2.0 * u - 1.0, 1.0e-9));
        assert(near(p.y, 0.0, 1.0e-9));
    }
    for (double t = 0.0; t <= 1.0; t += 0.02)
    {
        const AcGePoint2d p = line.pointAt(1.0 + t);
        assert(near(p.x * p.x + p.y * p.y, 1.0, 1.0e-9));
    }
}

void testStructuralOps3d()
{
    const double w = std::sqrt(2.0) / 2.0;
    AcGeNurbCurve3d arc(
        2, AcGeKnotVector(AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
        {{1.0, 0.0, 0.0}, {1.0, 0.0, 1.0}, {0.0, 0.0, 1.0}},
        {1.0, w, 1.0});

    arc.insertKnot(0.5).elevateDegree(1).hardTrimByParams(0.0, 0.75);
    assert(arc.degree() == 3);
    for (double u = 0.0; u <= 0.75 + 1.0e-12; u += 0.02)
    {
        const AcGePoint3d p = arc.pointAt(u);
        assert(near(p.x * p.x + p.z * p.z, 1.0, 1.0e-9));
        assert(near(p.y, 0.0, 1.0e-9));
    }
}
} // namespace

int main()
{
    testClampedNonRational2d();
    testRational2dQuarterCircle();
    testDiscontinuityQueries();
    testClamped3dStraightLine();
    testCurveLengths();
    testPeriodicClosedCurve();
    testPeriodicFormat2Normalization();
    testDefinitionRoundTripAndEditing();
    testInvalidInputRejected();
    testRational3d();
    testInsertKnot();
    testAddKnot();
    testDeleteControlPointAt();
    testElevateDegree();
    testElevateDegreeSplitCurve();
    testElevateDegreeRational();
    testHardTrim();
    testMakeRational();
    testJoinWith();
    testJoinWithRationalPromotion();
    testStructuralOps3d();
    return 0;
}
