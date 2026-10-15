#pragma once

#include <algorithm>
#include <cmath>

#include "AcGePoint.h"
#include "AcGeInterval.h"

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

struct AcGeCylinder
{
private:
    double m_radius = 1.0;
    AcGePoint3d m_origin{0.0, 0.0, 0.0};
    AcGeVector3d m_axis{0.0, 0.0, 1.0};
    AcGeVector3d m_refAxis{1.0, 0.0, 0.0};
    AcGeInterval m_height{};
    double m_startAngle = 0.0;
    double m_endAngle = 6.2831853071795864769;

    void setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
    {
        m_axis = axis.normal();
        if (m_axis.isZeroLength())
            m_axis = AcGeVector3d::kZAxis;

        m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
        if (m_refAxis.isZeroLength())
            m_refAxis = m_axis.perpVector();
        m_refAxis.normalize();
    }

public:
    AcGeCylinder() = default;
    AcGeCylinder(double radius, const AcGePoint3d &origin,
                 const AcGeVector3d &axis)
        : m_radius(std::abs(radius)), m_origin(origin)
    {
        setFrame(axis, axis.perpVector());
    }
    AcGeCylinder(double radius, const AcGePoint3d &origin,
                 const AcGeVector3d &axis, const AcGeVector3d &refAxis,
                 const AcGeInterval &height, double startAngle,
                 double endAngle)
        : m_radius(std::abs(radius)), m_origin(origin), m_height(height),
          m_startAngle(startAngle), m_endAngle(endAngle)
    {
        setFrame(axis, refAxis);
    }

    AcGeCylinder &set(double radius, const AcGePoint3d &origin,
                      const AcGeVector3d &axis)
    {
        m_radius = std::abs(radius);
        m_origin = origin;
        setFrame(axis, axis.perpVector());
        m_height.set();
        m_startAngle = 0.0;
        m_endAngle = 6.2831853071795864769;
        return *this;
    }
    AcGeCylinder &set(double radius, const AcGePoint3d &origin,
                      const AcGeVector3d &axis, const AcGeVector3d &refAxis,
                      const AcGeInterval &height, double startAngle,
                      double endAngle)
    {
        m_radius = std::abs(radius);
        m_origin = origin;
        setFrame(axis, refAxis);
        m_height = height;
        m_startAngle = startAngle;
        m_endAngle = endAngle;
        return *this;
    }

    double radius() const { return m_radius; }
    AcGePoint3d origin() const { return m_origin; }
    AcGeVector3d axisOfSymmetry() const { return m_axis; }
    AcGeVector3d refAxis() const { return m_refAxis; }
    void getHeight(AcGeInterval &height) const { height = m_height; }
    AcGeCylinder &setHeight(const AcGeInterval &height)
    {
        m_height = height;
        return *this;
    }
    void getAngles(double &start, double &end) const
    {
        start = m_startAngle;
        end = m_endAngle;
    }
    AcGeCylinder &setAngles(double start, double end)
    {
        m_startAngle = start;
        m_endAngle = end;
        return *this;
    }
    AcGeCylinder &setRadius(double radius)
    {
        m_radius = std::abs(radius);
        return *this;
    }

    AcGePoint3d pointAt(double angle, double height) const
    {
        const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
        const AcGeVector3d radial =
            m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
        return m_origin + m_axis * height + radial * m_radius;
    }
    AcGeVector3d normalAt(double angle, double) const
    {
        return (m_refAxis * std::cos(angle) +
                m_axis.crossProduct(m_refAxis) * std::sin(angle))
            .normal();
    }
    bool isClosed(const AcGeTol &tol = AcGeTol{}) const
    {
        constexpr double twoPi = 6.2831853071795864769;
        return std::abs((m_endAngle - m_startAngle) - twoPi) <=
               tol.equalVector();
    }
};

struct AcGeTorus
{
private:
    double m_majorRadius = 2.0;
    double m_minorRadius = 1.0;
    AcGePoint3d m_center{0.0, 0.0, 0.0};
    AcGeVector3d m_axis{0.0, 0.0, 1.0};
    AcGeVector3d m_refAxis{1.0, 0.0, 0.0};
    double m_startAngleU = 0.0;
    double m_endAngleU = 6.2831853071795864769;
    double m_startAngleV = 0.0;
    double m_endAngleV = 6.2831853071795864769;

