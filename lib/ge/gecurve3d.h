#pragma once

// AcGe-compatible 3D curve value types: line segment, circular arc, and
// ellipse.  Parameterized exactly like the ObjectARX classes (angles in
// radians measured in the plane's basis) so entity tessellation and AcGi
// draw callbacks can exchange them directly.

#include <cmath>

#include <glm/glm.hpp>

#include "gepoint.h"

struct AcGeLineSeg3d
{
    AcGePoint3d startPoint{0.0, 0.0, 0.0};
    AcGePoint3d endPoint{1.0, 0.0, 0.0};

    AcGeLineSeg3d() = default;
    AcGeLineSeg3d(const AcGePoint3d &start, const AcGePoint3d &end)
        : startPoint(start), endPoint(end)
    {
    }

    double length() const { return (endPoint - startPoint).length(); }

    AcGePoint3d pointAt(double t) const
    {
        return startPoint + (endPoint - startPoint) * t;
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

    // Point on the arc at an angle parameter (radians, plane basis).
    AcGePoint3d pointAt(double angle) const
    {
        const AcGeVector3d u = referenceAxis();
        const AcGeVector3d w = normal.crossProduct(u);
        return center + u * (radius * std::cos(angle)) +
               w * (radius * std::sin(angle));
    }

    // Stable in-plane reference vector perpendicular to the normal.
    AcGeVector3d referenceAxis() const
    {
        AcGeVector3d axis = std::abs(normal.z) < 0.9
            ? AcGeVector3d(0.0, 0.0, 1.0)
            : AcGeVector3d(1.0, 0.0, 0.0);
        return (axis - normal * axis.dotProduct(normal)).normal();
    }

    bool isFullCircle() const
    {
        return endAngle - startAngle >= 6.2831853071795864769 - 1.0e-12;
    }
};

struct AcGeEllip3d
{
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d majorAxis{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double radiusRatio = 1.0;
    double startAngle = 0.0;
    double endAngle = 6.2831853071795864769;

    AcGeEllip3d() = default;
    AcGeEllip3d(const AcGePoint3d &ellipseCenter,
                const AcGeVector3d &ellipseMajorAxis,
                const AcGeVector3d &ellipseNormal, double ellipseRadiusRatio)
        : center(ellipseCenter), majorAxis(ellipseMajorAxis),
          normal(ellipseNormal), radiusRatio(ellipseRadiusRatio)
    {
    }

    AcGeEllip3d(const AcGePoint3d &ellipseCenter,
                const AcGeVector3d &ellipseMajorAxis,
                const AcGeVector3d &ellipseNormal, double ellipseRadiusRatio,
                double ellipseStartAngle, double ellipseEndAngle)
        : center(ellipseCenter), majorAxis(ellipseMajorAxis),
          normal(ellipseNormal), radiusRatio(ellipseRadiusRatio),
          startAngle(ellipseStartAngle), endAngle(ellipseEndAngle)
    {
    }

    double majorRadius() const { return majorAxis.length(); }

    AcGePoint3d pointAt(double angle) const
    {
        const AcGeVector3d u = majorAxis.normal();
        const AcGeVector3d w = normal.crossProduct(u);
        const double minorRadius = majorRadius() * radiusRatio;
        return center + u * (majorRadius() * std::cos(angle)) +
               w * (minorRadius * std::sin(angle));
    }
};
