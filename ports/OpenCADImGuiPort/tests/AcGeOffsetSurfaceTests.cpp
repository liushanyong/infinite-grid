#include "acge/AcGeOffsetSurface.h"
#include "acge/AcGeBoundedPlane.h"
#include "acge/AcGeInterval.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void nearPoint3(const AcGePoint3d &actual, const AcGePoint3d &expected,
                double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void nearVector3(const AcGeVector3d &actual, const AcGeVector3d &expected,
                 double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void sphereOffset()
{
    const AcGeSphere sphere(2.0, AcGePoint3d{1.0, 2.0, 3.0});
    const AcGeOffsetSurface offset(sphere, 1.0);
    assert(!offset.isEmpty());
    assert(offset.offsetDist() == 1.0);
    assert(offset.isSphere());
    assert(!offset.isPlane() && !offset.isCylinder() && !offset.isCone() &&
           !offset.isTorus());

    const AcGePoint2d param{0.7, -0.3};
    const AcGePoint3d base = sphere.pointAt(param.x, param.y);
    const double baseDist = base.distanceTo(AcGePoint3d{1.0, 2.0, 3.0});
    const AcGePoint3d point = offset.pointAt(param.x, param.y);
    nearVector3(point - base, sphere.normalAt(param.x, param.y));
    const double dist = point.distanceTo(AcGePoint3d{1.0, 2.0, 3.0});
    assert(std::abs(dist - (baseDist + 1.0)) < 1e-9);

    // The offset preserves the base normal.
    nearVector3(offset.normalAt(param.x, param.y),
                sphere.normalAt(param.x, param.y));

    // Negative offset goes inward.
    const AcGeOffsetSurface inner(sphere, -0.5);
    const AcGePoint3d innerPoint = inner.pointAt(param.x, param.y);
    assert(std::abs(innerPoint.distanceTo(AcGePoint3d{1.0, 2.0, 3.0}) -
                    (baseDist - 0.5)) < 1e-9);
}

void planeOffset()
{
    const AcGeBoundedPlane plane(
        AcGePoint3d{0.0, 0.0, 5.0}, AcGeVector3d{0.0, 0.0, 1.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{0.0, 1.0},
        AcGeInterval{0.0, 1.0});
    AcGeOffsetSurface offset(plane, 2.0);
    assert(offset.isPlane() && offset.isBoundedPlane());
    assert(!offset.isSphere() && !offset.isCylinder() && !offset.isCone() &&
           !offset.isTorus());
    nearPoint3(offset.pointAt(0.25, 0.75), AcGePoint3d{0.25, 0.75, 7.0});

    // The construction-surface view evaluates exactly like the base.
    AcGeOffsetSurface::ErasedSurfaceView view;
    offset.getConstructionSurface(view);
    assert(static_cast<bool>(view.point));
    nearPoint3(view.point(0.25, 0.75), plane.pointAt(0.25, 0.75));
    nearVector3(view.normal(0.25, 0.75), AcGeVector3d{0.0, 0.0, 1.0});

    // Fitted-surface conversion is not available in the value-type port.
    AcGeOffsetSurface::ErasedSurfaceView fitted;
    assert(!offset.getSurface(fitted));
}

void unknownKind()
{
    // A cylinder-like surface without analytic normals: the offset falls
    // back to finite-difference normals.
    struct ConeLike
    {
        AcGePoint3d pointAt(double u, double v) const
        {
            return AcGePoint3d{std::cos(u), std::sin(u), v};
        }
    };
    const ConeLike base;
    AcGeOffsetSurface offset(base, 1.0);
    assert(!offset.isPlane() && !offset.isSphere() && !offset.isCylinder() &&
           !offset.isCone() && !offset.isTorus());

    const AcGePoint2d param{0.4, 0.5};
    const AcGePoint3d point = offset.pointAt(param.x, param.y);
    // Finite-difference normal of (cos u, sin u, v) is (cos u, sin u, 0),
    // so the offset point lands on radius 2 at the same height.
    nearPoint3(point, AcGePoint3d{2.0 * std::cos(0.4), 2.0 * std::sin(0.4),
                                  0.5},
               1e-4);
    nearVector3(offset.normalAt(param.x, param.y),
                AcGeVector3d{std::cos(0.4), std::sin(0.4), 0.0}, 1e-4);

    // Ownership flag parity (value semantics always own the base).
    assert(offset.isOwnerOfSurface());
    AcGeOffsetSurface noCopy(base, 1.0, false);
    assert(!noCopy.isOwnerOfSurface());
    noCopy.setToOwnSurface();
    assert(noCopy.isOwnerOfSurface());
}

void redefinition()
{
    const AcGeSphere sphere(1.0, AcGePoint3d{0.0, 0.0, 0.0});
    AcGeOffsetSurface offset(sphere, 1.0);
    const AcGeBoundedPlane plane(
        AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 1.0, 0.0},
        AcGeVector3d{1.0, 0.0, 0.0}, AcGeInterval{0.0, 1.0},
        AcGeInterval{0.0, 1.0});
    offset.set(plane, -1.0);
    assert(offset.isPlane() && !offset.isSphere());
    assert(offset.offsetDist() == -1.0);
    nearPoint3(offset.pointAt(0.0, 0.0), AcGePoint3d{0.0, -1.0, 0.0});
}

} // namespace

int main()
{
    sphereOffset();
    planeOffset();
    unknownKind();
    redefinition();
    std::cout << "AcGeOffsetSurfaceTests passed\n";
    return 0;
}
