#pragma once

#include "AcGePort.h"

#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <stdexcept>

#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGePointOnSurface aligned with the ObjectARX reference
// (geponsrf.h): a parameterized point wrapper that binds a surface and a
// (u, v) parameter pair and exposes the point coordinates, partial
// derivatives up to order 2, mixed partial, normal, and tangent-plane
// vector maps.
//
// Most surfaces remain independent value types; the AcGeSurface base
// currently covers the 3D B-spline and NURBS patches. This wrapper continues
// to type-erase any surface providing const pointAt(double, double), including
// Sphere, Cylinder, Cone, Torus, BoundedPlane, and swept surfaces. Partial
// derivatives are obtained by central finite differences of the point
// function; built-in surfaces use smooth radian/linear parameterizations.

namespace ge
{
namespace detail
{

struct ACGE_PORT PointOnSurfaceCore
{
    std::function<AcGePoint3d(double, double)> point;
    AcGePoint2d param{0.0, 0.0};

    bool empty() const { return !static_cast<bool>(point); }

    template <class Surface>
    void assign(const Surface &surface)
    {
        point = [surface](double u, double v)
        { return AcGePoint3d(surface.pointAt(u, v)); };
    }

    void reset()
    {
        point = {};
        param = {0.0, 0.0};
    }

    static double step(double u, double v)
    {
        return 1.0e-5 *
               std::max({1.0, std::abs(u), std::abs(v)});
    }
};

} // namespace detail
} // namespace ge

class ACGE_PORT AcGePointOnSurface
{
public:
    AcGePointOnSurface() = default;

    // Point on the surface at parameter (0, 0).
    template <class Surface>
    explicit AcGePointOnSurface(const Surface &surface)
    {
        m_core.assign(surface);
    }
    template <class Surface>
    AcGePointOnSurface(const Surface &surface, const AcGePoint2d &param)
    {
        m_core.assign(surface);
        m_core.param = param;
    }

    template <class Surface>
    AcGePointOnSurface &setSurface(const Surface &surface)
    {
        m_core.assign(surface);
        return *this;
    }
    template <class Surface>
    AcGePointOnSurface &setSurface(const Surface &surface,
                                   const AcGePoint2d &param)
    {
        m_core.assign(surface);
        m_core.param = param;
        return *this;
    }

    const AcGePoint2d &parameter() const { return m_core.param; }
    void setParameter(const AcGePoint2d &param) { m_core.param = param; }

    AcGePoint3d point() const
    {
        return evalPoint(m_core.param.x, m_core.param.y);
    }
    // Evaluates at param and makes it the new cached parameter.
    AcGePoint3d point(const AcGePoint2d &param)
    {
        m_core.param = param;
        return evalPoint(param.x, param.y);
    }
    template <class Surface>
    AcGePoint3d point(const Surface &surface, const AcGePoint2d &param)
    {
        m_core.assign(surface);
        m_core.param = param;
        return evalPoint(param.x, param.y);
    }

    // ARX: order must be greater than 0 and no larger than 2.
    AcGeVector3d uDeriv(int order) const
    {
        return evalPartial(order, true, m_core.param.x, m_core.param.y);
    }
    AcGeVector3d uDeriv(int order, const AcGePoint2d &param)
    {
        m_core.param = param;
        return evalPartial(order, true, param.x, param.y);
    }
    template <class Surface>
    AcGeVector3d uDeriv(int order, const Surface &surface,
                        const AcGePoint2d &param)
    {
        m_core.assign(surface);
        m_core.param = param;
        return evalPartial(order, true, param.x, param.y);
    }
    AcGeVector3d vDeriv(int order) const
    {
        return evalPartial(order, false, m_core.param.x, m_core.param.y);
    }
    AcGeVector3d vDeriv(int order, const AcGePoint2d &param)
    {
        m_core.param = param;
        return evalPartial(order, false, param.x, param.y);
    }
    template <class Surface>
    AcGeVector3d vDeriv(int order, const Surface &surface,
                        const AcGePoint2d &param)
    {
        m_core.assign(surface);
        m_core.param = param;
        return evalPartial(order, false, param.x, param.y);
    }

    // Mixed partial d2S/du/dv at the cached parameter.
    AcGeVector3d mixedPartial() const
    {
        return mixedPartialAt(m_core.param.x, m_core.param.y);
    }

    // Unit normal from the first partials; throws on a singular point.
    AcGeVector3d normal() const
    {
        return normalAt(m_core.param.x, m_core.param.y);
    }

