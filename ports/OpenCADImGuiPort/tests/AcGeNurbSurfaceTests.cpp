#include "acge/AcGeBspline.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void near(double actual, double expected, double tolerance = 1.0e-9)
{
    assert(std::abs(actual - expected) <= tolerance);
}

void testActiveParameterDomains()
{
    const ge::AcGeBsplineCurve3d bsplineCurve(
        {0.0, 1.0, 2.0, 3.0, 4.0},
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}}, 1);
    near(bsplineCurve.startParam(), 1.0);
    near(bsplineCurve.endParam(), 3.0);

    const ge::AcGeNurbsCurve3d nurbsCurve(
        {0.0, 1.0, 2.0, 3.0, 4.0},
        {{0.0, 0.0, 0.0, 1.0}, {1.0, 0.0, 0.0, 1.0},
         {2.0, 0.0, 0.0, 1.0}},
        1);
    near(nurbsCurve.startParam(), 1.0);
    near(nurbsCurve.endParam(), 3.0);

    ge::AcGeBsplineSurface3d bspline;
    bspline.degreeU = 1;
    bspline.degreeV = 1;
    bspline.knotsU = {0.0, 1.0, 2.0, 3.0, 4.0};
    bspline.knotsV = {0.0, 0.0, 1.0, 1.0};
    bspline.controlPointCountU = 3;
    bspline.controlPointCountV = 2;
    bspline.controlPoints = {
        {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0},
        {2.0, 0.0, 0.0}, {2.0, 1.0, 0.0}};
    bspline.validate();
    near(bspline.minParameterU(), 1.0);
    near(bspline.maxParameterU(), 3.0);
    near(bspline.minParameterV(), 0.0);
    near(bspline.maxParameterV(), 1.0);
}

void testNurbsSurfaceQueries()
{
    ge::AcGeNurbsSurface3d surface;
    surface.degreeU = 1;
    surface.degreeV = 1;
    surface.knotsU = {0.0, 1.0, 2.0, 3.0, 4.0};
    surface.knotsV = {0.0, 0.0, 1.0, 1.0};
    surface.controlPointCountU = 3;
    surface.controlPointCountV = 2;
    surface.controlPoints = {
        {0.0, 0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 2.0}, {1.0, 1.0, 0.0, 2.0},
        {2.0, 0.0, 0.0, 3.0}, {2.0, 1.0, 0.0, 3.0}};
    surface.validate();

    near(surface.minParameterU(), 1.0);
    near(surface.maxParameterU(), 3.0);
    near(surface.minParameterV(), 0.0);
    near(surface.maxParameterV(), 1.0);
    assert(surface.isRational());
    assert(surface.isRationalInU());
    assert(!surface.isRationalInV());
    const AcGeVector3d normal = surface.normalAt(2.0, 0.5);
    near(normal.x, 0.0);
    near(normal.y, 0.0);
    near(normal.z, 1.0);

    size_t degreeU = 0, degreeV = 0, countU = 0, countV = 0;
    bool rationalU = false, rationalV = true;
    AcGePoint3dArray points;
    AcGeDoubleArray weights;
    AcGeKnotVector knotsU, knotsV;
    surface.getDefinitionData(degreeU, degreeV, countU, countV, rationalU,
                              rationalV, points, weights, knotsU, knotsV);
    assert(degreeU == 1 && degreeV == 1);
    assert(countU == 3 && countV == 2);
    assert(rationalU && !rationalV);
    assert(points.size() == 6 && weights.size() == 6);
    near(points[4].x, 2.0);
    near(weights[4], 3.0);
    assert(knotsU.length() == 5 && knotsV.length() == 4);
    near(knotsU[1], 1.0);
    near(knotsU[3], 3.0);
}

void testPeriodicSurfaceQueries()
{
    ge::AcGeNurbsSurface3d cylinder;
    cylinder.degreeU = 2;
    cylinder.degreeV = 1;
    cylinder.knotsU = {0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0,
                       3.0, 3.0, 4.0, 4.0, 4.0};
    cylinder.knotsV = {0.0, 0.0, 1.0, 1.0};
    cylinder.controlPointCountU = 9;
    cylinder.controlPointCountV = 2;
    const double x[] = {1.0, 1.0, 0.0, -1.0, -1.0,
                        -1.0, 0.0, 1.0, 1.0};
    const double y[] = {0.0, 1.0, 1.0, 1.0, 0.0,
                        -1.0, -1.0, -1.0, 0.0};
    for (size_t u = 0; u < 9; ++u)
        for (size_t v = 0; v < 2; ++v)
            cylinder.controlPoints.push_back(
                {x[u], y[u], static_cast<double>(v), 1.0});
    cylinder.validate();

    double period = -1.0;
    assert(cylinder.isPeriodicInU(period));
    near(period, 4.0);
    period = -1.0;
    assert(!cylinder.isPeriodicInV(period));
    near(period, -1.0);
}
} // namespace

int main()
{
    testActiveParameterDomains();
    testNurbsSurfaceQueries();
    testPeriodicSurfaceQueries();
    std::cout << "AcGeNurbSurfaceTests passed\n";
    return 0;
}
