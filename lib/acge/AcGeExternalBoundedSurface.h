#pragma once

#include "AcGePort.h"

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGeCurveBoundary.h"
#include "AcGeExternalGeometry.h"
#include "AcGeCurve.h"

class ACGE_PORT AcGeExternalBoundedSurface : public AcGeSurface
{
public:
    enum class BaseKind
    {
        Unknown = 0,
        Plane,
        Sphere,
        Cylinder,
        Cone,
        Torus,
        Nurbs,
        External
    };

    struct ErasedSurfaceView
    {
        std::function<AcGePoint3d(double, double)> point;
        double minU = 0.0;
        double maxU = 1.0;
        double minV = 0.0;
        double maxV = 1.0;
    };

    AcGeExternalBoundedSurface() = default;

    AcGeExternalBoundedSurface(
        const AcGeExternalSurface &surface,
        const std::vector<AcGeCurveBoundary> &contours,
        BaseKind baseKind = BaseKind::External, bool makeCopy = true)
    {
        set(surface, contours, baseKind, makeCopy);
    }

    AcGeExternalBoundedSurface &set(
        const AcGeExternalSurface &surface,
        const std::vector<AcGeCurveBoundary> &contours,
        BaseKind baseKind = BaseKind::External, bool makeCopy = true)
    {
        if (!surface.isDefined())
            throw std::invalid_argument(
                "ExternalBoundedSurface requires a defined base surface.");
        m_surface.point = [surface](double u, double v)
        { return surface.pointAt(u, v); };
        m_surface.minU = surface.minParameterU();
        m_surface.maxU = surface.maxParameterU();
        m_surface.minV = surface.minParameterV();
        m_surface.maxV = surface.maxParameterV();
        m_surfaceKind = surface.externalSurfaceKind();
        m_contours = contours;
        m_baseKind = baseKind;
        m_makeCopy = makeCopy;
        return *this;
    }

    bool isDefined() const { return static_cast<bool>(m_surface.point); }
    AcGe::ExternalEntityKind externalSurfaceKind() const
    {
        return m_surfaceKind;
    }

    void getBaseSurface(AcGeExternalSurface &surface) const
    {
        requireDefined();
        surface.set(m_surface.point, m_surface.minU, m_surface.maxU,
                    m_surface.minV, m_surface.maxV, m_surfaceKind);
    }

    void getBaseSurface(ErasedSurfaceView &surface) const
    {
        requireDefined();
        surface = m_surface;
    }

    bool isPlane() const { return m_baseKind == BaseKind::Plane; }
    bool isSphere() const { return m_baseKind == BaseKind::Sphere; }
    bool isCylinder() const { return m_baseKind == BaseKind::Cylinder; }
    bool isCone() const { return m_baseKind == BaseKind::Cone; }
    bool isTorus() const { return m_baseKind == BaseKind::Torus; }
    bool isNurbs() const { return m_baseKind == BaseKind::Nurbs; }
    bool isExternalSurface() const { return m_baseKind == BaseKind::External; }

    int numContours() const { return static_cast<int>(m_contours.size()); }

    void getContours(std::vector<AcGeCurveBoundary> &contours) const
    {
        contours = m_contours;
    }

    AcGeExternalBoundedSurface &setToOwnSurface()
    {
        m_makeCopy = true;
        return *this;
    }

    bool isOwnerOfSurface() const { return m_makeCopy; }

    double minParameterU() const override
    {
        requireDefined();
        return m_surface.minU;
    }

    double maxParameterU() const override
    {
        requireDefined();
        return m_surface.maxU;
    }

    double minParameterV() const override
    {
        requireDefined();
        return m_surface.minV;
    }

    double maxParameterV() const override
    {
        requireDefined();
        return m_surface.maxV;
    }

    AcGePoint3d pointAt(double u, double v) const override
    {
        requireDefined();
        return m_surface.point(u, v);
    }

private:
    void requireDefined() const
    {
        if (!isDefined())
            throw std::logic_error(
                "AcGeExternalBoundedSurface is not defined.");
    }

    ErasedSurfaceView m_surface;
    std::vector<AcGeCurveBoundary> m_contours;
    AcGe::ExternalEntityKind m_surfaceKind =
        AcGe::kExternalEntityUndefined;
    BaseKind m_baseKind = BaseKind::Unknown;
    bool m_makeCopy = true;
};
