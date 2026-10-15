#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "geinterval.h"
#include "gepoint.h"

// Source-level AcGeBoundedPlane subset with an orthonormal parameter frame.
struct AcGeBoundedPlane
{
private:
    AcGePoint3d m_origin{0.0, 0.0, 0.0};
    AcGeVector3d m_normal{0.0, 0.0, 1.0};
    AcGeVector3d m_uAxis{1.0, 0.0, 0.0};
    AcGeVector3d m_vAxis{0.0, 1.0, 0.0};
    AcGeInterval m_uRange{0.0, 1.0};
    AcGeInterval m_vRange{0.0, 1.0};

    static void makeFrame(const AcGeVector3d &normal,
                          const AcGeVector3d &reference,
                          AcGeVector3d &unitNormal, AcGeVector3d &unitU,
                          AcGeVector3d &unitV)
    {
        if (normal.isZeroLength() || reference.isZeroLength())
            throw std::invalid_argument(
                "AcGeBoundedPlane frame vectors must be nonzero.");

        unitNormal = normal.normal();
        unitU = reference - unitNormal * reference.dotProduct(unitNormal);
        if (unitU.isZeroLength())
            throw std::invalid_argument(
                "AcGeBoundedPlane reference axis must not be parallel to its normal.");
        unitU.normalize();
        unitV = unitNormal.crossProduct(unitU).normal();
    }

    static void validateRange(const AcGeInterval &range)
    {
        if (!range.isBounded() || !std::isfinite(range.lowerBound()) ||
            !std::isfinite(range.upperBound()))
            throw std::invalid_argument(
                "AcGeBoundedPlane parameter ranges must be finite and bounded.");
    }

public:
    AcGeBoundedPlane() = default;

    AcGeBoundedPlane(const AcGePoint3d &origin, const AcGeVector3d &normal,
                     const AcGeVector3d &uAxis, const AcGeInterval &uRange,
                     const AcGeInterval &vRange)
    {
        set(origin, normal, uAxis, uRange, vRange);
    }

    AcGeBoundedPlane &set(const AcGePoint3d &origin,
                          const AcGeVector3d &normal,
                          const AcGeVector3d &uAxis,
                          const AcGeInterval &uRange,
                          const AcGeInterval &vRange)
    {
        validateRange(uRange);
        validateRange(vRange);
        AcGeVector3d unitNormal;
        AcGeVector3d unitU;
        AcGeVector3d unitV;
        makeFrame(normal, uAxis, unitNormal, unitU, unitV);

        m_origin = origin;
        m_normal = unitNormal;
        m_uAxis = unitU;
        m_vAxis = unitV;
        m_uRange = uRange;
        m_vRange = vRange;
        return *this;
    }

    AcGePoint3d origin() const { return m_origin; }
    AcGeVector3d normal() const { return m_normal; }
    AcGeVector3d uAxis() const { return m_uAxis; }
    AcGeVector3d vAxis() const { return m_vAxis; }
    AcGeInterval uRange() const { return m_uRange; }
    AcGeInterval vRange() const { return m_vRange; }

    AcGePoint3d pointAt(double u, double v) const
    {
        return m_origin + m_uAxis * u + m_vAxis * v;
    }

    AcGeVector3d normalAt(double, double) const { return m_normal; }

    double area() const { return m_uRange.length() * m_vRange.length(); }

    bool parametersOf(const AcGePoint3d &point, double &u, double &v,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        const AcGeVector3d offset = point - m_origin;
        if (std::abs(offset.dotProduct(m_normal)) > tol.equalPoint())
            return false;
        u = offset.dotProduct(m_uAxis);
        v = offset.dotProduct(m_vAxis);
        return true;
    }

    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const
    {
        double u = 0.0;
        double v = 0.0;
        return parametersOf(point, u, v, tol) && m_uRange.contains(u) &&
               m_vRange.contains(v);
    }

    AcGePoint3d closestPointTo(const AcGePoint3d &point) const
    {
        const AcGeVector3d offset = point - m_origin;
        const double u = std::clamp(offset.dotProduct(m_uAxis),
                                    m_uRange.lowerBound(),
                                    m_uRange.upperBound());
        const double v = std::clamp(offset.dotProduct(m_vAxis),
                                    m_vRange.lowerBound(),
                                    m_vRange.upperBound());
        return pointAt(u, v);
    }
};
