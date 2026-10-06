#pragma once

// geom2d (namespace ge) — the planar curve layer of milestone 1: line
// segments, circles, and circular arcs in 2D plus their pairwise
// intersections.  Sign decisions (collinearity, side tests) go through
// the robust predicates; magnitudes (parallelism, tangency) stay
// tolerance-based.

#include <algorithm>
#include <cmath>
#include <vector>

#include "ge/gepoint.h"
#include "ge/gepredicates.h"

#include <glm/glm.hpp>

namespace ge
{

inline constexpr double kGeom2dTolerance = 1.0e-12;

// ---- AcGeLineSeg2d ----

struct AcGeLineSeg2d
{
    AcGePoint2d start{0.0, 0.0};
    AcGePoint2d end{0.0, 0.0};

    AcGeVector2d direction() const
    {
        return {end.x - start.x, end.y - start.y};
    }

    double length() const
    {
        const AcGeVector2d d = direction();
        return std::sqrt(d.x * d.x + d.y * d.y);
    }

    // Point at parameter t in [0, 1].
    AcGePoint2d pointAt(double t) const
    {
        return {start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t};
    }
};

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

// ---- AcGeCircArc2d: CCW arc from startAngle to endAngle (radians) ----

struct AcGeCircArc2d
{
    AcGePoint2d center{0.0, 0.0};
    double radius = 1.0;
    double startAngle = 0.0;
    double endAngle = 0.0;

    double sweep() const
    {
        double sweep = endAngle - startAngle;
        while (sweep <= 0.0)
            sweep += 2.0 * 3.14159265358979323846;
        while (sweep > 2.0 * 3.14159265358979323846)
            sweep -= 2.0 * 3.14159265358979323846;
        return sweep;
    }

    AcGePoint2d pointAtAngle(double angle) const
    {
        return {center.x + radius * std::cos(angle),
                center.y + radius * std::sin(angle)};
    }

    AcGePoint2d startPoint() const { return pointAtAngle(startAngle); }
    AcGePoint2d endPoint() const { return pointAtAngle(endAngle); }

    // Is |angle| within the CCW span [startAngle, startAngle + sweep]?
    bool angleOnArc(double angle) const
    {
        const double sweep = this->sweep();
        double rel = std::fmod(angle - startAngle, 2.0 * 3.14159265358979323846);
        if (rel < 0.0)
            rel += 2.0 * 3.14159265358979323846;
        return rel <= sweep + kGeom2dTolerance;
    }

    bool contains(const AcGePoint2d &p) const
    {
        const double dx = p.x - center.x;
        const double dy = p.y - center.y;
        if (std::abs(std::sqrt(dx * dx + dy * dy) - radius) >
            kGeom2dTolerance)
            return false;
        return angleOnArc(std::atan2(dy, dx));
    }
};

// ---- intersections ----

// Line-line: exact collinearity through the robust predicate; returns
// 0 (parallel/collinear-distinct), 1 (point), or 2 (collinear-overlap
// reported as its two extreme points).
inline std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &a,
                                          const AcGeLineSeg2d &b)
{
    std::vector<AcGePoint2d> out;
    const double d1 = ge::orient2d(a.start, a.end, b.start);
    const double d2 = ge::orient2d(a.start, a.end, b.end);
    if (d1 == 0.0 && d2 == 0.0)
    {
        // Collinear: report the overlap extremes when the segments touch.
        const double dotAB = (b.start.x - a.start.x) * (a.end.x - a.start.x) +
                             (b.start.y - a.start.y) *
                                 (a.end.y - a.start.y);
        const double dotAC = (b.end.x - a.start.x) * (a.end.x - a.start.x) +
                             (b.end.y - a.start.y) * (a.end.y - a.start.y);
        const double lenA = a.length() * a.length();
        if (dotAB > lenA + kGeom2dTolerance &&
            dotAC > lenA + kGeom2dTolerance)
            return out; // b beyond a
        out.push_back(b.start);
        out.push_back(b.end);
        return out;
    }
    if ((d1 > 0.0) == (d2 > 0.0))
        return out; // b entirely on one side: no crossing

    // Robust side test of a's endpoints against b must also straddle.
    const double d3 = ge::orient2d(b.start, b.end, a.start);
    const double d4 = ge::orient2d(b.start, b.end, a.end);
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
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    const double fx = line.start.x - circle.center.x;
    const double fy = line.start.y - circle.center.y;
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
         intersect(line, AcGeCircle2d{arc.center, arc.radius}))
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
         intersect(AcGeCircle2d{a.center, a.radius},
                   AcGeCircle2d{b.center, b.radius}))
    {
        if (a.contains(p) && b.contains(p))
            out.push_back(p);
    }
    return out;
}

} // namespace ge
