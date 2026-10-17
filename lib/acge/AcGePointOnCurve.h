#pragma once

#include "AcGePort.h"

#include <cmath>
#include <concepts>
#include <functional>
#include <stdexcept>
#include <utility>

#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGePointOnCurve2d/3d aligned with the ObjectARX reference
// (geponc2d.h / geponc3d.h): a parameterized point wrapper that binds a
// curve and a parameter value and exposes the point coordinates, derivatives
// up to order 2, curvature, and singularity queries.
//
// The library keeps curve entities as independent value types (no
// AcGeCurve2d/3d hierarchy), so the wrapper type-erases any curve providing
// const pointAt(double) -- the same protocol as the swept-surface source
// curves. An analytic firstDeriv(double)/derivativeAt(double) is used when
// the curve provides one; otherwise derivatives are obtained by central
// finite differences of the point function.

namespace ge
{
namespace detail
{

// Dimension-agnostic evaluator shared by PointOnCurve2d/3d and the
// augmented polyline sampler.
template <class PointT, class VectorT>
struct PointOnCurveCore
{
    std::function<PointT(double)> point;
    std::function<VectorT(double)> firstDeriv;
    double param = 0.0;

    bool empty() const { return !static_cast<bool>(point); }
    bool hasAnalyticDeriv() const { return static_cast<bool>(firstDeriv); }

    template <class Curve>
    void assign(const Curve &curve)
    {
        // Curves expose either pointAt(double) (line segments, NURBS, ...)
        // or pointAtAngle(double) (conic arcs); accept both protocols.
        if constexpr (requires {
                          { curve.pointAt(0.0) } -> std::convertible_to<PointT>;
                      })
        {
            point = [curve](double u) { return PointT(curve.pointAt(u)); };
        }
        else
        {
            point = [curve](double u)
            { return PointT(curve.pointAtAngle(u)); };
        }
        if constexpr (requires {
                          { curve.firstDeriv(0.0) } -> std::convertible_to<VectorT>;
                      })
        {
            firstDeriv = [curve](double u)
            { return VectorT(curve.firstDeriv(u)); };
        }
        else if constexpr (requires {
                               { curve.derivativeAt(0.0) } -> std::convertible_to<VectorT>;
                           })
        {
            firstDeriv = [curve](double u)
            { return VectorT(curve.derivativeAt(u)); };
        }
        else
        {
            firstDeriv = {};
        }
    }

    void reset()
    {
        point = {};
        firstDeriv = {};
        param = 0.0;
    }

    // Finite-difference steps scaled to the parameter magnitude.
    static double step1(double u)
    {
        return 1.0e-6 * std::max(1.0, std::abs(u));
    }
    static double step2(double u)
    {
        return 1.0e-3 * std::max(1.0, std::abs(u));
    }
};

} // namespace detail
} // namespace ge

// Shared implementation for the 2D/3D point-on-curve wrappers.
template <class PointT, class VectorT>
class AcGePointOnCurveBase
{
public:
    AcGePointOnCurveBase() = default;

    // Point on the curve at parameter 0.0 (ObjectARX convention).
    template <class Curve>
    explicit AcGePointOnCurveBase(const Curve &curve)
    {
        m_core.assign(curve);
    }
    template <class Curve>
    AcGePointOnCurveBase(const Curve &curve, double param)
    {
        m_core.assign(curve);
        m_core.param = param;
    }

    template <class Curve>
    AcGePointOnCurveBase &setCurve(const Curve &curve)
    {
        m_core.assign(curve);
        return *this;
    }
    template <class Curve>
    AcGePointOnCurveBase &setCurve(const Curve &curve, double param)
    {
        m_core.assign(curve);
        m_core.param = param;
        return *this;
    }

    double parameter() const { return m_core.param; }
    AcGePointOnCurveBase &setParameter(double param)
    {
        m_core.param = param;
        return *this;
    }

    PointT point() const { return evalPoint(m_core.param); }
    // Evaluates at param and makes it the new cached parameter.
    PointT point(double param)
    {
        m_core.param = param;
        return evalPoint(param);
    }
    template <class Curve>
    PointT point(const Curve &curve, double param)
    {
        m_core.assign(curve);
        m_core.param = param;
        return evalPoint(param);
    }

