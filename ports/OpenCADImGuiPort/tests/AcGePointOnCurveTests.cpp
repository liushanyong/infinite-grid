#include "acge/AcGePointOnCurve.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGe2d.h"
#include "acge/AcGeNurbCurve.h"
#include "acge/AcGeKnotVector.h"
#include "acge/AcGeArrays.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>

namespace
{
constexpr double pi = std::numbers::pi;

template <class Exception, class F>
void throws(F &&operation)
{
    bool caught = false;
    try { operation(); }
    catch (const Exception &) { caught = true; }
    assert(caught);
}

void near(double actual, double expected, double tol = 1e-9)
{
    assert(std::abs(actual - expected) < tol);
}

void near(const AcGeVector3d &actual, const AcGeVector3d &expected, double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void near(const AcGePoint3d &actual, const AcGePoint3d &expected, double tol = 1e-9)
{
    near(actual - expected, AcGeVector3d{}, tol);
}

void near(const AcGeVector2d &actual, const AcGeVector2d &expected, double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y) < tol);
}

void near(const AcGePoint2d &actual, const AcGePoint2d &expected, double tol = 1e-9)
{
    near(actual - expected, AcGeVector2d{}, tol);
}

void line3dNumericDerivatives()
{
    const AcGeLineSeg3d segment{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{1.0, 2.0, 3.0}};
    AcGePointOnCurve3d pointOn(segment, 0.5);

    near(pointOn.parameter(), 0.5);
    near(pointOn.point(), {0.5, 1.0, 1.5});

    // No analytic derivative on AcGeLineSeg3d: finite differences.
    near(pointOn.deriv(1), {1.0, 2.0, 3.0}, 1e-9);
    near(pointOn.deriv(2), {0.0, 0.0, 0.0}, 1e-6);
    near(pointOn.curvature(), 0.0, 1e-6);

    double curvature = -1.0;
    assert(pointOn.curvature(0.25, curvature));
    near(curvature, 0.0, 1e-6);
    near(pointOn.parameter(), 0.25);
    assert(!pointOn.isSingular());

    // point(param) resets the cached parameter.
    const AcGePoint3d at025 = pointOn.point(0.25);
    near(at025, {0.25, 0.5, 0.75});
    near(pointOn.parameter(), 0.25);
}

void circle3dNumericCurvature()
{
    const AcGeCircArc3d circle{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 2.0};
    const AcGePointOnCurve3d pointOn(circle, 0.7);

    near(pointOn.point().distanceTo({0.0, 0.0, 0.0}), 2.0, 1e-9);
    near(pointOn.deriv(1).length(), 2.0, 1e-6);
    near(pointOn.curvature(), 0.5, 1e-4);
    assert(!pointOn.isSingular());
}

void analyticDerivativesFromNurbs()
{
    // Quadratic Bezier P0=(0,0,0), P1=(1,0,0), P2=(1,1,0).
    const AcGeNurbCurve3d curve(
        2, AcGeKnotVector{AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}},
        AcGePoint3dArray{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}});
    AcGePointOnCurve3d pointOn(curve, 0.5);

    near(pointOn.point(), {0.75, 0.25, 0.0});
    // Analytic B'(0.5) = (1, 1, 0).
    near(pointOn.deriv(1), {1.0, 1.0, 0.0}, 1e-12);
    // B'' = (P2 - P1) - (P1 - P0) scaled by 2: constant (-2, 2, 0).
    near(pointOn.deriv(2), {-2.0, 2.0, 0.0}, 1e-6);
    // |B' x B''| / |B'|^3 = 4 / (sqrt(2)^3).
    near(pointOn.curvature(), 4.0 / std::pow(std::sqrt(2.0), 3.0), 1e-9);

    // Overloads that rebind curve and parameter together.
    AcGePointOnCurve3d rebound;
    rebound.setCurve(curve, 0.5);
    near(rebound.deriv(1), {1.0, 1.0, 0.0}, 1e-12);
    near(rebound.point(curve, 0.25), {0.4375, 0.0625, 0.0}, 1e-12);
    near(rebound.parameter(), 0.25);
    const AcGeVector3d d1 = rebound.deriv(1, curve, 0.5);
    near(d1, {1.0, 1.0, 0.0}, 1e-12);
}

void curve2dWrappers()
{
    const AcGeLineSeg2d segment{AcGePoint2d{0.0, 0.0}, AcGePoint2d{2.0, 4.0}};
    AcGePointOnCurve2d pointOn(segment, 0.5);
    near(pointOn.point(), {1.0, 2.0});
    near(pointOn.deriv(1), {2.0, 4.0}, 1e-9);
    near(pointOn.deriv(2), {0.0, 0.0}, 1e-6);
    near(pointOn.curvature(), 0.0, 1e-6);

    const AcGeCircArc2d arc{{0.0, 0.0}, 1.0, 0.0, 2.0 * pi};
    const AcGePointOnCurve2d onArc(arc, 1.1);
    near(onArc.curvature(), 1.0, 1e-4);
}

void errorsAndDegenerateCases()
{
    AcGePointOnCurve3d empty;
    throws<std::logic_error>([&] { empty.point(); });
    throws<std::logic_error>([&] { empty.deriv(1); });
    throws<std::invalid_argument>([&] { empty.deriv(0); });

    // Zero-length AcGeLineSeg3d is rejected by the library, so use a
    // constant parametric curve to produce a genuine singular point.
    struct ConstantCurve
    {
        AcGePoint3d pointAt(double) const { return {1.0, 2.0, 3.0}; }
    };
    const ConstantCurve constant{};
    AcGePointOnCurve3d singular(constant, 0.5);
    assert(singular.isSingular());
    double curvature = 0.0;
    assert(!singular.curvature(0.5, curvature));
}} // namespace

int main()
{
    line3dNumericDerivatives();
    circle3dNumericCurvature();
    analyticDerivativesFromNurbs();
    curve2dWrappers();
    errorsAndDegenerateCases();
    std::cout << "AcGePointOnCurveTests passed\n";
    return 0;
}