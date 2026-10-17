#pragma once

#include "AcGePort.h"

// geom2d (namespace ge) — the planar curve layer of milestone 1: line
// segments, circles, and circular arcs in 2D plus their pairwise
// intersections.  Sign decisions (collinearity, side tests) go through
// the robust predicates; magnitudes (parallelism, tangency) stay
// tolerance-based.

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

#include "AcGePoint.h"
#include "AcGePredicates.h"
#include "AcGeLinear.h"

#include <glm/glm.hpp>

struct ACGE_PORT AcGeLineSeg2d
{
private:
    AcGePoint2d m_start{0.0, 0.0};
    AcGePoint2d m_end{0.0, 0.0};

public:
    AcGeLineSeg2d() = default;
    AcGeLineSeg2d(const AcGePoint2d &start, const AcGePoint2d &end);
    AcGeLineSeg2d(const AcGePoint2d &start, const AcGeVector2d &direction);

    AcGeLineSeg2d &set(const AcGePoint2d &start, const AcGePoint2d &end);
    AcGeLineSeg2d &set(const AcGePoint2d &start,
                       const AcGeVector2d &direction);

    AcGePoint2d startPoint() const { return m_start; }
    AcGePoint2d endPoint() const { return m_end; }

    AcGeVector2d direction() const { return m_end - m_start; }
    double parameterOf(const AcGePoint2d &point) const;
    double length() const { return m_start.distanceTo(m_end); }
    double length(double fromParam, double toParam,
                  double tol = AcGeTol{}.equalPoint()) const;
    AcGePoint2d midPoint() const { return baryComb(0.5); }
    AcGePoint2d baryComb(double blendCoeff) const
    {
        return m_start + (m_end - m_start) * blendCoeff;
    }
    AcGePoint2d pointAt(double parameter) const { return baryComb(parameter); }
    AcGePoint2d closestPointTo(
        const AcGePoint2d &point,
        const AcGeTol & = AcGeTol{}) const
    {
        return pointAt(std::clamp(parameterOf(point), 0.0, 1.0));
    }
    double distanceTo(const AcGePoint2d &point,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        return closestPointTo(point, tol).distanceTo(point);
    }
    bool isOn(double parameter, const AcGeTol &tol = AcGeTol{}) const;
    bool isOn(const AcGePoint2d &point, double &parameter,
              const AcGeTol &tol = AcGeTol{}) const;
    void getBisector(AcGeLine2d &line) const
    {
        line.set(midPoint(), direction().perpVector());
    }
};

struct ACGE_PORT AcGeCircArc2d
{
private:
    AcGePoint2d m_center{0.0, 0.0};
    double m_radius = 1.0;
    double m_startAngle = 0.0;
    double m_endAngle = 0.0;
    AcGeVector2d m_refVec{1.0, 0.0};
    bool m_clockwise = false;

    static double normalizedAngle(double angle);

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

    AcGeCircArc2d &set(const AcGePoint2d &center, double radius);
    AcGeCircArc2d &set(const AcGePoint2d &center, double radius,
                       double startAngle, double endAngle,
                       const AcGeVector2d &reference, bool clockwise);
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
    AcGeCircArc2d &setRefVec(const AcGeVector2d &reference);
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

    double sweep() const;

    AcGePoint2d pointAtAngle(double angle) const;
    AcGePoint2d startPoint() const { return pointAtAngle(m_startAngle); }
    AcGePoint2d endPoint() const { return pointAtAngle(m_endAngle); }

    bool angleOnArc(double worldAngle) const;
    bool isInside(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
    bool contains(const AcGePoint2d &point) const { return isInside(point); }
};

struct ACGE_PORT AcGeEllipArc2d
{
private:
    AcGePoint2d m_center{0.0, 0.0};
    AcGeVector2d m_majorAxis{1.0, 0.0};
    AcGeVector2d m_minorAxis{0.0, 1.0};
    double m_majorRadius = 1.0;
    double m_minorRadius = 1.0;
    double m_startAngle = 0.0;
    double m_endAngle = 6.2831853071795864769;

    static double normalizedAngle(double angle);

public:
    AcGeEllipArc2d() = default;
    AcGeEllipArc2d(const AcGeCircArc2d &circle);
    AcGeEllipArc2d(const AcGePoint2d &center,
                   const AcGeVector2d &majorAxis,
                   const AcGeVector2d &minorAxis, double majorRadius,
                   double minorRadius);
    AcGeEllipArc2d(const AcGePoint2d &center,
                   const AcGeVector2d &majorAxis,
                   const AcGeVector2d &minorAxis, double majorRadius,
                   double minorRadius, double startAngle, double endAngle);

    AcGeEllipArc2d &set(const AcGePoint2d &center,
                        const AcGeVector2d &majorAxis,
                        const AcGeVector2d &minorAxis, double majorRadius,
                        double minorRadius);
    AcGeEllipArc2d &set(const AcGeCircArc2d &circle);

