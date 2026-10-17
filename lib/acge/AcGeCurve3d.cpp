#include "acge/AcGeCurve3d.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// ---------------------------------------------------------------------------
// AcGeLineSeg3d
// ---------------------------------------------------------------------------

AcGeLineSeg3d::AcGeLineSeg3d(const AcGePoint3d &start, const AcGePoint3d &end)
{
    set(start, end);
}

AcGeLineSeg3d::AcGeLineSeg3d(const AcGePoint3d &start,
                             const AcGeVector3d &direction)
{
    set(start, direction);
}

AcGeLineSeg3d &AcGeLineSeg3d::set(const AcGePoint3d &start,
                                  const AcGePoint3d &end)
{
    if (start == end)
        throw std::invalid_argument("AcGeLineSeg3d endpoints must differ.");
    m_startPoint = start;
    m_endPoint = end;
    return *this;
}

AcGeLineSeg3d &AcGeLineSeg3d::set(const AcGePoint3d &start,
                                  const AcGeVector3d &direction)
{
    if (direction.isZeroLength())
        throw std::invalid_argument("AcGeLineSeg3d direction must be nonzero.");
    m_startPoint = start;
    m_endPoint = start + direction;
    return *this;
}

double AcGeLineSeg3d::parameterOf(const AcGePoint3d &point) const
{
    const AcGeVector3d segment = direction();
    const double lengthSquared = segment.lengthSq();
    return lengthSquared == 0.0
               ? 0.0
               : (point - m_startPoint).dotProduct(segment) /
                     lengthSquared;
}

double AcGeLineSeg3d::length(double fromParam, double toParam,
                             double tol) const
{
    const double span = std::abs(toParam - fromParam) * length();
    return span <= std::abs(tol) ? 0.0 : span;
}

bool AcGeLineSeg3d::isOn(double parameter, const AcGeTol &tol) const
{
    const double pointTolerance = std::abs(tol.equalPoint());
    const double parameterTolerance =
        pointTolerance / std::max(length(), pointTolerance);
    return parameter >= -parameterTolerance &&
           parameter <= 1.0 + parameterTolerance;
}

bool AcGeLineSeg3d::isOn(const AcGePoint3d &point, double &parameter,
                         const AcGeTol &tol) const
{
    const double candidate = parameterOf(point);
    const double pointTolerance = std::abs(tol.equalPoint());
    const double parameterTolerance =
        pointTolerance / std::max(length(), pointTolerance);
    if (candidate < -parameterTolerance ||
        candidate > 1.0 + parameterTolerance ||
        distanceTo(point, tol) > pointTolerance)
        return false;
    parameter = std::clamp(candidate, 0.0, 1.0);
    return true;
}

// ---------------------------------------------------------------------------
// AcGeCircArc3d
// ---------------------------------------------------------------------------

AcGePoint3d AcGeCircArc3d::pointAt(double angle) const
{
    const AcGeVector3d u = referenceAxis();
    const AcGeVector3d w = normal.normal().crossProduct(u);
    return center + u * (radius * std::cos(angle)) +
           w * (radius * std::sin(angle));
}

AcGeVector3d AcGeCircArc3d::referenceAxis() const
{
    const AcGeVector3d unitNormal = normal.normal();
    AcGeVector3d axis = std::abs(unitNormal.z) < 0.9
        ? AcGeVector3d(0.0, 0.0, 1.0)
        : AcGeVector3d(1.0, 0.0, 0.0);
    return (axis - unitNormal * axis.dotProduct(unitNormal)).normal();
}

bool AcGeCircArc3d::isInside(const AcGePoint3d &point,
                             const AcGeTol &tol) const
{
    const AcGeVector3d unitNormal = normal.normal();
    if (unitNormal.isZeroLength(tol))
        return false;
    const AcGeVector3d offset = point - center;
    if (std::abs(offset.dotProduct(unitNormal)) > tol.equalPoint() ||
        std::abs(offset.length() - std::abs(radius)) > tol.equalPoint())
        return false;

    const AcGeVector3d u = referenceAxis();
    const AcGeVector3d v = unitNormal.crossProduct(u);
    double angle = std::atan2(offset.dotProduct(v), offset.dotProduct(u));
    constexpr double twoPi = 6.2831853071795864769;
    if (angle < 0.0)
        angle += twoPi;
    double start = std::fmod(startAngle, twoPi);
    double end = std::fmod(endAngle, twoPi);
    if (start < 0.0) start += twoPi;
    if (end < 0.0) end += twoPi;
    const double span = std::fmod(end - start + twoPi, twoPi);
    const double fromStart = std::fmod(angle - start + twoPi, twoPi);
    return endAngle - startAngle >= twoPi - tol.equalVector() ||
           fromStart <= span + tol.equalVector();
}

