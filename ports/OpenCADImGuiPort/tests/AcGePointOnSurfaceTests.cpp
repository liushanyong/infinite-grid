#include "acge/AcGePointOnSurface.h"
#include "acge/AcGeBoundedPlane.h"
#include "acge/AcGeSurface3d.h"

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

void boundedPlaneAnalytic()
{
    const AcGeBoundedPlane plane{
        {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0},
        AcGeInterval{0.0, 1.0}, AcGeInterval{0.0, 1.0}};
    AcGePointOnSurface pointOn(plane, {0.25, 0.5});

    near(pointOn.point(), {0.25, 0.5, 0.0});
    near(pointOn.uDeriv(1), {1.0, 0.0, 0.0}, 1e-9);
    near(pointOn.vDeriv(1), {0.0, 1.0, 0.0}, 1e-9);
    near(pointOn.uDeriv(2), {0.0, 0.0, 0.0}, 1e-6);
    near(pointOn.vDeriv(2), {0.0, 0.0, 0.0}, 1e-6);
    near(pointOn.mixedPartial(), {0.0, 0.0, 0.0}, 1e-6);
    near(pointOn.normal(), {0.0, 0.0, 1.0}, 1e-9);

    // Tangent-plane maps.
    near(pointOn.tangentVector({2.0, 3.0}), {2.0, 3.0, 0.0}, 1e-9);
    const AcGeVector2d inverse = pointOn.inverseTangentVector({2.0, 3.0, 7.0});
    near(inverse.x, 2.0, 1e-9);
    near(inverse.y, 3.0, 1e-9);

    // point(param) resets the cached parameter.
    near(pointOn.point({0.75, 0.0}), {0.75, 0.0, 0.0});
    near(pointOn.parameter().x, 0.75);
    near(pointOn.parameter().y, 0.0);
}

void sphereNumericDerivatives()
{
    const AcGeSphere sphere(2.0, {1.0, 2.0, 3.0});
    const AcGePoint2d param{0.7, 0.4};
    const AcGePointOnSurface pointOn(sphere, param);

    // dP/du magnitude = r cos(v); dP/dv magnitude = r.
    near(pointOn.uDeriv(1).length(), 2.0 * std::cos(0.4), 1e-6);
    near(pointOn.vDeriv(1).length(), 2.0, 1e-6);
    // d2P/du2 magnitude = r cos(v).
    near(pointOn.uDeriv(2).length(), 2.0 * std::cos(0.4), 1e-4);

    const AcGePoint3d location = pointOn.point();
    const AcGeVector3d radial = location - sphere.center();
    near(radial.length(), 2.0, 1e-9);
    near(pointOn.normal(), radial * (1.0 / 2.0), 1e-7);
}

void cylinderNumericDerivatives()
{
    const AcGeCylinder cylinder(3.0, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    const AcGePointOnSurface pointOn(cylinder, {0.9, 1.5});

    near(pointOn.uDeriv(1).length(), 3.0, 1e-6);
    near(pointOn.vDeriv(1), {0.0, 0.0, 1.0}, 1e-9);
    near(pointOn.mixedPartial(), {0.0, 0.0, 0.0}, 1e-6);

    // Normal matches the analytic surface normal.
    const AcGeVector3d analytic = cylinder.normalAt(0.9, 1.5);
    near(pointOn.normal(), analytic, 1e-7);

    // Tangent round trip: parameter-space vector maps back through
    // tangentVector after inversion.
    const AcGeVector2d original{1.5, -2.0};
    const AcGeVector2d inverted =
        pointOn.inverseTangentVector(pointOn.tangentVector(original));
    near(inverted.x, original.x, 1e-7);
    near(inverted.y, original.y, 1e-7);
}

void errorsAndRebinding()
{
    AcGePointOnSurface empty;
    throws<std::logic_error>([&] { empty.point(); });
    throws<std::logic_error>([&] { empty.uDeriv(1); });
    throws<std::logic_error>([&] { empty.normal(); });
    throws<std::invalid_argument>([&] { empty.vDeriv(0); });

    const AcGeBoundedPlane plane{
        {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0},
        AcGeInterval{0.0, 1.0}, AcGeInterval{0.0, 1.0}};
    AcGePointOnSurface pointOn;
    pointOn.setSurface(plane, {0.5, 0.5});
    near(pointOn.point(), {0.5, 0.5, 0.0});
    pointOn.setParameter({0.25, 0.25});
    near(pointOn.point(), {0.25, 0.25, 0.0});

    // Surface + parameter overloads rebind both together.
    const AcGeSphere sphere(1.0, {0.0, 0.0, 0.0});
    const AcGePoint3d onSphere = pointOn.point(sphere, {0.0, 0.5});
    near(onSphere.distanceTo({0.0, 0.0, 0.0}), 1.0, 1e-9);
    near(pointOn.uDeriv(1, sphere, {0.3, 0.2}).length(), std::cos(0.2), 1e-6);
}
} // namespace

int main()
{
    boundedPlaneAnalytic();
    sphereNumericDerivatives();
    cylinderNumericDerivatives();
    errorsAndRebinding();
    std::cout << "AcGePointOnSurfaceTests passed\n";
    return 0;
}