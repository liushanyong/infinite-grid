#pragma once

// geom2d (namespace ge) — the planar curve layer of milestone 1: line
// segments, circles, and circular arcs in 2D plus their pairwise
// intersections.  Sign decisions (collinearity, side tests) go through
// the robust predicates; magnitudes (parallelism, tangency) stay
// tolerance-based.

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "ge/gepoint.h"
#include "ge/gepredicates.h"
#include "ge/gelinear.h"

#include <glm/glm.hpp>

struct AcGeLineSeg2d
{
private:
    AcGePoint2d m_start{0.0, 0.0};
    AcGePoint2d m_end{0.0, 0.0};

public:
    AcGeLineSeg2d() = default;
    AcGeLineSeg2d(const AcGePoint2d &start, const AcGePoint2d &end)
    {
        set(start, end);
    }

    AcGeLineSeg2d &set(const AcGePoint2d &start, const AcGePoint2d &end)
    {
        if (start == end)
            throw std::invalid_argument("AcGeLineSeg2d endpoints must differ.");
        m_start = start;
        m_end = end;
        return *this;
    }

    AcGePoint2d startPoint() const { return m_start; }
    AcGePoint2d endPoint() const { return m_end; }

    AcGeVector2d direction() const { return m_end - m_start; }
    double length() const { return m_start.distanceTo(m_end); }
    AcGePoint2d midPoint() const { return baryComb(0.5); }
    AcGePoint2d baryComb(double blendCoeff) const
    {
        return m_start + (m_end - m_start) * blendCoeff;
    }
    AcGePoint2d pointAt(double parameter) const { return baryComb(parameter); }
    void getBisector(AcGeLine2d &line) const
    {
        line.set(midPoint(), direction().perpVector());
    }
};

struct AcGeCircArc2d
{
private:
    AcGePoint2d m_center{0.0, 0.0};
    double m_radius = 1.0;
    double m_startAngle = 0.0;
    double m_endAngle = 0.0;
    AcGeVector2d m_refVec{1.0, 0.0};
    bool m_clockwise = false;

    static double normalizedAngle(double angle)
    {
        constexpr double twoPi = 6.2831853071795864769;
        angle = std::fmod(angle, twoPi);
        return angle < 0.0 ? angle + twoPi : angle;
    }

public:
    AcGeCircArc2d() = default;
    AcGeCircArc2d(const AcGePoint2d &center, double radius)
        : m_center(center), m_radius(std::abs(radius))
    {
    }
    AcGeCircArc2d(const AcGePoint2d &center, double radius,
                  double startAngle, double endAngle)
        : m_center(center), m_radius(std::abs(radius)),
          m_startAngle(startAngle), m_endAngle(endAngle)
    {
    }
    AcGeCircArc2d(const AcGePoint2d &center, double radius,
                  double startAngle, double endAngle,
                  const AcGeVector2d &reference, bool clockwise)
        : m_center(center), m_radius(std::abs(radius)),
          m_startAngle(startAngle), m_endAngle(endAngle),
          m_refVec(reference.normal()), m_clockwise(clockwise)
    {
        if (m_refVec.isZeroLength())
            m_refVec = AcGeVector2d::kXAxis;
    }

    AcGeCircArc2d &set(const AcGePoint2d &center, double radius)
    {
        m_center = center;
        m_radius = std::abs(radius);
        m_startAngle = 0.0;
        m_endAngle = 0.0;
        m_refVec = AcGeVector2d::kXAxis;
        m_clockwise = false;
        return *this;
    }
    AcGeCircArc2d &set(const AcGePoint2d &center, double radius,
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
    AcGeCircArc2d &setCenter(const AcGePoint2d &center)
    {
        m_center = center;
        return *this;
    }
    AcGeCircArc2d &setRadius(double radius)
    {
        m_radius = std::abs(radius);
        return *this;
    }
    AcGeCircArc2d &setRefVec(const AcGeVector2d &reference)
    {
        const AcGeVector2d normalized = reference.normal();
        if (!normalized.isZeroLength())
            m_refVec = normalized;
        return *this;
    }
    AcGeCircArc2d &setAngles(double startAngle, double endAngle)
    {
        m_startAngle = startAngle;
        m_endAngle = endAngle;
        return *this;
    }
    AcGeCircArc2d &setToComplement()
    {
        m_clockwise = !m_clockwise;
        return *this;
    }

    AcGePoint2d center() const { return m_center; }
    double radius() const { return m_radius; }
    double startAng() const { return m_startAngle; }
    double endAng() const { return m_endAngle; }
    AcGeVector2d refVec() const { return m_refVec; }
    bool isClockWise() const { return m_clockwise; }

    double sweep() const
    {
        constexpr double twoPi = 6.2831853071795864769;
        const double delta = m_clockwise
            ? normalizedAngle(m_startAngle - m_endAngle)
            : normalizedAngle(m_endAngle - m_startAngle);
        return delta == 0.0 ? twoPi : delta;
    }

    AcGePoint2d pointAtAngle(double angle) const
    {
        const AcGeVector2d side{-m_refVec.y, m_refVec.x};
        return m_center + m_refVec * (m_radius * std::cos(angle)) +
               side * (m_radius * std::sin(angle));
    }
    AcGePoint2d startPoint() const { return pointAtAngle(m_startAngle); }
    AcGePoint2d endPoint() const { return pointAtAngle(m_endAngle); }

    bool angleOnArc(double worldAngle) const
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

    bool isInside(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const
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

    bool contains(const AcGePoint2d &point) const { return isInside(point); }
};

namespace ge
{

inline constexpr double kGeom2dTolerance = 1.0e-12;

using ::AcGeLineSeg2d;

// ---- AcGeCircle2d ----

struct AcGeCircle2d
{
    AcGePoint2d center{0.0, 0.0};
    double radius = 1.0;

    bool contains(const AcGePoint2d &p) const
    {
        const double dx = p.x - center.x;
        const double dy = p.y - center.y;
        return std::sqrt(dx * dx + dy * dy) <= radius + kGeom2dTolerance;
    }
};

using ::AcGeCircArc2d;

// ---- intersections ----

// Line-line: exact collinearity through the robust predicate; returns
// 0 (parallel/collinear-distinct), 1 (point), or 2 (collinear-overlap
// reported as its two extreme points).
inline std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &a,
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

// Line-circle: 0, 1 (tangent), or 2 points.
inline std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
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

inline std::vector<AcGePoint2d> intersect(const AcGeCircle2d &circle,
                                          const AcGeLineSeg2d &line)
{
    return intersect(line, circle);
}

// Circle-circle: 0, 1 (tangent), or 2 points.
inline std::vector<AcGePoint2d> intersect(const AcGeCircle2d &c1,
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

// Segment-arc: circle-segment candidates filtered by the arc span.
inline std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
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

inline std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &arc,
                                          const AcGeLineSeg2d &line)
{
    return intersect(line, arc);
}

// Arc-arc: circle-circle candidates filtered by both arc spans.
inline std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &a,
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
