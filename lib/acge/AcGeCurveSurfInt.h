#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <limits>
#include <vector>

#include "AcGeInterval.h"
#include "AcGePoint.h"
#include "AcGePointOnCurve.h"
#include "AcGePointOnSurface.h"
#include "AcGeTol.h"

// Source-level AcGeCurveSurfInt aligned with the ObjectARX reference
// (gecsint.h): the intersection set of a curve and a surface.
//
// ARX builds this from the AcGeCurve3d/AcGeSurface hierarchy; this port
// type-erases any curve providing pointAt(double) (or pointAtAngle(double))
// and any surface providing pointAt(double, double). The engine seeds from
// curve samples with a coarse surface grid scan, then refines each seed
// with a 3x3 Gauss-Newton solve of C(t) - S(u, v) (NURBS Book section 7.4
// idea); when the system is singular (tangential contact or a surface
// pole) it falls back to alternating curve/surface projection. Partial
// derivatives come from analytic firstDeriv on the curve when available
// and central finite differences on the surface (all built-in surfaces
// use smooth parameterizations).
//
// Documented queries numIntPoints, intPoint, getIntParams,
// getPointOnCurve, getPointOnSurface, and tolerance are supported.
// getIntConfigs (csiConfig) and returning the original typed curve/surface
// entities are out of scope (this port keeps value types, no hierarchy).

namespace ge
{
namespace detail
{

// Type-erased adapters so the documented AcGePointOnCurve3d /
// AcGePointOnSurface queries can be filled from the stored cores.
struct ACGE_PORT ErasedCurve3dView
{
    const PointOnCurveCore<AcGePoint3d, AcGeVector3d> *core = nullptr;

    AcGePoint3d pointAt(double t) const { return core->point(t); }
};

struct ACGE_PORT ErasedSurfaceView
{
    PointOnSurfaceCore core{};

    AcGePoint3d pointAt(double u, double v) const
    {
        return core.point(u, v);
    }
};

class ACGE_PORT CurveSurfaceIntersector
{
public:
    struct Record
    {
        double curveParam = 0.0;
        double u = 0.0;
        double v = 0.0;
        AcGePoint3d point{};
        double tolerance = 0.0;
    };

    CurveSurfaceIntersector() = default;

    template <class Curve, class Surface>
    CurveSurfaceIntersector(const Curve &curve, const Surface &surface,
                            double curveLow, double curveHigh, double uLow,
                            double uHigh, double vLow, double vHigh,
                            double tolerance)
    {
        m_curve.assign(curve);
        m_surface.assign(surface);
        m_curveLow = curveLow;
        m_curveHigh = curveHigh;
        m_uLow = uLow;
        m_uHigh = uHigh;
        m_vLow = vLow;
        m_vHigh = vHigh;
        m_tolerance = tolerance;
        validateRanges();
        compute();
    }

    const std::vector<Record> &records() const { return m_records; }
    double tolerance() const { return m_tolerance; }

private:
    static constexpr int kCurveSamples = 48;
    static constexpr int kSurfaceGrid = 32;

    PointOnCurveCore<AcGePoint3d, AcGeVector3d> m_curve;
    PointOnSurfaceCore m_surface;
    double m_curveLow = 0.0;
    double m_curveHigh = 1.0;
    double m_uLow = 0.0;
    double m_uHigh = 1.0;
    double m_vLow = 0.0;
    double m_vHigh = 1.0;
    double m_tolerance = 1.0e-7;
    std::vector<Record> m_records{};

    void validateRanges() const
    {
        if (!(m_curveLow < m_curveHigh) || !(m_uLow < m_uHigh) ||
            !(m_vLow < m_vHigh) || !std::isfinite(m_curveLow) ||
            !std::isfinite(m_curveHigh) || !std::isfinite(m_uLow) ||
            !std::isfinite(m_uHigh) || !std::isfinite(m_vLow) ||
            !std::isfinite(m_vHigh))
            throw std::invalid_argument(
                "CurveSurfInt requires finite increasing parameter ranges.");
        if (!(m_tolerance > 0.0) || !std::isfinite(m_tolerance))
            throw std::invalid_argument(
                "CurveSurfInt requires a positive tolerance.");
    }

