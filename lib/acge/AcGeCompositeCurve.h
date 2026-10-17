#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "AcGeCurve.h"
#include "AcGeInterval.h"
#include "AcGePoint.h"

// Original source-level subset using the common AcGeCurve evaluation interface.
// Each segment owns a copy of a curve and an explicit, strictly increasing,
// finite local domain. Global [i, i+1] maps linearly to segment i's domain.
// Interior integer parameters select the RIGHT segment; the final parameter
// selects the last segment's upper endpoint. There is no extrapolation or
// tolerance-based parameter clamping.
//
// Curves must provide const pointAt(double), or const pointAtAngle(double)
// (e.g. AcGeCircArc2d). Angular domains are increasing angles, not implicit
// clockwise traversal. The caller must choose a domain supported by the curve;
// this subset does not infer trims, validate arbitrary curve internals, or
// implement derivatives, length, inversion, or nearest-point approximation.
namespace ge
{
namespace detail
{

template <typename Point>
class CompositeCurve
    : public std::conditional_t<std::is_same_v<Point, AcGePoint2d>,
                                AcGeCurve2d, AcGeCurve3d>
{
    static Point checkedPoint(const Point &point)
    {
        bool finite = std::isfinite(point.x) && std::isfinite(point.y);
        if constexpr (requires { point.z; })
            finite = finite && std::isfinite(point.z);
        if (!finite)
            throw std::domain_error("Composite curve evaluated to a non-finite point.");
        return point;
    }

    static bool coincident(const Point &a, const Point &b, double tolerance)
    {
        if constexpr (requires { a.z; })
            return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) <= tolerance;
        else
            return std::hypot(a.x - b.x, a.y - b.y) <= tolerance;
    }

    static double pointTolerance(const AcGeTol &tol)
    {
        const double tolerance = tol.equalPoint();
        if (!std::isfinite(tolerance) || tolerance < 0.0)
            throw std::invalid_argument("Composite curve tolerance must be finite and nonnegative.");
        return tolerance;
    }

public:
    class Segment
    {
        AcGeInterval m_domain;
        std::function<Point(double)> m_pointAt;

        void requireReady() const
        {
            if (!m_pointAt)
                throw std::logic_error("Composite curve segment was moved from.");
        }

        friend class CompositeCurve;

    public:
        template <typename Curve>
            requires std::copy_constructible<Curve> &&
                (requires(const Curve &curve, double parameter) {
                    { curve.pointAt(parameter) } -> std::same_as<Point>;
                } || requires(const Curve &curve, double parameter) {
                    { curve.pointAtAngle(parameter) } -> std::same_as<Point>;
                })
        Segment(Curve curve, const AcGeInterval &domain)
            : m_domain(domain)
        {
            if (!domain.isBounded() ||
                !std::isfinite(domain.lowerBound()) ||
                !std::isfinite(domain.upperBound()) ||
                !(domain.lowerBound() < domain.upperBound()) ||
                !std::isfinite(domain.tolerance()) || domain.tolerance() < 0.0)
                throw std::invalid_argument("Composite segment requires a finite, increasing domain.");

            // Existing spline value types expose these guards, but some of
            // their default-constructed pointAt implementations are unchecked.
            if constexpr (requires { curve.isEmpty(); })
            {
                if (curve.isEmpty())
                    throw std::invalid_argument("Composite segment curve is empty.");
            }
            if constexpr (requires { curve.controlPoints.empty(); })
            {
                if (curve.controlPoints.empty())
                    throw std::invalid_argument("Composite segment control points are empty.");
            }
            if constexpr (requires {
                curve.degree;
                curve.knots.size();
                curve.controlPoints.size();
            })
            {
                // Existing spline validate() does not check all dimensions
                // required by its unchecked span/basis evaluator. Subtract
                // only after ordering checks to avoid unsigned overflow.
                const auto count = curve.controlPoints.size();
                const auto degree = curve.degree;
                const auto knotCount = curve.knots.size();
                if (degree >= count || knotCount <= count ||
                    knotCount - count - 1 != degree)
                    throw std::invalid_argument("Composite spline has invalid dimensions.");
                for (double knot : curve.knots)
                    if (!std::isfinite(knot))
                        throw std::invalid_argument("Composite spline knots must be finite.");
                if (!std::is_sorted(curve.knots.begin(), curve.knots.end()) ||
                    !(curve.knots[degree] < curve.knots[count]))
                    throw std::invalid_argument("Composite spline has an invalid active domain.");
                if (domain.lowerBound() < curve.knots[degree] ||
                    domain.upperBound() > curve.knots[count])
                    throw std::invalid_argument("Composite spline domain exceeds its active domain.");
            }
            if constexpr (requires { curve.validate(); })
                curve.validate();

            m_pointAt = [curve = std::move(curve)](double parameter) -> Point {
                if constexpr (requires {
                    { curve.pointAt(parameter) } -> std::same_as<Point>;
                })
                    return curve.pointAt(parameter);
                else
                    return curve.pointAtAngle(parameter);
            };
            (void)startPoint();
            (void)endPoint();
        }

        Segment(const Segment &) = default;
        Segment(Segment &&) noexcept = default;
        Segment &operator=(Segment other) noexcept
        {
            swap(other);
            return *this;
        }
        void swap(Segment &other) noexcept
        {
            std::swap(m_domain, other.m_domain);
            m_pointAt.swap(other.m_pointAt);
        }

        AcGeInterval domain() const { requireReady(); return m_domain; }
        Point pointAt(double parameter) const
        {
            requireReady();
            if (!std::isfinite(parameter))
                throw std::invalid_argument("Composite local parameter must be finite.");
            if (parameter < m_domain.lowerBound() || parameter > m_domain.upperBound())
                throw std::out_of_range("Composite local parameter is outside its domain.");
            return checkedPoint(m_pointAt(parameter));
        }
        Point startPoint() const { return pointAt(m_domain.lowerBound()); }
        Point endPoint() const { return pointAt(m_domain.upperBound()); }
    };

private:
    std::vector<Segment> m_segments;

