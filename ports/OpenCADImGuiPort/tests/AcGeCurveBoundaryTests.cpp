#include "acge/AcGeCurveBoundary.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGe2d.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
template <class Exception, class F>
void throws(F &&operation)
{
    bool caught = false;
    try { operation(); }
    catch (const Exception &) { caught = true; }
    assert(caught);
}

void near(const AcGePoint3d &actual, const AcGePoint3d &expected, double tol = 1e-12)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void contourRoundtrip()
{
    const AcGeLineSeg3d edge{AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{1.0, 0.0, 0.0}};
    std::vector<AcGeCurveBoundary::Contour3dElement> contour3d(3);
    contour3d[0].curve = [edge](double u) { return edge.pointAt(u); };
    contour3d[0].orientation = true;
    contour3d[1].curve = [](double u)
    { return AcGePoint3d{std::cos(2.0 * u), std::sin(2.0 * u), 0.0}; };
    contour3d[1].orientation = false;
    contour3d[2].position = {1.0, 1.0, 0.0}; // degenerate element
    contour3d[2].orientation = true;

    std::vector<AcGeCurveBoundary::Contour2dElement> contour2d(3);
    contour2d[0].curve = [](double u) { return AcGePoint2d{u, 0.0}; };
    contour2d[1].curve = [](double u) { return AcGePoint2d{1.0, u}; };
    contour2d[1].orientation = false;
    contour2d[2].position = {1.0, 1.0};

    const AcGeCurveBoundary boundary(contour3d, contour2d, true);
    assert(boundary.numElements() == 3);
    assert(boundary.isOwnerOfCurves());

    std::vector<AcGeCurveBoundary::Contour3dElement> out3d;
    std::vector<AcGeCurveBoundary::Contour2dElement> out2d;
    boundary.getContour(out3d, out2d);
    assert(out3d.size() == 3);
    assert(out2d.size() == 3);
    assert(static_cast<bool>(out3d[0].curve));
    assert(!out3d[2].curve); // position element
    near(out3d[2].position, {1.0, 1.0, 0.0});
    assert(out3d[1].orientation == false);
    assert(out2d[1].orientation == false);
    // Evaluating the erased curve reaches the original geometry.
    near(out3d[0].curve(0.5), edge.pointAt(0.5));
    assert(out2d[0].curve(0.25).x == 0.25);
}

void degenerateBoundary()
{
    std::vector<AcGeCurveBoundary::Contour3dElement> contour3d(1);
    contour3d[0].position = {2.0, 3.0, 4.0};
    AcGeCurveBoundary boundary(contour3d, {});
    assert(boundary.numElements() == 1);
    assert(boundary.isDegenerate());
    AcGePoint3d position;
    assert(boundary.isDegenerate(position));
    near(position, {2.0, 3.0, 4.0});
    assert(!boundary.isOwnerOfCurves());
    boundary.setToOwnCurves();
    assert(boundary.isOwnerOfCurves());
}

void validation()
{
    std::vector<AcGeCurveBoundary::Contour3dElement> two(2);
    std::vector<AcGeCurveBoundary::Contour2dElement> three(3);
    throws<std::invalid_argument>(
        [&] { AcGeCurveBoundary bad(two, three); });
    throws<std::invalid_argument>([] { AcGeCurveBoundary empty({}, {}); });
}
} // namespace

int main()
{
    contourRoundtrip();
    degenerateBoundary();
    validation();
    std::cout << "AcGeCurveBoundaryTests passed\n";
    return 0;
}