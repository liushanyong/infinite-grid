#pragma once

#include "AcGsPort.h"

#include <limits>

#include "acge/AcGePoint.h"

namespace acgs
{

struct ACGS_PORT AcGsPickRay
{
    AcGePoint3d origin;
    AcGeVector3d direction;
};

struct ACGS_PORT AcGsPickView
{
    AcGePoint3d position;
    AcGeVector3d front{0.0, 0.0, -1.0};
    AcGeVector3d right{1.0, 0.0, 0.0};
    AcGeVector3d up{0.0, 1.0, 0.0};
    int viewportWidth = 1;
    int viewportHeight = 1;
    bool orthographic = false;
    double orthoHalfHeight = 1.0;
    double verticalFovRadians = 0.7853981633974483;
    double nearDepth = 0.0;
    double farDepth = std::numeric_limits<double>::infinity();
};

ACGS_PORT AcGsPickRay makePickRay(double ndcX, double ndcY,
                                  const AcGsPickView &view);
ACGS_PORT double pickMinDepth(const AcGsPickView &view);
ACGS_PORT double pickWorldPerPixel(const AcGsPickRay &ray,
                                   const AcGePoint3d &worldPoint,
                                   const AcGsPickView &view);
ACGS_PORT double pickCursorTolerance(const AcGsPickRay &ray,
                                     const AcGePoint3d &worldPoint,
                                     const AcGsPickView &view,
                                     double pixelRadius);
ACGS_PORT bool rayIntersectsAabb(const AcGsPickRay &ray,
                                 const AcGePoint3d &minimum,
                                 const AcGePoint3d &maximum,
                                 double nearDepth, double farDepth,
                                 double &hitDepth);
ACGS_PORT bool rayIntersectsSegment(
    const AcGsPickRay &ray, const AcGePoint3d &start,
    const AcGePoint3d &end, double tolerance, double depthNear,
    double depthFar, bool semiInfiniteRay, bool orthographic,
    double &hitDepth);
ACGS_PORT bool rayIntersectsTriangle(const AcGsPickRay &ray,
                                     const AcGePoint3d &a,
                                     const AcGePoint3d &b,
                                     const AcGePoint3d &c,
                                     double depthNear, double depthFar,
                                     double &hitDepth);
ACGS_PORT bool rayIntersectsPoint(const AcGsPickRay &ray,
                                  const AcGePoint3d &location,
                                  double tolerance, double depthNear,
                                  double depthFar, double &hitDepth);

} // namespace acgs
