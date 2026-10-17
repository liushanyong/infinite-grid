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
}

void surfaceBaseExposesDomainsAndEvaluation()
{
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
}
} // namespace

int main()
{
    curveBasesExposeDomainsAndEvaluation();
    surfaceBaseExposesDomainsAndEvaluation();
}
