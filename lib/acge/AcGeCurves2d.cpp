#include "acge/AcGeCurves2d.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ge
{
namespace detail
{

std::vector<AcGePoint3d> liftPoints(const std::vector<AcGePoint2d> &points)
{
    std::vector<AcGePoint3d> lifted;
    lifted.reserve(points.size());
    for (const AcGePoint2d &point : points)
        lifted.emplace_back(point.x, point.y, 0.0);
    return lifted;
}

} // namespace detail

AcGeBezierCurve2d::AcGeBezierCurve2d(std::vector<AcGePoint2d> points)
    : controlPoints(std::move(points))
{
    if (controlPoints.empty())
        throw std::invalid_argument("Bezier curve needs at least one point.");
}

AcGeBsplineCurve2d::AcGeBsplineCurve2d(std::vector<double> knotVector,
                                       std::vector<AcGePoint2d> points,
                                       size_t curveDegree)
    : degree(curveDegree), knots(std::move(knotVector)),
      controlPoints(std::move(points))
{
    validate();
}

void AcGeBsplineCurve2d::validate() const
{
    (void)as3d();
}

AcGeBsplineCurve2d AcGeBsplineCurve2d::differentiated() const
{
    const AcGeBsplineCurve3d derivative = as3d().differentiated();
    return {derivative.knots, projectPoints(derivative.controlPoints),
            derivative.degree};
}

std::vector<AcGeVector2d> AcGeBsplineCurve2d::derivativesAt(
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

AcGeBsplineCurve2d AcGeBsplineCurve2d::interpolate(
    const std::vector<AcGePoint2d> &fitPoints, size_t curveDegree, double alpha)
{
    const AcGeBsplineCurve3d curve =
        AcGeBsplineCurve3d::interpolate(detail::liftPoints(fitPoints),
                                        curveDegree, alpha);
    return {curve.knots, projectPoints(curve.controlPoints), curve.degree};
}

AcGeBsplineCurve3d AcGeBsplineCurve2d::as3d() const
{
    return {knots, detail::liftPoints(controlPoints), degree};
}

std::vector<AcGePoint2d> AcGeBsplineCurve2d::projectPoints(
    const std::vector<AcGePoint3d> &points)
{
    std::vector<AcGePoint2d> projected;
    projected.reserve(points.size());
    for (const AcGePoint3d &point : points)
        projected.push_back(detail::projectPoint(point));
    return projected;
}

AcGeNurbsCurve2d::AcGeNurbsCurve2d(std::vector<double> knotVector,
                                   std::vector<AcGePoint4d> points,
                                   size_t curveDegree)
    : degree(curveDegree), knots(std::move(knotVector)),
      controlPoints(std::move(points))
{
    validate();
}

void AcGeNurbsCurve2d::validate() const
{
    (void)as3d();
}

std::vector<AcGeVector2d> AcGeNurbsCurve2d::derivativesAt(
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

double AcGeNurbsCurve2d::curvatureAt(double parameter) const
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

AcGeNurbsCurve3d AcGeNurbsCurve2d::as3d() const
{
    std::vector<AcGePoint4d> lifted;
    lifted.reserve(controlPoints.size());
    for (const AcGePoint4d &point : controlPoints)
        lifted.emplace_back(point.x, point.y, 0.0, point.w);
    return {knots, std::move(lifted), degree};
}

AcGeFitSpline2d AcGeFitSpline2d::fromChordLength(
    const std::vector<AcGePoint2d> &points, bool periodic)
{
    if (points.size() < 2)
        throw std::invalid_argument(
            "Fit-point spline needs at least two points.");
    return AcGeFitSpline2d(
        AcGeFitSpline3d::fromChordLength(detail::liftPoints(points),
                                         periodic));
}

} // namespace ge