// ---------------------------------------------------------------------------
// AcGeCircle3d
// ---------------------------------------------------------------------------

AcGePoint3d AcGeCircle3d::closestPointTo(const AcGePoint3d &point) const
{
    const AcGeVector3d unitNormal = normal.normal();
    if (unitNormal.isZeroLength())
        return center;
    const AcGeVector3d offset = point - center;
    const AcGeVector3d radial =
        offset - unitNormal * offset.dotProduct(unitNormal);
    const AcGeVector3d direction =
        radial.isZeroLength() ? referenceAxis() : radial.normal();
    return center + direction * radius;
}

bool AcGeCircle3d::isOn(const AcGePoint3d &point, const AcGeTol &tol) const
{
    const AcGeVector3d unitNormal = normal.normal();
    if (unitNormal.isZeroLength(tol))
        return false;
    const AcGeVector3d offset = point - center;
    return std::abs(offset.dotProduct(unitNormal)) <= tol.equalPoint() &&
           std::abs(offset.length() - radius) <= tol.equalPoint();
}

bool AcGeCircle3d::contains(const AcGePoint3d &point, const AcGeTol &tol) const
{
    const AcGeVector3d unitNormal = normal.normal();
    if (unitNormal.isZeroLength(tol))
        return false;
    const AcGeVector3d offset = point - center;
    return std::abs(offset.dotProduct(unitNormal)) <= tol.equalPoint() &&
           offset.length() <= radius + tol.equalPoint();
}

// ---------------------------------------------------------------------------
// AcGeEllipArc3d
// ---------------------------------------------------------------------------

AcGeEllipArc3d &AcGeEllipArc3d::setMajorRadius(double radius)
{
    majorAxis = majorAxis.normal() * std::abs(radius);
    return *this;
}

AcGeEllipArc3d &AcGeEllipArc3d::setMinorRadius(double radius)
{
    const double major = majorRadius();
    radiusRatio = major == 0.0 ? 0.0 : std::abs(radius) / major;
    return *this;
}

AcGeEllipArc3d &AcGeEllipArc3d::setAxes(const AcGeVector3d &newMajorAxis,
                                        const AcGeVector3d &newMinorAxis)
{
    majorAxis = newMajorAxis;
    normal = newMajorAxis.crossProduct(newMinorAxis).normal();
    const double major = majorRadius();
    radiusRatio = major == 0.0 ? 0.0 : newMinorAxis.length() / major;
    return *this;
}

AcGePoint3d AcGeEllipArc3d::pointAt(double angle) const
{
    const AcGeVector3d u = majorAxis.normal();
    const AcGeVector3d w = normal.normal().crossProduct(u);
    const double minor = minorRadius();
    return center + u * (majorRadius() * std::cos(angle)) +
           w * (minor * std::sin(angle));
}

bool AcGeEllipArc3d::isInside(const AcGePoint3d &point,
                              const AcGeTol &tol) const
{
    const AcGeVector3d unitNormal = normal.normal();
    const AcGeVector3d u = majorAxis.normal();
    if (unitNormal.isZeroLength(tol) || u.isZeroLength(tol) ||
        std::abs((point - center).dotProduct(unitNormal)) >
            tol.equalPoint())
        return false;
    const AcGeVector3d v = unitNormal.crossProduct(u);
    const AcGeVector3d offset = point - center;
    const double a = majorRadius();
    const double b = minorRadius();
    if (a <= tol.equalVector() || b <= tol.equalVector())
        return false;
    const double angle =
        std::atan2(offset.dotProduct(v) / b, offset.dotProduct(u) / a);
    const AcGePoint3d nearest = pointAt(angle);
    if (nearest.distanceTo(point) > tol.equalPoint())
        return false;
    constexpr double twoPi = 6.2831853071795864769;
    const double span = std::fmod(endAngle - startAngle + twoPi, twoPi);
    const double fromStart = std::fmod(angle - startAngle + twoPi, twoPi);
    return endAngle - startAngle >= twoPi - tol.equalVector() ||
           fromStart <= span + tol.equalVector();
}
