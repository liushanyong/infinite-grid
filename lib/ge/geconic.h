#pragma once

#include <cmath>
#include <stdexcept>

#include "gepoint.h"

namespace ge
{

class AcGeParabola2d
{
public:
    AcGePoint2d vertex{0.0, 0.0};
    AcGeVector2d axis{1.0, 0.0};
    double focalLength = 1.0;

    AcGeParabola2d() = default;
    AcGeParabola2d(const AcGePoint2d &parabolaVertex,
                   const AcGeVector2d &axisDirection, double focalParameter)
        : vertex(parabolaVertex), focalLength(std::abs(focalParameter))
    {
        setAxis(axisDirection);
        if (focalLength == 0.0)
            throw std::invalid_argument("Parabola focal length must be nonzero.");
    }

    AcGeParabola2d &setAxis(const AcGeVector2d &direction)
    {
        const AcGeVector2d unit = direction.normal();
        if (unit.isZeroLength())
            throw std::invalid_argument("Parabola axis must be nonzero.");
        axis = unit;
        return *this;
    }
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

class AcGeParabola3d
{
public:
    AcGePoint3d vertex{0.0, 0.0, 0.0};
    AcGeVector3d axis{1.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};
    double focalLength = 1.0;

    AcGeParabola3d() = default;
    AcGeParabola3d(const AcGePoint3d &parabolaVertex,
                   const AcGeVector3d &axisDirection,
                   const AcGeVector3d &planeNormal, double focalParameter)
        : vertex(parabolaVertex), focalLength(std::abs(focalParameter))
    {
        setFrame(axisDirection, planeNormal);
        if (focalLength == 0.0)
            throw std::invalid_argument("Parabola focal length must be nonzero.");
    }

    AcGeParabola3d &setFrame(const AcGeVector3d &axisDirection,
                             const AcGeVector3d &planeNormal)
    {
        normal = planeNormal.normal();
        if (normal.isZeroLength())
            throw std::invalid_argument("Parabola plane normal must be nonzero.");
        axis = axisDirection - normal * axisDirection.dotProduct(normal);
        axis.normalize();
        if (axis.isZeroLength())
            throw std::invalid_argument("Parabola axis must lie in its plane.");
        return *this;
    }
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

class AcGeHyperbola2d
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
                    int branchSign = 1)
        : center(hyperbolaCenter), majorRadius(std::abs(semiMajorRadius)),
          minorRadius(std::abs(semiMinorRadius)), branch(branchSign < 0 ? -1 : 1)
    {
        setAxes(majorDirection, minorDirection);
        if (majorRadius == 0.0 || minorRadius == 0.0)
            throw std::invalid_argument("Hyperbola radii must be nonzero.");
    }

    AcGeHyperbola2d &setAxes(const AcGeVector2d &majorDirection,
                             const AcGeVector2d &minorDirection)
    {
        majorAxis = majorDirection.normal();
        if (majorAxis.isZeroLength())
            throw std::invalid_argument("Hyperbola major axis must be nonzero.");
        minorAxis = minorDirection -
                    majorAxis * minorDirection.dotProduct(majorAxis);
        minorAxis.normalize();
        if (minorAxis.isZeroLength())
            throw std::invalid_argument("Hyperbola axes must not be parallel.");
        return *this;
    }
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

class AcGeHyperbola3d
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
                    int branchSign = 1)
        : center(hyperbolaCenter), majorRadius(std::abs(semiMajorRadius)),
          minorRadius(std::abs(semiMinorRadius)), branch(branchSign < 0 ? -1 : 1)
    {
        setFrame(majorDirection, planeNormal);
        if (majorRadius == 0.0 || minorRadius == 0.0)
            throw std::invalid_argument("Hyperbola radii must be nonzero.");
    }

    AcGeHyperbola3d &setFrame(const AcGeVector3d &majorDirection,
                              const AcGeVector3d &planeNormal)
    {
        normal = planeNormal.normal();
        if (normal.isZeroLength())
            throw std::invalid_argument("Hyperbola plane normal must be nonzero.");
        majorAxis =
            majorDirection - normal * majorDirection.dotProduct(normal);
        majorAxis.normalize();
        if (majorAxis.isZeroLength())
            throw std::invalid_argument("Hyperbola major axis must lie in its plane.");
        return *this;
    }
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
