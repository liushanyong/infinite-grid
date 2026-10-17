#include "acge/AcGeCurveSurfInt.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGeBoundedPlane.h"
#include "acge/AcGeSurface3d.h"
#include "acge/AcGeBspline.h"
#include "acge/AcGeNurbSurface.h"
#include "acge/AcGeIntersect.h"
#include "acge/AcGeKnotVector.h"
#include "acge/AcGeArrays.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <type_traits>
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

void near(double actual, double expected, double tol = 1.0e-6)
{
    assert(std::abs(actual - expected) < tol);
}

void nearPoint(const AcGePoint3d &actual, const AcGePoint3d &expected,
               double tol = 1.0e-6)
{
    assert((actual - expected).length() < tol);
}

void lineCrossesBoundedPlane()
{
    const AcGeBoundedPlane plane{
        AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 0.0, 1.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{-1.0, 1.0},
        AcGeInterval{-1.0, 1.0}};
    const AcGeLineSeg3d line{AcGePoint3d{-0.5, 0.5, -1.0},
                             AcGePoint3d{-0.5, 0.5, 1.0}};

    const AcGeCurveSurfInt intersections(line, plane);

    assert(intersections.numIntPoints() == 1);
    nearPoint(intersections.intPoint(0), {-0.5, 0.5, 0.0});
    double curveParam = -1.0;
    AcGePoint2d surfaceParam;
    assert(intersections.getIntParams(0, curveParam, surfaceParam));
    near(curveParam, 0.5, 1.0e-4);
    near(surfaceParam.x, -0.5, 1.0e-4);
    near(surfaceParam.y, 0.5, 1.0e-4);
    assert(intersections.tolerance() > 0.0);
    assert(intersections.surfaceParameter(0).x < -0.4);

    // Documented point-on-curve / point-on-surface queries.
    AcGePointOnCurve3d onCurve;
    assert(intersections.getPointOnCurve(0, onCurve));
    nearPoint(onCurve.point(), {-0.5, 0.5, 0.0});
    AcGePointOnSurface onSurface;
    assert(intersections.getPointOnSurface(0, onSurface));
    nearPoint(onSurface.point(), {-0.5, 0.5, 0.0});
}

void lineCrossesCylinderTwice()
{
    const AcGeCylinder cylinder{
        1.0, AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 0.0, 1.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{-1.0, 1.0}, 0.0,
        2.0 * pi};
    const AcGeLineSeg3d line{AcGePoint3d{0.5, -2.0, 0.0},
                             AcGePoint3d{0.5, 2.0, 0.0}};

    const AcGeCurveSurfInt intersections(line, cylinder);

    assert(intersections.numIntPoints() == 2);
    const double expectedY = std::sqrt(0.75);
    nearPoint(intersections.intPoint(0), {0.5, -expectedY, 0.0}, 1.0e-5);
    nearPoint(intersections.intPoint(1), {0.5, expectedY, 0.0}, 1.0e-5);
    // Sorted by curve parameter: y increases along the line.
    assert(intersections.intPoint(0).y < intersections.intPoint(1).y);
    AcGePoint2d surfaceParam = intersections.surfaceParameter(1);
    // 60 degrees in the cylinder's angle/height domain.
    near(surfaceParam.x, pi / 3.0, 1.0e-4);
    near(surfaceParam.y, 0.0, 1.0e-4);
}

void lineCrossesSphereTwice()
{
    const AcGeSphere sphere{1.0, AcGePoint3d{0.0, 0.0, 0.0}};
    const AcGeLineSeg3d line{AcGePoint3d{0.0, -2.0, 0.5},
                             AcGePoint3d{0.0, 2.0, 0.5}};

    const AcGeCurveSurfInt intersections(line, sphere);

    assert(intersections.numIntPoints() == 2);
    const double expectedY = std::sqrt(0.75);
    nearPoint(intersections.intPoint(0), {0.0, -expectedY, 0.5}, 1.0e-5);
    nearPoint(intersections.intPoint(1), {0.0, expectedY, 0.5}, 1.0e-5);
    const AcGePoint2d param = intersections.surfaceParameter(0);
    // Latitude 30 degrees north; longitude on the far side.
    near(param.y, pi / 6.0, 1.0e-4);
    double du = std::abs(param.x - 1.5 * pi);
    du = std::min(du, 2.0 * pi - du);
    near(du, 0.0, 1.0e-4);
}

