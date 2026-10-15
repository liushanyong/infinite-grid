#pragma once

#include "AcGePort.h"

// AcGe-compatible 3D curve value types: line segment, circular arc, and
// ellipse.  Parameterized exactly like the ObjectARX classes (angles in
// radians measured in the plane's basis) so entity tessellation and AcGi
// draw callbacks can exchange them directly.

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "AcGeMatrix.h"
#include "AcGePoint.h"

struct ACGE_PORT AcGeLineSeg3d
{
private:
    AcGePoint3d m_startPoint{0.0, 0.0, 0.0};
    AcGePoint3d m_endPoint{1.0, 0.0, 0.0};

public:
    AcGeLineSeg3d() = default;
    AcGeLineSeg3d(const AcGePoint3d &start, const AcGePoint3d &end);
    AcGeLineSeg3d(const AcGePoint3d &start, const AcGeVector3d &direction);

    AcGeLineSeg3d &set(const AcGePoint3d &start, const AcGePoint3d &end);
    AcGeLineSeg3d &set(const AcGePoint3d &start,
                       const AcGeVector3d &direction);

    AcGePoint3d startPoint() const { return m_startPoint; }
    AcGePoint3d endPoint() const { return m_endPoint; }
    AcGeVector3d direction() const { return m_endPoint - m_startPoint; }
    double parameterOf(const AcGePoint3d &point) const;

    double length() const { return m_startPoint.distanceTo(m_endPoint); }
    double length(double fromParam, double toParam,
                  double tol = AcGeTol{}.equalPoint()) const;
    AcGePoint3d midPoint() const { return baryComb(0.5); }
    AcGePoint3d baryComb(double blendCoeff) const
    {
        return m_startPoint + direction() * blendCoeff;
    }

    AcGePoint3d pointAt(double t) const
    {
        return baryComb(t);
    }
    AcGePoint3d closestPointTo(
        const AcGePoint3d &point,
        const AcGeTol & = AcGeTol{}) const
    {
        return pointAt(std::clamp(parameterOf(point), 0.0, 1.0));
    }
    double distanceTo(const AcGePoint3d &point,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        return closestPointTo(point, tol).distanceTo(point);
    }
    bool isOn(double parameter, const AcGeTol &tol = AcGeTol{}) const;
    bool isOn(const AcGePoint3d &point, double &parameter,
              const AcGeTol &tol = AcGeTol{}) const;

    void getBisector(AcGePlane &plane) const
    {
        plane.set(midPoint(), direction());
    }
};

struct ACGE_PORT AcGeCircArc3d
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
    AcGePoint3d pointAt(double angle) const;

    // Stable in-plane reference vector perpendicular to the normal.
    AcGeVector3d referenceAxis() const;

    AcGePoint3d startPoint() const { return pointAt(startAngle); }
    AcGePoint3d endPoint() const { return pointAt(endAngle); }

    void getPlane(AcGePlane &plane) const
    {
        plane.set(center, normal);
    }

    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const;

    bool isFullCircle() const
    {
        return endAngle - startAngle >= 6.2831853071795864769 - 1.0e-12;
    }
};

struct ACGE_PORT AcGeCircle3d
{
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double radius = 1.0;

    AcGeCircle3d() = default;
    AcGeCircle3d(const AcGePoint3d &circleCenter,
                 const AcGeVector3d &circleNormal, double circleRadius)
        : center(circleCenter), normal(circleNormal),
          radius(std::abs(circleRadius))
    {
    }

    AcGeCircle3d &set(const AcGePoint3d &circleCenter,
                      const AcGeVector3d &circleNormal, double circleRadius)
    {
        center = circleCenter;
        normal = circleNormal;
        radius = std::abs(circleRadius);
        return *this;
    }
    AcGeCircle3d &setCenter(const AcGePoint3d &circleCenter)
    {
        center = circleCenter;
        return *this;
    }
    AcGeCircle3d &setNormal(const AcGeVector3d &circleNormal)
    {
        normal = circleNormal;
        return *this;
    }
    AcGeCircle3d &setRadius(double circleRadius)
    {
        radius = std::abs(circleRadius);
        return *this;
    }

    AcGePoint3d pointAt(double angle) const
    {
        return AcGeCircArc3d{center, normal, radius}.pointAt(angle);
    }
    AcGeVector3d referenceAxis() const
    {
        return AcGeCircArc3d{center, normal, radius}.referenceAxis();
    }
    AcGePoint3d closestPointTo(const AcGePoint3d &point) const;

    void getPlane(AcGePlane &plane) const
    {
        plane.set(center, normal);
    }
    double circumference() const
    {
        return 6.2831853071795864769 * radius;
    }
    double area() const
    {
        return 3.14159265358979323846 * radius * radius;
    }
    bool isOn(const AcGePoint3d &point,
              const AcGeTol &tol = AcGeTol{}) const;
    bool contains(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
};

struct ACGE_PORT AcGeEllipArc3d
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
    AcGeEllipArc3d &setMajorRadius(double radius);
    AcGeEllipArc3d &setMinorRadius(double radius);
    AcGeEllipArc3d &setAxes(const AcGeVector3d &newMajorAxis,
                            const AcGeVector3d &newMinorAxis);

    void getPlane(AcGePlane &plane) const
    {
        plane.set(center, normal);
    }

    bool isCircular(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(majorRadius() - minorRadius()) <= tol.equalVector();
    }

    AcGePoint3d pointAt(double angle) const;

    AcGePoint3d startPoint() const { return pointAt(startAngle); }
    AcGePoint3d endPoint() const { return pointAt(endAngle); }

    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
};

using AcGeEllip3d = AcGeEllipArc3d;
using AcGeEllipse3d = AcGeEllipArc3d;
