#include "acge/AcGe.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    const AcGeExternalSurface base{
        [](double u, double v) { return AcGePoint3d{u, v, 0.0}; },
        -2.0, 2.0, -3.0, 3.0, AcGe::kAcisEntity};
    const AcGeCurveBoundary boundary{
        {AcGeCurveBoundary::Contour3dElement{
            {}, AcGePoint3d{0.0, 0.0, 0.0}, true}},
        {AcGeCurveBoundary::Contour2dElement{
            {}, AcGePoint2d{0.0, 0.0}, true}}};
    AcGeExternalBoundedSurface surface{
        base, {boundary}, AcGeExternalBoundedSurface::BaseKind::Plane, false};

    assert(surface.isDefined());
    assert(surface.externalSurfaceKind() == AcGe::kAcisEntity);
    assert(surface.isPlane());
    assert(!surface.isExternalSurface());
    assert(surface.numContours() == 1);
    assert(surface.minParameterU() == -2.0);
    assert(surface.maxParameterV() == 3.0);
    assert(surface.pointAt(1.0, 2.0).distanceTo({1.0, 2.0, 0.0}) < 1.0e-12);

    AcGeSurface &asSurface = surface;
    assert(asSurface.pointAt(-1.0, 0.5).distanceTo({-1.0, 0.5, 0.0}) <
           1.0e-12);
    std::vector<AcGeCurveBoundary> contours;
    surface.getContours(contours);
    assert(contours.size() == 1);
    AcGeExternalSurface copiedBase;
    surface.getBaseSurface(copiedBase);
    assert(copiedBase.pointAt(0.5, -0.5).distanceTo({0.5, -0.5, 0.0}) <
           1.0e-12);

    surface.setToOwnSurface();
    assert(surface.isOwnerOfSurface());

    AcGeExternalBoundedSurface undefined;
    assert(!undefined.isDefined());
    bool threw = false;
    try
    {
        (void)undefined.pointAt(0.0, 0.0);
    }
    catch (const std::logic_error &)
    {
        threw = true;
    }
    assert(threw);
}
