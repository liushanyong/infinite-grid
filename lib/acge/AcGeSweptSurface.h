#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "AcGeInterval.h"
#include "AcGePoint.h"

// Original project extensions, not native ObjectARX surface API compatibility.
// All domains are finite, strictly increasing, and closed. Parameters outside
// them are rejected (no tolerance, clamping, extrapolation, or angle wrapping).
namespace acgeSweptDetail
{
inline void validateDomain(const AcGeInterval &domain)
{
    if (!domain.isBounded() || !std::isfinite(domain.lowerBound()) ||
        !std::isfinite(domain.upperBound()) ||
        !(domain.lowerBound() < domain.upperBound()) ||
        !std::isfinite(domain.length()) || !std::isfinite(domain.tolerance()))
        throw std::invalid_argument("Swept surface requires a finite bounded domain.");
}

inline void checkParameter(double value, const AcGeInterval &domain)
{
    if (!std::isfinite(value) || value < domain.lowerBound() ||
        value > domain.upperBound())
        throw std::out_of_range("Swept surface parameter is outside its domain.");
}

template <class T>
inline bool finite(const T &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

template <class T>
inline T checked(T value)
{
    if (!finite(value))
        throw std::domain_error("Swept surface evaluation produced a nonfinite result.");
    return value;
}

inline double scale(const AcGeVector3d &value)
{
    return std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
}

inline void validateVector(const AcGeVector3d &value)
{
    if (!finite(value) || scale(value) == 0.0)
        throw std::invalid_argument("Swept surface vector must be finite and nonzero.");
}

inline AcGeVector3d unit(const AcGeVector3d &value)
{
    const AcGeVector3d scaled = value / scale(value);
    return scaled / std::hypot(scaled.x, scaled.y, scaled.z);
}

inline AcGeVector3d normal(const AcGeVector3d &du, const AcGeVector3d &dv)
{
    if (scale(du) == 0.0 || scale(dv) == 0.0)
        throw std::domain_error("Swept surface normal is singular.");
    const AcGeVector3d cross = unit(du).crossProduct(unit(dv));
    // Relative angular criterion, independent of the parameter/geometry scale.
    const double length = std::hypot(cross.x, cross.y, cross.z);
    if (length <= 64.0 * std::numeric_limits<double>::epsilon())
        throw std::domain_error("Swept surface normal is singular or ill-conditioned.");
    return cross / length;
}

inline AcGeVector3d rotate(const AcGeVector3d &value,
                          const AcGeVector3d &axis, double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return checked(value * c + axis.crossProduct(value) * s +
                   axis * (axis.dotProduct(value) * (1.0 - c)));
}
}

// Owning, type-erased source curve. Value curves need only pointAt(double);
// derivativeAt(double), if present, is used analytically. Alternatively supply
// point and derivative callbacks. A derivative callback means dC/d(parameter),
// NOT a normalized tangent. There is deliberately no numerical fallback.
// Callbacks must be pure and own their captured data; copying a callback cannot
// deep-copy pointers/references that its author chose to capture.
class AcGeSweptCurve3d
{
public:
    using PointFunction = std::function<AcGePoint3d(double)>;
    using DerivativeFunction = std::function<AcGeVector3d(double)>;

private:
    PointFunction m_point;
    DerivativeFunction m_derivative;
    AcGeInterval m_domain;

    void validate() const
    {
        acgeSweptDetail::validateDomain(m_domain);
        if (!m_point)
            throw std::invalid_argument("Swept source point callback is empty.");
        // Catch invalid setup at representative points; arbitrary callbacks are
        // also checked on EVERY evaluation, not assumed finite between samples.
        for (double u : {m_domain.lowerBound(),
                         std::midpoint(m_domain.lowerBound(), m_domain.upperBound()),
                         m_domain.upperBound()})
        {
            pointAt(u);
            if (hasDerivative())
                derivativeAt(u);
        }
    }

public:
    AcGeSweptCurve3d(PointFunction point, const AcGeInterval &domain,
                     DerivativeFunction derivative = {})
        : m_point(std::move(point)), m_derivative(std::move(derivative)),
          m_domain(domain)
    {
        validate();
    }

    template <class Curve>
        requires requires(const Curve &curve, double u) {
            { curve.pointAt(u) } -> std::convertible_to<AcGePoint3d>;
        }
    AcGeSweptCurve3d(Curve curve, const AcGeInterval &domain)
        : m_domain(domain)
    {
        acgeSweptDetail::validateDomain(m_domain);
        if constexpr (requires { { curve.controlPoints.empty() } -> std::convertible_to<bool>; })
        {
            if (curve.controlPoints.empty())
                throw std::invalid_argument("Swept source control points are empty.");
        }
        if constexpr (requires { { curve.isEmpty() } -> std::convertible_to<bool>; })
        {
            if (curve.isEmpty())
                throw std::invalid_argument("Swept source curve is empty.");
        }
        if constexpr (requires { curve.validate(); })
            curve.validate();
        m_point = [curve](double u) { return curve.pointAt(u); };
        if constexpr (requires(const Curve &c, double u) {
                          { c.derivativeAt(u) } -> std::convertible_to<AcGeVector3d>;
                      })
            m_derivative = [curve](double u) { return curve.derivativeAt(u); };
        validate();
    }

