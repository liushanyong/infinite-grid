#include "acge/AcGeBoundedPlane.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

void AcGeBoundedPlane::makeFrame(const AcGeVector3d &normal,
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

void AcGeBoundedPlane::validateRange(const AcGeInterval &range)
{
    if (!range.isBounded() || !std::isfinite(range.lowerBound()) ||
        !std::isfinite(range.upperBound()))
        throw std::invalid_argument(
            "AcGeBoundedPlane parameter ranges must be finite and bounded.");
}

AcGeBoundedPlane::AcGeBoundedPlane(const AcGePoint3d &origin,
                                   const AcGeVector3d &normal,
                                   const AcGeVector3d &uAxis,
                                   const AcGeInterval &uRange,
                                   const AcGeInterval &vRange)
{
    set(origin, normal, uAxis, uRange, vRange);
}

AcGeBoundedPlane &AcGeBoundedPlane::set(const AcGePoint3d &origin,
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

bool AcGeBoundedPlane::parametersOf(const AcGePoint3d &point, double &u,
                                    double &v, const AcGeTol &tol) const
{
    const AcGeVector3d offset = point - m_origin;
    if (std::abs(offset.dotProduct(m_normal)) > tol.equalPoint())
        return false;
    u = offset.dotProduct(m_uAxis);
    v = offset.dotProduct(m_vAxis);
    return true;
}

bool AcGeBoundedPlane::isInside(const AcGePoint3d &point,
                                const AcGeTol &tol) const
{
    double u = 0.0;
    double v = 0.0;
    return parametersOf(point, u, v, tol) && m_uRange.contains(u) &&
           m_vRange.contains(v);
}

AcGePoint3d AcGeBoundedPlane::closestPointTo(const AcGePoint3d &point) const
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
