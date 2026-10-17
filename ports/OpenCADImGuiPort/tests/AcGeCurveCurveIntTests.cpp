#include "acge/AcGeCurveCurveInt.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGe2d.h"
#include "acge/AcGeNurbCurve.h"
#include "acge/AcGeKnotVector.h"
#include "acge/AcGeArrays.h"
#include "acge/AcGeInterval.h"

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

void lineCrossing3d()
{
    const AcGeLineSeg3d lineA{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{4.0, 0.0, 0.0}};
    const AcGeLineSeg3d lineB{AcGePoint3d{2.0, -1.0, 1.0}, AcGePoint3d{2.0, 1.0, -1.0}};
    const AcGeCurveCurveInt3d intersections(lineA, lineB);

    assert(intersections.numIntPoints() == 1);
    near(intersections.intPoint(0), {2.0, 0.0, 0.0}, 1e-6);
    double param1 = -1.0;
    double param2 = -1.0;
    assert(intersections.getIntParams(0, param1, param2));
    near(param1, 0.5, 1e-6);
    near(param2, 0.5, 1e-6);
    assert(intersections.isTransversal(0));
    assert(!intersections.isTangential(0));
    assert(intersections.overlapCount() == 0);
    assert(intersections.orderWrt1(0) == 0);
    assert(intersections.orderWrt2(0) == 0);
    assert(intersections.tolerance() > 0.0);
}

void lineCrossesCircleTwice()
{
    const AcGeCircArc3d circle{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 1.0, 0.0, 2.0 * pi};
    const AcGeLineSeg3d chord{AcGePoint3d{-2.0, 0.0, 0.0}, AcGePoint3d{2.0, 0.0, 0.0}};
    const AcGeCurveCurveInt3d intersections(chord, circle);

    assert(intersections.numIntPoints() == 2);
    near(intersections.intPoint(0), {-1.0, 0.0, 0.0}, 1e-6);
    near(intersections.intPoint(1), {1.0, 0.0, 0.0}, 1e-6);
    assert(intersections.isTransversal(0));
    assert(intersections.isTransversal(1));
    // Order along the chord: param 0.25 then 0.75; along the circle the
    // second crossing (angle pi) comes after the first (angle 0).
    assert(intersections.orderWrt1(0) == 0);
    assert(intersections.orderWrt1(1) == 1);
}

void lineTangentToCircle()
{
    const AcGeCircArc3d circle{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 1.0, 0.0, 2.0 * pi};
    const AcGeLineSeg3d tangent{AcGePoint3d{-2.0, 1.0, 0.0}, AcGePoint3d{2.0, 1.0, 0.0}};
    const AcGeCurveCurveInt3d intersections(tangent, circle);

    assert(intersections.numIntPoints() == 1);
    near(intersections.intPoint(0), {0.0, 1.0, 0.0}, 1e-5);
    assert(intersections.isTangential(0));
}

void collinearOverlap3d()
{
    const AcGeLineSeg3d lineA{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{2.0, 0.0, 0.0}};
    const AcGeLineSeg3d lineB{AcGePoint3d{0.5, 0.0, 0.0}, AcGePoint3d{3.0, 0.0, 0.0}};
    const AcGeCurveCurveInt3d intersections(lineA, lineB);

    assert(intersections.overlapCount() == 1);
    assert(intersections.numIntPoints() == 0);
    assert(intersections.overlapDirection() == 1);

    AcGeInterval range1;
    AcGeInterval range2;
    assert(intersections.getOverlapRanges(0, range1, range2));
    near(range1.lowerBound(), 0.25, 1e-3);
    near(range1.upperBound(), 1.0, 1e-3);
    near(range2.lowerBound(), 0.0, 1e-3);
    near(range2.upperBound(), 0.6, 1e-3);

    // Opposite parameter direction reports -1.
    const AcGeLineSeg3d reversed{AcGePoint3d{3.0, 0.0, 0.0}, AcGePoint3d{0.5, 0.0, 0.0}};
    const AcGeCurveCurveInt3d opposite(lineA, reversed);
    assert(opposite.overlapCount() == 1);
    assert(opposite.overlapDirection() == -1);
}

