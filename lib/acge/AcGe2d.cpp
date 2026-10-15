#include "acge/AcGe2d.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// AcGeLineSeg2d
// ---------------------------------------------------------------------------

AcGeLineSeg2d::AcGeLineSeg2d(const AcGePoint2d &start, const AcGePoint2d &end)
{
    set(start, end);
}

AcGeLineSeg2d::AcGeLineSeg2d(const AcGePoint2d &start,
                             const AcGeVector2d &direction)
{
    set(start, direction);
}

AcGeLineSeg2d &AcGeLineSeg2d::set(const AcGePoint2d &start,
                                  const AcGePoint2d &end)
{
    if (start == end)
        throw std::invalid_argument("AcGeLineSeg2d endpoints must differ.");
    m_start = start;
    m_end = end;
    return *this;
}

AcGeLineSeg2d &AcGeLineSeg2d::set(const AcGePoint2d &start,
                                  const AcGeVector2d &direction)
{
    if (direction.isZeroLength())
        throw std::invalid_argument("AcGeLineSeg2d direction must be nonzero.");
    m_start = start;
    m_end = start + direction;
    return *this;
}

double AcGeLineSeg2d::parameterOf(const AcGePoint2d &point) const
{
    const AcGeVector2d segment = direction();
    const double lengthSquared = segment.lengthSq();
    return lengthSquared == 0.0
               ? 0.0
               : (point - m_start).dotProduct(segment) / lengthSquared;
}

double AcGeLineSeg2d::length(double fromParam, double toParam,
                             double tol) const
{
    const double span = std::abs(toParam - fromParam) * length();
    return span <= std::abs(tol) ? 0.0 : span;
}

bool AcGeLineSeg2d::isOn(double parameter, const AcGeTol &tol) const
{
    const double pointTolerance = std::abs(tol.equalPoint());
    const double parameterTolerance =
        pointTolerance / std::max(length(), pointTolerance);
    return parameter >= -parameterTolerance &&
           parameter <= 1.0 + parameterTolerance;
}

