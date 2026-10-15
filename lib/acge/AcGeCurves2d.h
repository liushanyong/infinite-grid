#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGeBspline.h"
#include "AcGeNurbsSpline.h"

namespace ge
{
namespace detail
{

inline std::vector<AcGePoint3d> liftPoints(
    const std::vector<AcGePoint2d> &points)
{
    std::vector<AcGePoint3d> lifted;
    lifted.reserve(points.size());
    for (const AcGePoint2d &point : points)
        lifted.emplace_back(point.x, point.y, 0.0);
    return lifted;
}

inline AcGePoint2d projectPoint(const AcGePoint3d &point)
{
    return {point.x, point.y};
}

inline AcGeVector2d projectVector(const AcGeVector3d &vector)
{
    return {vector.x, vector.y};
}

} // namespace detail

class AcGeBezierCurve2d
{
public:
    std::vector<AcGePoint2d> controlPoints;

    AcGeBezierCurve2d() = default;
    explicit AcGeBezierCurve2d(std::vector<AcGePoint2d> points)
        : controlPoints(std::move(points))
    {
        if (controlPoints.empty())
            throw std::invalid_argument("Bezier curve needs at least one point.");
    }

    size_t degree() const
    {
        return controlPoints.empty() ? 0 : controlPoints.size() - 1;
    }
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

class AcGeBsplineCurve2d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint2d> controlPoints;

    AcGeBsplineCurve2d() = default;
    AcGeBsplineCurve2d(std::vector<double> knotVector,
                       std::vector<AcGePoint2d> points, size_t curveDegree)
        : degree(curveDegree), knots(std::move(knotVector)),
          controlPoints(std::move(points))
    {
        validate();
    }

    void validate() const { (void)as3d(); }
    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }
    AcGePoint2d pointAt(double parameter) const
    {
        return detail::projectPoint(as3d().pointAt(parameter));
    }
    AcGeBsplineCurve2d differentiated() const
    {
        const AcGeBsplineCurve3d derivative = as3d().differentiated();
        return {derivative.knots, projectPoints(derivative.controlPoints),
                derivative.degree};
    }
    std::vector<AcGeVector2d> derivativesAt(
        double parameter, size_t derivativeCount) const
    {
        const auto derivatives = as3d().derivativesAt(parameter,
                                                       derivativeCount);
        std::vector<AcGeVector2d> result;
        result.reserve(derivatives.size());
        for (const AcGeVector3d &derivative : derivatives)
            result.push_back(detail::projectVector(derivative));
        return result;
    }
    static AcGeBsplineCurve2d interpolate(
        const std::vector<AcGePoint2d> &fitPoints, size_t curveDegree,
        double alpha = 0.0)
    {
        const AcGeBsplineCurve3d curve =
            AcGeBsplineCurve3d::interpolate(detail::liftPoints(fitPoints),
                                            curveDegree, alpha);
        return {curve.knots, projectPoints(curve.controlPoints), curve.degree};
    }

private:
    AcGeBsplineCurve3d as3d() const
    {
        return {knots, detail::liftPoints(controlPoints), degree};
    }
    static std::vector<AcGePoint2d> projectPoints(
        const std::vector<AcGePoint3d> &points)
    {
        std::vector<AcGePoint2d> projected;
        projected.reserve(points.size());
        for (const AcGePoint3d &point : points)
            projected.push_back(detail::projectPoint(point));
        return projected;
    }
};

class AcGeNurbsCurve2d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint4d> controlPoints;

    AcGeNurbsCurve2d() = default;
    AcGeNurbsCurve2d(std::vector<double> knotVector,
                     std::vector<AcGePoint4d> points, size_t curveDegree)
        : degree(curveDegree), knots(std::move(knotVector)),
          controlPoints(std::move(points))
    {
        validate();
    }

    void validate() const { (void)as3d(); }
    double minParameter() const { return knots.front(); }
    double maxParameter() const { return knots.back(); }
    AcGePoint2d pointAt(double parameter) const
    {
        return detail::projectPoint(as3d().pointAt(parameter));
    }
    std::vector<AcGeVector2d> derivativesAt(
        double parameter, size_t derivativeCount) const
    {
        const auto derivatives = as3d().derivativesAt(parameter,
                                                       derivativeCount);
        std::vector<AcGeVector2d> result;
        result.reserve(derivatives.size());
        for (const AcGeVector3d &derivative : derivatives)
            result.push_back(detail::projectVector(derivative));
        return result;
    }
    double curvatureAt(double parameter) const
    {
        const auto derivatives = derivativesAt(parameter, 2);
        const double speed = derivatives[1].length();
        if (speed < 1.0e-14)
            return 0.0;
        const double cross =
            derivatives[1].x * derivatives[2].y -
            derivatives[1].y * derivatives[2].x;
        return std::abs(cross) / (speed * speed * speed);
    }

private:
    AcGeNurbsCurve3d as3d() const
    {
        std::vector<AcGePoint4d> lifted;
        lifted.reserve(controlPoints.size());
        for (const AcGePoint4d &point : controlPoints)
            lifted.emplace_back(point.x, point.y, 0.0, point.w);
        return {knots, std::move(lifted), degree};
    }
};

class AcGeFitSpline2d
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
        const std::vector<AcGePoint2d> &points, bool periodic = false)
    {
        if (points.size() < 2)
            throw std::invalid_argument(
                "Fit-point spline needs at least two points.");
        return AcGeFitSpline2d(
            AcGeFitSpline3d::fromChordLength(detail::liftPoints(points),
                                             periodic));
    }

    bool isEmpty() const { return curve_.isEmpty(); }
    double minParameter() const { return curve_.minParameter(); }
    double maxParameter() const { return curve_.maxParameter(); }
    AcGePoint2d pointAt(double parameter) const
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
