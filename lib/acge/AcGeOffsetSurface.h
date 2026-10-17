#pragma once

#include "AcGePort.h"

#include <cmath>
#include <concepts>
#include <functional>
#include <stdexcept>
#include <type_traits>

#include "AcGeBoundedPlane.h"
#include "AcGePoint.h"
#include "AcGeSurface3d.h"

// Source-level AcGeOffsetSurface aligned with the ObjectARX reference
// (geoffsf.h): an offset of a base surface evaluated as
//
//     pointAt(u, v) = base.pointAt(u, v) + offsetDist * baseNormal(u, v)
//
// where the base normal is taken analytically from the base surface's
// `normalAt(u, v)` when available and from central finite differences of
// its point function otherwise. The offset preserves the base normal, so
// normalAt() equals the base surface normal.
//
// Because the library keeps surfaces as independent value types (no
// AcGeSurface hierarchy), the base surface is type-erased behind a
// point/normal callback pair. The isPlane/isSphere/isCylinder/isCone/
// isTorus/isBoundedPlane flags are resolved from the base surface's C++
// type at construction: the built-in surface families report their own
// flag (the planar family reports both isPlane and isBoundedPlane),
// everything else reports all flags false. getSurface() (conversion of
// the offset to a simple fitted surface) has no value-type equivalent and
// returns kFalse without touching its output.

class ACGE_PORT AcGeOffsetSurface
{
public:
    // Kind of the construction surface, resolved from its C++ type.
    enum class BaseKind
    {
        Unknown = 0,
        Plane,
        Sphere,
        Cylinder,
        Cone,
        Torus
    };

    AcGeOffsetSurface() = default;

    template <class Surface>
        requires requires(const Surface &s) {
            { s.pointAt(0.0, 0.0) } -> std::convertible_to<AcGePoint3d>;
        }
    AcGeOffsetSurface(const Surface &base, double offsetDist,
                      bool makeCopy = true)
    {
        set(base, offsetDist, makeCopy);
    }

    // Value semantics: the base surface is always stored by value, so the
    // ARX makeCopy ownership flag is recorded for API parity only.
    template <class Surface>
        requires requires(const Surface &s) {
            { s.pointAt(0.0, 0.0) } -> std::convertible_to<AcGePoint3d>;
        }
    AcGeOffsetSurface &set(const Surface &base, double offsetDist,
                           bool makeCopy = true)
    {
        if (!std::isfinite(offsetDist))
            throw std::invalid_argument(
                "OffsetSurface requires a finite offset distance.");
        m_point = [base](double u, double v)
        { return AcGePoint3d(base.pointAt(u, v)); };
        if constexpr (requires(const Surface &s) {
                          { s.normalAt(0.0, 0.0) } ->
                              std::convertible_to<AcGeVector3d>;
                      })
        {
            m_normal = [base](double u, double v)
            { return AcGeVector3d(base.normalAt(u, v)); };
        }
        else
        {
            m_normal = {};
        }
        m_offsetDist = offsetDist;
        m_makeCopy = makeCopy;
        m_kind = kindOf(base);
        return *this;
    }

    bool isEmpty() const { return !static_cast<bool>(m_point); }
    double offsetDist() const { return m_offsetDist; }
    bool isOwnerOfSurface() const { return m_makeCopy; }
    void setToOwnSurface() { m_makeCopy = true; }

    // Type-erased view of the construction (base) surface; an empty point
    // callback marks the default-constructed state.
    struct ErasedSurfaceView
    {
        std::function<AcGePoint3d(double, double)> point;
        std::function<AcGeVector3d(double, double)> normal;
    };

    void getConstructionSurface(ErasedSurfaceView &view) const
    {
        requireNotEmpty();
        view = {m_point, m_normal};
    }

    // ARX converts the offset into a fitted simple surface; this port has
    // no fitted-surface value type, so the documented Boolean contract
    // reports failure without modifying the output.
    bool getSurface(ErasedSurfaceView &) const { return false; }

    BaseKind baseKind() const { return m_kind; }

    bool isPlane() const { return m_kind == BaseKind::Plane; }
    bool isBoundedPlane() const { return isPlane(); }
    bool isSphere() const { return m_kind == BaseKind::Sphere; }
    bool isCylinder() const { return m_kind == BaseKind::Cylinder; }
    bool isCone() const { return m_kind == BaseKind::Cone; }
    bool isTorus() const { return m_kind == BaseKind::Torus; }

    AcGePoint3d pointAt(double u, double v) const
    {
        requireNotEmpty();
        const AcGeVector3d normal = surfaceNormal(u, v);
        if (normal.isZeroLength())
            throw std::runtime_error(
                "OffsetSurface: degenerate base-surface normal.");
        return m_point(u, v) + normal * m_offsetDist;
    }

    // The offset preserves the base normal direction.
    AcGeVector3d normalAt(double u, double v) const
    {
        requireNotEmpty();
        return surfaceNormal(u, v);
    }

private:
    void requireNotEmpty() const
    {
        if (isEmpty())
            throw std::logic_error(
                "AcGeOffsetSurface: no construction surface.");
    }

    AcGeVector3d surfaceNormal(double u, double v) const
    {
        if (m_normal)
            return m_normal(u, v).normal();
        // Central finite differences of the base point function; the
        // built-in families are smooth in both parameters.
        const double h = 1.0e-5 * std::max({1.0, std::abs(u), std::abs(v)});
        const AcGeVector3d du = (m_point(u + h, v) - m_point(u - h, v)) *
                                (0.5 / h);
        const AcGeVector3d dv = (m_point(u, v + h) - m_point(u, v - h)) *
                                (0.5 / h);
        return du.crossProduct(dv).normal();
    }

    template <class Surface>
    static BaseKind kindOf(const Surface &)
    {
        if constexpr (std::is_same_v<std::decay_t<Surface>,
                                     AcGeBoundedPlane>)
            return BaseKind::Plane;
        else if constexpr (std::is_same_v<std::decay_t<Surface>,
                                          AcGeSphere>)
            return BaseKind::Sphere;
        else if constexpr (std::is_same_v<std::decay_t<Surface>,
                                          AcGeCylinder>)
            return BaseKind::Cylinder;
        else if constexpr (std::is_same_v<std::decay_t<Surface>, AcGeCone>)
            return BaseKind::Cone;
        else if constexpr (std::is_same_v<std::decay_t<Surface>, AcGeTorus>)
            return BaseKind::Torus;
        else
            return BaseKind::Unknown;
    }

    std::function<AcGePoint3d(double, double)> m_point;
    std::function<AcGeVector3d(double, double)> m_normal;
    double m_offsetDist = 0.0;
    bool m_makeCopy = true;
    BaseKind m_kind = BaseKind::Unknown;
};
