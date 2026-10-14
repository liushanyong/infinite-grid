#pragma once

#include <algorithm>
#include <cmath>

#include "gepoint.h"

// Source-level AcGeSphere subset for a spherical surface or angular segment.
struct AcGeSphere
{
private:
    double m_radius = 1.0;
    AcGePoint3d m_center{0.0, 0.0, 0.0};
    AcGeVector3d m_northAxis{0.0, 0.0, 1.0};
    AcGeVector3d m_refAxis{1.0, 0.0, 0.0};
    double m_startAngleU = 0.0;
    double m_endAngleU = 6.2831853071795864769;
    double m_startAngleV = -1.5707963267948966192;
    double m_endAngleV = 1.5707963267948966192;

    void setFrame(const AcGeVector3d &north, const AcGeVector3d &reference)
    {
        m_northAxis = north.normal();
        if (m_northAxis.isZeroLength())
            m_northAxis = AcGeVector3d::kZAxis;

        m_refAxis = reference -
                    m_northAxis * reference.dotProduct(m_northAxis);
        if (m_refAxis.isZeroLength())
            m_refAxis = m_northAxis.perpVector();
        m_refAxis.normalize();
    }

public:
    AcGeSphere() = default;
    AcGeSphere(double radius, const AcGePoint3d &center)
        : m_radius(std::abs(radius)), m_center(center)
    {
    }
    AcGeSphere(double radius, const AcGePoint3d &center,
               const AcGeVector3d &northAxis, const AcGeVector3d &refAxis,
               double startU, double endU, double startV, double endV)
        : m_radius(std::abs(radius)), m_center(center),
          m_startAngleU(startU), m_endAngleU(endU),
          m_startAngleV(startV), m_endAngleV(endV)
    {
        setFrame(northAxis, refAxis);
    }

    AcGeSphere &set(double radius, const AcGePoint3d &center)
    {
        m_radius = std::abs(radius);
        m_center = center;
        m_northAxis = AcGeVector3d::kZAxis;
        m_refAxis = AcGeVector3d::kXAxis;
        m_startAngleU = 0.0;
        m_endAngleU = 6.2831853071795864769;
        m_startAngleV = -1.5707963267948966192;
        m_endAngleV = 1.5707963267948966192;
        return *this;
    }

    AcGeSphere &set(double radius, const AcGePoint3d &center,
                    const AcGeVector3d &northAxis,
                    const AcGeVector3d &refAxis, double startU, double endU,
                    double startV, double endV)
    {
        m_radius = std::abs(radius);
        m_center = center;
        setFrame(northAxis, refAxis);
        m_startAngleU = startU;
        m_endAngleU = endU;
        m_startAngleV = startV;
        m_endAngleV = endV;
        return *this;
    }

    double radius() const { return m_radius; }
    AcGePoint3d center() const { return m_center; }
    AcGeVector3d northAxis() const { return m_northAxis; }
    AcGeVector3d refAxis() const { return m_refAxis; }
    AcGePoint3d northPole() const
    {
        return m_center + m_northAxis * m_radius;
    }
    AcGePoint3d southPole() const
    {
        return m_center - m_northAxis * m_radius;
    }

    void getAnglesInU(double &start, double &end) const
    {
        start = m_startAngleU;
        end = m_endAngleU;
    }
    void getAnglesInV(double &start, double &end) const
    {
        start = m_startAngleV;
        end = m_endAngleV;
    }
    AcGeSphere &setAnglesInU(double start, double end)
    {
        m_startAngleU = start;
        m_endAngleU = end;
        return *this;
    }
    AcGeSphere &setAnglesInV(double start, double end)
    {
        m_startAngleV = start;
        m_endAngleV = end;
        return *this;
    }
    AcGeSphere &setRadius(double radius)
    {
        m_radius = std::abs(radius);
        return *this;
    }

    AcGePoint3d pointAt(double u, double v) const
    {
        const AcGeVector3d sideAxis =
            m_northAxis.crossProduct(m_refAxis).normal();
        const double cosV = std::cos(v);
        const AcGeVector3d radial =
            m_refAxis * (cosV * std::cos(u)) +
            sideAxis * (cosV * std::sin(u)) +
            m_northAxis * std::sin(v);
        return m_center + radial * m_radius;
    }

    AcGeVector3d normalAt(double u, double v) const
    {
        return (pointAt(u, v) - m_center).normal();
    }

    bool isClosed(const AcGeTol &tol = AcGeTol{}) const
    {
        constexpr double twoPi = 6.2831853071795864769;
        constexpr double pi = 3.14159265358979323846;
        return std::abs((m_endAngleU - m_startAngleU) - twoPi) <=
                   tol.equalVector() &&
               std::abs((m_endAngleV - m_startAngleV) - pi) <=
                   tol.equalVector();
    }
};