    AcGeInterval domain() const { return m_domain; }
    bool hasDerivative() const { return static_cast<bool>(m_derivative); }
    AcGePoint3d pointAt(double u) const
    {
        acgeSweptDetail::checkParameter(u, m_domain);
        return acgeSweptDetail::checked(m_point(u));
    }
    AcGeVector3d derivativeAt(double u) const
    {
        acgeSweptDetail::checkParameter(u, m_domain);
        if (!m_derivative)
            throw std::logic_error("Swept source has no analytic derivative.");
        return acgeSweptDetail::checked(m_derivative(u));
    }
    void swap(AcGeSweptCurve3d &other) noexcept
    {
        m_point.swap(other.m_point);
        m_derivative.swap(other.m_derivative);
        std::swap(m_domain, other.m_domain);
    }
};

// S(u,v) = (1-v) C0(u) + v C1(u). Source domains must match;
// the explicit v domain is a nonempty subinterval of [0,1].
class AcGeRuledSurface3d
{
    AcGeSweptCurve3d m_first;
    AcGeSweptCurve3d m_second;
    AcGeInterval m_vDomain;

    void check(double u, double v) const
    {
        acgeSweptDetail::checkParameter(u, uDomain());
        acgeSweptDetail::checkParameter(v, m_vDomain);
    }

public:
    AcGeRuledSurface3d(AcGeSweptCurve3d first, AcGeSweptCurve3d second,
                       const AcGeInterval &vDomain)
        : m_first(std::move(first)), m_second(std::move(second)), m_vDomain(vDomain)
    {
        acgeSweptDetail::validateDomain(m_vDomain);
        if (m_first.domain().lowerBound() != m_second.domain().lowerBound() ||
            m_first.domain().upperBound() != m_second.domain().upperBound() ||
            m_vDomain.lowerBound() < 0.0 || m_vDomain.upperBound() > 1.0)
            throw std::invalid_argument("Ruled surface domains are incompatible.");
    }
    AcGeRuledSurface3d &set(AcGeSweptCurve3d first, AcGeSweptCurve3d second,
                            const AcGeInterval &vDomain)
    {
        AcGeRuledSurface3d candidate(std::move(first), std::move(second), vDomain);
        m_first.swap(candidate.m_first);
        m_second.swap(candidate.m_second);
        std::swap(m_vDomain, candidate.m_vDomain);
        return *this;
    }
    const AcGeSweptCurve3d &firstCurve() const { return m_first; }
    const AcGeSweptCurve3d &secondCurve() const { return m_second; }
    AcGeInterval uDomain() const { return m_first.domain(); }
    AcGeInterval vDomain() const { return m_vDomain; }
    AcGePoint3d pointAt(double u, double v) const
    {
        check(u, v);
        const auto a = m_first.pointAt(u);
        const auto b = m_second.pointAt(u);
        return acgeSweptDetail::checked(AcGePoint3d{
            std::lerp(a.x, b.x, v), std::lerp(a.y, b.y, v), std::lerp(a.z, b.z, v)});
    }
    AcGeVector3d partialU(double u, double v) const
    {
        check(u, v);
        const auto a = m_first.derivativeAt(u);
        const auto b = m_second.derivativeAt(u);
        return acgeSweptDetail::checked(AcGeVector3d{
            std::lerp(a.x, b.x, v), std::lerp(a.y, b.y, v), std::lerp(a.z, b.z, v)});
    }
    AcGeVector3d partialV(double u, double v) const
    {
        check(u, v);
        return acgeSweptDetail::checked(m_second.pointAt(u) - m_first.pointAt(u));
    }
    AcGeVector3d normalAt(double u, double v) const
    {
        return acgeSweptDetail::normal(partialU(u, v), partialV(u, v));
    }
};

// S(u,v) = C(u) + v D. D is displacement per unit v, not normalized.
class AcGeExtrudedSurface3d
{
    AcGeSweptCurve3d m_curve;
    AcGeVector3d m_direction;
    AcGeInterval m_vDomain;