    void requireNonempty() const
    {
        if (isEmpty())
            throw std::logic_error("Composite curve is empty.");
    }

public:
    CompositeCurve() = default;
    explicit CompositeCurve(std::vector<Segment> segments)
    {
        set(std::move(segments));
    }
    CompositeCurve(std::initializer_list<Segment> segments)
        : CompositeCurve(std::vector<Segment>(segments))
    {
    }
    template <typename Curve>
        requires std::constructible_from<Segment, Curve, const AcGeInterval &>
    CompositeCurve(Curve curve, const AcGeInterval &domain)
    {
        addSegment(std::move(curve), domain);
    }
    CompositeCurve(const CompositeCurve &) = default;
    CompositeCurve(CompositeCurve &&) noexcept = default;
    CompositeCurve &operator=(CompositeCurve other) noexcept
    {
        swap(other);
        return *this;
    }
    void swap(CompositeCurve &other) noexcept { m_segments.swap(other.m_segments); }

    bool isEmpty() const noexcept { return m_segments.empty(); }
    std::size_t numSegments() const noexcept { return m_segments.size(); }
    void clear() noexcept { m_segments.clear(); }
    const Segment &segment(std::size_t index) const { return m_segments.at(index); }

    CompositeCurve &set(std::vector<Segment> segments)
    {
        for (const Segment &segment : segments)
            segment.requireReady();
        m_segments.swap(segments);
        return *this;
    }
    CompositeCurve &addSegment(Segment segment)
    {
        segment.requireReady();
        m_segments.push_back(std::move(segment));
        return *this;
    }
    template <typename Curve>
        requires std::constructible_from<Segment, Curve, const AcGeInterval &>
    CompositeCurve &addSegment(Curve curve, const AcGeInterval &domain)
    {
        return addSegment(Segment(std::move(curve), domain));
    }
    CompositeCurve &setSegment(std::size_t index, Segment segment)
    {
        segment.requireReady();
        m_segments.at(index).swap(segment);
        return *this;
    }
    template <typename Curve>
        requires std::constructible_from<Segment, Curve, const AcGeInterval &>
    CompositeCurve &setSegment(std::size_t index, Curve curve,
                               const AcGeInterval &domain)
    {
        return setSegment(index, Segment(std::move(curve), domain));
    }

    // Empty curves have no domain/endpoints/parameters (logic_error); their
    // connectivity and closedness predicates are false.
    double minParameter() const { requireNonempty(); return 0.0; }
    double maxParameter() const { requireNonempty(); return double(numSegments()); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }
    AcGeInterval domain() const { return {minParameter(), maxParameter(), 0.0}; }

    double globalToLocalParam(double parameter, std::size_t &segmentIndex) const
    {
        requireNonempty();
        if (!std::isfinite(parameter))
            throw std::invalid_argument("Composite global parameter must be finite.");
        if (parameter < 0.0 || parameter > maxParameter())
            throw std::out_of_range("Composite global parameter is outside its domain.");
        const std::size_t index = parameter == maxParameter()
            ? numSegments() - 1 : static_cast<std::size_t>(parameter);
        const AcGeInterval local = segment(index).domain();
        const double result = std::lerp(local.lowerBound(), local.upperBound(),
                                        parameter - double(index));
        segmentIndex = index;
        return result;
    }
    double localToGlobalParam(double parameter, std::size_t segmentIndex) const
    {
        requireNonempty();
        const AcGeInterval local = segment(segmentIndex).domain();
        if (!std::isfinite(parameter))
            throw std::invalid_argument("Composite local parameter must be finite.");
        const double lower = local.lowerBound();
        const double upper = local.upperBound();
        if (parameter < lower || parameter > upper)
            throw std::out_of_range("Composite local parameter is outside its domain.");
        // Halve only overflowing spans: retain precision for tiny domains.
        const double span = upper - lower;
        const double fraction = std::isfinite(span)
            ? (parameter - lower) / span
            : (parameter * 0.5 - lower * 0.5) / (upper * 0.5 - lower * 0.5);
        // A left segment's upper endpoint maps to the shared global integer;
        // evaluating that integer uses the right segment, even at a gap.
        return double(segmentIndex) + fraction;
    }
    Point pointAt(double parameter) const override
    {
        std::size_t index;
        const double local = globalToLocalParam(parameter, index);
        return segment(index).pointAt(local);
    }
    Point startPoint() const { requireNonempty(); return m_segments.front().startPoint(); }
    Point endPoint() const { requireNonempty(); return m_segments.back().endPoint(); }

    // C0 endpoint matching only: continuity inside each supplied curve is the
    // caller's responsibility. Closedness additionally requires all joins.
    bool isContinuous(const AcGeTol &tol = AcGeTol{}) const
    {
        const double tolerance = pointTolerance(tol);
        if (isEmpty())
            return false;
        for (std::size_t i = 1; i < numSegments(); ++i)
            if (!coincident(m_segments[i - 1].endPoint(),
                            m_segments[i].startPoint(), tolerance))
                return false;
        return true;
    }
    bool isClosed(const AcGeTol &tol = AcGeTol{}) const
    {
        return isContinuous(tol) &&
               coincident(startPoint(), endPoint(), pointTolerance(tol));
    }
};

} // namespace detail
} // namespace ge

using AcGeCompositeCurve2d = ge::detail::CompositeCurve<AcGePoint2d>;
using AcGeCompositeCurve3d = ge::detail::CompositeCurve<AcGePoint3d>;
