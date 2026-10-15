#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

#include "AcGePoint.h"

// Original, bounded parametric offset value wrappers, not a full ObjectARX
// curve API. No tessellation, finite differences, trimming, or cusp removal.
namespace ge::detail
{
template <class Point, class Vector>
struct OffsetCurveData
{
    using PointFunction = std::function<Point(double)>;
    using TangentFunction = std::function<Vector(double)>;
    using Evaluator = std::function<std::pair<Point, Vector>(double, bool)>;

    Evaluator evaluate;
    double first;
    double last;
    double distance;

    OffsetCurveData(Evaluator evaluator, double from, double to, double offset)
        : evaluate(std::move(evaluator)), first(from), last(to), distance(offset)
    {
        validateDomain(from, to);
        validateDistance(offset);
    }

    static void validateDomain(double from, double to)
    {
        if (!std::isfinite(from) || !std::isfinite(to) || !(from < to))
            throw std::invalid_argument("Offset curve requires finite min < max.");
    }
    static void validateDistance(double distance)
    {
        if (!std::isfinite(distance))
            throw std::invalid_argument("Offset distance must be finite.");
    }
    std::pair<Point, Vector> sample(double parameter) const
    {
        if (!std::isfinite(parameter) || parameter < first || parameter > last)
            throw std::out_of_range("Offset curve parameter is outside its domain.");
        if (!evaluate)
            throw std::logic_error("Cannot evaluate a moved-from offset curve.");
        return evaluate(parameter, distance != 0.0);
    }

    static Evaluator callbacks(PointFunction point, TangentFunction tangent)
    {
        if (!point || !tangent)
            throw std::invalid_argument("Offset curve requires both callbacks.");
        return [point = std::move(point), tangent = std::move(tangent)](
                   double t, bool needsTangent) -> std::pair<Point, Vector> {
            const Point p = point(t);
            return {p, needsTangent ? tangent(t) : Vector{}};
        };
    }

    template <class Curve, class Tangent>
    static Evaluator source(Curve curve, Tangent tangent)
    {
        if constexpr (requires { curve.isEmpty(); })
            if (curve.isEmpty())
                throw std::invalid_argument("Offset source curve is empty.");
        if constexpr (requires { curve.controlPoints.empty(); })
            if (curve.controlPoints.empty())
                throw std::invalid_argument("Offset source has no control points.");
        if constexpr (requires {
                          curve.degree;
                          curve.knots.size();
                          curve.controlPoints.size();
                      })
        {
            // Existing spline validate() checks knot-count equality but not
            // the index bounds required by its unchecked span evaluator.
            const auto count = curve.controlPoints.size();
            const auto degree = curve.degree;
            if (degree >= count || curve.knots.size() <= count ||
                curve.knots.size() - count - 1 != degree)
                throw std::invalid_argument("Offset source spline has invalid dimensions.");
            for (double knot : curve.knots)
                if (!std::isfinite(knot))
                    throw std::invalid_argument("Offset source knots must be finite.");
            if (!std::is_sorted(curve.knots.begin(), curve.knots.end()) ||
                !(curve.knots[degree] < curve.knots[count]))
                throw std::invalid_argument("Offset source spline has an invalid knot domain.");
        }
        if constexpr (requires { curve.validate(); })
            curve.validate();
        return [curve = std::move(curve), tangent = std::move(tangent)](
                   double t, bool needsTangent) -> std::pair<Point, Vector> {
            const Point p = curve.pointAt(t);
            return {p, needsTangent ? std::invoke(tangent, curve, t) : Vector{}};
        };
    }
};

template <class Value>
inline bool offsetCurveFinite(const Value &value)
{
    if constexpr (requires { value.z; })
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z);
    else
        return std::isfinite(value.x) && std::isfinite(value.y);
}

// Scale first: finite components near DBL_MAX or in the subnormal range
// must not overflow/underflow during normalization.
inline AcGeVector2d offsetCurveUnit(AcGeVector2d value)
{
    if (!offsetCurveFinite(value))
        throw std::domain_error("Offset tangent must be finite.");
    const double scale = std::max(std::abs(value.x), std::abs(value.y));
    if (scale == 0.0)
        throw std::domain_error("Offset tangent must be nonzero.");
    value = value / scale;
    return value / std::hypot(value.x, value.y);
}

