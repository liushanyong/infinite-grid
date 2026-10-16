#include <cassert>
#include <cmath>

#include "acge/AcGe.h"

namespace
{

void assertClosest(const ge::AcGeClosestPoints3d &closest,
                   double parameterA, double parameterB,
                   const AcGePoint3d &pointA, const AcGePoint3d &pointB,
                   double distance)
{
    assert(std::abs(closest.parameterA - parameterA) < 1.0e-12);
    assert(std::abs(closest.parameterB - parameterB) < 1.0e-12);
    assert(closest.pointA.isEqualTo(pointA));
    assert(closest.pointB.isEqualTo(pointB));
    assert(std::abs(closest.distance - distance) < 1.0e-12);
}

void testLineLine()
{
    const auto skew = ge::closestPoints(
        AcGeLine3d{AcGePoint3d{0.0, 0.0, 0.0},
                   AcGeVector3d{1.0, 0.0, 0.0}},
        AcGeLine3d{AcGePoint3d{0.0, 1.0, 1.0},
                   AcGeVector3d{0.0, 1.0, 0.0}});
    assertClosest(skew, 0.0, -1.0, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 1.0);

    const auto crossing = ge::closestPoints(
        AcGeLine3d{AcGePoint3d{0.0, 0.0, 0.0},
                   AcGeVector3d{1.0, 0.0, 0.0}},
        AcGeLine3d{AcGePoint3d{1.0, -1.0, 0.0},
                   AcGeVector3d{0.0, 1.0, 0.0}});
    assertClosest(crossing, 1.0, 1.0, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                  0.0);

    const auto parallel = ge::closestPoints(
        AcGeLine3d{AcGePoint3d{0.0, 0.0, 0.0},
                   AcGeVector3d{2.0, 0.0, 0.0}},
        AcGeLine3d{AcGePoint3d{1.0, 2.0, 3.0},
                   AcGeVector3d{4.0, 0.0, 0.0}});
    assert(std::abs(parallel.distance - std::sqrt(13.0)) < 1.0e-12);
    assert(std::abs(parallel.pointA.x - parallel.pointB.x) < 1.0e-12);
    assert(std::abs(parallel.pointA.y) < 1.0e-12);
    assert(std::abs(parallel.pointB.y - 2.0) < 1.0e-12);
    assert(std::abs(parallel.pointA.z) < 1.0e-12);
    assert(std::abs(parallel.pointB.z - 3.0) < 1.0e-12);
}

void testRayDomainsAndMixedPairs()
{
    const AcGeRay3d first{AcGePoint3d{0.0, 0.0, 0.0},
                          AcGeVector3d{1.0, 0.0, 0.0}};
    const AcGeRay3d opposing{AcGePoint3d{1.0, 1.0, 0.0},
                             AcGeVector3d{-1.0, 0.0, 0.0}};
    const auto rayRay = ge::closestPoints(first, opposing);
    assert(rayRay.parameterA >= 0.0 && rayRay.parameterB >= 0.0);
    assert(std::abs(rayRay.distance - 1.0) < 1.0e-12);

    const auto rayRayParallel = ge::closestPoints(
        first, AcGeRay3d{AcGePoint3d{0.0, 2.0, 0.0},
                         AcGeVector3d{1.0, 0.0, 0.0}});
    assert(rayRayParallel.parameterA >= 0.0);
    assert(rayRayParallel.parameterB >= 0.0);
    assert(std::abs(rayRayParallel.distance - 2.0) < 1.0e-12);

    const AcGeLine3d vertical{AcGePoint3d{0.0, 0.0, -1.0},
                              AcGeVector3d{0.0, 0.0, 1.0}};
    const auto lineRay = ge::closestPoints(vertical, first);
    const auto rayLine = ge::closestPoints(first, vertical);
    assert(lineRay.distance < 1.0e-12);
    assert(rayLine.distance < 1.0e-12);
    assert(lineRay.pointA.isEqualTo(lineRay.pointB));
    assert(rayLine.pointA.isEqualTo(rayLine.pointB));
}

void testDegenerateDirections()
{
    const auto pointAndLine = ge::closestPoints(
        AcGeRay3d{AcGePoint3d{1.0, 2.0, 3.0},
                  AcGeVector3d{0.0, 0.0, 0.0}},
        AcGeLine3d{AcGePoint3d{0.0, 0.0, 0.0},
                   AcGeVector3d{1.0, 0.0, 0.0}});
    assert(pointAndLine.parameterA == 0.0);
    assert(pointAndLine.pointA.isEqualTo({1.0, 2.0, 3.0}));
    assert(pointAndLine.pointB.isEqualTo({1.0, 0.0, 0.0}));
    assert(std::abs(pointAndLine.distance - std::sqrt(13.0)) < 1.0e-12);

    const auto twoPoints = ge::closestPoints(
        AcGeRay3d{AcGePoint3d{1.0, 0.0, 0.0},
                  AcGeVector3d{0.0, 0.0, 0.0}},
        AcGeRay3d{AcGePoint3d{0.0, 2.0, 0.0},
                  AcGeVector3d{0.0, 0.0, 0.0}});
    assert(twoPoints.parameterA == 0.0 && twoPoints.parameterB == 0.0);
    assert(std::abs(twoPoints.distance - std::sqrt(5.0)) < 1.0e-12);
}

void testSegmentCompatibilityAlias()
{
    const ge::AcGeSegmentClosestPoints3d closest = ge::closestPoints(
        AcGeLineSeg3d{AcGePoint3d{0.0, 0.0, 0.0},
                      AcGePoint3d{1.0, 0.0, 0.0}},
        AcGeLineSeg3d{AcGePoint3d{2.0, 1.0, 0.0},
                      AcGePoint3d{2.0, 2.0, 0.0}});
    assert(closest.parameterA == 1.0);
    assert(closest.parameterB == 0.0);
    assert(closest.pointA.isEqualTo({1.0, 0.0, 0.0}));
    assert(closest.pointB.isEqualTo({2.0, 1.0, 0.0}));
    assert(std::abs(closest.distance - std::sqrt(2.0)) < 1.0e-12);
}

void testSegmentLineAndRayPairs()
{
    const AcGeLineSeg3d segment{AcGePoint3d{0.0, 0.0, 0.0},
                                AcGePoint3d{1.0, 0.0, 0.0}};
    const AcGeLine3d line{AcGePoint3d{2.0, 1.0, 0.0},
                          AcGeVector3d{0.0, 1.0, 0.0}};
    const auto segmentLine = ge::closestPoints(segment, line);
    assert(std::abs(segmentLine.parameterA - 1.0) < 1.0e-12);
    assert(std::abs(segmentLine.parameterB + 1.0) < 1.0e-12);
    assert(std::abs(segmentLine.distance - 1.0) < 1.0e-12);

    const auto lineSegment = ge::closestPoints(line, segment);
    assert(std::abs(lineSegment.distance - segmentLine.distance) < 1.0e-12);
    assert(lineSegment.pointA.isEqualTo(segmentLine.pointB));
    assert(lineSegment.pointB.isEqualTo(segmentLine.pointA));

    const AcGeRay3d ray{AcGePoint3d{2.0, 1.0, 0.0},
                        AcGeVector3d{0.0, -2.0, 0.0}};
    const auto segmentRay = ge::closestPoints(segment, ray);
    assert(std::abs(segmentRay.parameterA - 1.0) < 1.0e-12);
    assert(std::abs(segmentRay.parameterB - 0.5) < 1.0e-12);
    assert(std::abs(segmentRay.distance - 1.0) < 1.0e-12);

    const auto raySegment = ge::closestPoints(ray, segment);
    assert(std::abs(raySegment.distance - segmentRay.distance) < 1.0e-12);
    assert(raySegment.pointA.isEqualTo(segmentRay.pointB));
    assert(raySegment.pointB.isEqualTo(segmentRay.pointA));
}

} // namespace

int main()
{
    testLineLine();
    testRayDomainsAndMixedPairs();
    testDegenerateDirections();
    testSegmentCompatibilityAlias();
    testSegmentLineAndRayPairs();
}
