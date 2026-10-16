#pragma once

#include "application/DemoSceneTypes.hpp"

namespace application::demo
{

CameraSpaceAabb cameraAabbBounds(
    const acgs::math::DVec3 &worldCenter,
    const acgs::math::DVec3 &halfExtent,
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront);
CameraSpaceAabb cameraAabbBounds(
    const WorldAabb &worldBounds,
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront);
bool aabbIntersectsOrthoViewport(const CameraSpaceAabb &bounds,
                                 double halfWidth,
                                 double halfHeight);
bool aabbIntersectsPerspectiveFrustum(const CameraSpaceAabb &bounds,
                                      double nearPlane,
                                      double farPlane,
                                      double tanHalfVertical,
                                      double tanHalfHorizontal);

struct UnifiedVisibilityQuery
{
    bool isOrtho = false;
    acgs::math::DVec3 cameraPos{0.0};
    acgs::math::DVec3 right{1.0, 0.0, 0.0};
    acgs::math::DVec3 up{0.0, 1.0, 0.0};
    acgs::math::DVec3 front{0.0, 0.0, -1.0};
    double halfWidth = 1.0;
    double halfHeight = 1.0;
    double nearDepth = 0.05;
    double farDepth = 1.0e9;
    double tanHalfVertical = 0.4142;
    double tanHalfHorizontal = 0.4142;
    double drawableHeight = 1.0;
    double minPixelExtent = 0.1;

    static UnifiedVisibilityQuery makeOrtho(
        const acgs::math::DVec3 &cameraPosition,
        const acgs::math::DVec3 &cameraRight,
        const acgs::math::DVec3 &cameraUp,
        const acgs::math::DVec3 &cameraFront,
        double halfWidthValue, double halfHeightValue, double pixelsHigh);
    static UnifiedVisibilityQuery makePerspective(
        const acgs::math::DVec3 &cameraPosition,
        const acgs::math::DVec3 &cameraRight,
        const acgs::math::DVec3 &cameraUp,
        const acgs::math::DVec3 &cameraFront,
        double tanHalfV, double tanHalfH, double pixelsHigh,
        double nearPlane, double farPlane);

    CameraSpaceAabb cameraAabb(const VisibilityCandidate &candidate) const;
    bool intersects(const VisibilityCandidate &candidate) const;
    double screenExtent(const VisibilityCandidate &candidate) const;
    VisibilityState classify(const VisibilityCandidate &candidate) const;
};

} // namespace application::demo
