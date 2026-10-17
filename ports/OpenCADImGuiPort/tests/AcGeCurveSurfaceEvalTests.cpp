#include "acge/AcGeCurve.h"
#include "acge/AcGeSurface3d.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
class Parabola final : public AcGeCurve2d
{
public:
    double startParam() const override { return 0.0; }
    double endParam() const override { return 1.0; }
    AcGePoint2d pointAt(double t) const override { return {t, t * t}; }
};

class QuadraticPatch final : public AcGeSurface
{
public:
    double minParameterU() const override { return 0.0; }
    double maxParameterU() const override { return 1.0; }
    double minParameterV() const override { return 0.0; }
    double maxParameterV() const override { return 1.0; }
    AcGePoint3d pointAt(double u, double v) const override
    {
        return {u, v, 2.0 * u + 3.0 * v + u * u + 2.0 * u * v +
                         4.0 * v * v};
    }
};

class UnboundedPatch final : public AcGeSurface
{
public:
    double minParameterU() const override { return 0.0; }
    double maxParameterU() const override { return 1.0; }
    double minParameterV() const override
    {
        return -std::numeric_limits<double>::infinity();
    }
    double maxParameterV() const override
    {
        return std::numeric_limits<double>::infinity();
    }
    AcGePoint3d pointAt(double u, double v) const override
    {
        return {u, v, u + 2.0 * v};
    }
};

class ClosedUUnboundedV final : public AcGeSurface
{
public:
    double minParameterU() const override { return 0.0; }
    double maxParameterU() const override { return 2.0 * std::acos(-1.0); }
    double minParameterV() const override
    {
        return -std::numeric_limits<double>::infinity();
    }
    double maxParameterV() const override
    {
        return std::numeric_limits<double>::infinity();
    }
    AcGePoint3d pointAt(double u, double v) const override
    {
        return {std::cos(u), std::sin(u), v};
    }
};

void near(double actual, double expected, double tolerance = 2.0e-4)
{
    assert(std::abs(actual - expected) <= tolerance);
}

void testCurveEvaluation()
{
    const Parabola curve;
    AcGeVector2dArray derivatives;
    const AcGePoint2d point = curve.evalPoint(0.5, 2, derivatives);
    near(point.x, 0.5);
    near(point.y, 0.25);
    assert(derivatives.size() == 2);
    near(derivatives[0].x, 1.0);
    near(derivatives[0].y, 1.0);
    near(derivatives[1].x, 0.0);
    near(derivatives[1].y, 2.0);

    const AcGePoint2d start = curve.evalPoint(0.0, 2, derivatives);
    near(start.x, 0.0);
    near(derivatives[0].x, 1.0);
    near(derivatives[0].y, 0.0);
    near(derivatives[1].y, 2.0);

    bool rejected = false;
    try
    {
        (void)curve.evalPoint(0.5, 3, derivatives);
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    assert(rejected);
}

void testSurfaceEvaluation()
{
    const QuadraticPatch surface;
    assert(!surface.isClosedInU());
    assert(!surface.isClosedInV());
    AcGeVector3dArray derivatives;
    AcGeVector3d normal;
    const AcGePoint3d point =
        surface.evalPoint({0.5, 0.5}, 2, derivatives, normal);
    near(point.z, 4.25);
    assert(derivatives.size() == 5);
    near(derivatives[0].x, 1.0);
    near(derivatives[0].y, 0.0);
    near(derivatives[0].z, 4.0);
    near(derivatives[1].x, 0.0);
    near(derivatives[1].y, 1.0);
    near(derivatives[1].z, 8.0);
    near(derivatives[2].z, 2.0);
    near(derivatives[3].z, 2.0);
    near(derivatives[4].z, 8.0);
    near(normal.length(), 1.0);
    const AcGeVector3d virtualNormal = surface.normalAt(0.5, 0.5);
    near((virtualNormal - normal).length(), 0.0);
    near(surface.evalPoint({0.5, 0.5}).z, point.z);
    const AcGePoint3d pointOnly =
        surface.evalPoint({0.5, 0.5}, 0, derivatives, normal);
    near(pointOnly.z, point.z);
    assert(derivatives.empty());
    near(normal.length(), 1.0);

    const UnboundedPatch unbounded;
    const AcGePoint3d unboundedPoint =
        unbounded.evalPoint({0.5, 10.0}, 1, derivatives);
    near(unboundedPoint.z, 20.5);
    near(derivatives[0].z, 1.0);
    near(derivatives[1].z, 2.0);

    const ClosedUUnboundedV closed;
    assert(closed.isClosedInU());
    assert(!closed.isClosedInV());

    const AcGeCylinder cylinder{2.0, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
    assert(cylinder.isClosedInU());
    assert(!cylinder.isClosedInV());
    const AcGeTorus torus;
    assert(torus.isClosedInU());
    assert(torus.isClosedInV());
}
} // namespace

int main()
{
    testCurveEvaluation();
    testSurfaceEvaluation();
    std::cout << "AcGeCurveSurfaceEvalTests passed\n";
    return 0;
}
