#include "acge/AcGeUtil.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void nearPoint(const AcGePoint2d &actual, const AcGePoint2d &expected,
               double tol = 1e-12)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y) < tol);
}

void nearPoint3(const AcGePoint3d &actual, const AcGePoint3d &expected,
                double tol = 1e-12)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void nearVector(const AcGeVector3d &actual, const AcGeVector3d &expected,
                double tol = 1e-12)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void emptyBlock2d()
{
    const AcGeBoundBlock2d empty;
    assert(empty.isEmpty());
    assert(!empty.contains(AcGePoint2d{0.0, 0.0}));
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    empty.getMinMaxPoints(minPt, maxPt);
    nearPoint(minPt, {0.0, 0.0});
    nearPoint(maxPt, {0.0, 0.0});

    AcGeBoundBlock2d point = empty;
    point.extend(AcGePoint2d{3.0, -2.0});
    point.getMinMaxPoints(minPt, maxPt);
    nearPoint(minPt, {3.0, -2.0});
    nearPoint(maxPt, {3.0, -2.0});
    assert(point.isBox());
    assert(point.contains(AcGePoint2d{3.0, -2.0}));
    assert(!point.contains(AcGePoint2d{3.0, -1.9}));
}

void coordinateBox2d()
{
    const AcGeBoundBlock2d box(AcGePoint2d{2.0, -1.0}, AcGePoint2d{0.0, 3.0});
    assert(box.isBox());
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    box.getMinMaxPoints(minPt, maxPt);
    nearPoint(minPt, {0.0, -1.0});
    nearPoint(maxPt, {2.0, 3.0});
    assert(box.contains(AcGePoint2d{0.0, -1.0}));
    assert(box.contains(AcGePoint2d{1.0, 1.0}));
    assert(!box.contains(AcGePoint2d{2.1, 1.0}));

    AcGePoint2d base;
    AcGeVector2d side1;
    AcGeVector2d side2;
    box.get(base, side1, side2);
    nearPoint(base, {0.0, -1.0});
    assert(side1.x == 2.0 && side1.y == 0.0);
    assert(side2.x == 0.0 && side2.y == 4.0);
}

void orientedBlock2d()
{
    // Unit square rotated 45 degrees (diamond with corners
    // (0,0), (1,1), (0,2), (-1,1)).
    AcGeBoundBlock2d diamond(AcGePoint2d{0.0, 0.0},
                             AcGeVector2d{1.0, 1.0},
                             AcGeVector2d{-1.0, 1.0});
    assert(!diamond.isBox());
    assert(diamond.contains(AcGePoint2d{0.5, 0.5}));
    assert(diamond.contains(AcGePoint2d{0.0, 2.0}));
    assert(!diamond.contains(AcGePoint2d{1.0, 0.0}));

    AcGePoint2d base;
    AcGeVector2d side1;
    AcGeVector2d side2;
    diamond.get(base, side1, side2);
    nearPoint(base, {0.0, 0.0});
    assert(side1.x == 1.0 && side1.y == 1.0);
    assert(side2.x == -1.0 && side2.y == 1.0);

    // Parametric extend: each edge parameter range grows independently,
    // so the result is a parallelogram containing the point (the ARX
    // extend() contract; not the minimal convex hull).
    diamond.extend(AcGePoint2d{2.0, 1.0}); // t1 = 1.5, t2 = -0.5
    diamond.get(base, side1, side2);
    nearPoint(base, {0.5, -0.5});
    assert(std::abs(side1.x - 1.5) < 1e-12 &&
           std::abs(side1.y - 1.5) < 1e-12);
    assert(std::abs(side2.x + 1.5) < 1e-12 &&
           std::abs(side2.y - 1.5) < 1e-12);
    assert(diamond.contains(AcGePoint2d{2.0, 1.0}));
    assert(!diamond.isBox());

    // setToBox converts to the coordinate-aligned box of the corners.
    diamond.setToBox();
    assert(diamond.isBox());
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    diamond.getMinMaxPoints(minPt, maxPt);
    nearPoint(minPt, {-1.0, -0.5});
    nearPoint(maxPt, {2.0, 2.5});
    assert(diamond.contains(AcGePoint2d{2.0, 1.0}));
}

