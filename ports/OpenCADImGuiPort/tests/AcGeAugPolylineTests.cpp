#include "acge/AcGeAugPolyline3d.h"
#include "acge/AcGeCurve3d.h"
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

void sampleArc()
{
    const AcGeCircArc3d arc{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 2.0};
    const AcGeAugPolyline3d polyline(arc, 0.0, pi, 1.0e-3);

    assert(polyline.numPoints() > 5);
    near(polyline.approxTol(), 1.0e-3);

    const AcGeKnotVector &knots = polyline.knots();
    assert(knots.length() == polyline.numPoints());
    near(knots[0], 0.0, 1e-12);
    near(knots[knots.length() - 1], pi, 1e-12);

    for (int i = 0; i < polyline.numPoints(); ++i)
    {
        const AcGePoint3d point = polyline.getPoint(i);
        // Every vertex lies on the arc.
        near(point.distanceTo({0.0, 0.0, 0.0}), 2.0, 1e-9);

        // First derivative is tangent: orthogonal to the radius, |d1| = r.
        const AcGeVector3d d1 = polyline.getVector(i);
        near(d1.dotProduct(point - AcGePoint3d{0.0, 0.0, 0.0}), 0.0, 1e-6);
        near(d1.length(), 2.0, 1e-6);

        // Second derivative points to the center, |d2| = r.
        const AcGeVector3d d2 = polyline.getD2Vector(i);
        near(d2.length(), 2.0, 1e-5);
        near(d2.dotProduct(point - AcGePoint3d{0.0, 0.0, 0.0}), -4.0, 1e-4);
    }

    // Bundle accessors agree with the per-vertex accessors.
    AcGePoint3dArray points;
    AcGeVector3dArray d1Vectors;
    AcGeVector3dArray d2Vectors;
    polyline.getPoints(points);
    polyline.getD1Vectors(d1Vectors);
    polyline.getD2Vectors(d2Vectors);
    assert(points.size() == static_cast<size_t>(polyline.numPoints()));
    near(points[3], polyline.getPoint(3));
    near(d1Vectors[3], polyline.getVector(3));
    near(d2Vectors[3], polyline.getD2Vector(3));
}

void sampleLine()
{
    const AcGeLineSeg3d segment{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{4.0, 0.0, 0.0}};
    const AcGeAugPolyline3d polyline(segment, 0.0, 1.0, 1.0e-6);

    // A straight chord already satisfies any tolerance.
    assert(polyline.numPoints() == 5);
    for (int i = 0; i < polyline.numPoints(); ++i)
    {
        near(polyline.getVector(i), {4.0, 0.0, 0.0}, 1e-6);
        near(polyline.getD2Vector(i), {0.0, 0.0, 0.0}, 1e-6);
    }
    near(polyline.getPoint(2), {2.0, 0.0, 0.0}, 1e-9);
}

void directData()
{
    AcGeKnotVector knots;
    knots.set(4, 0.0, 3.0);
    const AcGePoint3dArray points{
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 1.0, 0.0}};
    const AcGeVector3dArray d1Vectors{
        {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {1.0, 0.0, 0.0}};
    const AcGeAugPolyline3d polyline(knots, points, d1Vectors);

    assert(polyline.numPoints() == 4);
    near(polyline.getD2Vector(2), {0.0, 0.0, 0.0}); // defaults to zero

    AcGeAugPolyline3d edited = polyline;
    edited.setPoint(1, {1.0, 2.0, 0.0});
    edited.setVector(1, {0.0, 1.0, 0.0});
    edited.setD2Vector(1, {0.0, 0.0, 5.0});
    edited.setApproxTol(0.25);
    near(edited.getPoint(1), {1.0, 2.0, 0.0});
    near(edited.getVector(1), {0.0, 1.0, 0.0});
    near(edited.getD2Vector(1), {0.0, 0.0, 5.0});
    near(edited.approxTol(), 0.25);
    // The original is untouched (value semantics).
    near(polyline.getPoint(1), {1.0, 0.0, 0.0});
}

void validation()
{
    AcGeKnotVector knots;
    knots.set(3, 0.0, 1.0);
    const AcGePoint3dArray twoPoints{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    const AcGeVector3dArray twoVectors{{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};

    throws<std::invalid_argument>(
        [&] { AcGeAugPolyline3d bad(knots, twoPoints, twoVectors); });

    const AcGeLineSeg3d segment{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{1.0, 0.0, 0.0}};
    throws<std::invalid_argument>(
        [&] { AcGeAugPolyline3d bad(segment, 1.0, 0.0, 1e-3); });
    throws<std::invalid_argument>(
        [&] { AcGeAugPolyline3d bad(segment, 0.0, 1.0, 0.0); });

    const AcGeVector3dArray threeVectors{{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                                         {1.0, 0.0, 0.0}};
    AcGeAugPolyline3d polyline(knots, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                                       {2.0, 0.0, 0.0}},
                               threeVectors);
    throws<std::out_of_range>([&] { polyline.getPoint(3); });
    throws<std::out_of_range>([&] { polyline.getVector(-1); });
}
} // namespace

int main()
{
    sampleArc();
    sampleLine();
    directData();
    validation();
    std::cout << "AcGeAugPolylineTests passed\n";
    return 0;
}