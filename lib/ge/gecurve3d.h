#pragma once

// AcGe-compatible 3D curve value types: line segment, circular arc, and
// ellipse.  Parameterized exactly like the ObjectARX classes (angles in
// radians measured in the plane's basis) so entity tessellation and AcGi
// draw callbacks can exchange them directly.

#include <cmath>
#include <stdexcept>

#include <glm/glm.hpp>

#include "gematrix.h"
#include "gepoint.h"

struct AcGeLineSeg3d
{
private:
    AcGePoint3d m_startPoint{0.0, 0.0, 0.0};
    AcGePoint3d m_endPoint{1.0, 0.0, 0.0};

public:
    AcGeLineSeg3d() = default;
    AcGeLineSeg3d(const AcGePoint3d &start, const AcGePoint3d &end)
    {
        set(start, end);
    }

    AcGeLineSeg3d &set(const AcGePoint3d &start, const AcGePoint3d &end)
    {
        if (start == end)
            throw std::invalid_argument("AcGeLineSeg3d endpoints must differ.");
        m_startPoint = start;
        m_endPoint = end;
        return *this;
    }

    AcGePoint3d startPoint() const { return m_startPoint; }
    AcGePoint3d endPoint() const { return m_endPoint; }
    AcGeVector3d direction() const { return m_endPoint - m_startPoint; }

    double length() const { return m_startPoint.distanceTo(m_endPoint); }
    AcGePoint3d midPoint() const { return baryComb(0.5); }
    AcGePoint3d baryComb(double blendCoeff) const
    {
        return m_startPoint + direction() * blendCoeff;
    }

    AcGePoint3d pointAt(double t) const
    {
        return baryComb(t);
    }

    void getBisector(AcGePlane &plane) const
    {
        plane.set(midPoint(), direction());
    }
};

struct AcGeCircArc3d
{
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double radius = 1.0;
    double startAngle = 0.0;
    double endAngle = 6.2831853071795864769;

    AcGeCircArc3d() = default;
    AcGeCircArc3d(const AcGePoint3d &arcCenter, const AcGeVector3d &arcNormal,
                  double arcRadius)
        : center(arcCenter), normal(arcNormal), radius(arcRadius)
    {
    }
    AcGeCircArc3d(const AcGePoint3d &arcCenter, const AcGeVector3d &arcNormal,
                  double arcRadius, double arcStartAngle, double arcEndAngle)
        : center(arcCenter), normal(arcNormal), radius(arcRadius),
          startAngle(arcStartAngle), endAngle(arcEndAngle)
    {
    }

    AcGeCircArc3d &set(const AcGePoint3d &arcCenter,
                       const AcGeVector3d &arcNormal, double arcRadius,
                       double arcStartAngle, double arcEndAngle)
    {
        center = arcCenter;
        normal = arcNormal;
        radius = arcRadius;
        startAngle = arcStartAngle;
        endAngle = arcEndAngle;
        return *this;
    }
    AcGeCircArc3d &setCenter(const AcGePoint3d &arcCenter)
    {
        center = arcCenter;
        return *this;
    }
    AcGeCircArc3d &setRadius(double arcRadius)
    {
        radius = arcRadius;
        return *this;
    }
    AcGeCircArc3d &setAngles(double arcStartAngle, double arcEndAngle)
    {
        startAngle = arcStartAngle;
        endAngle = arcEndAngle;
        return *this;
    }

    // Point on the arc at an angle parameter (radians, plane basis).
    AcGePoint3d pointAt(double angle) const
    {
        const AcGeVector3d u = referenceAxis();
        const AcGeVector3d w = normal.normal().crossProduct(u);
        return center + u * (radius * std::cos(angle)) +
               w * (radius * std::sin(angle));
    }