void disjoint2d()
{
    const AcGeBoundBlock2d a(AcGePoint2d{0.0, 0.0}, AcGePoint2d{1.0, 1.0});
    const AcGeBoundBlock2d b(AcGePoint2d{2.0, 0.0}, AcGePoint2d{3.0, 1.0});
    const AcGeBoundBlock2d c(AcGePoint2d{0.5, 0.5}, AcGePoint2d{1.5, 1.5});
    const AcGeBoundBlock2d touch(AcGePoint2d{1.0, 0.0}, AcGePoint2d{2.0, 1.0});
    assert(a.isDisjoint(b));
    assert(!a.isDisjoint(c));
    assert(!a.isDisjoint(touch)); // touching is not disjoint
    assert(a.isDisjoint(AcGeBoundBlock2d{}));
}

void swell2d()
{
    AcGeBoundBlock2d box(AcGePoint2d{0.0, 0.0}, AcGePoint2d{1.0, 2.0});
    box.swell(0.5);
    AcGePoint2d minPt;
    AcGePoint2d maxPt;
    box.getMinMaxPoints(minPt, maxPt);
    nearPoint(minPt, {-0.5, -0.5});
    nearPoint(maxPt, {1.5, 2.5});
    assert(box.contains(AcGePoint2d{-0.4, 1.0}));
    assert(!box.contains(AcGePoint2d{-0.6, 1.0}));

    // Oriented swell keeps the edges: diamond with corners
    // (0,0), (1,1), (0,2), (-1,1) swelled by 1/2 along each face.
    AcGeBoundBlock2d diamond(AcGePoint2d{0.0, 0.0},
                             AcGeVector2d{1.0, 1.0},
                             AcGeVector2d{-1.0, 1.0});
    diamond.swell(0.5); // parameter delta 0.5/sqrt(2) per edge
    assert(!diamond.isBox());
    // Face midpoints move outward by 0.5: the face center (0.5, 0.5)
    // shifted along its normal (1, -1)/sqrt(2) lands exactly on the
    // swollen face.
    const double half = 0.5 / std::sqrt(2.0);
    assert(diamond.contains(AcGePoint2d{0.5 + half, 0.5 - half}));
}

void coordinateBox3d()
{
    const AcGeBoundBlock3d box(AcGePoint3d{1.0, 2.0, 3.0},
                               AcGePoint3d{-1.0, 4.0, 5.0});
    assert(box.isBox());
    AcGePoint3d minPt;
    AcGePoint3d maxPt;
    box.getMinMaxPoints(minPt, maxPt);
    nearPoint3(minPt, {-1.0, 2.0, 3.0});
    nearPoint3(maxPt, {1.0, 4.0, 5.0});
    assert(box.contains(AcGePoint3d{0.0, 3.0, 4.0}));
    assert(box.contains(AcGePoint3d{1.0, 4.0, 5.0}));
    assert(!box.contains(AcGePoint3d{1.1, 3.0, 4.0}));

    AcGePoint3d base;
    AcGeVector3d side1;
    AcGeVector3d side2;
    AcGeVector3d side3;
    box.get(base, side1, side2, side3);
    nearPoint3(base, {-1.0, 2.0, 3.0});
    nearVector(side1, {2.0, 0.0, 0.0});
    nearVector(side2, {0.0, 2.0, 0.0});
    nearVector(side3, {0.0, 0.0, 2.0});
}

void orientedBlock3d()
{
    // Slanted prism: base corner at origin, sides along x, y and (1,1,1).
    AcGeBoundBlock3d block(AcGePoint3d{0.0, 0.0, 0.0},
                           AcGeVector3d{2.0, 0.0, 0.0},
                           AcGeVector3d{0.0, 2.0, 0.0},
                           AcGeVector3d{1.0, 1.0, 1.0});
    assert(!block.isBox());
    // Interior point: t = (0.25, 0.25, 0.5) -> (1.0, 1.0, 0.5).
    assert(block.contains(AcGePoint3d{1.0, 1.0, 0.5}));
    // Outside the oriented block (t2 < 0), but inside its coordinate box.
    assert(!block.contains(AcGePoint3d{2.9, 0.5, 0.9}));

    block.setToBox();
    assert(block.isBox());
    AcGePoint3d minPt;
    AcGePoint3d maxPt;
    block.getMinMaxPoints(minPt, maxPt);
    nearPoint3(minPt, {0.0, 0.0, 0.0});
    nearPoint3(maxPt, {3.0, 3.0, 1.0});
    assert(block.contains(AcGePoint3d{2.9, 0.5, 0.9}));
}