void intervalRestricted3d()
{
    const AcGeCircArc3d circle{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 1.0, 0.0, 2.0 * pi};
    const AcGeLineSeg3d chord{AcGePoint3d{-2.0, 0.0, 0.0}, AcGePoint3d{2.0, 0.0, 0.0}};
    // Restrict the chord to [0, 0.5]: only the first crossing remains.
    const AcGeCurveCurveInt3d intersections(
        chord, circle, AcGeInterval{0.0, 0.5},
        AcGeInterval{circle.startAngle, circle.endAngle});
    assert(intersections.numIntPoints() == 1);
    near(intersections.intPoint(0), {-1.0, 0.0, 0.0}, 1e-6);
}

void nurbsAnalyticDerivativePath()
{
    // Straight quadratic "NURBS" from (0,0) to (2,0).
    const AcGeNurbCurve2d curve(
        2, AcGeKnotVector{AcGeDoubleArray{0.0, 0.0, 0.0, 1.0, 1.0, 1.0}},
        AcGePoint2dArray{{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}});
    const AcGeLineSeg2d vertical{AcGePoint2d{1.0, -1.0}, AcGePoint2d{1.0, 1.0}};

    const AcGeCurveCurveInt2d intersections(curve, vertical);

    assert(intersections.numIntPoints() == 1);
    near(intersections.intPoint(0).x, 1.0, 1e-6);
    near(intersections.intPoint(0).y, 0.0, 1e-6);
    near(intersections.intPointTol(0), intersections.tolerance(), 1e-15);

    double param1 = -1.0;
    double param2 = -1.0;
    assert(intersections.getIntParams(0, param1, param2));
    near(param1, 0.5, 1e-6); // NURBS domain [0, 1]
    near(param2, 0.5, 1e-6); // line domain [0, 1]
}

void curve2dCrossing()
{
    const AcGeLineSeg2d diagonal{AcGePoint2d{0.0, 0.0}, AcGePoint2d{2.0, 2.0}};
    const AcGeLineSeg2d antiDiagonal{AcGePoint2d{0.0, 2.0}, AcGePoint2d{2.0, 0.0}};
    const AcGeCurveCurveInt2d intersections(diagonal, antiDiagonal);
    assert(intersections.numIntPoints() == 1);
    near(intersections.intPoint(0).x, 1.0, 1e-6);
    near(intersections.intPoint(0).y, 1.0, 1e-6);
}

void errorsAndDefaultObject()
{
    const AcGeCurveCurveInt2d empty;
    assert(empty.numIntPoints() == 0);
    assert(empty.overlapCount() == 0);
    throws<std::logic_error>([&] { empty.intPoint(0); });

    const AcGeLineSeg2d diagonal{AcGePoint2d{0.0, 0.0}, AcGePoint2d{2.0, 2.0}};
    const AcGeLineSeg2d antiDiagonal{AcGePoint2d{0.0, 2.0}, AcGePoint2d{2.0, 0.0}};
    throws<std::invalid_argument>([&] {
        AcGeCurveCurveInt2d bad(diagonal, antiDiagonal,
                                AcGeInterval{0.5, 0.5},
                                AcGeInterval{0.0, 1.0});
    });
    throws<std::out_of_range>([&] {
        AcGeCurveCurveInt2d valid(diagonal, antiDiagonal);
        valid.intPoint(5);
    });

    // 3D plane-normal overload stores and returns the normal.
    const AcGeLineSeg3d lineA{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{4.0, 0.0, 0.0}};
    const AcGeLineSeg3d lineB{AcGePoint3d{2.0, -1.0, 0.0}, AcGePoint3d{2.0, 1.0, 0.0}};
    const AcGeVector3d normal{0.0, 0.0, 1.0};
    const AcGeCurveCurveInt3d withNormal(lineA, lineB, normal);
    assert(withNormal.numIntPoints() == 1);
    near(withNormal.planeNormal(), normal);
}
} // namespace

int main()
{
    lineCrossing3d();
    lineCrossesCircleTwice();
    lineTangentToCircle();
    collinearOverlap3d();
    intervalRestricted3d();
    nurbsAnalyticDerivativePath();
    curve2dCrossing();
    errorsAndDefaultObject();
    std::cout << "AcGeCurveCurveIntTests passed\n";
    return 0;
}