bool AcGeLineSeg2d::isOn(const AcGePoint2d &point, double &parameter,
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
// AcGeCircArc2d
// ---------------------------------------------------------------------------

double AcGeCircArc2d::normalizedAngle(double angle)
{
    constexpr double twoPi = 6.2831853071795864769;
    angle = std::fmod(angle, twoPi);
    return angle < 0.0 ? angle + twoPi : angle;
}

AcGeCircArc2d &AcGeCircArc2d::set(const AcGePoint2d &center, double radius)
{
    m_center = center;
    m_radius = std::abs(radius);
    m_startAngle = 0.0;
    m_endAngle = 0.0;
    m_refVec = AcGeVector2d::kXAxis;
    m_clockwise = false;
    return *this;
}

AcGeCircArc2d &AcGeCircArc2d::set(const AcGePoint2d &center, double radius,
                                  double startAngle, double endAngle,
                                  const AcGeVector2d &reference, bool clockwise)
{
    m_center = center;
    m_radius = std::abs(radius);
    m_startAngle = startAngle;
    m_endAngle = endAngle;
    m_refVec = reference.normal();
    if (m_refVec.isZeroLength())
        m_refVec = AcGeVector2d::kXAxis;
    m_clockwise = clockwise;
    return *this;
}

AcGeCircArc2d &AcGeCircArc2d::setRefVec(const AcGeVector2d &reference)
{
    const AcGeVector2d normalized = reference.normal();
    if (!normalized.isZeroLength())
        m_refVec = normalized;
    return *this;
}

double AcGeCircArc2d::sweep() const
{
    constexpr double twoPi = 6.2831853071795864769;
    const double delta = m_clockwise
        ? normalizedAngle(m_startAngle - m_endAngle)
        : normalizedAngle(m_endAngle - m_startAngle);
    return delta == 0.0 ? twoPi : delta;
}

AcGePoint2d AcGeCircArc2d::pointAtAngle(double angle) const
{
    const AcGeVector2d side{-m_refVec.y, m_refVec.x};
    return m_center + m_refVec * (m_radius * std::cos(angle)) +
           side * (m_radius * std::sin(angle));
}

bool AcGeCircArc2d::angleOnArc(double worldAngle) const
{
    const AcGeVector2d radial{std::cos(worldAngle), std::sin(worldAngle)};
    const double relative =
        std::atan2(m_refVec.x * radial.y - m_refVec.y * radial.x,
                   m_refVec.dotProduct(radial));
    const double fromStart = m_clockwise
        ? normalizedAngle(m_startAngle - relative)
        : normalizedAngle(relative - m_startAngle);
    return fromStart <= sweep() + 1.0e-12;
}

bool AcGeCircArc2d::isInside(const AcGePoint2d &point,
                             const AcGeTol &tol) const
{
    const AcGeVector2d fromCenter = point - m_center;
    if (std::abs(fromCenter.length() - m_radius) > tol.equalPoint())
        return false;
    if (fromCenter.isZeroLength(tol))
        return m_radius <= tol.equalPoint();
    const double relative =
        std::atan2(m_refVec.x * fromCenter.y - m_refVec.y * fromCenter.x,
                   m_refVec.dotProduct(fromCenter));
    const double fromStart = m_clockwise
        ? normalizedAngle(m_startAngle - relative)
        : normalizedAngle(relative - m_startAngle);
    return fromStart <= sweep() + tol.equalVector();
}

// ---------------------------------------------------------------------------
// AcGeEllipArc2d
// ---------------------------------------------------------------------------

double AcGeEllipArc2d::normalizedAngle(double angle)
{
    constexpr double twoPi = 6.2831853071795864769;
    angle = std::fmod(angle, twoPi);
    return angle < 0.0 ? angle + twoPi : angle;
}

AcGeEllipArc2d::AcGeEllipArc2d(const AcGeCircArc2d &circle)
{
    set(circle);
}

AcGeEllipArc2d::AcGeEllipArc2d(const AcGePoint2d &center,
                               const AcGeVector2d &majorAxis,
                               const AcGeVector2d &minorAxis, double majorRadius,
                               double minorRadius)
{
    set(center, majorAxis, minorAxis, majorRadius, minorRadius);
}

AcGeEllipArc2d::AcGeEllipArc2d(const AcGePoint2d &center,
                               const AcGeVector2d &majorAxis,
                               const AcGeVector2d &minorAxis, double majorRadius,
                               double minorRadius, double startAngle,
                               double endAngle)
{
    set(center, majorAxis, minorAxis, majorRadius, minorRadius);
    setAngles(startAngle, endAngle);
}

AcGeEllipArc2d &AcGeEllipArc2d::set(const AcGePoint2d &center,
                                    const AcGeVector2d &majorAxis,
                                    const AcGeVector2d &minorAxis,
                                    double majorRadius, double minorRadius)
{
    m_center = center;
    m_majorAxis = majorAxis.normal();
    if (m_majorAxis.isZeroLength())
        m_majorAxis = AcGeVector2d::kXAxis;
    m_minorAxis = minorAxis -
                  m_majorAxis * minorAxis.dotProduct(m_majorAxis);
    if (m_minorAxis.isZeroLength())
        m_minorAxis = m_majorAxis.perpVector();
    m_minorAxis.normalize();
    m_majorRadius = std::abs(majorRadius);
    m_minorRadius = std::abs(minorRadius);
    m_startAngle = 0.0;
    m_endAngle = 6.2831853071795864769;
    return *this;
}

AcGeEllipArc2d &AcGeEllipArc2d::set(const AcGeCircArc2d &circle)
{
    const AcGeVector2d major = circle.refVec();
    AcGeVector2d minor{-major.y, major.x};
    set(circle.center(), major, minor, circle.radius(), circle.radius());
    if (circle.isClockWise())
    {
        m_minorAxis.negate();
        m_startAngle = normalizedAngle(-circle.startAng());
        m_endAngle = m_startAngle + circle.sweep();
    }
    else
    {
        m_startAngle = circle.startAng();
        m_endAngle = m_startAngle + circle.sweep();
    }
    return *this;
}

AcGeEllipArc2d &AcGeEllipArc2d::setAxes(const AcGeVector2d &majorAxis,
                                        const AcGeVector2d &minorAxis)
{
    const double majorRadius = m_majorRadius;
    const double minorRadius = m_minorRadius;
    const double start = m_startAngle;
    const double end = m_endAngle;
    set(m_center, majorAxis, minorAxis, majorRadius, minorRadius);
    m_startAngle = start;
    m_endAngle = end;
    return *this;
}

AcGeEllipArc2d &AcGeEllipArc2d::setAngles(double startAngle, double endAngle)
{
    if (endAngle <= startAngle)
        throw std::invalid_argument(
            "AcGeEllipArc2d end angle must exceed start angle.");
    m_startAngle = startAngle;
    m_endAngle = endAngle;
    return *this;
}

bool AcGeEllipArc2d::isInside(const AcGePoint2d &point,
                              const AcGeTol &tol) const
{
    if (m_majorRadius <= tol.equalVector() ||
        m_minorRadius <= tol.equalVector())
        return false;
    const AcGeVector2d offset = point - m_center;
    const double majorCoordinate =
        offset.dotProduct(m_majorAxis) / m_majorRadius;
    const double minorCoordinate =
        offset.dotProduct(m_minorAxis) / m_minorRadius;
    const double angle = normalizedAngle(
        std::atan2(minorCoordinate, majorCoordinate));
    if (pointAt(angle).distanceTo(point) > tol.equalPoint())
        return false;

    constexpr double twoPi = 6.2831853071795864769;
    const double span = m_endAngle - m_startAngle;
    const double fromStart = normalizedAngle(angle - m_startAngle);
    return span >= twoPi - tol.equalVector() ||
           fromStart <= span + tol.equalVector();
}

// ---------------------------------------------------------------------------
// ge::AcGeCircle2d
// ---------------------------------------------------------------------------

namespace ge
{

bool AcGeCircle2d::contains(const AcGePoint2d &p) const
{
    const double dx = p.x - center.x;
    const double dy = p.y - center.y;
    return std::sqrt(dx * dx + dy * dy) <= radius + kGeom2dTolerance;
}

// ---------------------------------------------------------------------------
// intersections
// ---------------------------------------------------------------------------

std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &a,
                                   const AcGeLineSeg2d &b)
{
    std::vector<AcGePoint2d> out;
    const AcGePoint2d aStart = a.startPoint();
    const AcGePoint2d aEnd = a.endPoint();
    const AcGePoint2d bStart = b.startPoint();
    const AcGePoint2d bEnd = b.endPoint();
    const double d1 = ge::orient2d(aStart, aEnd, bStart);
    const double d2 = ge::orient2d(aStart, aEnd, bEnd);
    if (d1 == 0.0 && d2 == 0.0)
    {
        // Collinear: report the overlap extremes when the segments touch.
        const double dotAB = (bStart.x - aStart.x) * (aEnd.x - aStart.x) +
                             (bStart.y - aStart.y) * (aEnd.y - aStart.y);
        const double dotAC = (bEnd.x - aStart.x) * (aEnd.x - aStart.x) +
                             (bEnd.y - aStart.y) * (aEnd.y - aStart.y);
        const double lenA = a.length() * a.length();
        if (dotAB > lenA + kGeom2dTolerance &&
            dotAC > lenA + kGeom2dTolerance)
            return out; // b beyond a
        out.push_back(bStart);
        out.push_back(bEnd);
        return out;
    }
    if ((d1 > 0.0) == (d2 > 0.0))
        return out; // b entirely on one side: no crossing

    // Robust side test of a's endpoints against b must also straddle.
    const double d3 = ge::orient2d(bStart, bEnd, aStart);
    const double d4 = ge::orient2d(bStart, bEnd, aEnd);
    if ((d3 > 0.0) == (d4 > 0.0))
        return out;

    const double denominator = d1 - d2;
    const double t = d1 / denominator;
    out.push_back(a.pointAt(t));
    return out;
}

std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
                                   const AcGeCircle2d &circle)
{
    std::vector<AcGePoint2d> out;
    const AcGePoint2d lineStart = line.startPoint();
    const AcGePoint2d lineEnd = line.endPoint();
    const double dx = lineEnd.x - lineStart.x;
    const double dy = lineEnd.y - lineStart.y;
    const double fx = lineStart.x - circle.center.x;
    const double fy = lineStart.y - circle.center.y;
    const double a = dx * dx + dy * dy;
    if (a < kGeom2dTolerance * kGeom2dTolerance)
        return out;
    const double b = 2.0 * (fx * dx + fy * dy);
    const double c = fx * fx + fy * fy - circle.radius * circle.radius;
    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
    {
        if (discriminant > -kGeom2dTolerance * kGeom2dTolerance)
            discriminant = 0.0; // tangent within tolerance
        else
            return out;
    }
    const double root = std::sqrt(discriminant);
    const double t1 = (-b - root) / (2.0 * a);
    const double t2 = (-b + root) / (2.0 * a);
    if (t1 >= -kGeom2dTolerance && t1 <= 1.0 + kGeom2dTolerance)
        out.push_back(line.pointAt(std::clamp(t1, 0.0, 1.0)));
    if (root > kGeom2dTolerance &&
        t2 >= -kGeom2dTolerance && t2 <= 1.0 + kGeom2dTolerance)
        out.push_back(line.pointAt(std::clamp(t2, 0.0, 1.0)));
    return out;
}