    AcGeEllipArc2d &setCenter(const AcGePoint2d &center)
    {
        m_center = center;
        return *this;
    }
    AcGeEllipArc2d &setAxes(const AcGeVector2d &majorAxis,
                            const AcGeVector2d &minorAxis);
    AcGeEllipArc2d &setMajorRadius(double radius)
    {
        m_majorRadius = std::abs(radius);
        return *this;
    }
    AcGeEllipArc2d &setMinorRadius(double radius)
    {
        m_minorRadius = std::abs(radius);
        return *this;
    }
    AcGeEllipArc2d &setAngles(double startAngle, double endAngle);
    AcGeEllipArc2d &setToComplement()
    {
        const double start = m_startAngle;
        m_startAngle = m_endAngle;
        m_endAngle = start + 6.2831853071795864769;
        return *this;
    }

    AcGePoint2d center() const { return m_center; }
    AcGeVector2d majorAxis() const { return m_majorAxis; }
    AcGeVector2d minorAxis() const { return m_minorAxis; }
    double majorRadius() const { return m_majorRadius; }
    double minorRadius() const { return m_minorRadius; }
    double startAng() const { return m_startAngle; }
    double endAng() const { return m_endAngle; }
    bool isClockWise() const
    {
        return m_majorAxis.x * m_minorAxis.y -
                   m_majorAxis.y * m_minorAxis.x <
               0.0;
    }
    bool isCircular(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(m_majorRadius - m_minorRadius) <=
               tol.equalVector();
    }

    AcGePoint2d pointAt(double angle) const
    {
        return m_center + m_majorAxis * (m_majorRadius * std::cos(angle)) +
               m_minorAxis * (m_minorRadius * std::sin(angle));
    }
    AcGePoint2d startPoint() const { return pointAt(m_startAngle); }
    AcGePoint2d endPoint() const { return pointAt(m_endAngle); }

    bool isInside(const AcGePoint2d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
};


// Value-semantic type-erased planar curve.  This mirrors the AcGeCurve3d
// facade and lets AcGi edge() accept ObjectARX-style AcGeCurve2d arrays
// without converting this value-geometry layer to a virtual hierarchy.
struct ACGE_PORT AcGeCurve2d
{
    std::variant<AcGeLineSeg2d, AcGeCircArc2d, AcGeEllipArc2d> curve;

    AcGeCurve2d() = default;
    AcGeCurve2d(const AcGeLineSeg2d &value) : curve(value) {}
    AcGeCurve2d(const AcGeCircArc2d &value) : curve(value) {}
    AcGeCurve2d(const AcGeEllipArc2d &value) : curve(value) {}

    AcGePoint2d startPoint() const
    {
        return std::visit(
            [](const auto &value) { return value.startPoint(); }, curve);
    }
    AcGePoint2d endPoint() const
    {
        return std::visit(
            [](const auto &value) { return value.endPoint(); }, curve);
    }
    AcGePoint2d pointAt(double parameter) const
    {
        return std::visit(
            [parameter](const auto &value) {
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>,
                                             AcGeCircArc2d>)
                {
                    const double sweep = value.sweep() *
                                         (value.isClockWise() ? -1.0 : 1.0);
                    return value.pointAtAngle(value.startAng() + sweep *
                                                                  parameter);
                }
                else
                {
                    return value.pointAt(parameter);
                }
            },
            curve);
    }
    double radius() const
    {
        return std::visit(
            [](const auto &value) -> double {
                using Curve = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Curve, AcGeLineSeg2d>)
                    return 0.0;
                else if constexpr (std::is_same_v<Curve, AcGeCircArc2d>)
                    return value.radius();
                else
                    return value.majorRadius();
            },
            curve);
    }
};

using AcGeEllip2d = AcGeEllipArc2d;
using AcGeEllipse2d = AcGeEllipArc2d;

namespace ge
{

inline constexpr double kGeom2dTolerance = 1.0e-12;

using ::AcGeLineSeg2d;
using ::AcGeEllipArc2d;

// ---- AcGeCircle2d ----

struct ACGE_PORT AcGeCircle2d
{
    AcGePoint2d center{0.0, 0.0};
    double radius = 1.0;

    bool contains(const AcGePoint2d &p) const;
};

using ::AcGeCircArc2d;

// ---- intersections ----

// Line-line: exact collinearity through the robust predicate; returns
// 0 (parallel/collinear-distinct), 1 (point), or 2 (collinear-overlap
// reported as its two extreme points).
std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &a,
                                   const AcGeLineSeg2d &b);

// Line-circle: 0, 1 (tangent), or 2 points.
std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
                                   const AcGeCircle2d &circle);
std::vector<AcGePoint2d> intersect(const AcGeCircle2d &circle,
                                   const AcGeLineSeg2d &line);

// Circle-circle: 0, 1 (tangent), or 2 points.
std::vector<AcGePoint2d> intersect(const AcGeCircle2d &c1,
                                   const AcGeCircle2d &c2);

// Segment-arc: circle-segment candidates filtered by the arc span.
std::vector<AcGePoint2d> intersect(const AcGeLineSeg2d &line,
                                   const AcGeCircArc2d &arc);
std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &arc,
                                   const AcGeLineSeg2d &line);

// Arc-arc: circle-circle candidates filtered by both arc spans.
std::vector<AcGePoint2d> intersect(const AcGeCircArc2d &a,
                                   const AcGeCircArc2d &b);

} // namespace ge
