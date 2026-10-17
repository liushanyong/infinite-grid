#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <stdexcept>

#include "AcGeInterval.h"
#include "AcGePoint.h"
#include "AcGeUtil.h"

// Source-level AcGeCurve2d/3d::boundBlock()/orthoBoundBlock() aligned with
// the ObjectARX reference (gecv2d.h/gecv3d.h): both spellings return the
// coordinate-aligned bounding box of the curve (the reference HTML
// documents identical coordinate-parallel sides for the two names, so
// this port computes one box and offers both names for API parity).
//
// Because the library keeps curves as independent value types (no
// AcGeCurve2d/3d hierarchy), these are free functions over any curve
// providing pointAt(double) or pointAtAngle(double). The parameter domain
// is discovered through the shared protocol chain (minParameter/
// maxParameter, then startParam/endParam, then the startAngle/endAngle
// members or startAng()/endAng() accessors used by the 2D conic arcs,
// then [0,1]); an explicit AcGeInterval restricts evaluation to a
// subrange. The block is built from uniform samples plus the domain
// endpoints, which is exact for the built-in line/arc/conic/spline types.

namespace ge
{
namespace detail
{

inline void boundBlockInterval(const AcGeInterval &range, double &low,
                               double &high)
{
    if (range.isUnBounded())
        return;
    if (range.isBoundedBelow())
        low = range.lowerBound();
    if (range.isBoundedAbove())
        high = range.upperBound();
    if (!(low < high))
        throw std::invalid_argument(
            "boundBlock requires an increasing parameter interval.");
}

template <class Curve>
auto curveEvalAt(const Curve &curve, double t)
{
    if constexpr (requires { curve.pointAt(t); })
        return curve.pointAt(t);
    else
        return curve.pointAtAngle(t);
}

template <class Curve>
void curveDomain(const Curve &curve, double &low, double &high)
{
    if constexpr (requires
                  {
                      { curve.minParameter() } -> std::convertible_to<double>;
                      { curve.maxParameter() } -> std::convertible_to<double>;
                  })
    {
        low = curve.minParameter();
        high = curve.maxParameter();
    }
    else if constexpr (requires
                       {
                           { curve.startParam() } ->
                               std::convertible_to<double>;
                           { curve.endParam() } -> std::convertible_to<double>;
                       })
    {
        low = curve.startParam();
        high = curve.endParam();
    }
    else if constexpr (requires
                       {
                           { curve.startAngle } ->
                               std::convertible_to<double>;
                           { curve.endAngle } ->
                               std::convertible_to<double>;
                       })
    {
        low = curve.startAngle;
        high = curve.endAngle;
    }
    else if constexpr (requires
                       {
                           { curve.startAng() } ->
                               std::convertible_to<double>;
                           { curve.endAng() } ->
                               std::convertible_to<double>;
                       })
    {
        low = curve.startAng();
        high = curve.endAng();
    }
    else
    {
        low = 0.0;
        high = 1.0;
    }
}

template <class Curve>
auto curveBoundingBox(const Curve &curve, const AcGeInterval &range)
{
    using PointT = decltype(curveEvalAt(curve, 0.0));

    double low = 0.0;
    double high = 0.0;
    curveDomain(curve, low, high);
    boundBlockInterval(range, low, high);

    constexpr int kSamples = 64;
    PointT minPt = curveEvalAt(curve, low);
    PointT maxPt = minPt;
    for (int i = 1; i <= kSamples; ++i)
    {
        const double t = low + (high - low) * i / kSamples;
        const PointT point = curveEvalAt(curve, t);
        minPt.x = std::min(minPt.x, point.x);
        minPt.y = std::min(minPt.y, point.y);
        if constexpr (requires { minPt.z; })
            minPt.z = std::min(minPt.z, point.z);
        maxPt.x = std::max(maxPt.x, point.x);
        maxPt.y = std::max(maxPt.y, point.y);
        if constexpr (requires { maxPt.z; })
            maxPt.z = std::max(maxPt.z, point.z);
    }

    AcGePoint2d min2{minPt.x, minPt.y};
    AcGePoint2d max2{maxPt.x, maxPt.y};
    if constexpr (requires { minPt.z; })
    {
        AcGePoint3d min3{minPt.x, minPt.y, minPt.z};
        AcGePoint3d max3{maxPt.x, maxPt.y, maxPt.z};
        AcGeBoundBlock3d block;
        block.set(min3, max3);
        return block;
    }
    else
    {
        AcGeBoundBlock2d block;
        block.set(min2, max2);
        return block;
    }
}

} // namespace detail

// 3D curve bound blocks (coordinate-aligned per the reference docs).
template <class Curve>
AcGeBoundBlock3d orthoBoundBlock(const Curve &curve,
                                 const AcGeInterval &range = AcGeInterval{})
{
    return detail::curveBoundingBox(curve, range);
}

template <class Curve>
AcGeBoundBlock3d boundBlock(const Curve &curve,
                            const AcGeInterval &range = AcGeInterval{})
{
    return orthoBoundBlock(curve, range);
}

// 2D curve bound blocks (coordinate-aligned per the reference docs).
template <class Curve>
AcGeBoundBlock2d orthoBoundBlock2d(const Curve &curve,
                                   const AcGeInterval &range = AcGeInterval{})
{
    return detail::curveBoundingBox(curve, range);
}

template <class Curve>
AcGeBoundBlock2d boundBlock2d(const Curve &curve,
                              const AcGeInterval &range = AcGeInterval{})
{
    return orthoBoundBlock2d(curve, range);
}

} // namespace ge