    static AcGeVector3d curveDeriv(
        const PointOnCurveCore<AcGePoint3d, AcGeVector3d> &curve, double t)
    {
        if (curve.firstDeriv)
            return curve.firstDeriv(t);
        const double h = PointOnCurveCore<AcGePoint3d, AcGeVector3d>::step1(t);
        return (curve.point(t + h) - curve.point(t - h)) * (0.5 / h);
    }

    void surfacePartials(double u, double v, AcGeVector3d &su,
                         AcGeVector3d &sv) const
    {
        const double h = PointOnSurfaceCore::step(u, v);
        su = (m_surface.point(u + h, v) - m_surface.point(u - h, v)) *
             (0.5 / h);
        sv = (m_surface.point(u, v + h) - m_surface.point(u, v - h)) *
             (0.5 / h);
    }

    // Distance from point p to the surface, refining (u, v) by 2-var
    // Gauss-Newton on |S(u, v) - p|^2.
    double projectOnSurface(const AcGePoint3d &p, double &u, double &v) const
    {
        for (int iteration = 0; iteration < 24; ++iteration)
        {
            AcGeVector3d su;
            AcGeVector3d sv;
            surfacePartials(u, v, su, sv);
            const AcGeVector3d d = m_surface.point(u, v) - p;
            const double k11 = su.dotProduct(su);
            const double k12 = su.dotProduct(sv);
            const double k22 = sv.dotProduct(sv);
            const double determinant = k11 * k22 - k12 * k12;
            if (!(std::abs(determinant) > 1.0e-30))
                break;
            const double g1 = d.dotProduct(su);
            const double g2 = d.dotProduct(sv);
            const double deltaU = (-k22 * g1 + k12 * g2) / determinant;
            const double deltaV = (k12 * g1 - k11 * g2) / determinant;
            if (!std::isfinite(deltaU) || !std::isfinite(deltaV))
                break;
            u = std::clamp(u + deltaU, m_uLow, m_uHigh);
            v = std::clamp(v + deltaV, m_vLow, m_vHigh);
            if (std::abs(deltaU) <= 1.0e-12 * std::max(1.0, std::abs(u)) &&
                std::abs(deltaV) <= 1.0e-12 * std::max(1.0, std::abs(v)))
                break;
        }
        return (m_surface.point(u, v) - p).length();
    }