inline AcGeVector3d offsetCurveUnit(AcGeVector3d value)
{
    if (!offsetCurveFinite(value))
        throw std::domain_error("Offset direction must be finite.");
    const double scale =
        std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
    if (scale == 0.0)
        throw std::domain_error("Offset direction must be nonzero.");
    value = value / scale;
    return value / std::hypot(value.x, value.y, value.z);
}

template <class Point>
inline Point offsetCurveCheckedPoint(Point point)
{
    if (!offsetCurveFinite(point))
        throw std::domain_error("Offset curve point is not finite.");
    return point;
}
} // namespace ge::detail

// Positive distance is left of the supplied increasing-parameter tangent:
// P(t) + distance * (-Ty, Tx) / |T|. Tangents must be oriented derivatives
// (positive rescalings are fine). The explicit domain is closed, finite and
// strictly increasing; it must be valid for the source, with no extrapolation.
//
// Curve overloads capture a value snapshot, calling const pointAt/derivativeAt.
// The explicit tangent overload receives that same owned const curve.
// Callback overloads own their callable copies, NOT objects referenced by them;
// callers must avoid dangling references and supply consistent pure callbacks.
//
// Typed construction also rejects detectable empty/invalid source state and
// invokes validate() when available. It does not sample the entire source.
// Construction/setters validate configuration, not all source parameters.
// Evaluation propagates source exceptions; nonfinite points/tangents, zero
// tangents and arithmetic overflow throw domain_error. At distance zero only
// the source point is evaluated, even at a stationary point. Bad parameters
// throw out_of_range. Setters have the strong guarantee and invoke no callbacks.
class AcGeOffsetCurve2d
{
    using Data = ge::detail::OffsetCurveData<AcGePoint2d, AcGeVector2d>;
    Data data_;

public:
    using PointFunction = Data::PointFunction;
    using TangentFunction = Data::TangentFunction;

    AcGeOffsetCurve2d(PointFunction point, TangentFunction tangent,
                      double from, double to, double distance)
        : data_(Data::callbacks(std::move(point), std::move(tangent)),
                from, to, distance)
    {
    }

    template <class Curve>
        requires std::copy_constructible<Curve> &&
                 requires(const Curve &curve, double t) {
                     { curve.pointAt(t) } -> std::convertible_to<AcGePoint2d>;
                     { curve.derivativeAt(t) } -> std::convertible_to<AcGeVector2d>;
                 }
    AcGeOffsetCurve2d(Curve curve, double from, double to, double distance)
        : data_(Data::source(std::move(curve),
                            [](const Curve &source, double t) {
                                return source.derivativeAt(t);
                            }),
                from, to, distance)
    {
    }

    template <class Curve, class Tangent>
        requires std::copy_constructible<Curve> &&
                 std::copy_constructible<Tangent> &&
                 requires(const Curve &curve, const Tangent &tangent, double t) {
                     { curve.pointAt(t) } -> std::convertible_to<AcGePoint2d>;
                     { std::invoke(tangent, curve, t) } ->
                         std::convertible_to<AcGeVector2d>;
                 }
    AcGeOffsetCurve2d(Curve curve, Tangent tangent,
                      double from, double to, double distance)
        : data_(Data::source(std::move(curve), std::move(tangent)),
                from, to, distance)
    {
    }

    double minParameter() const noexcept { return data_.first; }
    double maxParameter() const noexcept { return data_.last; }
    double distance() const noexcept { return data_.distance; }
    AcGeOffsetCurve2d &setDistance(double distance)
    {
        Data::validateDistance(distance);
        data_.distance = distance;
        return *this;
    }
    AcGeOffsetCurve2d &setDomain(double from, double to)
    {
        Data::validateDomain(from, to);
        data_.first = from;
        data_.last = to;
        return *this;
    }
    AcGePoint2d pointAt(double parameter) const
    {
        const auto [point, tangent] = data_.sample(parameter);
        ge::detail::offsetCurveCheckedPoint(point);
        if (data_.distance == 0.0)
            return point;
        const auto unit = ge::detail::offsetCurveUnit(tangent);
        return ge::detail::offsetCurveCheckedPoint(
            point + AcGeVector2d{-unit.y, unit.x} * data_.distance);
    }
    AcGePoint2d startPoint() const { return pointAt(minParameter()); }
    AcGePoint2d endPoint() const { return pointAt(maxParameter()); }
};

