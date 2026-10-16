#include "application/DemoSceneVisibility.hpp"

#include <algorithm>
#include <cmath>

namespace application::demo
{

CameraSpaceAabb cameraAabbBounds(
    const acgs::math::DVec3 &worldCenter,
    const acgs::math::DVec3 &halfExtent,
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront)
{
    const acgs::math::DVec3 delta = worldCenter - cameraPosition;
    const double rightRadius =
        acgs::math::dot(acgs::math::abs(cameraRight), halfExtent);
    const double upRadius =
        acgs::math::dot(acgs::math::abs(cameraUp), halfExtent);
    const double frontRadius =
        acgs::math::dot(acgs::math::abs(cameraFront), halfExtent);
    const double centerX = acgs::math::dot(delta, cameraRight);
    const double centerY = acgs::math::dot(delta, cameraUp);
    const double centerDepth = acgs::math::dot(delta, cameraFront);

    return {centerX - rightRadius, centerX + rightRadius,
            centerY - upRadius, centerY + upRadius,
            centerDepth - frontRadius, centerDepth + frontRadius};
}

CameraSpaceAabb cameraAabbBounds(
    const WorldAabb &worldBounds,
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront)
{
    const acgs::math::DVec3 center =
        (worldBounds.min + worldBounds.max) * 0.5;
    const acgs::math::DVec3 halfExtent =
        (worldBounds.max - worldBounds.min) * 0.5;
    return cameraAabbBounds(center, halfExtent, cameraPosition, cameraRight,
                            cameraUp, cameraFront);
}

bool aabbIntersectsOrthoViewport(const CameraSpaceAabb &bounds,
                                 double halfWidth, double halfHeight)
{
    return bounds.maxX >= -halfWidth && bounds.minX <= halfWidth &&
           bounds.maxY >= -halfHeight && bounds.minY <= halfHeight;
}

bool aabbIntersectsPerspectiveFrustum(const CameraSpaceAabb &bounds,
                                      double nearPlane, double farPlane,
                                      double tanHalfVertical,
                                      double tanHalfHorizontal)
{
    const auto outside = [&](double x, double y, double z, double offset) {
        const double maximum =
            (x < 0.0 ? bounds.minX : bounds.maxX) * x +
            (y < 0.0 ? bounds.minY : bounds.maxY) * y +
            (z < 0.0 ? bounds.minDepth : bounds.maxDepth) * z + offset;
        return maximum < 0.0;
    };

    return !outside(0.0, 0.0, 1.0, -nearPlane) &&
           !outside(0.0, 0.0, -1.0, farPlane) &&
           !outside(1.0, 0.0, tanHalfHorizontal, 0.0) &&
           !outside(-1.0, 0.0, tanHalfHorizontal, 0.0) &&
           !outside(0.0, 1.0, tanHalfVertical, 0.0) &&
           !outside(0.0, -1.0, tanHalfVertical, 0.0);
}

UnifiedVisibilityQuery UnifiedVisibilityQuery::makeOrtho(
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront,
    double halfWidthValue, double halfHeightValue, double pixelsHigh)
{
    return {true, cameraPosition, cameraRight, cameraUp, cameraFront,
            halfWidthValue, halfHeightValue, 0.0, 0.0, 0.0, 0.0,
            pixelsHigh, 0.1};
}

UnifiedVisibilityQuery UnifiedVisibilityQuery::makePerspective(
    const acgs::math::DVec3 &cameraPosition,
    const acgs::math::DVec3 &cameraRight,
    const acgs::math::DVec3 &cameraUp,
    const acgs::math::DVec3 &cameraFront,
    double tanHalfV, double tanHalfH, double pixelsHigh,
    double nearPlane, double farPlane)
{
    return {false, cameraPosition, cameraRight, cameraUp, cameraFront,
            0.0, 0.0, nearPlane, farPlane, tanHalfV, tanHalfH,
            pixelsHigh, 0.1};
}

CameraSpaceAabb UnifiedVisibilityQuery::cameraAabb(
    const VisibilityCandidate &candidate) const
{
    const acgs::math::DVec3 center = (candidate.min + candidate.max) * 0.5;
    const acgs::math::DVec3 halfExtent =
        acgs::math::max((candidate.max - candidate.min) * 0.5,
                        acgs::math::DVec3(0.5e-3));
    return cameraAabbBounds(center, halfExtent, cameraPos, right, up, front);
}

bool UnifiedVisibilityQuery::intersects(
    const VisibilityCandidate &candidate) const
{
    const CameraSpaceAabb bounds = cameraAabb(candidate);
    if (isOrtho)
        return aabbIntersectsOrthoViewport(bounds, halfWidth, halfHeight);

    return aabbIntersectsPerspectiveFrustum(
        bounds, nearDepth, farDepth, tanHalfVertical, tanHalfHorizontal);
}

double UnifiedVisibilityQuery::screenExtent(
    const VisibilityCandidate &candidate) const
{
    const CameraSpaceAabb bounds = cameraAabb(candidate);
    const double worldSize = candidate.lodSize > 0.0
                                 ? candidate.lodSize
                                 : acgs::math::length(candidate.max - candidate.min);
    if (isOrtho)
        return worldSize * drawableHeight / (2.0 * halfHeight);

    const double nearestDepth =
        std::max(nearDepth, std::max(0.0, bounds.minDepth));
    return worldSize * drawableHeight /
           (2.0 * nearestDepth * tanHalfVertical);
}

VisibilityState UnifiedVisibilityQuery::classify(
    const VisibilityCandidate &candidate) const
{
    if (!intersects(candidate))
        return VisibilityState::Offscreen;
    if (!isOrtho && screenExtent(candidate) < minPixelExtent)
        return VisibilityState::Tiny;
    return VisibilityState::Visible;
}

} // namespace application::demo
