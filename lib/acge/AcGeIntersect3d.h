#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "AcGeCurve3d.h"
#include "AcGeMatrix.h"
#include "AcGeLinear.h"
#include "AcGeSurface3d.h"
#include "AcGeTriangle.h"

namespace ge
{
namespace detail
{

inline bool parameterInRange(double parameter, double minimum, double maximum,
                             double tolerance)
{
    return parameter >= minimum - tolerance &&
           parameter <= maximum + tolerance;
}

std::vector<AcGePoint3d> intersectParametricPlane(
    const AcGePoint3d &base, const AcGeVector3d &direction,
    double minParameter, double maxParameter, const AcGePlane &plane,
    const AcGeTol &tol);

std::vector<AcGePoint3d> intersectParametricSphere(
    const AcGePoint3d &base, const AcGeVector3d &direction,
    double minParameter, double maxParameter, const AcGeSphere &sphere,
    const AcGeTol &tol);

} // namespace detail

std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                   const AcGePlane &plane,
                                   const AcGeTol &tol = AcGeTol{});
std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                   const AcGePlane &plane,
                                   const AcGeTol &tol = AcGeTol{});
std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                   const AcGePlane &plane,
                                   const AcGeTol &tol = AcGeTol{});
std::optional<AcGeLine3d> intersect(const AcGePlane &first,
                                    const AcGePlane &second,
                                    const AcGeTol &tol = AcGeTol{});
std::optional<AcGePoint3d> intersect(const AcGeLine3d &first,
                                     const AcGeLine3d &second,
                                     const AcGeTol &tol = AcGeTol{});
std::optional<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                     const AcGeTriangle3d &triangle,
                                     const AcGeTol &tol = AcGeTol{});

struct ACGE_PORT AcGeRayTriangleHit
{
    double parameter = 0.0;
    double barycentricU = 0.0;
    double barycentricV = 0.0;
    AcGePoint3d point{};
};

std::optional<AcGeRayTriangleHit> intersectRayTriangle(
    const AcGeRay3d &ray, const AcGeTriangle3d &triangle,
    const AcGeTol &tol = AcGeTol{});

std::vector<AcGePoint3d> intersect(const AcGeLine3d &line,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol = AcGeTol{});
std::vector<AcGePoint3d> intersect(const AcGeRay3d &ray,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol = AcGeTol{});
std::vector<AcGePoint3d> intersect(const AcGeLineSeg3d &segment,
                                   const AcGeSphere &sphere,
                                   const AcGeTol &tol = AcGeTol{});

struct ACGE_PORT AcGeClosestPoints3d
{
    double parameterA = 0.0;
    double parameterB = 0.0;
    AcGePoint3d pointA{};
    AcGePoint3d pointB{};
    double distance = 0.0;
};

using AcGeSegmentClosestPoints3d = AcGeClosestPoints3d;

AcGeClosestPoints3d closestPoints(const AcGeLineSeg3d &first,
                                  const AcGeLineSeg3d &second,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeLineSeg3d &segment,
                                  const AcGeLine3d &line,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeLine3d &line,
                                  const AcGeLineSeg3d &segment,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeLineSeg3d &segment,
                                  const AcGeRay3d &ray,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeRay3d &ray,
                                  const AcGeLineSeg3d &segment,
                                  const AcGeTol &tol = AcGeTol{});

// Parameters are measured using each entity's supplied direction vector;
// line parameters are unbounded and ray parameters are nonnegative.
AcGeClosestPoints3d closestPoints(const AcGeLine3d &first,
                                  const AcGeLine3d &second,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeRay3d &first,
                                  const AcGeRay3d &second,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeLine3d &line,
                                  const AcGeRay3d &ray,
                                  const AcGeTol &tol = AcGeTol{});
AcGeClosestPoints3d closestPoints(const AcGeRay3d &ray,
                                  const AcGeLine3d &line,
                                  const AcGeTol &tol = AcGeTol{});

} // namespace ge