    void setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
    {
        m_axis = axis.normal();
        if (m_axis.isZeroLength())
            m_axis = AcGeVector3d::kZAxis;

        m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
        if (m_refAxis.isZeroLength())
            m_refAxis = m_axis.perpVector();
        m_refAxis.normalize();
    }

public:
    AcGeTorus() = default;
    AcGeTorus(double majorRadius, double minorRadius,
              const AcGePoint3d &center, const AcGeVector3d &axis)
        : m_majorRadius(std::abs(majorRadius)),
          m_minorRadius(std::abs(minorRadius)), m_center(center)
    {
        setFrame(axis, axis.perpVector());
    }
    AcGeTorus(double majorRadius, double minorRadius,
              const AcGePoint3d &center, const AcGeVector3d &axis,
              const AcGeVector3d &refAxis, double startU, double endU,
              double startV, double endV)
        : m_majorRadius(std::abs(majorRadius)),
          m_minorRadius(std::abs(minorRadius)), m_center(center),
          m_startAngleU(startU), m_endAngleU(endU),
          m_startAngleV(startV), m_endAngleV(endV)
    {
        setFrame(axis, refAxis);
    }

    AcGeTorus &set(double majorRadius, double minorRadius,
                   const AcGePoint3d &center, const AcGeVector3d &axis)
    {
        m_majorRadius = std::abs(majorRadius);
        m_minorRadius = std::abs(minorRadius);
        m_center = center;
        setFrame(axis, axis.perpVector());
        m_startAngleU = 0.0;
        m_endAngleU = 6.2831853071795864769;
        m_startAngleV = 0.0;
        m_endAngleV = 6.2831853071795864769;
        return *this;
    }
    AcGeTorus &set(double majorRadius, double minorRadius,
                   const AcGePoint3d &center, const AcGeVector3d &axis,
                   const AcGeVector3d &refAxis, double startU, double endU,
                   double startV, double endV)
    {
        m_majorRadius = std::abs(majorRadius);
        m_minorRadius = std::abs(minorRadius);
        m_center = center;
        setFrame(axis, refAxis);
        m_startAngleU = startU;
        m_endAngleU = endU;
        m_startAngleV = startV;
        m_endAngleV = endV;
        return *this;
    }

    double majorRadius() const { return m_majorRadius; }
    double minorRadius() const { return m_minorRadius; }
    AcGePoint3d center() const { return m_center; }
    AcGeVector3d axisOfSymmetry() const { return m_axis; }
    AcGeVector3d refAxis() const { return m_refAxis; }
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
    AcGeTorus &setAnglesInU(double start, double end)
    {
        m_startAngleU = start;
        m_endAngleU = end;
        return *this;
    }
    AcGeTorus &setAnglesInV(double start, double end)
    {
        m_startAngleV = start;
        m_endAngleV = end;
        return *this;
    }
    AcGeTorus &setMajorRadius(double radius)
    {
        m_majorRadius = std::abs(radius);
        return *this;
    }
    AcGeTorus &setMinorRadius(double radius)
    {
        m_minorRadius = std::abs(radius);
        return *this;
    }

    AcGePoint3d pointAt(double u, double v) const
    {
        const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
        const AcGeVector3d radial =
            m_refAxis * std::cos(u) + sideAxis * std::sin(u);
        const double ringRadius = m_majorRadius + m_minorRadius * std::cos(v);
        return m_center + radial * ringRadius +
               m_axis * (m_minorRadius * std::sin(v));
    }
    AcGeVector3d normalAt(double u, double v) const
    {
        const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
        const AcGeVector3d radial =
            m_refAxis * std::cos(u) + sideAxis * std::sin(u);
        return (radial * std::cos(v) + m_axis * std::sin(v)).normal();
    }

    bool isDoughnut() const { return m_majorRadius > m_minorRadius; }
    bool isHollow() const { return isDoughnut(); }
    bool isVortex(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(m_majorRadius - m_minorRadius) <= tol.equalPoint();
    }
    bool isDegenerate() const
    {
        return m_minorRadius <= 0.0 ||
               m_majorRadius <= -m_minorRadius;
    }
};

struct AcGeCone
{
private:
    double m_halfAngle = 0.78539816339744830962;
    AcGePoint3d m_baseCenter{0.0, 0.0, 0.0};
    double m_baseRadius = 1.0;
    AcGeVector3d m_axis{0.0, 0.0, 1.0};
    AcGeVector3d m_refAxis{1.0, 0.0, 0.0};
    AcGeInterval m_height{0.0, 1.0};
    double m_startAngle = 0.0;
    double m_endAngle = 6.2831853071795864769;

