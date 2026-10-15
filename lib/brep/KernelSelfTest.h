#pragma once

// Kernel self-test (hidden runtime mode): GRID_SELFTEST=kernel runs the
// geom2d + brep milestone checks in-process inside the trusted WINDOW
// host, printing PASS/FAIL and setting the process exit path.  Exists
// because freshly linked small test executables from temp directories
// are flagged by antivirus on this machine.

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "ge/ge2d.h"
#include "ge/geboundedplane.h"
#include "ge/geinterval.h"
#include "ge/gelinear.h"
#include "ge/gematrix2d.h"
#include "ge/gescale.h"
#include "ge/gesurface3d.h"
#include "ge/geutil.h"
#include "brep/BRepBuild.h"
#include "brep/Validate.h"
#include "brep/BRepBoolean.h"
#include "brep/BRepBlend.h"
#include "brep/BRepRayCast.h"
#include "brep/BRepSweep.h"
#include "brep/BRepHistory.h"
#include "brep/StepWriter.h"

namespace brep
{

inline int runKernelSelfTest()
{
    // GUI-subsystem processes have no valid stderr handle; the
    // self-test report goes through a real file instead.
    FILE *report = std::fopen("kernel_selftest.log", "w");
    if (report == nullptr)
        return 99;
    int fails = 0;
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::printf("FAIL %s\n", what);
        }
    };

    // ---------------- bounded planar surface ----------------
    {
        const AcGeBoundedPlane plane{
            AcGePoint3d{1.0, 2.0, 3.0}, AcGeVector3d{0.0, 0.0, 2.0},
            AcGeVector3d{2.0, 0.0, 0.0}, AcGeInterval{-1.0, 1.0},
            AcGeInterval{0.0, 3.0}};
        double u = 0.0;
        double v = 0.0;
        check(plane.pointAt(1.0, 2.0) == AcGePoint3d{2.0, 4.0, 3.0} &&
                  plane.normalAt(0.0, 0.0) == AcGeVector3d{0.0, 0.0, 1.0} &&
                  plane.area() == 6.0 &&
                  plane.parametersOf({2.0, 4.0, 3.0}, u, v) && u == 1.0 &&
                  v == 2.0 && plane.isInside({2.0, 4.0, 3.0}) &&
                  !plane.isInside({2.0, 4.0, 3.1}) &&
                  plane.closestPointTo({5.0, 4.0, 4.0}) ==
                      AcGePoint3d{2.0, 4.0, 3.0},
              "bounded plane evaluation, projection, containment, and area");
    }

    // ---------------- geom2d (milestone 1 tail) ----------------
    {
        using namespace ge;
        AcGeLineSeg2d a{AcGePoint2d{0.0, 0.0}, AcGePoint2d{4.0, 0.0}};
        check(a.startPoint() == AcGePoint2d{0.0, 0.0} &&
                  a.endPoint() == AcGePoint2d{4.0, 0.0} &&
                  a.midPoint() == AcGePoint2d{2.0, 0.0} &&
                  a.baryComb(0.25) == AcGePoint2d{1.0, 0.0} &&
                  a.length(0.25, 0.75) == 2.0 &&
                  a.length(0.75, 0.25) == 2.0 &&
                  a.length(0.25, 0.5, 2.0) == 0.0,
              "2D segment endpoint and barycentric queries");
        AcGeLineSeg2d vectorSegment2d{AcGePoint2d{1.0, 2.0},
                                      AcGeVector2d{3.0, 4.0}};
        vectorSegment2d.set(AcGePoint2d{2.0, 3.0},
                            AcGeVector2d{4.0, 5.0});
        check(vectorSegment2d.endPoint() == AcGePoint2d{6.0, 8.0},
              "2D segment point-vector construction");
        double segmentParameter = -1.0;
        check(a.closestPointTo({2.0, 3.0}) == AcGePoint2d{2.0, 0.0} &&
                  a.distanceTo({2.0, 3.0}) == 3.0 &&
                  a.isOn(AcGePoint2d{3.0, 0.0}, segmentParameter) &&
                  segmentParameter == 0.75 && !a.isOn(1.1),
              "2D segment closest point and membership");
        AcGeLine2d bisector;
        a.getBisector(bisector);
        check(bisector.basePoint == AcGePoint2d{2.0, 0.0} &&
                  bisector.direction.isPerpendicularTo(a.direction()),
              "2D segment perpendicular bisector");

        const AcGeLineSeg3d segment3d{AcGePoint3d{0.0, 0.0, 0.0},
                                      AcGePoint3d{0.0, 0.0, 4.0}};
        AcGeLineSeg3d vectorSegment3d{AcGePoint3d{1.0, 2.0, 3.0},
                                      AcGeVector3d{3.0, 4.0, 5.0}};
        vectorSegment3d.set(AcGePoint3d{2.0, 3.0, 4.0},
                            AcGeVector3d{4.0, 5.0, 6.0});
        check(vectorSegment3d.endPoint() == AcGePoint3d{6.0, 8.0, 10.0},
              "3D segment point-vector construction");
        segmentParameter = -1.0;
        check(segment3d.closestPointTo({1.0, 0.0, 2.0}) ==
                      AcGePoint3d{0.0, 0.0, 2.0} &&
                  segment3d.distanceTo({1.0, 0.0, 2.0}) == 1.0 &&
                  segment3d.isOn(AcGePoint3d{0.0, 0.0, 3.0},
                                 segmentParameter) &&
                  segmentParameter == 0.75,
              "3D segment closest point and membership");
        AcGePlane bisector3d;
        segment3d.getBisector(bisector3d);
        check(segment3d.length() == 4.0 &&
                  segment3d.length(0.25, 0.75) == 2.0 &&
                  segment3d.midPoint() == AcGePoint3d{0.0, 0.0, 2.0} &&
                  segment3d.baryComb(0.25) ==
                      AcGePoint3d{0.0, 0.0, 1.0} &&
                  bisector3d.origin == AcGePoint3d{0.0, 0.0, 2.0} &&
                  bisector3d.normal.isParallelTo(segment3d.direction()),
              "3D segment parameterization and bisector plane");

        AcGeLineSeg2d cross{AcGePoint2d{2.0, -2.0},
                            AcGePoint2d{2.0, 2.0}};
        auto hit = intersect(a, cross);
        check(hit.size() == 1, "line-line crossing yields one point");
        check(!hit.empty() && std::abs(hit[0].x - 2.0) < 1.0e-9 &&
                  (!hit.empty() && std::abs(hit[0].y) < 1.0e-9),
              "line-line crossing point");

        AcGeLineSeg2d par{AcGePoint2d{0.0, 1.0},
                          AcGePoint2d{4.0, 1.0}};
        check(intersect(a, par).empty(), "parallel lines yield none");

        AcGeCircle2d unit{{0.0, 0.0}, 1.0};
        AcGeLineSeg2d through{AcGePoint2d{-2.0, 0.0},
                              AcGePoint2d{2.0, 0.0}};
        check(intersect(through, unit).size() == 2,
              "line-circle yields two points");
        AcGeLineSeg2d tangentY{AcGePoint2d{-2.0, 1.0},
                               AcGePoint2d{2.0, 1.0}};
        auto tangentHit = intersect(tangentY, unit);
        check(tangentHit.size() == 1, "tangent line yields one point");

        AcGeCircle2d offset{{1.0, 0.0}, 1.0};
        check(intersect(unit, offset).size() == 2,
              "circle-circle yields two points");

        AcGeCircArc2d quarter{unit.center, 1.0, 0.0, 1.5707963267948966};
        AcGeLineSeg2d diagonal{AcGePoint2d{-2.0, -2.0},
                               AcGePoint2d{2.0, 2.0}};
        auto arcHit = intersect(diagonal, quarter);
        check(arcHit.size() == 1 &&
                  !arcHit.empty() &&
                  std::abs(arcHit[0].x - 0.7071067811865476) < 1.0e-6,
              "line-arc filters to the quarter span");
        auto arcArc = intersect(
            quarter, AcGeCircArc2d{{1.0, 0.0}, 1.0, 0.0, 6.283185307179586});
        check(arcArc.size() == 1 &&
                  !arcArc.empty() &&
                  std::abs(arcArc[0].y - 0.8660254037844386) < 1.0e-6,
              "arc-arc filters to both spans");

        const ::AcGeCircArc2d clockwise{
            AcGePoint2d{0.0, 0.0}, 1.0, 0.0, 1.5707963267948966,
            AcGeVector2d::kXAxis, true};
        const ::AcGeCircArc2d rotatedFrame{
            AcGePoint2d{0.0, 0.0}, 2.0, 0.0, 1.5707963267948966,
            AcGeVector2d::kYAxis, false};
        check(clockwise.isClockWise() &&
                  clockwise.isInside({0.0, -1.0}) &&
                  !clockwise.isInside({0.7071067811865476,
                                       0.7071067811865476}) &&
                  rotatedFrame.startPoint().isEqualTo({0.0, 2.0}) &&
                  rotatedFrame.endPoint().isEqualTo({-2.0, 0.0}),
              "2D circular arc direction and reference frame");

        const ::AcGeEllipArc2d ellipse2d{
            AcGePoint2d{1.0, 2.0}, AcGeVector2d::kXAxis,
            AcGeVector2d::kYAxis, 3.0, 1.5, 0.0,
            1.5707963267948966};
        check(ellipse2d.majorRadius() == 3.0 &&
                  ellipse2d.minorRadius() == 1.5 &&
                  ellipse2d.startPoint().isEqualTo({4.0, 2.0}) &&
                  ellipse2d.endPoint().isEqualTo({1.0, 3.5}) &&
                  ellipse2d.isInside({1.0, 3.5}) &&
                  !ellipse2d.isInside({1.0, 0.5}) &&
                  !ellipse2d.isCircular(),
              "2D ellipse axes, parameterization, and membership");
        const ::AcGeEllipArc2d circleAsEllipse{clockwise};
        check(circleAsEllipse.isClockWise() &&
                  circleAsEllipse.startPoint().isEqualTo({1.0, 0.0}) &&
                  circleAsEllipse.endPoint().isEqualTo({0.0, 1.0}) &&
                  circleAsEllipse.isInside({0.0, -1.0}) &&
                  !circleAsEllipse.isInside({0.7071067811865476,
                                             0.7071067811865476}),
              "circular arc conversion preserves clockwise span");
    }

    // ---------------- AcGeMatrix2d (ObjectARX 2026 baseline) ----------------
    {
        const AcGePoint2d center{2.0, -1.0};
        const AcGeMatrix2d rotate =
            AcGeMatrix2d::rotation(1.5707963267948966, center);
        const AcGePoint2d rotated =
            transformBy(AcGePoint2d{3.0, -1.0}, rotate);
        AcGePoint2d memberTransformed{3.0, -1.0};
        memberTransformed.transformBy(rotate);
        check(std::abs(rotated.x - 2.0) < 1.0e-9 &&
                  std::abs(rotated.y) < 1.0e-9 &&
                  memberTransformed.distanceTo(rotated) < 1.0e-9,
              "2D rotation about center");

        const AcGeMatrix2d translate =
            AcGeMatrix2d::translation({4.0, 3.0});
        const AcGePoint2d moved =
            transformBy(transformBy(AcGePoint2d{1.0, 2.0}, translate), rotate);
        const AcGePoint2d composed =
            transformBy(AcGePoint2d{1.0, 2.0}, rotate * translate);
        check(moved.distanceTo(composed) < 1.0e-9,
              "2D affine multiplication order");

        const AcGeMatrix2d inverse = (rotate * translate).inverse();
        const AcGePoint2d restored =
            transformBy(transformBy(AcGePoint2d{1.0, 2.0},
                                    rotate * translate),
                        inverse);
        check(restored.distanceTo(AcGePoint2d{1.0, 2.0}) < 1.0e-9,
              "2D affine inverse");

        const AcGeVector2d unchanged =
            transformBy(AcGeVector2d{1.0, 2.0}, translate);
        check(unchanged == AcGeVector2d{1.0, 2.0},
              "2D vector transform ignores translation");

        AcGeMatrix2d singular;
        singular[1][0] = 0.0;
        singular[1][1] = 0.0;
        check(singular.isSingular(), "2D singular matrix detection");
    }

    // ---------------- AcGe linear entities and scale vectors ----------------
    {
        AcGeTol tol;
        tol.setEqualPoint(1.0e-8);
        check(AcGePoint2d{1.0, 2.0}.isEqualTo(
                  AcGePoint2d{1.0, 2.0 + 1.0e-9}, tol) &&
                  AcGePoint3d::kOrigin.isEqualTo({0.0, 0.0, 0.0}),
              "point tolerance comparison and origin constants");
        AcGePosition2d position2d{1.0, 2.0};
        AcGePosition3d position3d{1.0, 2.0, 3.0};
        position2d.set(AcGePoint2d{4.0, 5.0});
        position3d.set(6.0, 7.0, 8.0);
        const AcGePoint2d pointFromPosition2d = position2d;
        const AcGePoint3d pointFromPosition3d = position3d;
        check(pointFromPosition2d == AcGePoint2d{4.0, 5.0} &&
                  pointFromPosition3d == AcGePoint3d{6.0, 7.0, 8.0},
              "position point-entity interoperability");
        AcGeInterval cylinderHeight{-2.0, 3.0};
        AcGeCylinder cylinder{2.0, AcGePoint3d{1.0, 2.0, 3.0},
                              AcGeVector3d::kZAxis, AcGeVector3d::kXAxis,
                              cylinderHeight, 0.0,
                              6.2831853071795864769};
        AcGeInterval reportedHeight;
        cylinder.getHeight(reportedHeight);
        check(cylinder.pointAt(1.5707963267948966192, 2.0)
                      .isEqualTo({1.0, 4.0, 5.0}) &&
                  cylinder.normalAt(1.5707963267948966192, 2.0)
                      .isEqualTo({0.0, 1.0, 0.0}) &&
                  reportedHeight.lowerBound() == -2.0 &&
                  reportedHeight.upperBound() == 3.0 && cylinder.isClosed(),
              "cylinder parameterization and height interval");
        AcGeTorus torus{3.0, 1.0, AcGePoint3d{0.0, 0.0, 0.0},
                        AcGeVector3d::kZAxis, AcGeVector3d::kXAxis,
                        0.0, 6.2831853071795864769,
                        0.0, 6.2831853071795864769};
        check(torus.pointAt(0.0, 0.0).isEqualTo({4.0, 0.0, 0.0}) &&
                  torus.normalAt(0.0, 0.0).isEqualTo({1.0, 0.0, 0.0}) &&
                  torus.isDoughnut() && torus.isHollow() &&
                  !torus.isDegenerate(),
              "torus parametric surface and topology predicates");
        AcGeInterval coneHeight{0.0, 2.0};
        AcGeCone cone{0.7071067811865475244, 0.7071067811865475244,
                      AcGePoint3d{0.0, 0.0, 0.0}, 2.0,
                      AcGeVector3d::kZAxis, AcGeVector3d::kXAxis,
                      coneHeight, 0.0, 6.2831853071795864769};
        check(cone.apex().isEqualTo({0.0, 0.0, 2.0}) &&
                  cone.pointAt(0.0, 1.0).isEqualTo({1.0, 0.0, 1.0}) &&
                  cone.normalAt(0.0, 1.0).isEqualTo(
                      {0.7071067811865475244, 0.0,
                       0.7071067811865475244}) &&
                  cone.isClosed(),
              "cone apex and parametric surface");
        const AcGePlane planeFromPoints{
            AcGePoint3d{1.0, 0.0, 1.0}, AcGePoint3d{0.0, 0.0, 1.0},
            AcGePoint3d{0.0, 1.0, 1.0}};
        AcGePoint3d lineHit;
        check(planeFromPoints.intersectWith(
                  AcGeLine3d{AcGePoint3d{0.0, 0.0, 0.0},
                             AcGeVector3d::kZAxis},
                  lineHit) &&
                  lineHit.isEqualTo({0.0, 0.0, 1.0}) &&
                  !planeFromPoints.intersectWith(
                      AcGeLine3d{AcGePoint3d{0.0, 0.0, 2.0},
                                 AcGeVector3d::kXAxis},
                      lineHit),
              "plane construction and line intersection");
        const AcGePlane planeFromEquation{0.0, 0.0, 2.0, -2.0};
        AcGeLine3d planeIntersection;
        check(planeFromEquation.origin.isEqualTo({0.0, 0.0, 1.0}) &&
                  planeFromPoints.intersectWith(
                      AcGePlane{AcGePoint3d{2.0, 0.0, 0.0},
                                AcGeVector3d::kXAxis},
                      planeIntersection) &&
                  planeIntersection.pointAt(0.0)
                      .isEqualTo({2.0, 0.0, 1.0}),
              "plane equation and plane intersection");
        check(AcGeVector2d{2.0, 0.0}.isParallelTo({-1.0, 0.0}) &&
                  AcGeVector2d{2.0, 0.0}.isCodirectionalTo({1.0, 0.0}) &&
                  AcGeVector2d{1.0, 0.0}.isPerpendicularTo({0.0, 4.0}) &&
                  AcGeVector2d{0.0, 0.0}.isZeroLength(),
              "2D vector direction predicates");
        check(AcGeVector3d{1.0, 0.0, 0.0}.isParallelTo({-5.0, 0.0, 0.0}) &&
                  AcGeVector3d{1.0, 0.0, 0.0}.isPerpendicularTo(
                      {0.0, 1.0, 0.0}) &&
                  AcGeVector3d{0.0, 1.0, 0.0}.isUnitLength(),
              "3D vector direction predicates");

        const AcGePlane plane{{0.0, 0.0, 2.0}, {0.0, 0.0, 3.0}};
        AcGePoint3d projected;
        check(std::abs(plane.signedDistanceTo({1.0, 2.0, 5.0}) - 3.0) <
                  1.0e-9 &&
                  plane.closestPointTo({1.0, 2.0, 5.0})
                          .distanceTo({1.0, 2.0, 2.0}) < 1.0e-9 &&
                  plane.project({1.0, 2.0, 5.0}, {0.0, 0.0, -2.0},
                                projected) &&
                  projected.distanceTo({1.0, 2.0, 2.0}) < 1.0e-9,
              "plane signed distance and projection");
        check(!plane.project({1.0, 2.0, 5.0}, {1.0, 0.0, 0.0},
                             projected),
              "parallel direction cannot project to plane");

        const AcGeSphere sphere{2.0, {1.0, 2.0, 3.0}};
        check(sphere.isClosed() &&
                  sphere.pointAt(0.0, 0.0)
                          .distanceTo({3.0, 2.0, 3.0}) < 1.0e-9 &&
                  sphere.northPole().distanceTo({1.0, 2.0, 5.0}) < 1.0e-9 &&
                  sphere.normalAt(0.0, 0.0)
                          .isEqualTo({1.0, 0.0, 0.0}),
              "spherical surface frame and parameterization");
        check(!AcGeSphere{2.0, {0.0, 0.0, 0.0},
                          AcGeVector3d::kZAxis, AcGeVector3d::kXAxis,
                          0.0, 1.0, -0.5, 0.5}.isClosed(),
              "spherical patch is not closed");

        const AcGeCircArc3d arc3d{{0.0, 0.0, 0.0},
                                  {0.0, 0.0, 2.0}, 1.0, 0.0,
                                  1.5707963267948966};
        AcGePlane arcPlane;
        arc3d.getPlane(arcPlane);
        check(arc3d.startPoint().isEqualTo({1.0, 0.0, 0.0}) &&
                  arc3d.endPoint().isEqualTo({0.0, 1.0, 0.0}) &&
                  arc3d.isInside({0.7071067811865476,
                                  0.7071067811865476, 0.0}) &&
                  !arc3d.isInside({0.0, 0.0, 1.0}) &&
                  arcPlane.isOn({0.0, 0.0, 0.0}),
              "3D circular arc endpoints, membership, and plane");

        const AcGeEllipArc3d ellipse{{0.0, 0.0, 0.0},
                                    {2.0, 0.0, 0.0},
                                    {0.0, 0.0, 1.0}, 0.5};
        check(ellipse.majorRadius() == 2.0 && ellipse.minorRadius() == 1.0 &&
                  ellipse.minorAxis().isEqualTo({0.0, 1.0, 0.0}) &&
                  ellipse.startPoint().isEqualTo({2.0, 0.0, 0.0}) &&
                  ellipse.endPoint().isEqualTo({2.0, 0.0, 0.0}) &&
                  ellipse.isInside({0.0, 1.0, 0.0}) &&
                  !ellipse.isCircular(),
              "3D ellipse arc axes, radius, and membership");

        const AcGeInterval interval{1.0, 5.0};
        AcGeInterval overlap;
        check(interval.contains(1.0) && interval.contains(5.0) &&
                  !interval.contains(5.1) && interval.length() == 4.0 &&
                  interval.intersectWith(AcGeInterval{4.0, 8.0}, overlap) &&
                  overlap.isEqualAtLower(4.0) &&
                  overlap.isEqualAtUpper(5.0),
              "bounded parameter interval queries");
        check(interval.isDisjoint(AcGeInterval{6.0, 8.0}) &&
                  interval.contains(AcGeInterval{2.0, 3.0}),
              "parameter interval containment and disjointness");
        AcGeInterval merged;
        interval.getMerge(AcGeInterval{7.0, 9.0}, merged);
        check(merged.isEqualAtLower(1.0) && merged.isEqualAtUpper(9.0),
              "parameter interval merge");
        const AcGeInterval lowerBounded{false, 3.0};
        check(lowerBounded.isBoundedAbove() &&
                  !lowerBounded.isBoundedBelow() &&
                  lowerBounded.contains(-100.0) &&
                  lowerBounded.element() == 3.0 &&
                  lowerBounded.length() == -1.0,
              "one-sided unbounded parameter interval");
        AcGeInterval resetInterval{1.0, 2.0};
        resetInterval.set();
        check(resetInterval.isUnBounded() &&
                  resetInterval.length() == -1.0,
              "interval reset to unbounded");

        const AcGeLine2d line{AcGePoint2d{1.0, 2.0},
                              AcGePoint2d{3.0, 2.0}};
        check(line.pointAt(-1.0).distanceTo({-1.0, 2.0}) < 1.0e-9,
        "2D line parameterization");
        const AcGeRay2d ray{AcGePoint2d{1.0, 2.0},
                            AcGeVector2d{2.0, 0.0}};
        check(ray.pointAt(2.0).distanceTo({5.0, 2.0}) < 1.0e-9 &&
            ray.pointAt(-1.0).distanceTo({1.0, 2.0}) < 1.0e-9,
        "2D ray is half-bounded");

        const AcGeLine3d line3d{AcGePoint3d{1.0, 2.0, 3.0},
                                AcGeVector3d{0.0, 0.0, 2.0}};
        check(line3d.pointAt(-1.0).distanceTo({1.0, 2.0, 1.0}) < 1.0e-9,
        "3D line parameterization");
        const AcGeRay3d ray3d{AcGePoint3d{1.0, 2.0, 3.0},
                              AcGeVector3d{0.0, 0.0, 2.0}};
        check(ray3d.pointAt(2.0).distanceTo({1.0, 2.0, 7.0}) < 1.0e-9 &&
            ray3d.pointAt(-1.0).distanceTo({1.0, 2.0, 3.0}) < 1.0e-9,
        "3D ray is half-bounded");

        const AcGeScale2d scale2d{2.0, 3.0};
        check(transformBy(AcGePoint2d{1.0, 2.0}, scale2d.getMatrix())
                .distanceTo({2.0, 6.0}) < 1.0e-9 &&
            (scale2d * scale2d.inverse()).isEqualTo(
                AcGeScale2d::kIdentity),
        "2D scale matrix and inverse");

        const AcGeScale3d scale3d{2.0, 3.0, 4.0};
        check(transformBy(AcGePoint3d{1.0, 2.0, 3.0}, scale3d.getMatrix())
                .distanceTo({2.0, 6.0, 12.0}) < 1.0e-9 &&
            (scale3d * scale3d.inverse()).isEqualTo(
                AcGeScale3d::kIdentity),
        "3D scale matrix and inverse");

        AcGeBoundBlock2d bounds2d{{3.0, 4.0}, {-1.0, 2.0}};
        check(bounds2d.contains({0.0, 3.0}) &&
            !bounds2d.contains({4.0, 3.0}) &&
            bounds2d.isBox(),
        "2D coordinate bound containment");
        bounds2d.extend({5.0, -2.0});
        check(bounds2d.contains({5.0, -2.0}) &&
            bounds2d.isDisjoint(AcGeBoundBlock2d{{6.0, -1.0},
                                                 {8.0, 1.0}}),
        "2D bound extension and disjointness");
        bounds2d.swell(1.0);
        check(bounds2d.contains({-2.0, 1.0}),
        "2D bound swelling");
        const AcGeBoundBlock2d oriented2d{
            AcGePoint2d{0.0, 0.0}, AcGeVector2d{1.0, 1.0},
            AcGeVector2d{-1.0, 1.0}};
        AcGePoint2d base2d;
        AcGeVector2d dir2d1, dir2d2;
        oriented2d.get(base2d, dir2d1, dir2d2);
        const AcGeBoundBlock2d separated2d{
            AcGePoint2d{1.5, 1.5}, AcGeVector2d{1.0, 1.0},
            AcGeVector2d{-1.0, 1.0}};
        check(oriented2d.contains({0.0, 1.0}) &&
                  !oriented2d.contains({0.9, 0.1}) &&
                  !oriented2d.isBox() &&
                  oriented2d.isDisjoint(separated2d) &&
                  !oriented2d.isDisjoint(oriented2d) &&
                  base2d == AcGePoint2d{0.0, 0.0} &&
                  dir2d1 == AcGeVector2d{1.0, 1.0} &&
                  dir2d2 == AcGeVector2d{-1.0, 1.0},
              "2D oriented bound containment and separation");

        AcGeBoundBlock3d bounds3d{{3.0, 4.0, 5.0}, {-1.0, 2.0, 3.0}};
        AcGePoint3d origin;
        AcGeVector3d side1, side2, side3;
        bounds3d.get(origin, side1, side2, side3);
        check(bounds3d.contains({0.0, 3.0, 4.0}) &&
            !bounds3d.contains({4.0, 3.0, 4.0}) &&
            origin == AcGePoint3d{-1.0, 2.0, 3.0} &&
            side1 == AcGeVector3d{4.0, 0.0, 0.0} &&
            bounds3d.isBox(),
        "3D coordinate bound query");
        bounds3d.swell(1.0);
        check(bounds3d.contains({-2.0, 1.0, 2.0}) &&
            bounds3d.isDisjoint(
                AcGeBoundBlock3d{{5.0, 0.0, 0.0}, {6.0, 1.0, 1.0}}),
        "3D bound swelling and disjointness");
        const AcGeBoundBlock3d oriented3d{
            AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{1.0, 1.0, 0.0},
            AcGeVector3d{-1.0, 1.0, 0.0}, AcGeVector3d{0.0, 0.0, 2.0}};
        const AcGeBoundBlock3d separated3d{
            AcGePoint3d{1.5, 1.5, 0.0}, AcGeVector3d{1.0, 1.0, 0.0},
            AcGeVector3d{-1.0, 1.0, 0.0}, AcGeVector3d{0.0, 0.0, 2.0}};
        check(oriented3d.contains({0.0, 1.0, 1.0}) &&
                  !oriented3d.contains({0.9, 0.1, 1.0}) &&
                  !oriented3d.isBox() &&
                  oriented3d.isDisjoint(separated3d) &&
                  !oriented3d.isDisjoint(oriented3d),
              "3D oriented bound containment and separation");
    }

    // ---------------- brep (milestone 2) ----------------
    {
        Arena arena;
        std::fflush(report);
        Body *box = makeBox(arena, {0.0, 0.0, 0.0}, 10.0);
        std::fflush(report);
        std::fflush(report);
        const double volume = signedVolume(box->shell);
        std::fflush(report);
        {
            int fi = 0;
            for (const Face *f = box->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double faceVol = 0.0;
                const CoEdge *pc = first;
                const CoEdge *ce = first->next;
                while (ce != first)
                {
                    const AcGePoint3d pv = pc->forward
                        ? pc->edge->end->point : pc->edge->start->point;
                    const AcGePoint3d cv = ce->forward
                        ? ce->edge->end->point : ce->edge->start->point;
                    const AcGeVector3d pv3(pv.x, pv.y, pv.z);
                    const AcGeVector3d cv3(cv.x, cv.y, cv.z);
                    faceVol += v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(report, "face[%d] vol=%.4f normal=(%.1f,%.1f,%.1f)\n",
                             fi, faceVol, f->surface.normal.x,
                             f->surface.normal.y, f->surface.normal.z);
                ++fi;
            }
        }

        std::printf("volume=%.6f\n", volume);
        check(std::abs(volume - 1000.0) < 1.0e-6,
              "box signed volume is +1000");
        {
            int fi = 0;
            for (const Face *f = box->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double faceVol = 0.0;
                int ringLen = 0;
                const CoEdge *pc = first;
                const CoEdge *ce = first->next;
                while (ce != first)
                {
                    ++ringLen;
                    const AcGePoint3d pv = pc->forward
                        ? pc->edge->end->point : pc->edge->start->point;
                    const AcGePoint3d cv = ce->forward
                        ? ce->edge->end->point : ce->edge->start->point;
                    const AcGeVector3d pv3(pv.x, pv.y, pv.z);
                    const AcGeVector3d cv3(cv.x, cv.y, cv.z);
                    const double triDet =
                        v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    faceVol += triDet;
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(report, "face[%d] ring=%d closed=%s vol=%.4f normal=(%.1f,%.1f,%.1f)\n",
                             fi, ringLen, ce == first ? "yes" : "NO", faceVol,
                             f->surface.normal.x, f->surface.normal.y,
                             f->surface.normal.z);
                {
                    const brep::CoEdge *ce2 = first;
                    int k = 0;
                    do {
                        std::fprintf(report, "   [%d] ce=%p edge=%p next=%p fwd=%d start=(%.1f,%.1f,%.1f)\n",
                                     k, (const void*)ce2, (const void*)ce2->edge,
                                     (const void*)ce2->next, int(ce2->forward),
                                     ce2->edge->start->point.x,
                                     ce2->edge->start->point.y,
                                     ce2->edge->start->point.z);
                        ce2 = ce2->next;
                        ++k;
                    } while (ce2 != first && k < 8);
                }
                ++fi;
            }
        }
        std::fflush(report);
        check(validate(box).empty(), "clean box validates with no issues");
        std::fflush(report);

        // Inside-out detection: an inside-out shell has every
        // coedge traversing its edge backwards (all face rings run
        // clockwise around the outward normals).  Radial pairing
        // stays consistent (both sides flip), but the signed volume
        // flips negative.
        Arena inwardArena;
        Body *inward = makeBox(inwardArena, {0.0, 0.0, 0.0}, 10.0);
        for (Face *face = inward->shell->firstFace; face;
             face = face->next)
        {
                Loop *loop = face->outerLoop;
                std::vector<CoEdge *> ring;
                CoEdge *c = loop->first;
                do {
                    ring.push_back(c);
                    c = c->next;
                } while (c != loop->first);
                const std::size_t n = ring.size();
                for (std::size_t i = 0; i < n; ++i)
                {
                    ring[i]->next =
                        ring[(i + n - 1) % n];
                    ring[i]->prev = ring[(i + 1) % n];
                    ring[i]->forward = !ring[i]->forward;
                }
                loop->first = ring[0];
            }
        std::fprintf(report, "flipped-first-fwd=%d\n",
                     int(inward->shell->firstFace->outerLoop->first->forward));
        std::fprintf(report, "inward volume=%.4f\n",
                     signedVolume(inward->shell));
        {
            int fi = 0;
            for (const Face *f = inward->shell->firstFace; f;
                 f = f->next, ++fi)
            {
                const CoEdge *first = f->outerLoop->first;
                const AcGePoint3d v0 = first->forward
                    ? first->edge->start->point
                    : first->edge->end->point;
                const AcGeVector3d v0v(v0.x, v0.y, v0.z);
                double fv = 0.0;
                const CoEdge *pc = first;
                const CoEdge *ce = first->next;
                while (ce != first)
                {
                    const AcGePoint3d pv = pc->forward
                        ? pc->edge->end->point : pc->edge->start->point;
                    const AcGePoint3d cv = ce->forward
                        ? ce->edge->end->point : ce->edge->start->point;
                    const AcGeVector3d pv3(pv.x, pv.y, pv.z);
                    const AcGeVector3d cv3(cv.x, cv.y, cv.z);
                    fv += v0v.crossProduct(pv3).dotProduct(cv3) / 6.0;
                    pc = ce;
                    ce = ce->next;
                }
                std::fprintf(stderr, "inward face[%d] vol=%.4f\n", fi, fv);
                ++fi;
            }
        }
        std::fprintf(report, "POST fwd=%d\n",
                     int(inward->shell->firstFace->outerLoop->first->forward));
        // ---- milestone 3-a: extrudePolygon ----
        {
            Arena extArena;
            AcGePoint3d profile[4] = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
                                     {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}};
            AcGeVector3d normal;
            Body *extruded = extrudePolygon(extArena, profile, 4,
                                            {0.0, 0.0, 10.0}, normal);
            check(std::abs(normal.z - 1.0) < 1.0e-9,
                  "extrude profile normal by Newell is +z");
            const double extVol = signedVolume(extruded->shell);
            std::fprintf(report, "extrude volume=%.4f\n", extVol);
            check(std::abs(extVol - 1000.0) < 1.0e-6,
                  "extruded box volume is +1000");
            const auto extIssues = validate(extruded);
            check(extIssues.empty(), "extruded box validates clean");

            std::size_t vtx = 0, edges = 0, faces = 0;
            std::map<const void *, int> seen;
            for (const Face *f = extruded->shell->firstFace; f;
                 f = f->next)
            {
                ++faces;
                const CoEdge *ce2 = f->outerLoop->first;
                do {
                    ++edges;
                    seen[ce2->edge->start] += 1;
                    ce2 = ce2->next;
                } while (ce2 != f->outerLoop->first);
            }
            vtx = seen.size();
            check(faces == 6, "extruded box has 6 faces");
            check(edges == 24, "extruded box has 24 loop coedges (12 edges x 2 sides)");
            check(vtx == 8, "extruded box has 8 deduplicated vertices");
            std::fprintf(report, "extrude faces=%zu coedge-runs=%zu vertices=%zu\n",
                         faces, edges, vtx);
        }
        // ---- milestone 3-b: box-box boolean ----
        {
            Arena boolArena;
            Body *boxA = makeBox(boolArena, {0.0, 0.0, 0.0}, 10.0);
            Body *boxB = makeBox(boolArena, {5.0, 2.0, 3.0}, 10.0);

            std::fprintf(report, "[BOOL] union\n");
            Body *uni = performBoolean(boxA, boxB, BoolOp::Union);
            const double uniVol = signedVolume(uni->shell);
            std::fprintf(report, "[BOOL] union volume=%.4f\n", uniVol);
            check(std::abs(uniVol - 1720.0) < 1.0e-6,
                  "union volume is 1720");
            {
                const auto uniIssues = validate(uni);
                check(uniIssues.empty(), "union validates clean");
                for (const ValidateIssue &issue : uniIssues)
                    std::fprintf(report, "  union issue: %s\n",
                                 validateErrorName(issue.error));
            }

            std::fprintf(report, "[BOOL] intersect\n");
            Body *inter = performBoolean(boxA, boxB,
                                         BoolOp::Intersect);
            const double interVol = signedVolume(inter->shell);
            std::fprintf(report, "[BOOL] intersect volume=%.4f\n", interVol);
            check(std::abs(interVol - 280.0) < 1.0e-6,
                  "intersect volume is 280");
            check(validate(inter).empty(), "intersect validates clean");

            std::fprintf(report, "[BOOL] subtract\n");
            Body *sub = performBoolean(boxA, boxB,
                                       BoolOp::Subtract);
            const double subVol = signedVolume(sub->shell);
            std::fprintf(report, "[BOOL] subtract volume=%.4f\n", subVol);
            check(std::abs(subVol - 720.0) < 1.0e-6,
                  "subtract volume is 720");
            check(validate(sub).empty(), "subtract validates clean");
        }
        bool inwardDetected = false;
        for (const ValidateIssue &issue : validate(inward))
            if (issue.error == ValidateError::kShellInward)
                inwardDetected = true;
        check(inwardDetected, "inside-out shell detected");
        check(signedVolume(inward->shell) < 0.0,
              "inside-out volume negative");

        // Broken radial pairing detected.
        Arena brokenArena;
        Body *broken = makeBox(brokenArena, {0.0, 0.0, 0.0}, 10.0);
        {
            Face *face = broken->shell->firstFace;
            Edge *edge = face->outerLoop->first->edge;
            edge->coedge[0]->partner = edge->coedge[0];
            edge->coedge[1]->partner = edge->coedge[1];
        }
        bool partnerDetected = false;
        for (const ValidateIssue &issue : validate(broken))
            if (issue.error == ValidateError::kCoedgePartnerBroken)
                partnerDetected = true;
        check(partnerDetected, "broken radial pairing detected");

        // ---- milestone 6: sweep / loft / history / STEP ----
        auto findEdgeByPoints = [](Body *b, const AcGePoint3d &pa,
                                   const AcGePoint3d &pb) -> Edge * {
            for (Face *f = b->shell->firstFace; f != nullptr;
                 f = f->next)
            {
                CoEdge *ce = f->outerLoop->first;
                do {
                    if ((ce->edge->start->point.distanceTo(pa) <
                             1.0e-9 &&
                         ce->edge->end->point.distanceTo(pb) <
                             1.0e-9) ||
                        (ce->edge->start->point.distanceTo(pb) <
                             1.0e-9 &&
                         ce->edge->end->point.distanceTo(pa) <
                             1.0e-9))
                        return ce->edge;
                    ce = ce->next;
                } while (ce != f->outerLoop->first);
            }
            return nullptr;
        };
        auto countFaces = [](const Body *b) {
            int count = 0;
            for (const Face *f = b->shell->firstFace; f != nullptr;
                 f = f->next)
                ++count;
            return count;
        };

        {
            Arena swArena;
            // Meridian rectangle rho in [1,2], z in [0,1], CCW around
            // theta-hat = z x x-hat = +y.
            const AcGePoint3d profile[4] = {
                {1.0, 0.0, 0.0}, {1.0, 0.0, 1.0},
                {2.0, 0.0, 1.0}, {2.0, 0.0, 0.0}};
            const double pi = 3.14159265358979323846;
            Body *ring = sweepAroundAxis(swArena, profile, 4,
                                         {0.0, 0.0, 0.0},
                                         {0.0, 0.0, 1.0}, pi / 2.0);
            check(ring != nullptr, "sweep around axis succeeds");
            if (ring != nullptr)
            {
                // Pappus: 1 * 1.5 * (pi/2) = 3*pi/4.
                const double swVol = signedVolume(ring->shell);
                std::fprintf(report, "sweep volume=%.9f faces=%d\n",
                             swVol, countFaces(ring));
                check(std::abs(swVol - 0.75 * pi) < 1.0e-6,
                      "sweep volume satisfies Pappus (3*pi/4)");
                check(validate(ring).empty(), "sweep validates clean");
                check(countFaces(ring) == 6,
                      "sweep has 2 caps + 2 cylinders + 2 sectors");
            }
            Body *full = sweepAroundAxis(swArena, profile, 4,
                                         {0.0, 0.0, 0.0},
                                         {0.0, 0.0, 1.0}, pi / 2.0);
            (void)full;
        }

        {
            Arena loArena;
            const AcGePoint3d profileA[4] = {
                {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
                {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}};
            const AcGePoint3d profileB[4] = {
                {2.5, 2.5, 10.0}, {7.5, 2.5, 10.0},
                {7.5, 7.5, 10.0}, {2.5, 7.5, 10.0}};
            AcGeVector3d loftNormal;
            Body *frustum = loftProfiles(loArena, profileA, profileB, 4,
                                         loftNormal);
            check(frustum != nullptr, "loft succeeds");
            if (frustum != nullptr)
            {
                // Truncated pyramid: h/3*(A1 + A2 + sqrt(A1*A2)) =
                // 10/3*(100 + 25 + 50) = 1750/3.
                const double loVol = signedVolume(frustum->shell);
                std::fprintf(report, "loft volume=%.9f faces=%d\n",
                             loVol, countFaces(frustum));
                check(std::abs(loVol - 1750.0 / 3.0) < 1.0e-6,
                      "loft volume is the prismatoid formula value");
                check(validate(frustum).empty(),
                      "loft validates clean");
                check(countFaces(frustum) == 6, "loft has 6 faces");
            }
        }

        {
            Arena hiArena;
            std::vector<FeatureRecord> journal;
            FeatureRecord boxRecord;
            boxRecord.kind = FeatureKind::MakeBox;
            boxRecord.point = {0.0, 0.0, 0.0};
            boxRecord.sizeA = 10.0;
            journal.push_back(boxRecord);

            FeatureRecord chamferRecord;
            chamferRecord.kind = FeatureKind::ChamferEdge;
            chamferRecord.lhs = 0;
            chamferRecord.point = {10.0, 0.0, 10.0};
            chamferRecord.point2 = {10.0, 0.0, 0.0};
            chamferRecord.sizeA = 1.0;
            journal.push_back(chamferRecord);

            Body *replayed = replayFeatures(hiArena, journal);
            check(replayed != nullptr, "history replay succeeds");
            if (replayed != nullptr)
            {
                const double hiVol = signedVolume(replayed->shell);
                std::fprintf(report, "history replay volume=%.6f\n",
                             hiVol);
                check(std::abs(hiVol - 995.0) < 1.0e-6,
                      "replayed journal reproduces chamfer (995)");
                check(validate(replayed).empty(),
                      "replayed body validates clean");
            }

            // Undo = truncate + replay.
            journal.pop_back();
            Body *undone = replayFeatures(hiArena, journal);
            check(undone != nullptr && std::abs(signedVolume(
                                           undone->shell) -
                                       1000.0) < 1.0e-6,
                  "journal undo returns the bare box");

            FeatureRecord secondBox;
            secondBox.kind = FeatureKind::MakeBox;
            secondBox.point = {5.0, 2.0, 3.0};
            secondBox.sizeA = 10.0;
            journal.push_back(secondBox);
            FeatureRecord cut;
            cut.kind = FeatureKind::BooleanOp;
            cut.lhs = 0;
            cut.rhs = 1;
            cut.boolOp = BoolOp::Subtract;
            journal.push_back(cut);
            Body *cutBody = replayFeatures(hiArena, journal);
            check(cutBody != nullptr &&
                      std::abs(signedVolume(cutBody->shell) - 720.0) <
                          1.0e-6,
                  "journal boolean subtract replays to 720");
        }

        {
            // STEP export: structure counts must mirror the topology.
            Arena stArena;
            Body *box = makeBox(stArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                box, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            Body *ch = chamferEdge(box, target, 1.0);
            Body *fr = filletEdge(box, target, 2.0);
            check(ch != nullptr && fr != nullptr,
                  "step export bodies built");
            auto countTopology = [](const Body *b, int &vertices,
                                    int &edges, int &faces) {
                vertices = edges = faces = 0;
                std::map<const void *, bool> vseen, eseen;
                for (const Face *f = b->shell->firstFace; f != nullptr;
                     f = f->next)
                {
                    ++faces;
                    const CoEdge *ce = f->outerLoop->first;
                    do {
                        vseen[ce->edge->start] = true;
                        vseen[ce->edge->end] = true;
                        eseen[ce->edge] = true;
                        ce = ce->next;
                    } while (ce != f->outerLoop->first);
                }
                vertices = int(vseen.size());
                edges = int(eseen.size());
            };
            auto countInFile = [](const char *path, const char *needle) {
                FILE *f = std::fopen(path, "rb");
                if (f == nullptr)
                    return -1;
                std::string contents;
                char buffer[4096];
                std::size_t got;
                while ((got = std::fread(buffer, 1, sizeof(buffer), f)) >
                       0)
                    contents.append(buffer, got);
                std::fclose(f);
                int count = 0;
                std::size_t pos = 0;
                while ((pos = contents.find(needle, pos)) !=
                       std::string::npos)
                {
                    ++count;
                    pos += 1;
                }
                return count;
            };

            int vtx = 0, eds = 0, fcs = 0;
            countTopology(ch, vtx, eds, fcs);
            const int chamferEntities =
                writeStepBody(ch, "step_chamfer.step");
            check(chamferEntities > 0, "chamfer STEP export succeeds");
            check(countInFile("step_chamfer.step", "ADVANCED_FACE(") ==
                      fcs,
                  "chamfer STEP has one ADVANCED_FACE per face");
            check(countInFile("step_chamfer.step", "EDGE_CURVE(") == eds,
                  "chamfer STEP has one EDGE_CURVE per edge");
            check(countInFile("step_chamfer.step", "VERTEX_POINT(") ==
                      vtx,
                  "chamfer STEP has one VERTEX_POINT per vertex");
            check(countInFile("step_chamfer.step",
                              "MANIFOLD_SOLID_BREP(") == 1,
                  "chamfer STEP has one solid");
            std::fprintf(report,
                         "step chamfer entities=%d faces=%d edges=%d "
                         "vertices=%d\n",
                         chamferEntities, fcs, eds, vtx);

            countTopology(fr, vtx, eds, fcs);
            const int filletEntities =
                writeStepBody(fr, "step_fillet.step");
            check(filletEntities > 0, "fillet STEP export succeeds");
            check(countInFile("step_fillet.step", "ADVANCED_FACE(") ==
                      fcs,
                  "fillet STEP has one ADVANCED_FACE per face");
            check(countInFile("step_fillet.step", "EDGE_CURVE(") == eds,
                  "fillet STEP has one EDGE_CURVE per edge");
            check(countInFile("step_fillet.step", "CIRCLE(") == 2,
                  "fillet STEP carries both arc edges as CIRCLEs");
            check(countInFile("step_fillet.step",
                              "CYLINDRICAL_SURFACE(") == 1,
                  "fillet STEP carries the cylinder surface");
        }

        // ---- milestone 4: chamfer / thicken / fillet / raycast ----
        {
            Arena chArena;
            Body *box = makeBox(chArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                box, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            check(target != nullptr, "chamfer target edge found");
            Body *ch = chamferEdge(box, target, 1.0);
            check(ch != nullptr, "chamfer succeeds");
            if (ch != nullptr)
            {
                const double chVol = signedVolume(ch->shell);
                std::fprintf(report, "chamfer volume=%.6f faces=%d\n",
                             chVol, countFaces(ch));
                check(std::abs(chVol - 995.0) < 1.0e-6,
                      "chamfer volume is 995");
                check(validate(ch).empty(), "chamfer validates clean");
                check(countFaces(ch) == 7,
                      "chamfered box has 7 faces");
            }
        }

        {
            Arena thArena;
            Builder builder(thArena);
            std::map<PosKey, Vertex *> vertices;
            std::map<std::pair<PosKey, PosKey>,
                     std::pair<Edge *, bool>> edgeMap;
            Body *sheet = builder.createBody();
            const AcGePoint3d square[4] = {
                {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
                {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}};
            makePlanarFaceFromCorners(builder, sheet->shell, vertices,
                                      edgeMap, square, 4,
                                      {0.0, 0.0, 1.0});
            AcGeVector3d outNormal;
            Body *slab = thickenSheet(thArena, sheet, 2.0, outNormal);
            check(slab != nullptr, "thicken succeeds");
            if (slab != nullptr)
            {
                const double slabVol = signedVolume(slab->shell);
                std::fprintf(report, "thicken volume=%.6f\n", slabVol);
                check(std::abs(slabVol - 200.0) < 1.0e-6,
                      "thickened slab volume is 200");
                check(validate(slab).empty(),
                      "thickened slab validates clean");
                check(std::abs(outNormal.z - 1.0) < 1.0e-9,
                      "thicken normal is +z");
            }
        }

        {
            Arena fiArena;
            Body *box = makeBox(fiArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                box, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            check(target != nullptr, "fillet target edge found");
            Body *fr = filletEdge(box, target, 2.0);
            check(fr != nullptr, "fillet succeeds");
            if (fr != nullptr)
            {
                const double pi = 3.14159265358979323846;
                const double expected =
                    1000.0 - (1.0 - pi / 4.0) * 2.0 * 2.0 * 10.0;
                const double frVol = signedVolume(fr->shell);
                int cylinderFaces = 0;
                int arcEdges = 0;
                std::map<const void *, bool> arcSeen;
                for (const Face *f = fr->shell->firstFace; f != nullptr;
                     f = f->next)
                {
                    if (f->cylindrical)
                        ++cylinderFaces;
                    const CoEdge *ce = f->outerLoop->first;
                    do {
                        if (ce->edge->isArc)
                            arcSeen[ce->edge] = true;
                        ce = ce->next;
                    } while (ce != f->outerLoop->first);
                }
                arcEdges = int(arcSeen.size());
                std::fprintf(report,
                             "fillet volume=%.9f faces=%d cylinders=%d"
                             " arcs=%d\n",
                             frVol, countFaces(fr), cylinderFaces,
                             arcEdges);
                check(std::abs(frVol - expected) < 1.0e-6,
                      "fillet volume is 991.4159");
                check(validate(fr).empty(), "fillet validates clean");
                check(countFaces(fr) == 7, "filleted box has 7 faces");
                check(cylinderFaces == 1,
                      "fillet adds one cylinder face");
                check(arcEdges == 2,
                      "fillet has two shared arc edges");
            }
        }

        {
            Arena rcArena;
            Body *box = makeBox(rcArena, {0.0, 0.0, 0.0}, 10.0);
            auto hit = rayCast(box, {20.0, 5.0, 5.0}, {-1.0, 0.0, 0.0});
            check(hit.has_value(), "raycast hits the box");
            if (hit.has_value())
            {
                check(std::abs(hit->point.x - 10.0) < 1.0e-9 &&
                          std::abs(hit->point.y - 5.0) < 1.0e-9,
                      "raycast hit point on px face");
                check(hit->face != nullptr &&
                          hit->face->surface.normal.x > 0.9,
                      "raycast hit face is px");
                check(std::abs(hit->distance - 10.0) < 1.0e-9,
                      "raycast hit distance is 10");
            }
            auto miss = rayCast(box, {20.0, 5.0, 5.0}, {1.0, 0.0, 0.0});
            check(!miss.has_value(), "raycast away from the box misses");

            // Curved patch picking through the filleted body.
            Arena frArena;
            Body *fbox = makeBox(frArena, {0.0, 0.0, 0.0}, 10.0);
            Edge *target = findEdgeByPoints(
                fbox, {10.0, 0.0, 10.0}, {10.0, 0.0, 0.0});
            Body *fr = filletEdge(fbox, target, 2.0);
            check(fr != nullptr, "fillet for raycast succeeds");
            if (fr != nullptr)
            {
                const double quarter = 0.5857864376269049;
                auto chit = rayCast(fr, {20.0, quarter, 5.0},
                                    {-1.0, 0.0, 0.0});
                check(chit.has_value() && chit->face != nullptr &&
                          chit->face->cylindrical,
                      "raycast hits the fillet cylinder");
                if (chit.has_value())
                {
                    // Tessellated chord error ~0.015 at 16 slices.
                    std::fprintf(report,
                                 "raycast cylinder hit=(%.4f,%.4f,%.4f)"
                                 " n=(%.3f,%.3f,%.3f)\n",
                                 chit->point.x, chit->point.y,
                                 chit->point.z, chit->normal.x,
                                 chit->normal.y, chit->normal.z);
                    check(std::abs(chit->point.x - 9.41421356237) < 0.03,
                          "cylinder hit x matches the arc");
                    check(chit->normal.x > 0.65 &&
                              chit->normal.x < 0.76 &&
                              chit->normal.y < -0.65 &&
                              chit->normal.y > -0.76,
                          "cylinder hit normal is radial");
                }
            }
        }
    }

    std::fprintf(report, fails == 0 ? "KERNEL SELFTEST PASSED\n"
                                : "KERNEL SELFTEST FAILED\n");
    std::fflush(report);
    std::fclose(report);
    std::printf(fails == 0 ? "KERNEL SELFTEST PASSED\n"
                           : "KERNEL SELFTEST FAILED\n");
    return fails;
}

} // namespace brep