    // Maps an input tangent vector to the tangent plane:
    //   uDeriv(1) * vec.x + vDeriv(1) * vec.y
    AcGeVector3d tangentVector(const AcGeVector2d &vec) const
    {
        return uPartial1At(m_core.param.x, m_core.param.y) * vec.x +
               vPartial1At(m_core.param.x, m_core.param.y) * vec.y;
    }

    // Projects vec onto the tangent plane and returns the parameter-space
    // vector that maps to the projected vector (least squares).
    AcGeVector2d inverseTangentVector(const AcGeVector3d &vec) const
    {
        const AcGeVector3d su = uPartial1At(m_core.param.x, m_core.param.y);
        const AcGeVector3d sv = vPartial1At(m_core.param.x, m_core.param.y);
        const AcGeVector3d projected =
            vec - normalAt(m_core.param.x, m_core.param.y) *
                      vec.dotProduct(
                          normalAt(m_core.param.x, m_core.param.y));
        const double g11 = su.dotProduct(su);
        const double g12 = su.dotProduct(sv);
        const double g22 = sv.dotProduct(sv);
        const double b1 = su.dotProduct(projected);
        const double b2 = sv.dotProduct(projected);
        const double determinant = g11 * g22 - g12 * g12;
        if (determinant <= 1.0e-12 * g11 * g22)
            throw std::domain_error(
                "PointOnSurface tangent plane is degenerate.");
        return AcGeVector2d((b1 * g22 - b2 * g12) / determinant,
                            (b2 * g11 - b1 * g12) / determinant);
    }

private:
    ge::detail::PointOnSurfaceCore m_core;

    void requireSurface() const
    {
        if (m_core.empty())
            throw std::logic_error(
                "PointOnSurface has no surface assigned.");
    }

    AcGePoint3d evalPoint(double u, double v) const
    {
        requireSurface();
        return m_core.point(u, v);
    }

    AcGeVector3d uPartial1At(double u, double v) const
    {
        requireSurface();
        const double h = ge::detail::PointOnSurfaceCore::step(u, v);
        return (m_core.point(u + h, v) - m_core.point(u - h, v)) *
               (0.5 / h);
    }
    AcGeVector3d vPartial1At(double u, double v) const
    {
        requireSurface();
        const double h = ge::detail::PointOnSurfaceCore::step(u, v);
        return (m_core.point(u, v + h) - m_core.point(u, v - h)) *
               (0.5 / h);
    }
    AcGeVector3d uPartial2At(double u, double v) const
    {
        requireSurface();
        const double h = ge::detail::PointOnSurfaceCore::step(u, v);
        const AcGeVector3d forward = m_core.point(u + h, v) - m_core.point(u, v);
        const AcGeVector3d backward = m_core.point(u, v) - m_core.point(u - h, v);
        return (forward - backward) * (1.0 / (h * h));
    }
    AcGeVector3d vPartial2At(double u, double v) const
    {
        requireSurface();
        const double h = ge::detail::PointOnSurfaceCore::step(u, v);
        const AcGeVector3d forward = m_core.point(u, v + h) - m_core.point(u, v);
        const AcGeVector3d backward = m_core.point(u, v) - m_core.point(u, v - h);
        return (forward - backward) * (1.0 / (h * h));
    }
    AcGeVector3d mixedPartialAt(double u, double v) const
    {
        requireSurface();
        const double h = ge::detail::PointOnSurfaceCore::step(u, v);
        const AcGeVector3d duForward =
            m_core.point(u + h, v + h) - m_core.point(u + h, v - h);
        const AcGeVector3d duBackward =
            m_core.point(u - h, v + h) - m_core.point(u - h, v - h);
        return (duForward - duBackward) * (0.25 / (h * h));
    }

    AcGeVector3d evalPartial(int order, bool alongU, double u, double v) const
    {
        if (order <= 0 || order > 2)
            throw std::invalid_argument(
                "PointOnSurface derivative order must be 1 or 2.");
        requireSurface();
        if (order == 1)
            return alongU ? uPartial1At(u, v) : vPartial1At(u, v);
        return alongU ? uPartial2At(u, v) : vPartial2At(u, v);
    }

    AcGeVector3d normalAt(double u, double v) const
    {
        requireSurface();
        const AcGeVector3d su = uPartial1At(u, v);
        const AcGeVector3d sv = vPartial1At(u, v);
        const double scale = su.length() * sv.length();
        const AcGeVector3d cross = su.crossProduct(sv);
        const double length = cross.length();
        if (scale == 0.0 ||
            length <= 64.0 * std::numeric_limits<double>::epsilon() * scale)
            throw std::domain_error(
                "PointOnSurface normal is singular or ill-conditioned.");
        return cross * (1.0 / length);
    }
};