    static bool invert3x3(const double m[3][3], double inverse[3][3])
    {
        const double c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
        const double c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
        const double c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
        const double determinant =
            m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
        if (!(std::abs(determinant) > 1.0e-30))
            return false;
        const double scale = 1.0 / determinant;
        inverse[0][0] = c00 * scale;
        inverse[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * scale;
        inverse[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * scale;
        inverse[1][0] = c01 * scale;
        inverse[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * scale;
        inverse[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * scale;
        inverse[2][0] = c02 * scale;
        inverse[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * scale;
        inverse[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * scale;
        return true;
    }

    // Gauss-Newton over (t, u, v): solve [C' -Su -Sv] x = -r via the
    // normal equations. Falls back to alternating curve/surface
    // projection when the 3x3 system is singular (tangential contact or
    // a surface pole where Su x Sv degenerates).
    bool refineTriple(double &t, double &u, double &v) const
    {
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            const AcGeVector3d d =
                m_curve.point(t) - m_surface.point(u, v);
            if (d.length() <= m_tolerance)
                return true;
            const AcGeVector3d ct = curveDeriv(m_curve, t);
            AcGeVector3d su;
            AcGeVector3d sv;
            surfacePartials(u, v, su, sv);
            const AcGeVector3d columns[3] = {-ct, su, sv};
            double matrix[3][3];
            double rhs[3];
            for (int r = 0; r < 3; ++r)
            {
                rhs[r] = columns[r].dotProduct(d);
                for (int c = 0; c < 3; ++c)
                    matrix[r][c] = columns[r].dotProduct(columns[c]);
            }
            double inverse[3][3];
            if (!invert3x3(matrix, inverse))
            {
                return tangentialFallback(t, u, v);
            }
            const double deltaT = inverse[0][0] * rhs[0] +
                                  inverse[0][1] * rhs[1] +
                                  inverse[0][2] * rhs[2];
            const double deltaU = inverse[1][0] * rhs[0] +
                                  inverse[1][1] * rhs[1] +
                                  inverse[1][2] * rhs[2];
            const double deltaV = inverse[2][0] * rhs[0] +
                                  inverse[2][1] * rhs[1] +
                                  inverse[2][2] * rhs[2];
            if (!std::isfinite(deltaT) || !std::isfinite(deltaU) ||
                !std::isfinite(deltaV))
                return false;
            t = std::clamp(t + deltaT, m_curveLow, m_curveHigh);
            u = std::clamp(u + deltaU, m_uLow, m_uHigh);
            v = std::clamp(v + deltaV, m_vLow, m_vHigh);
        }
        return (m_curve.point(t) - m_surface.point(u, v)).length() <=
               m_tolerance;
    }

    bool tangentialFallback(double &t, double &u, double &v) const
    {
        for (int iteration = 0; iteration < 12; ++iteration)
        {
            // Project the surface point onto the curve (1-var step), then
            // the curve point onto the surface (2-var projection).
            const AcGeVector3d d = m_surface.point(u, v) - m_curve.point(t);
            const AcGeVector3d ct = curveDeriv(m_curve, t);
            const double aa = ct.dotProduct(ct);
            if (aa > 0.0)
            {
                const double step = d.dotProduct(ct) / aa;
                if (std::isfinite(step))
                    t = std::clamp(t + step, m_curveLow, m_curveHigh);
            }
            const AcGePoint3d p = m_curve.point(t);
            projectOnSurface(p, u, v);
        }
        return (m_curve.point(t) - m_surface.point(u, v)).length() <=
               m_tolerance;
    }

    void compute()
    {
        const double curveSpan = m_curveHigh - m_curveLow;
        const double uSpan = m_uHigh - m_uLow;
        const double vSpan = m_vHigh - m_vLow;

        for (int i = 0; i <= kCurveSamples; ++i)
        {
            const double t =
                m_curveLow + curveSpan * double(i) / kCurveSamples;
            const AcGePoint3d pointOnCurve = m_curve.point(t);

            // Coarse surface scan for the nearest (u, v) seed.
            double bestDistance = std::numeric_limits<double>::max();
            double seedU = m_uLow;
            double seedV = m_vLow;
            for (int su = 0; su <= kSurfaceGrid; ++su)
            {
                for (int sv = 0; sv <= kSurfaceGrid; ++sv)
                {
                    const double uu =
                        m_uLow + uSpan * double(su) / kSurfaceGrid;
                    const double vv =
                        m_vLow + vSpan * double(sv) / kSurfaceGrid;
                    const double distance =
                        (pointOnCurve - m_surface.point(uu, vv)).length();
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        seedU = uu;
                        seedV = vv;
                    }
                }
            }

            double rt = t;
            double ru = seedU;
            double rv = seedV;
            if (!refineTriple(rt, ru, rv))
                continue;

            // Deduplicate: same physical point (also merges closed-surface
            // seam duplicates such as u = 0 vs u = 2*pi).
            bool duplicate = false;
            for (const Record &existing : m_records)
            {
                if ((existing.point - m_curve.point(rt)).length() <=
                    m_tolerance)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
                continue;

            Record record;
            record.curveParam = rt;
            record.u = ru;
            record.v = rv;
            record.point = m_curve.point(rt);
            record.tolerance = m_tolerance;
            m_records.push_back(record);
        }

        std::sort(m_records.begin(), m_records.end(),
                  [](const Record &a, const Record &b)
                  { return a.curveParam < b.curveParam; });
    }
};

} // namespace detail
} // namespace ge

class ACGE_PORT AcGeCurveSurfInt
{
public:
    AcGeCurveSurfInt() = default;

    template <class Curve, class Surface>
    AcGeCurveSurfInt(const Curve &curve, const Surface &surface,
                     const AcGeTol &tol = AcGeTol{})
    {
        set(curve, surface, tol);
    }

    template <class Curve, class Surface>
    AcGeCurveSurfInt &set(const Curve &curve, const Surface &surface,
                          const AcGeTol &tol = AcGeTol{})
    {
        double curveLow = 0.0;
        double curveHigh = 1.0;
        curveDomainOf(curve, curveLow, curveHigh);
        double uLow = 0.0;
        double uHigh = 1.0;
        double vLow = 0.0;
        double vHigh = 1.0;
        surfaceDomainOf(surface, uLow, uHigh, vLow, vHigh);
        const double equalPoint =
            tol.equalPoint() > 0.0 ? tol.equalPoint() : 1.0e-7;
        m_intersector.emplace(curve, surface, curveLow, curveHigh, uLow,
                              uHigh, vLow, vHigh, equalPoint);
        m_curveCore.emplace();
        m_curveCore->assign(curve);
        m_surfaceCore.emplace();
        m_surfaceCore->assign(surface);
        return *this;
    }

    int numIntPoints() const
    {
        return m_intersector
                   ? static_cast<int>(m_intersector->records().size())
                   : 0;
    }

    AcGePoint3d intPoint(int index) const
    {
        return records().at(static_cast<std::size_t>(checkIndex(index)))
            .point;
    }

    bool getIntParams(int index, double &curveParam,
                      AcGePoint2d &surfaceParam) const
    {
        const auto &record =
            records().at(static_cast<std::size_t>(checkIndex(index)));
        curveParam = record.curveParam;
        surfaceParam = {record.u, record.v};
        return true;
    }

    double curveParameter(int index) const
    {
        return records().at(static_cast<std::size_t>(checkIndex(index)))
            .curveParam;
    }

    AcGePoint2d surfaceParameter(int index) const
    {
        const auto &record =
            records().at(static_cast<std::size_t>(checkIndex(index)));
        return {record.u, record.v};
    }

    // ARX getPointOnCurve/getPointOnSurface equivalents, filled from the
    // stored type-erased cores.
    bool getPointOnCurve(int index, AcGePointOnCurve3d &pointOnCurve) const
    {
        requireCurve();
        pointOnCurve.setCurve(m_curveView())
            .setParameter(curveParameter(index));
        return true;
    }

    bool getPointOnSurface(int index, AcGePointOnSurface &pointOnSurface) const
    {
        requireSurface();
        pointOnSurface.setSurface(m_surfaceView())
            .setParameter(surfaceParameter(index));
        return true;
    }

    double tolerance() const
    {
        return m_intersector ? m_intersector->tolerance() : 0.0;
    }

private:
    const std::vector<ge::detail::CurveSurfaceIntersector::Record> &
    records() const
    {
        if (!m_intersector)
            throw std::logic_error("CurveSurfInt has no curves assigned.");
        return m_intersector->records();
    }

    static int checkIndex(int index)
    {
        if (index < 0)
            throw std::out_of_range(
                "CurveSurfInt intersection index is out of range.");
        return index;
    }

    void requireCurve() const
    {
        if (!m_curveCore)
            throw std::logic_error("CurveSurfInt has no curves assigned.");
    }
    void requireSurface() const
    {
        if (!m_surfaceCore)
            throw std::logic_error("CurveSurfInt has no surfaces assigned.");
    }

    ge::detail::ErasedCurve3dView m_curveView() const
    {
        ge::detail::ErasedCurve3dView view;
        view.core = &*m_curveCore;
        return view;
    }
    ge::detail::ErasedSurfaceView m_surfaceView() const
    {
        ge::detail::ErasedSurfaceView view;
        view.core = *m_surfaceCore;
        return view;
    }

    template <class Curve>
    static void curveDomainOf(const Curve &curve, double &low, double &high)
    {
        if constexpr (requires {
                          {
                              curve.minParameter()
                          } -> std::convertible_to<double>;
                          {
                              curve.maxParameter()
                          } -> std::convertible_to<double>;
                      })
        {
            low = curve.minParameter();
            high = curve.maxParameter();
        }
        else if constexpr (requires {
                               {
                                   curve.startParam()
                               } -> std::convertible_to<double>;
                               {
                                   curve.endParam()
                               } -> std::convertible_to<double>;
                           })
        {
            low = curve.startParam();
            high = curve.endParam();
        }
        else if constexpr (requires {
                               {
                                   curve.startAngle
                               } -> std::convertible_to<double>;
                               {
                                   curve.endAngle
                               } -> std::convertible_to<double>;
                           })
        {
            low = curve.startAngle;
            high = curve.endAngle;
        }
        else
        {
            low = 0.0;
            high = 1.0;
        }
    }

    template <class Surface>
    static void surfaceDomainOf(const Surface &surface, double &uLow,
                                double &uHigh, double &vLow, double &vHigh)
    {
        if constexpr (requires {
                          {
                              surface.minParameterU()
                          } -> std::convertible_to<double>;
                          {
                              surface.maxParameterU()
                          } -> std::convertible_to<double>;
                          {
                              surface.minParameterV()
                          } -> std::convertible_to<double>;
                          {
                              surface.maxParameterV()
                          } -> std::convertible_to<double>;
                      })
        {
            uLow = surface.minParameterU();
            uHigh = surface.maxParameterU();
            vLow = surface.minParameterV();
            vHigh = surface.maxParameterV();
        }
        else if constexpr (requires {
                               {
                                   surface.uRange()
                               } -> std::convertible_to<AcGeInterval>;
                               {
                                   surface.vRange()
                               } -> std::convertible_to<AcGeInterval>;
                           })
        {
            uLow = surface.uRange().lowerBound();
            uHigh = surface.uRange().upperBound();
            vLow = surface.vRange().lowerBound();
            vHigh = surface.vRange().upperBound();
        }
        else if constexpr (requires {
                               surface.getAnglesInU(
                                   std::declval<double &>(),
                                   std::declval<double &>());
                               surface.getAnglesInV(
                                   std::declval<double &>(),
                                   std::declval<double &>());
                           })
        {
            surface.getAnglesInU(uLow, uHigh);
            surface.getAnglesInV(vLow, vHigh);
        }
        else if constexpr (requires {
                               surface.getAngles(
                                   std::declval<double &>(),
                                   std::declval<double &>());
                               surface.getHeight(
                                   std::declval<AcGeInterval &>());
                           })
        {
            surface.getAngles(uLow, uHigh);
            AcGeInterval height{};
            surface.getHeight(height);
            vLow = height.lowerBound();
            vHigh = height.upperBound();
        }
        else
        {
            uLow = 0.0;
            uHigh = 1.0;
            vLow = 0.0;
            vHigh = 1.0;
        }
    }

    std::optional<ge::detail::CurveSurfaceIntersector> m_intersector;
    std::optional<ge::detail::PointOnCurveCore<AcGePoint3d, AcGeVector3d>>
        m_curveCore;
    std::optional<ge::detail::PointOnSurfaceCore> m_surfaceCore;
};
