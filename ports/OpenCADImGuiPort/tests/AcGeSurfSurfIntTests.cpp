#include "acge/AcGe.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void boundedPlaneIntersection()
{
    const AcGeBoundedPlane horizontal{
        AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 0.0, 1.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{-1.0, 1.0},
        AcGeInterval{-1.0, 1.0}};
    const AcGeBoundedPlane vertical{
        AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{1.0, 0.0, 0.0},
        AcGeVector3d{0.0, 1.0, 0.0}, AcGeInterval{-1.0, 1.0},
        AcGeInterval{-1.0, 1.0}};

    const AcGeSurfSurfInt intersections(horizontal, vertical);
    assert(intersections.numResults() == 1);
    assert(intersections.getDimension(0) == 1);
    const auto curve = intersections.intCurve(0);
    const auto firstParams = intersections.intParamCurve(0, true);
    const auto secondParams = intersections.intParamCurve(0, false);
    assert(curve.size() >= 40);
    assert(curve.size() == firstParams.size());
    assert(curve.size() == secondParams.size());
    for (const AcGePoint3d &point : curve)
    {
        assert(std::abs(point.x) < 1.0e-9);
        assert(std::abs(point.z) < 1.0e-9);
        assert(point.y >= -1.0 && point.y <= 1.0);
    }
    assert(intersections.tolerance() > 0.0);
    bool wrongDimension = false;
    try
    {
        (void)intersections.intPoint(0);
    }
    catch (const std::logic_error &)
    {
        wrongDimension = true;
    }
    assert(wrongDimension);
}

void externalAdapters()
{
    AcGeExternalCurve2d curve2d{
        [](double t) { return AcGePoint2d{t, t * t}; }, -1.0, 1.0,
        AcGe::kAcisEntity};
    assert(curve2d.isDefined());
    assert(curve2d.externalCurveKind() == AcGe::kAcisEntity);
    assert(std::abs(curve2d.pointAt(0.5).y - 0.25) < 1.0e-12);
    curve2d.setToOwnCurve();
    assert(curve2d.isOwnerOfCurve());

    const AcGeExternalCurve3d curve3d{
        [](double t) { return AcGePoint3d{0.0, 0.0, t}; }, -1.0, 1.0,
        AcGe::kExternalEntityUndefined,
        [](double) { return AcGeVector3d{0.0, 0.0, 1.0}; }};
    const AcGeExternalSurface surface{
        [](double u, double v) { return AcGePoint3d{u, v, 0.0}; },
        -1.0, 1.0, -1.0, 1.0};
    const AcGeCurveSurfInt intersections(curve3d, surface);
    assert(intersections.numIntPoints() == 1);
    assert(intersections.intPoint(0).distanceTo({0.0, 0.0, 0.0}) < 1.0e-6);

    const AcGeExternalCurve3d finiteDifferenceCurve{
        [](double t) { return AcGePoint3d{t, 0.0, 0.0}; }, -1.0, 1.0};
    assert(std::abs(finiteDifferenceCurve.firstDeriv(0.25).x - 1.0) < 1.0e-8);

    AcGeExternalSurface undefined;
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

void emptyAndInvalidCases()
{
    const AcGeSurfSurfInt empty;
    assert(empty.numResults() == 0);
    bool outOfRange = false;
    try
    {
        (void)empty.getDimension(0);
    }
    catch (const std::out_of_range &)
    {
        outOfRange = true;
    }
    assert(outOfRange);

    bool invalidRange = false;
    try
    {
        AcGeExternalSurface invalid{
            [](double u, double v) { return AcGePoint3d{u, v, 0.0}; },
            0.0, 0.0, 0.0, 1.0};
        (void)invalid;
    }
    catch (const std::invalid_argument &)
    {
        invalidRange = true;
    }
    assert(invalidRange);
}
} // namespace

int main()
{
    boundedPlaneIntersection();
    externalAdapters();
    emptyAndInvalidCases();
    std::cout << "AcGeSurfSurfIntTests passed\n";
    return 0;
}
