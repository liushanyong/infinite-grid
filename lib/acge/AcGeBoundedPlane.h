#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "AcGeInterval.h"
#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGeBoundedPlane subset with an orthonormal parameter frame.
struct ACGE_PORT AcGeBoundedPlane
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
                          AcGeVector3d &unitV);
    static void validateRange(const AcGeInterval &range);

public:
    AcGeBoundedPlane() = default;

    AcGeBoundedPlane(const AcGePoint3d &origin, const AcGeVector3d &normal,
                     const AcGeVector3d &uAxis, const AcGeInterval &uRange,
                     const AcGeInterval &vRange);

    AcGeBoundedPlane &set(const AcGePoint3d &origin,
                          const AcGeVector3d &normal,
                          const AcGeVector3d &uAxis,
                          const AcGeInterval &uRange,
                          const AcGeInterval &vRange);

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
                      const AcGeTol &tol = AcGeTol{}) const;
    bool isInside(const AcGePoint3d &point,
                  const AcGeTol &tol = AcGeTol{}) const;
    AcGePoint3d closestPointTo(const AcGePoint3d &point) const;
};