void lineCrossesNurbsSurface()
{
    // Bilinear NURBS patch z = x + y over [0, 1]^2.
    ge::AcGeNurbsSurface3d surface;
    surface.degreeU = 1;
    surface.degreeV = 1;
    surface.knotsU = {0.0, 0.0, 1.0, 1.0};
    surface.knotsV = {0.0, 0.0, 1.0, 1.0};
    surface.controlPointCountU = 2;
    surface.controlPointCountV = 2;
    surface.controlPoints = {
        ge::AcGePoint4d{0.0, 0.0, 0.0, 1.0}, ge::AcGePoint4d{0.0, 1.0, 1.0, 1.0},
        ge::AcGePoint4d{1.0, 0.0, 1.0, 1.0}, ge::AcGePoint4d{1.0, 1.0, 2.0, 1.0}};
    surface.validate();

    const AcGeLineSeg3d line{AcGePoint3d{0.5, 0.5, -1.0},
                             AcGePoint3d{0.5, 0.5, 3.0}};

    const AcGeCurveSurfInt intersections(line, surface);

    assert(intersections.numIntPoints() == 1);
    nearPoint(intersections.intPoint(0), {0.5, 0.5, 1.0}, 1.0e-5);
    double curveParam = -1.0;
    AcGePoint2d surfaceParam;
    assert(intersections.getIntParams(0, curveParam, surfaceParam));
    near(curveParam, 0.5, 1.0e-4);
    near(surfaceParam.x, 0.5, 1.0e-4);
    near(surfaceParam.y, 0.5, 1.0e-4);

    // The documented AcGeNurbSurface spelling aliases the same type.
    AcGeNurbSurface &alias = surface;
    (void)alias;
    static_assert(std::is_same_v<AcGeNurbSurface, ge::AcGeNurbsSurface3d>);
}

void freeFunctionRegressions()
{
    // Straight degree-1 B-splines crossing at (1, 0, 0).
    ge::AcGeBsplineCurve3d lineA(
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<AcGePoint3d>{{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}}, 1);
    ge::AcGeBsplineCurve3d lineB(
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<AcGePoint3d>{{0.7, -1.0, 0.0}, {0.7, 1.0, 0.0}}, 1);
    const auto curveCurve = ge::intersectCurveCurve(lineA, lineB);
    assert(curveCurve.size() == 1);
    nearPoint(curveCurve[0].point, {0.7, 0.0, 0.0}, 1.0e-5);

    // Piercing a plane-like NURBS patch.
    ge::AcGeNurbsSurface3d patch;
    patch.degreeU = 1;
    patch.degreeV = 1;
    patch.knotsU = {0.0, 0.0, 1.0, 1.0};
    patch.knotsV = {0.0, 0.0, 1.0, 1.0};
    patch.controlPointCountU = 2;
    patch.controlPointCountV = 2;
    patch.controlPoints = {
        ge::AcGePoint4d{0.0, 0.0, 0.0, 1.0}, ge::AcGePoint4d{1.0, 0.0, 0.0, 1.0},
        ge::AcGePoint4d{0.0, 1.0, 0.0, 1.0}, ge::AcGePoint4d{1.0, 1.0, 0.0, 1.0}};
    patch.validate();
    ge::AcGeBsplineCurve3d piercer(
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<AcGePoint3d>{{0.5, 0.5, -0.9}, {0.5, 0.5, 1.1}}, 1);
    const auto curveSurface =
        ge::intersectCurveSurface(piercer, patch);
    assert(curveSurface.size() == 1);
    nearPoint(curveSurface[0].point, {0.5, 0.5, 0.0}, 1.0e-5);
    near(curveSurface[0].curveParameter, 0.45, 1.0e-3);
}
void errorsAndDefaultObject()
{
    const AcGeCurveSurfInt empty;
    assert(empty.numIntPoints() == 0);
    assert(empty.tolerance() == 0.0);
    throws<std::logic_error>([&] { empty.intPoint(0); });

    const AcGeBoundedPlane plane{
        AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 0.0, 1.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{-1.0, 1.0},
        AcGeInterval{-1.0, 1.0}};
    const AcGeLineSeg3d line{AcGePoint3d{-0.5, 0.5, -1.0},
                             AcGePoint3d{-0.5, 0.5, 1.0}};
    throws<std::out_of_range>([&] {
        AcGeCurveSurfInt valid(line, plane);
        valid.intPoint(5);
    });
}
} // namespace

int main()
{
    freeFunctionRegressions();
    lineCrossesBoundedPlane();
    lineCrossesCylinderTwice();
    lineCrossesSphereTwice();
    lineCrossesNurbsSurface();
    errorsAndDefaultObject();
    std::cout << "AcGeCurveSurfIntTests passed\n";
    return 0;
}
