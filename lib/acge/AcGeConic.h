#pragma once

#include "AcGePort.h"

#include <cmath>
#include <stdexcept>

#include "AcGePoint.h"

namespace ge
{

class ACGE_PORT AcGeParabola2d
{
public:
    AcGePoint2d vertex{0.0, 0.0};
    AcGeVector2d axis{1.0, 0.0};
    double focalLength = 1.0;

    AcGeParabola2d() = default;
    AcGeParabola2d(const AcGePoint2d &parabolaVertex,
                   const AcGeVector2d &axisDirection, double focalParameter);

    AcGeParabola2d &setAxis(const AcGeVector2d &direction);
    AcGeVector2d sideAxis() const { return axis.perpVector(); }
    AcGePoint2d focus() const { return vertex + axis * focalLength; }
    AcGePoint2d pointAt(double parameter) const
    {
        return vertex + axis * (parameter * parameter /
                                (4.0 * focalLength)) +
               sideAxis() * parameter;
    }
    AcGeVector2d derivativeAt(double parameter) const
    {
        return axis * (parameter / (2.0 * focalLength)) + sideAxis();
    }
    double parameterOf(const AcGePoint2d &point) const
    {
        return (point - vertex).dotProduct(sideAxis());
    }
    bool isOn(const AcGePoint2d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return pointAt(parameterOf(point)).distanceTo(point) <= tol.equalPoint();
    }
};

class ACGE_PORT AcGeParabola3d
{
public:
    AcGePoint3d vertex{0.0, 0.0, 0.0};
    AcGeVector3d axis{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double focalLength = 1.0;

    AcGeParabola3d() = default;
    AcGeParabola3d(const AcGePoint3d &parabolaVertex,
                   const AcGeVector3d &axisDirection,
                   const AcGeVector3d &planeNormal, double focalParameter);

    AcGeParabola3d &setFrame(const AcGeVector3d &axisDirection,
                             const AcGeVector3d &planeNormal);
    AcGeVector3d sideAxis() const { return normal.crossProduct(axis).normal(); }
    AcGePoint3d focus() const { return vertex + axis * focalLength; }
    AcGePoint3d pointAt(double parameter) const
    {
        return vertex + axis * (parameter * parameter /
                                (4.0 * focalLength)) +
               sideAxis() * parameter;
    }
    AcGeVector3d derivativeAt(double parameter) const
    {
        return axis * (parameter / (2.0 * focalLength)) + sideAxis();
    }
    double parameterOf(const AcGePoint3d &point) const
    {
        return (point - vertex).dotProduct(sideAxis());
    }
    bool isOn(const AcGePoint3d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs((point - vertex).dotProduct(normal)) <=
                   tol.equalPoint() &&
               pointAt(parameterOf(point)).distanceTo(point) <= tol.equalPoint();
    }
};

class ACGE_PORT AcGeHyperbola2d
{
public:
    AcGePoint2d center{0.0, 0.0};
    AcGeVector2d majorAxis{1.0, 0.0};
    AcGeVector2d minorAxis{0.0, 1.0};
    double majorRadius = 1.0;
    double minorRadius = 1.0;
    int branch = 1;

    AcGeHyperbola2d() = default;
    AcGeHyperbola2d(const AcGePoint2d &hyperbolaCenter,
                    const AcGeVector2d &majorDirection,
                    const AcGeVector2d &minorDirection,
                    double semiMajorRadius, double semiMinorRadius,
                    int branchSign = 1);

    AcGeHyperbola2d &setAxes(const AcGeVector2d &majorDirection,
                             const AcGeVector2d &minorDirection);
    AcGePoint2d pointAt(double parameter) const
    {
        return center + majorAxis * (branch * majorRadius * std::cosh(parameter)) +
               minorAxis * (minorRadius * std::sinh(parameter));
    }
    double parameterOf(const AcGePoint2d &point) const
    {
        return std::asinh((point - center).dotProduct(minorAxis) / minorRadius);
    }
    bool isOn(const AcGePoint2d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return pointAt(parameterOf(point)).distanceTo(point) <= tol.equalPoint();
    }
    double focalLength() const
    {
        return std::sqrt(majorRadius * majorRadius +
                         minorRadius * minorRadius);
    }
};

class ACGE_PORT AcGeHyperbola3d
{
public:
    AcGePoint3d center{0.0, 0.0, 0.0};
    AcGeVector3d majorAxis{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double majorRadius = 1.0;
    double minorRadius = 1.0;
    int branch = 1;

    AcGeHyperbola3d() = default;
    AcGeHyperbola3d(const AcGePoint3d &hyperbolaCenter,
                    const AcGeVector3d &majorDirection,
                    const AcGeVector3d &planeNormal,
                    double semiMajorRadius, double semiMinorRadius,
                    int branchSign = 1);

    AcGeHyperbola3d &setFrame(const AcGeVector3d &majorDirection,
                              const AcGeVector3d &planeNormal);
    AcGeVector3d minorAxis() const
    {
        return normal.crossProduct(majorAxis).normal();
    }
    AcGePoint3d pointAt(double parameter) const
    {
        return center +
               majorAxis * (branch * majorRadius * std::cosh(parameter)) +
               minorAxis() * (minorRadius * std::sinh(parameter));
    }
    double parameterOf(const AcGePoint3d &point) const
    {
        return std::asinh((point - center).dotProduct(minorAxis()) /
                          minorRadius);
    }
    bool isOn(const AcGePoint3d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs((point - center).dotProduct(normal)) <=
                   tol.equalPoint() &&
               pointAt(parameterOf(point)).distanceTo(point) <= tol.equalPoint();
    }
    double focalLength() const
    {
        return std::sqrt(majorRadius * majorRadius +
                         minorRadius * minorRadius);
    }
};

} // namespace ge