// As above, with positive direction normalize(planeNormal x tangent).
// The normal is required, finite, nonzero, and stored normalized; reversing
// it reverses the offset side. Cross products with magnitude <= 64*epsilon
// after input normalization are rejected as parallel/ill-conditioned.
//
// For a true planar offset the caller must ensure the entire source lies in
// a plane perpendicular to this normal. Global planarity cannot be certified
// from arbitrary callbacks and is NOT tested or inferred by sampling. For
// nonplanar sources this API defines only the same pointwise displacement,
// perpendicular to both the tangent and normal; it does not project the
// source, use a Frenet frame, or promise a planar result. No plane origin is
// needed. Zero distance still requires a valid configured normal.
class AcGeOffsetCurve3d
{
    using Data = ge::detail::OffsetCurveData<AcGePoint3d, AcGeVector3d>;
    Data data_;
    AcGeVector3d normal_;

    static AcGeVector3d checkedNormal(AcGeVector3d normal)
    {
        if (!ge::detail::offsetCurveFinite(normal) ||
            (normal.x == 0.0 && normal.y == 0.0 && normal.z == 0.0))
            throw std::invalid_argument("Offset plane normal must be finite and nonzero.");
        return ge::detail::offsetCurveUnit(normal);
    }

public:
    using PointFunction = Data::PointFunction;
    using TangentFunction = Data::TangentFunction;

    AcGeOffsetCurve3d(PointFunction point, TangentFunction tangent,
                      double from, double to, double distance,
                      AcGeVector3d planeNormal)
        : data_(Data::callbacks(std::move(point), std::move(tangent)),
                from, to, distance),
          normal_(checkedNormal(planeNormal))
    {
    }

    template <class Curve>
        requires std::copy_constructible<Curve> &&
                 requires(const Curve &curve, double t) {
                     { curve.pointAt(t) } -> std::convertible_to<AcGePoint3d>;
                     { curve.derivativeAt(t) } -> std::convertible_to<AcGeVector3d>;
                 }
    AcGeOffsetCurve3d(Curve curve, double from, double to, double distance,
                      AcGeVector3d planeNormal)
        : data_(Data::source(std::move(curve),
                            [](const Curve &source, double t) {
                                return source.derivativeAt(t);
                            }),
                from, to, distance),
          normal_(checkedNormal(planeNormal))
    {
    }

    template <class Curve, class Tangent>
        requires std::copy_constructible<Curve> &&
                 std::copy_constructible<Tangent> &&
                 requires(const Curve &curve, const Tangent &tangent, double t) {
                     { curve.pointAt(t) } -> std::convertible_to<AcGePoint3d>;
                     { std::invoke(tangent, curve, t) } ->
                         std::convertible_to<AcGeVector3d>;
                 }
    AcGeOffsetCurve3d(Curve curve, Tangent tangent, double from, double to,
                      double distance, AcGeVector3d planeNormal)
        : data_(Data::source(std::move(curve), std::move(tangent)),
                from, to, distance),
          normal_(checkedNormal(planeNormal))
    {
    }

    double minParameter() const noexcept { return data_.first; }
    double maxParameter() const noexcept { return data_.last; }
    double distance() const noexcept { return data_.distance; }
    AcGeVector3d normal() const noexcept { return normal_; }
    AcGeOffsetCurve3d &setDistance(double distance)
    {
        Data::validateDistance(distance);
        data_.distance = distance;
        return *this;
    }
    AcGeOffsetCurve3d &setDomain(double from, double to)
    {
        Data::validateDomain(from, to);
        data_.first = from;
        data_.last = to;
        return *this;
    }
    AcGeOffsetCurve3d &setNormal(AcGeVector3d normal)
    {
        normal_ = checkedNormal(normal);
        return *this;
    }
    AcGePoint3d pointAt(double parameter) const
    {
        const auto [point, tangent] = data_.sample(parameter);
        ge::detail::offsetCurveCheckedPoint(point);
        if (data_.distance == 0.0)
            return point;
        const auto unit = ge::detail::offsetCurveUnit(tangent);
        const auto cross = normal_.crossProduct(unit);
        if (std::hypot(cross.x, cross.y, cross.z) <=
            64.0 * std::numeric_limits<double>::epsilon())
            throw std::domain_error("Offset tangent is parallel to the plane normal.");
        return ge::detail::offsetCurveCheckedPoint(
            point + ge::detail::offsetCurveUnit(cross) * data_.distance);
    }
    AcGePoint3d startPoint() const { return pointAt(minParameter()); }
    AcGePoint3d endPoint() const { return pointAt(maxParameter()); }
};