    void setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
    {
        m_axis = axis.normal();
        if (m_axis.isZeroLength())
            m_axis = AcGeVector3d::kZAxis;

        m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
        if (m_refAxis.isZeroLength())
            m_refAxis = m_axis.perpVector();
        m_refAxis.normalize();
    }
    void setAngle(double cosine, double sine)
    {
        m_halfAngle = std::atan2(std::abs(sine), cosine);
    }
    double apexHeight() const
    {
        const double sine = std::sin(m_halfAngle);
        return std::abs(sine) <= 1.0e-15
                   ? 0.0
                   : m_baseRadius * std::cos(m_halfAngle) / sine;
    }
    double radiusAtHeight(double height) const
    {
        return std::max(0.0, m_baseRadius -
                                  height * std::tan(m_halfAngle));
    }

public:
    AcGeCone() = default;
    AcGeCone(double cosineAngle, double sineAngle,
             const AcGePoint3d &baseCenter, double baseRadius,
             const AcGeVector3d &axis)
        : m_baseCenter(baseCenter), m_baseRadius(std::abs(baseRadius))
    {
        setAngle(cosineAngle, sineAngle);
        setFrame(axis, axis.perpVector());
        m_height.set(0.0, apexHeight());
    }
    AcGeCone(double cosineAngle, double sineAngle,
             const AcGePoint3d &baseCenter, double baseRadius,
             const AcGeVector3d &axis, const AcGeVector3d &refAxis,
             const AcGeInterval &height, double startAngle, double endAngle)
        : m_baseCenter(baseCenter), m_baseRadius(std::abs(baseRadius)),
          m_height(height), m_startAngle(startAngle), m_endAngle(endAngle)
    {
        setAngle(cosineAngle, sineAngle);
        setFrame(axis, refAxis);
    }

    AcGeCone &set(double cosineAngle, double sineAngle,
                  const AcGePoint3d &baseCenter, double baseRadius,
                  const AcGeVector3d &axis)
    {
        m_baseCenter = baseCenter;
        m_baseRadius = std::abs(baseRadius);
        setAngle(cosineAngle, sineAngle);
        setFrame(axis, axis.perpVector());
        m_height.set(0.0, apexHeight());
        m_startAngle = 0.0;
        m_endAngle = 6.2831853071795864769;
        return *this;
    }
    AcGeCone &set(double cosineAngle, double sineAngle,
                  const AcGePoint3d &baseCenter, double baseRadius,
                  const AcGeVector3d &axis, const AcGeVector3d &refAxis,
                  const AcGeInterval &height, double startAngle,
                  double endAngle)
    {
        m_baseCenter = baseCenter;
        m_baseRadius = std::abs(baseRadius);
        setAngle(cosineAngle, sineAngle);
        setFrame(axis, refAxis);
        m_height = height;
        m_startAngle = startAngle;
        m_endAngle = endAngle;
        return *this;
    }

    AcGePoint3d baseCenter() const { return m_baseCenter; }
    AcGePoint3d apex() const
    {
        return m_baseCenter + m_axis * apexHeight();
    }
    double baseRadius() const { return m_baseRadius; }
    AcGeVector3d axisOfSymmetry() const { return m_axis; }
    AcGeVector3d refAxis() const { return m_refAxis; }
    double halfAngle() const { return m_halfAngle; }
    void getHalfAngle(double &cosine, double &sine) const
    {
        cosine = std::cos(m_halfAngle);
        sine = std::sin(m_halfAngle);
    }
    void getHeight(AcGeInterval &height) const { height = m_height; }
    AcGeCone &setHeight(const AcGeInterval &height)
    {
        m_height = height;
        return *this;
    }
    void getAngles(double &start, double &end) const
    {
        start = m_startAngle;
        end = m_endAngle;
    }
    AcGeCone &setAngles(double start, double end)
    {
        m_startAngle = start;
        m_endAngle = end;
        return *this;
    }
    AcGeCone &setBaseRadius(double radius)
    {
        m_baseRadius = std::abs(radius);
        return *this;
    }

    AcGePoint3d pointAt(double angle, double height) const
    {
        const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
        const AcGeVector3d radial =
            m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
        return m_baseCenter + m_axis * height +
               radial * radiusAtHeight(height);
    }
    AcGeVector3d normalAt(double angle, double) const
    {
        const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
        const AcGeVector3d radial =
            m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
        return (radial * std::cos(m_halfAngle) +
                m_axis * std::sin(m_halfAngle))
            .normal();
    }
    bool isClosed(const AcGeTol &tol = AcGeTol{}) const
    {
        constexpr double twoPi = 6.2831853071795864769;
        return std::abs((m_endAngle - m_startAngle) - twoPi) <=
               tol.equalVector();
    }
};