    // ARX: order must be greater than 0 and no larger than 2.
    VectorT deriv(int order) const { return evalDeriv(order, m_core.param); }
    VectorT deriv(int order, double param)
    {
        m_core.param = param;
        return evalDeriv(order, param);
    }
    template <class Curve>
    VectorT deriv(int order, const Curve &curve, double param)
    {
        m_core.assign(curve);
        m_core.param = param;
        return evalDeriv(order, param);
    }

    double curvature() const { return curvatureValue(m_core.param); }
    double curvature(double param)
    {
        m_core.param = param;
        return curvatureValue(param);
    }
    // ARX overload: always true unless the first derivative vanishes.
    bool curvature(double param, double &res)
    {
        m_core.param = param;
        VectorT d1, d2;
        derivativesAt(param, d1, d2);
        if (d1.length() <= AcGeTol{}.equalVector())
            return false;
        res = curvatureFrom(d1, d2);
        return true;
    }

    bool isSingular(const AcGeTol &tol = AcGeTol{}) const
    {
        return firstDerivAt(m_core.param).length() <= tol.equalVector();
    }

protected:
    ge::detail::PointOnCurveCore<PointT, VectorT> m_core;

    void requireCurve() const
    {
        if (m_core.empty())
            throw std::logic_error(
                "PointOnCurve has no curve assigned.");
    }

    PointT evalPoint(double u) const
    {
        requireCurve();
        return m_core.point(u);
    }

    VectorT firstDerivAt(double u) const
    {
        requireCurve();
        if (m_core.firstDeriv)
        {
            return m_core.firstDeriv(u);
        }
        const double h = ge::detail::PointOnCurveCore<PointT, VectorT>::step1(u);
        return (m_core.point(u + h) - m_core.point(u - h)) * (0.5 / h);
    }

    VectorT secondDerivAt(double u) const
    {
        requireCurve();
        const double h = ge::detail::PointOnCurveCore<PointT, VectorT>::step2(u);
        if (m_core.firstDeriv)
        {
            return (m_core.firstDeriv(u + h) - m_core.firstDeriv(u - h)) *
                   (0.5 / h);
        }
        // Three-point second difference of the point function.
        const VectorT forward = m_core.point(u + h) - m_core.point(u);
        const VectorT backward = m_core.point(u) - m_core.point(u - h);
        return (forward - backward) * (1.0 / (h * h));
    }

    void derivativesAt(double u, VectorT &d1, VectorT &d2) const
    {
        d1 = firstDerivAt(u);
        d2 = secondDerivAt(u);
    }

    VectorT evalDeriv(int order, double u) const
    {
        if (order <= 0 || order > 2)
            throw std::invalid_argument(
                "PointOnCurve derivative order must be 1 or 2.");
        requireCurve();
        return order == 1 ? firstDerivAt(u) : secondDerivAt(u);
    }

    static double curvatureFrom(const VectorT &d1, const VectorT &d2)
    {
        const double speed = d1.length();
        if (speed <= AcGeTol{}.equalVector())
            throw std::domain_error(
                "PointOnCurve curvature is undefined at a singular point.");
        double numerator;
        if constexpr (requires { d1.crossProduct(d2).length(); })
        {
            numerator = d1.crossProduct(d2).length();
        }
        else
        {
            numerator = std::abs(d1.x * d2.y - d1.y * d2.x);
        }
        return numerator / (speed * speed * speed);
    }

    double curvatureValue(double u) const
    {
        requireCurve();
        VectorT d1, d2;
        derivativesAt(u, d1, d2);
        return curvatureFrom(d1, d2);
    }
};

class ACGE_PORT AcGePointOnCurve2d
    : public AcGePointOnCurveBase<AcGePoint2d, AcGeVector2d>
{
public:
    using AcGePointOnCurveBase<AcGePoint2d, AcGeVector2d>::AcGePointOnCurveBase;
};

class ACGE_PORT AcGePointOnCurve3d
    : public AcGePointOnCurveBase<AcGePoint3d, AcGeVector3d>
{
public:
    using AcGePointOnCurveBase<AcGePoint3d, AcGeVector3d>::AcGePointOnCurveBase;
};