std::vector<AcGePoint2d> intersect(const AcGeCircle2d &circle,
                                   const AcGeLineSeg2d &line)
{
    return intersect(line, circle);
}

std::vector<AcGePoint2d> intersect(const AcGeCircle2d &c1,
                                   const AcGeCircle2d &c2)
{
    std::vector<AcGePoint2d> out;
    const double dx = c2.center.x - c1.center.x;
    const double dy = c2.center.y - c1.center.y;
    const double distance = std::sqrt(dx * dx + dy * dy);
    if (distance < kGeom2dTolerance)
        return out; // concentric
    if (distance > c1.radius + c2.radius + kGeom2dTolerance)
        return out; // separate
    if (distance < std::abs(c1.radius - c2.radius) - kGeom2dTolerance)
        return out; // nested

    const double a = (c1.radius * c1.radius - c2.radius * c2.radius +
                      distance * distance) /
                     (2.0 * distance);
    const double h2 = c1.radius * c1.radius - a * a;
    const double h = h2 > 0.0 ? std::sqrt(h2) : 0.0;
    const double midX = c1.center.x + a * dx / distance;
    const double midY = c1.center.y + a * dy / distance;
    out.push_back({midX + h * dy / distance, midY - h * dx / distance});
    if (h > kGeom2dTolerance)
        out.push_back({midX - h * dy / distance, midY + h * dx / distance});
    return out;
}

std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
                                   const AcGeCircArc2d &arc)
{
    std::vector<AcGePoint2d> out;
    for (const AcGePoint2d &p :
         intersect(line, AcGeCircle2d{arc.center(), arc.radius()}))
    {
        if (arc.contains(p))
            out.push_back(p);
    }
    return out;
}

std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &arc,
                                   const AcGeLineSeg2d &line)
{
    return intersect(line, arc);
}

std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &a,
                                   const AcGeCircArc2d &b)
{
    std::vector<AcGePoint2d> out;
    for (const AcGePoint2d &p :
         intersect(AcGeCircle2d{a.center(), a.radius()},
                   AcGeCircle2d{b.center(), b.radius()}))
    {
        if (a.contains(p) && b.contains(p))
            out.push_back(p);
    }
    return out;
}

} // namespace ge
