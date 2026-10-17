#include "acge/AcGeCurveBoundBlock.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGe2d.h"
#include "acge/AcGeInterval.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void nearBox3(const AcGePoint3d &actual, const AcGePoint3d &expected,
              double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void nearBox2(const AcGePoint2d &actual, const AcGePoint2d &expected,
              double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y) < tol);
}

void lineBox3d()
{
    const AcGeLineSeg3d line(AcGePoint3d{0.0, 0.0, 0.0},
                             AcGePoint3d{2.0, 3.0, -1.0});
    const AcGeBoundBlock3d block = ge::boundBlock(line);
    assert(block.isBox());
    AcGePoint3d minPt;
    AcGePoint3d maxPt;
    block.getMinMaxPoints(minPt, maxPt);
    nearBox3(minPt, {0.0, 0.0, -1.0});
    nearBox3(maxPt, {2.0, 3.0, 0.0});

    // boundBlock and orthoBoundBlock agree (both coordinate-aligned).
    const AcGeBoundBlock3d ortho = ge::orthoBoundBlock(line);
    AcGePoint3d oMin;
    AcGePoint3d oMax;
    ortho.getMinMaxPoints(oMin, oMax);
    nearBox3(oMin, minPt);
    nearBox3(oMax, maxPt);

    // Interval subrange shrinks the box to the sub-segment.
    const AcGeBoundBlock3d sub = ge::boundBlock(
        line, AcGeInterval{0.25, 0.75});
    sub.getMinMaxPoints(minPt, maxPt);
    nearBox3(minPt, {0.5, 0.75, -0.75});
    nearBox3(maxPt, {1.5, 2.25, -0.25});
}

void arcBox3d()
{
    const AcGeCircArc3d quarter(AcGePoint3d{0.0, 0.0, 0.0},
                                AcGeVector3d{0.0, 0.0, 1.0}, 2.0,
                                0.0, 1.57079632679489661923);
    AcGePoint3d minPt;
    AcGePoint3d maxPt;
    ge::boundBlock(quarter).getMinMaxPoints(minPt, maxPt);
    nearBox3(minPt, {0.0, 0.0, 0.0});
    nearBox3(maxPt, {2.0, 2.0, 0.0});

    const AcGeCircArc3d full(AcGePoint3d{1.0, 2.0, 3.0},
                             AcGeVector3d{0.0, 0.0, 1.0}, 2.0);
    ge::boundBlock(full).getMinMaxPoints(minPt, maxPt);
    nearBox3(minPt, {-1.0, 0.0, 3.0});
    nearBox3(maxPt, {3.0, 4.0, 3.0});
}

void lineBox2d()
{
    const AcGeLineSeg2d line(AcGePoint2d{-1.0, -2.0}, AcGePoint2d{3.0, 4.0});
    const AcGeBoundBlock2d block = ge::boundBlock2d(line);
    assert(block.isBox());
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    block.getMinMaxPoints(minPt, maxPt);
    nearBox2(minPt, {-1.0, -2.0});
    nearBox2(maxPt, {3.0, 4.0});

    const AcGeBoundBlock2d ortho = ge::orthoBoundBlock2d(line);
    AcGePoint2d oMin;
    AcGePoint2d oMax;
    ortho.getMinMaxPoints(oMin, oMax);
    nearBox2(oMin, minPt);
    nearBox2(oMax, maxPt);
}

void arcBox2d()
{
    // Half circle through (-2, 0): angles pi/2 .. 3pi/2.
    const AcGeCircArc2d half(AcGePoint2d{0.0, 0.0}, 2.0,
                             1.57079632679489661923,
                             4.71238898038468985769);
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    ge::boundBlock2d(half).getMinMaxPoints(minPt, maxPt);
    nearBox2(minPt, {-2.0, -2.0});
    nearBox2(maxPt, {0.0, 2.0});

    // Interval limits a full circle to the first quadrant angles.
    const AcGeCircArc2d circle(AcGePoint2d{0.0, 0.0}, 2.0,
                               0.0, 6.28318530717958647693);
    const AcGeBoundBlock2d quadrant = ge::boundBlock2d(
        circle, AcGeInterval{0.0, 1.57079632679489661923});
    quadrant.getMinMaxPoints(minPt, maxPt);
    nearBox2(minPt, {0.0, 0.0});
    nearBox2(maxPt, {2.0, 2.0});
}

} // namespace

int main()
{
    lineBox3d();
    arcBox3d();
    lineBox2d();
    arcBox2d();
    std::cout << "AcGeCurveBoundBlockTests passed\n";
    return 0;
}
