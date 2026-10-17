#include "acge/AcGe.h"

#include <cassert>
#include <cmath>
#include <type_traits>
#include <vector>

namespace
{
bool near(double a, double b, double tolerance = 1.0e-9)
{
    return std::abs(a - b) <= tolerance;
}

void curveBasesExposeDomainsAndEvaluation()
{
    static_assert(std::is_base_of_v<AcGeCurve2d, AcGeLineSeg2d>);
    static_assert(std::is_base_of_v<AcGeCurve2d, AcGeCircArc2d>);
    static_assert(std::is_base_of_v<AcGeCurve2d, AcGeEllipArc2d>);
    static_assert(std::is_base_of_v<AcGeCurve2d, ge::AcGeCircle2d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeLineSeg3d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeCircArc3d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeCircle3d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeEllipArc3d>);
    static_assert(std::is_base_of_v<AcGeCurve2d, AcGeNurbCurve2d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeNurbCurve3d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, ge::AcGeBezierCurve3d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, ge::AcGeNurbsCurve3d>);
    static_assert(std::is_base_of_v<AcGeCurve2d, ge::AcGeFitSpline2d>);
    static_assert(std::is_base_of_v<AcGeCurve3d, AcGeFitSpline3d>);

    const AcGeKnotVector knots(AcGeDoubleArray{2.0, 2.0, 4.0, 4.0});
    const AcGeNurbCurve2d curve2d(
        1, knots, AcGePoint2dArray{{0.0, 0.0}, {2.0, 0.0}});
    const AcGeCurve2d &curve2dBase = curve2d;
    assert(near(curve2dBase.startParam(), 2.0));
    assert(near(curve2dBase.endParam(), 4.0));
    assert(near(curve2dBase.pointAt(3.0).x, 1.0));

    const ge::AcGeBezierCurve3d bezier(
        std::vector<AcGePoint3d>{{0.0, 0.0, 0.0}, {0.0, 2.0, 0.0}});
    const AcGeCurve3d &curve3dBase = bezier;
    assert(near(curve3dBase.startParam(), 0.0));
    assert(near(curve3dBase.endParam(), 1.0));
    assert(near(curve3dBase.pointAt(0.25).y, 0.5));

    const AcGeCircArc2d clockwise(
        AcGePoint2d{0.0, 0.0}, 2.0, 0.0, 1.5707963267948966,
        AcGeVector2d{1.0, 0.0}, true);
    const AcGeCurve2d &arcBase = clockwise;
    assert(near(arcBase.startParam(), 0.0));
    assert(near(arcBase.endParam(), 4.71238898038469));
    const AcGePoint2d clockwiseMid = arcBase.pointAt(1.5707963267948966);
    assert(near(clockwiseMid.x, 0.0));
    assert(near(clockwiseMid.y, -2.0));

    const AcGeLineSeg2d line2d{AcGePoint2d{-1.0, 3.0},
                               AcGePoint2d{1.0, 5.0}};
    const AcGeCurve2d &line2dBase = line2d;
    assert(near(line2dBase.pointAt(0.5).x, 0.0));
    assert(near(line2dBase.pointAt(0.5).y, 4.0));
    assert(near(line2d.derivativeAt(0.0).x, 2.0));
    assert(near(line2d.derivativeAt(0.75).y, 2.0));

    const ge::AcGeCircle2d circle2d{{2.0, -1.0}, 3.0};
    const AcGeCurve2d &circle2dBase = circle2d;
    assert(near(circle2dBase.startParam(), 0.0));
    assert(near(circle2dBase.endParam(), 6.283185307179586));
    assert(near(circle2dBase.pointAt(0.0).x, 5.0));
    const AcGeEllipArc2d ellipse2d;
    const AcGeCurve2d &ellipse2dBase = ellipse2d;
    assert(near(ellipse2dBase.pointAt(0.0).x, 1.0));

    const AcGeLineSeg3d line3d{AcGePoint3d{1.0, 2.0, 3.0},
                               AcGePoint3d{4.0, 6.0, 3.0}};
    assert(near(line3d.derivativeAt(0.0).x, 3.0));
    assert(near(line3d.derivativeAt(1.0).y, 4.0));
    const AcGeCircle3d circle3d{{1.0, 2.0, 3.0},
                                {0.0, 0.0, 1.0}, 2.0};
    const AcGeCurve3d &circle3dBase = circle3d;
    assert(near(circle3dBase.endParam(), 6.283185307179586));
    assert(near(circle3dBase.pointAt(0.0).x, 3.0));
    const AcGeEllipArc3d ellipse3d;
    const AcGeCurve3d &ellipse3dBase = ellipse3d;
    assert(near(ellipse3dBase.pointAt(0.0).x, 1.0));

    AcGeCompositeCurve2d composite;
    composite.addSegment(line2d, AcGeInterval{0.0, 1.0});
    const AcGeCurve2d &compositeBase = composite;
    assert(near(compositeBase.startParam(), 0.0));
    assert(near(compositeBase.endParam(), 1.0));
    assert(near(compositeBase.pointAt(0.5).y, 4.0));
}

void surfaceBaseExposesDomainsAndEvaluation()
{
    static_assert(std::is_base_of_v<AcGeSurface, AcGeSphere>);
    static_assert(std::is_base_of_v<AcGeSurface, AcGeCylinder>);
    static_assert(std::is_base_of_v<AcGeSurface, AcGeTorus>);
    static_assert(std::is_base_of_v<AcGeSurface, AcGeCone>);
    static_assert(std::is_base_of_v<AcGeSurface, AcGeBoundedPlane>);
    static_assert(std::is_base_of_v<AcGeSurface, ge::AcGeBsplineSurface3d>);
    static_assert(std::is_base_of_v<AcGeSurface, ge::AcGeNurbsSurface3d>);

    ge::AcGeNurbsSurface3d patch;
    patch.degreeU = 1;
    patch.degreeV = 1;
    patch.knotsU = {0.0, 0.0, 1.0, 1.0};
    patch.knotsV = {0.0, 0.0, 1.0, 1.0};
    patch.controlPointCountU = 2;
    patch.controlPointCountV = 2;
    patch.controlPoints = {
        {0.0, 0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 0.0, 1.0}};

    const AcGeSurface &surface = patch;
    assert(near(surface.minParameterU(), 0.0));
    assert(near(surface.maxParameterU(), 1.0));
    assert(near(surface.minParameterV(), 0.0));
    assert(near(surface.maxParameterV(), 1.0));
    const AcGePoint3d center = surface.pointAt(0.5, 0.5);
    assert(near(center.x, 0.5));
    assert(near(center.y, 0.5));
    assert(near(center.z, 0.0));

    const AcGeSphere sphere{2.0, {1.0, 2.0, 3.0}};
    const AcGeSurface &sphereBase = sphere;
    assert(near(sphereBase.minParameterU(), 0.0));
    assert(near(sphereBase.maxParameterU(), 6.283185307179586));
    assert(near(sphereBase.minParameterV(), -1.5707963267948966));
    assert(near(sphereBase.maxParameterV(), 1.5707963267948966));
    const AcGePoint3d north = sphereBase.pointAt(0.0, 1.5707963267948966);
    assert(near(north.x, 1.0));
    assert(near(north.y, 2.0));
    assert(near(north.z, 5.0));

    const AcGeCylinder cylinder;
    const AcGeSurface &cylinderBase = cylinder;
    assert(std::isinf(cylinderBase.minParameterV()) &&
           cylinderBase.minParameterV() < 0.0);
    assert(std::isinf(cylinderBase.maxParameterV()) &&
           cylinderBase.maxParameterV() > 0.0);

    const AcGeBoundedPlane boundedPlane{
        {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0},
        AcGeInterval{-2.0, 2.0}, AcGeInterval{-1.0, 3.0}};
    const AcGeSurface &boundedBase = boundedPlane;
    assert(near(boundedBase.minParameterU(), -2.0));
    assert(near(boundedBase.maxParameterU(), 2.0));
    assert(near(boundedBase.minParameterV(), -1.0));
    assert(near(boundedBase.maxParameterV(), 3.0));
    assert(near(boundedBase.pointAt(1.0, 2.0).x, 1.0));
    assert(near(boundedBase.pointAt(1.0, 2.0).y, 2.0));
}
} // namespace

int main()
{
    curveBasesExposeDomainsAndEvaluation();
    surfaceBaseExposesDomainsAndEvaluation();
}