    // Stable in-plane reference vector perpendicular to the normal.
    AcGeVector3d referenceAxis() const
    {
        const AcGeVector3d unitNormal = normal.normal();
        AcGeVector3d axis = std::abs(unitNormal.z) < 0.9
            ? AcGeVector3d(0.0, 0.0, 1.0)
            : AcGeVector3d(1.0, 0.0, 0.0);
        return (axis - unitNormal * axis.dotProduct(unitNormal)).normal();
    }

    AcGePoint3d startPoint() const { return pointAt(startAngle); }
    AcGePoint3d endPoint() const { return pointAt(endAngle); }

    void getPlane(AcGePlane &plane) const
    {
        plane.set(center, normal);
    }

    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const
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

    bool isFullCircle() const
    {
        return endAngle - startAngle >= 6.2831853071795864769 - 1.0e-12;
    }
};

struct AcGeEllipArc3d
{
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d majorAxis{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double radiusRatio = 1.0;
    double startAngle = 0.0;
    double endAngle = 6.2831853071795864769;

    AcGeEllipArc3d() = default;
    AcGeEllipArc3d(const AcGePoint3d &ellipseCenter,
                const AcGeVector3d &ellipseMajorAxis,
                const AcGeVector3d &ellipseNormal, double ellipseRadiusRatio)
        : center(ellipseCenter), majorAxis(ellipseMajorAxis),
          normal(ellipseNormal), radiusRatio(ellipseRadiusRatio)
    {
    }

    AcGeEllipArc3d(const AcGePoint3d &ellipseCenter,
                const AcGeVector3d &ellipseMajorAxis,
                const AcGeVector3d &ellipseNormal, double ellipseRadiusRatio,
                double ellipseStartAngle, double ellipseEndAngle)
        : center(ellipseCenter), majorAxis(ellipseMajorAxis),
          normal(ellipseNormal), radiusRatio(ellipseRadiusRatio),
          startAngle(ellipseStartAngle), endAngle(ellipseEndAngle)
    {
    }

    double majorRadius() const { return majorAxis.length(); }
    double minorRadius() const
    {
        return majorRadius() * std::abs(radiusRatio);
    }
    AcGeVector3d minorAxis() const
    {
        return normal.normal().crossProduct(majorAxis.normal()) * minorRadius();
    }

    AcGeEllipArc3d &setCenter(const AcGePoint3d &ellipseCenter)
    {
        center = ellipseCenter;
        return *this;
    }
    AcGeEllipArc3d &setAngles(double start, double end)
    {
        startAngle = start;
        endAngle = end;
        return *this;
    }
    AcGeEllipArc3d &setMajorRadius(double radius)
    {
        majorAxis = majorAxis.normal() * std::abs(radius);
        return *this;
    }
    AcGeEllipArc3d &setMinorRadius(double radius)
    {
        const double major = majorRadius();
        radiusRatio = major == 0.0 ? 0.0 : std::abs(radius) / major;
        return *this;
    }
    AcGeEllipArc3d &setAxes(const AcGeVector3d &newMajorAxis,
                            const AcGeVector3d &newMinorAxis)
    {
        majorAxis = newMajorAxis;
        normal = newMajorAxis.crossProduct(newMinorAxis).normal();
        const double major = majorRadius();
        radiusRatio = major == 0.0 ? 0.0 : newMinorAxis.length() / major;
        return *this;
    }

    void getPlane(AcGePlane &plane) const
    {
        plane.set(center, normal);
    }

    bool isCircular(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(majorRadius() - minorRadius()) <= tol.equalVector();
    }

    AcGePoint3d pointAt(double angle) const
    {
        const AcGeVector3d u = majorAxis.normal();
        const AcGeVector3d w = normal.normal().crossProduct(u);
        const double minor = minorRadius();
        return center + u * (majorRadius() * std::cos(angle)) +
               w * (minor * std::sin(angle));
    }

    AcGePoint3d startPoint() const { return pointAt(startAngle); }
    AcGePoint3d endPoint() const { return pointAt(endAngle); }

    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const
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
};

using AcGeEllip3d = AcGeEllipArc3d;