    void check(double u, double v) const
    {
        acgeSweptDetail::checkParameter(u, uDomain());
        acgeSweptDetail::checkParameter(v, m_vDomain);
    }

public:
    AcGeExtrudedSurface3d(AcGeSweptCurve3d curve, const AcGeVector3d &direction,
                          const AcGeInterval &vDomain)
        : m_curve(std::move(curve)), m_direction(direction), m_vDomain(vDomain)
    {
        acgeSweptDetail::validateVector(m_direction);
        acgeSweptDetail::validateDomain(m_vDomain);
    }
    AcGeExtrudedSurface3d &set(AcGeSweptCurve3d curve,
                               const AcGeVector3d &direction,
                               const AcGeInterval &vDomain)
    {
        AcGeExtrudedSurface3d candidate(std::move(curve), direction, vDomain);
        m_curve.swap(candidate.m_curve);
        std::swap(m_direction, candidate.m_direction);
        std::swap(m_vDomain, candidate.m_vDomain);
        return *this;
    }
    const AcGeSweptCurve3d &sourceCurve() const { return m_curve; }
    AcGeVector3d direction() const { return m_direction; }
    AcGeInterval uDomain() const { return m_curve.domain(); }
    AcGeInterval vDomain() const { return m_vDomain; }
    AcGePoint3d pointAt(double u, double v) const
    {
        check(u, v);
        return acgeSweptDetail::checked(m_curve.pointAt(u) + m_direction * v);
    }
    AcGeVector3d partialU(double u, double v) const
    {
        check(u, v);
        return m_curve.derivativeAt(u);
    }
    AcGeVector3d partialV(double u, double v) const
    {
        check(u, v);
        return m_direction;
    }
    AcGeVector3d normalAt(double u, double v) const
    {
        return acgeSweptDetail::normal(partialU(u, v), partialV(u, v));
    }
};

// S(u,v) = O + R(axis,v)(C(u)-O), v in radians, right-handed.
// All normals use partialU x partialV, not an inferred "outward" direction.
// Singular/near-parallel tangents throw domain_error; missing derivatives throw
// logic_error. No surface has an unconfigured default state. set() is transactional.
class AcGeRevolvedSurface3d
{
    AcGeSweptCurve3d m_curve;
    AcGePoint3d m_origin;
    AcGeVector3d m_axis;
    AcGeInterval m_vDomain;

    void check(double u, double v) const
    {
        acgeSweptDetail::checkParameter(u, uDomain());
        acgeSweptDetail::checkParameter(v, m_vDomain);
    }

public:
    AcGeRevolvedSurface3d(AcGeSweptCurve3d curve, const AcGePoint3d &origin,
                          const AcGeVector3d &axis, const AcGeInterval &vDomain)
        : m_curve(std::move(curve)), m_origin(origin), m_axis(axis),
          m_vDomain(vDomain)
    {
        if (!acgeSweptDetail::finite(m_origin))
            throw std::invalid_argument("Revolution origin must be finite.");
        acgeSweptDetail::validateVector(m_axis);
        acgeSweptDetail::validateDomain(m_vDomain);
        m_axis = acgeSweptDetail::unit(m_axis);
    }
    AcGeRevolvedSurface3d &set(AcGeSweptCurve3d curve, const AcGePoint3d &origin,
                               const AcGeVector3d &axis, const AcGeInterval &vDomain)
    {
        AcGeRevolvedSurface3d candidate(std::move(curve), origin, axis, vDomain);
        m_curve.swap(candidate.m_curve);
        std::swap(m_origin, candidate.m_origin);
        std::swap(m_axis, candidate.m_axis);
        std::swap(m_vDomain, candidate.m_vDomain);
        return *this;
    }
    const AcGeSweptCurve3d &sourceCurve() const { return m_curve; }
    AcGePoint3d axisOrigin() const { return m_origin; }
    AcGeVector3d axisDirection() const { return m_axis; }
    AcGeInterval uDomain() const { return m_curve.domain(); }
    AcGeInterval vDomain() const { return m_vDomain; }
    AcGePoint3d pointAt(double u, double v) const
    {
        check(u, v);
        return acgeSweptDetail::checked(m_origin + acgeSweptDetail::rotate(
            acgeSweptDetail::checked(m_curve.pointAt(u) - m_origin), m_axis, v));
    }
    AcGeVector3d partialU(double u, double v) const
    {
        check(u, v);
        return acgeSweptDetail::rotate(m_curve.derivativeAt(u), m_axis, v);
    }
    AcGeVector3d partialV(double u, double v) const
    {
        check(u, v);
        const auto offset = acgeSweptDetail::checked(m_curve.pointAt(u) - m_origin);
        return acgeSweptDetail::rotate(
            acgeSweptDetail::checked(m_axis.crossProduct(offset)), m_axis, v);
    }
    AcGeVector3d normalAt(double u, double v) const
    {
        check(u, v);
        const auto offset = acgeSweptDetail::checked(m_curve.pointAt(u) - m_origin);
        // On an arbitrary axis, cancellation may leave a spurious tiny angular
        // tangent. Treat axis-coincident/roundoff-sized radii as singular.
        if (acgeSweptDetail::scale(offset) == 0.0)
            throw std::domain_error("Revolved surface normal is singular on its axis.");
        const auto radial = m_axis.crossProduct(acgeSweptDetail::unit(offset));
        if (std::hypot(radial.x, radial.y, radial.z) <=
            64.0 * std::numeric_limits<double>::epsilon())
            throw std::domain_error("Revolved surface normal is singular on its axis.");
        return acgeSweptDetail::normal(partialU(u, v), partialV(u, v));
    }
};
