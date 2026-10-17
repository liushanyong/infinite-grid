#pragma once

#include "AcGePort.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGeBspline.h"
#include "AcGeCurve.h"
#include "AcGeNurbsSpline.h"

namespace ge
{
namespace detail
{

std::vector<AcGePoint3d> liftPoints(const std::vector<AcGePoint2d> &points);

inline AcGePoint2d projectPoint(const AcGePoint3d &point)
{
    return {point.x, point.y};
}

inline AcGeVector2d projectVector(const AcGeVector3d &vector)
{
    return {vector.x, vector.y};
}

} // namespace detail

class ACGE_PORT AcGeBezierCurve2d : public AcGeCurve2d
{
public:
    std::vector<AcGePoint2d> controlPoints;

    AcGeBezierCurve2d() = default;
    explicit AcGeBezierCurve2d(std::vector<AcGePoint2d> points);

    size_t degree() const
    {
        return controlPoints.empty() ? 0 : controlPoints.size() - 1;
    }
    double startParam() const override { return 0.0; }
    double endParam() const override { return 1.0; }
    AcGePoint2d pointAt(double parameter) const
    {
        return detail::projectPoint(
            AcGeBezierCurve3d(detail::liftPoints(controlPoints))
                .pointAt(parameter));
    }
    AcGeVector2d derivativeAt(double parameter) const
    {
        return detail::projectVector(
            AcGeBezierCurve3d(detail::liftPoints(controlPoints))
                .derivativeAt(parameter));
    }
};

class ACGE_PORT AcGeBsplineCurve2d : public AcGeCurve2d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint2d> controlPoints;

    AcGeBsplineCurve2d() = default;
    AcGeBsplineCurve2d(std::vector<double> knotVector,
                       std::vector<AcGePoint2d> points, size_t curveDegree);

    void validate() const;
    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }
    AcGePoint2d pointAt(double parameter) const override
    {
        return detail::projectPoint(as3d().pointAt(parameter));
    }
    AcGeBsplineCurve2d differentiated() const;
    std::vector<AcGeVector2d> derivativesAt(
        double parameter, size_t derivativeCount) const;
    static AcGeBsplineCurve2d interpolate(
        const std::vector<AcGePoint2d> &fitPoints, size_t curveDegree,
        double alpha = 0.0);

private:
    AcGeBsplineCurve3d as3d() const;
    static std::vector<AcGePoint2d> projectPoints(
        const std::vector<AcGePoint3d> &points);
};

class ACGE_PORT AcGeNurbsCurve2d : public AcGeCurve2d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint4d> controlPoints;

    AcGeNurbsCurve2d() = default;
    AcGeNurbsCurve2d(std::vector<double> knotVector,
                     std::vector<AcGePoint4d> points, size_t curveDegree);

    void validate() const;
    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }
    AcGePoint2d pointAt(double parameter) const override
    {
        return detail::projectPoint(as3d().pointAt(parameter));
    }
    std::vector<AcGeVector2d> derivativesAt(
        double parameter, size_t derivativeCount) const;
    double curvatureAt(double parameter) const;

private:
    AcGeNurbsCurve3d as3d() const;
};

class ACGE_PORT AcGeFitSpline2d : public AcGeCurve2d
{
public:
    AcGeFitSpline2d() = default;
    AcGeFitSpline2d(const std::vector<double> &parameters,
                    const std::vector<AcGePoint2d> &points,
                    bool periodic = false)
        : curve_(parameters, detail::liftPoints(points), periodic)
    {
    }

    static AcGeFitSpline2d fromChordLength(
        const std::vector<AcGePoint2d> &points, bool periodic = false);

    bool isEmpty() const { return curve_.isEmpty(); }
    double minParameter() const { return curve_.minParameter(); }
    double maxParameter() const { return curve_.maxParameter(); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }
    AcGePoint2d pointAt(double parameter) const override
    {
        return detail::projectPoint(curve_.pointAt(parameter));
    }
    AcGeVector2d gradientAt(double parameter) const
    {
        return detail::projectVector(curve_.gradientAt(parameter));
    }
    double arcLength(double from, double to) const
    {
        return curve_.arcLength(from, to);
    }
    double arcLength() const { return curve_.arcLength(); }

private:
    explicit AcGeFitSpline2d(AcGeFitSpline3d curve)
        : curve_(std::move(curve))
    {
    }

    AcGeFitSpline3d curve_;
};

} // namespace ge