void extendAndSwell3d()
{
    AcGeBoundBlock3d box(AcGePoint3d{0.0, 0.0, 0.0}, AcGePoint3d{1.0, 1.0, 1.0});
    box.extend(AcGePoint3d{2.0, 0.5, -1.0});
    AcGePoint3d minPt;
    AcGePoint3d maxPt;
    box.getMinMaxPoints(minPt, maxPt);
    nearPoint3(minPt, {0.0, 0.0, -1.0});
    nearPoint3(maxPt, {2.0, 1.0, 1.0});
    assert(box.contains(AcGePoint3d{1.5, 0.5, -0.5}));

    // Parametric extend on an oriented block.
    AcGeBoundBlock3d unit(AcGePoint3d{0.0, 0.0, 0.0},
                          AcGeVector3d{1.0, 0.0, 0.0},
                          AcGeVector3d{0.0, 1.0, 0.0},
                          AcGeVector3d{0.0, 0.0, 1.0});
    unit.extend(AcGePoint3d{1.5, 0.5, 0.5});
    AcGePoint3d base;
    AcGeVector3d side1;
    AcGeVector3d side2;
    AcGeVector3d side3;
    unit.get(base, side1, side2, side3);
    nearPoint3(base, {0.0, 0.0, 0.0});
    nearVector(side1, {1.5, 0.0, 0.0});
    nearVector(side2, {0.0, 1.0, 0.0});
    nearVector(side3, {0.0, 0.0, 1.0});
    assert(unit.contains(AcGePoint3d{1.2, 0.5, 0.5}));

    AcGeBoundBlock3d swellBox(AcGePoint3d{0.0, 0.0, 0.0},
                              AcGePoint3d{2.0, 2.0, 2.0});
    swellBox.swell(0.5);
    swellBox.getMinMaxPoints(minPt, maxPt);
    nearPoint3(minPt, {-0.5, -0.5, -0.5});
    nearPoint3(maxPt, {2.5, 2.5, 2.5});
    assert(swellBox.contains(AcGePoint3d{-0.4, 1.0, 1.0}));
    assert(!swellBox.contains(AcGePoint3d{-0.6, 1.0, 1.0}));
}

void disjoint3d()
{
    const AcGeBoundBlock3d a(AcGePoint3d{0.0, 0.0, 0.0},
                             AcGePoint3d{1.0, 1.0, 1.0});
    const AcGeBoundBlock3d b(AcGePoint3d{2.0, 0.0, 0.0},
                             AcGePoint3d{3.0, 1.0, 1.0});
    const AcGeBoundBlock3d c(AcGePoint3d{0.5, 0.5, 0.5},
                             AcGePoint3d{1.5, 1.5, 1.5});
    const AcGeBoundBlock3d touch(AcGePoint3d{1.0, 0.0, 0.0},
                                 AcGePoint3d{2.0, 1.0, 1.0});
    assert(a.isDisjoint(b));
    assert(!a.isDisjoint(c));
    assert(!a.isDisjoint(touch));
    assert(a.isDisjoint(AcGeBoundBlock3d{}));

    // Two unit squares rotated 45 degrees around z, offset along x so the
    // rotated prisms stay separated.
    const AcGeBoundBlock3d r1(AcGePoint3d{0.0, 0.0, 0.0},
                              AcGeVector3d{1.0, 1.0, 0.0},
                              AcGeVector3d{-1.0, 1.0, 0.0},
                              AcGeVector3d{0.0, 0.0, 1.0});
    const AcGeBoundBlock3d r2(AcGePoint3d{2.2, 0.0, 0.0},
                              AcGeVector3d{1.0, 1.0, 0.0},
                              AcGeVector3d{-1.0, 1.0, 0.0},
                              AcGeVector3d{0.0, 0.0, 1.0});
    assert(r1.isDisjoint(r2));
}

} // namespace

int main()
{
    emptyBlock2d();
    coordinateBox2d();
    orientedBlock2d();
    disjoint2d();
    swell2d();
    coordinateBox3d();
    orientedBlock3d();
    extendAndSwell3d();
    disjoint3d();
    std::cout << "AcGeBoundBlockTests passed\n";
    return 0;
}
