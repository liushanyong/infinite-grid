#include "acge/AcGeSurface3d.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// AcGeSphere
// ---------------------------------------------------------------------------

void AcGeSphere::setFrame(const AcGeVector3d &north, const AcGeVector3d &reference)
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

AcGeSphere::AcGeSphere(double radius, const AcGePoint3d &center,
                       const AcGeVector3d &northAxis, const AcGeVector3d &refAxis,
                       double startU, double endU, double startV, double endV)
    : m_radius(std::abs(radius)), m_center(center),
      m_startAngleU(startU), m_endAngleU(endU),
      m_startAngleV(startV), m_endAngleV(endV)
{
    setFrame(northAxis, refAxis);
}

AcGeSphere &AcGeSphere::set(double radius, const AcGePoint3d &center)
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

AcGeSphere &AcGeSphere::set(double radius, const AcGePoint3d &center,
                            const AcGeVector3d &northAxis,
                            const AcGeVector3d &refAxis, double startU,
                            double endU, double startV, double endV)
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

AcGePoint3d AcGeSphere::pointAt(double u, double v) const
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

// ---------------------------------------------------------------------------
// AcGeCylinder
// ---------------------------------------------------------------------------

void AcGeCylinder::setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
{
    m_axis = axis.normal();
    if (m_axis.isZeroLength())
        m_axis = AcGeVector3d::kZAxis;

    m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
    if (m_refAxis.isZeroLength())
        m_refAxis = m_axis.perpVector();
    m_refAxis.normalize();
}

AcGeCylinder::AcGeCylinder(double radius, const AcGePoint3d &origin,
                           const AcGeVector3d &axis)
    : m_radius(std::abs(radius)), m_origin(origin)
{
    setFrame(axis, axis.perpVector());
}

AcGeCylinder::AcGeCylinder(double radius, const AcGePoint3d &origin,
                           const AcGeVector3d &axis, const AcGeVector3d &refAxis,
                           const AcGeInterval &height, double startAngle,
                           double endAngle)
    : m_radius(std::abs(radius)), m_origin(origin), m_height(height),
      m_startAngle(startAngle), m_endAngle(endAngle)
{
    setFrame(axis, refAxis);
}

AcGeCylinder &AcGeCylinder::set(double radius, const AcGePoint3d &origin,
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

AcGeCylinder &AcGeCylinder::set(double radius, const AcGePoint3d &origin,
                                const AcGeVector3d &axis,
                                const AcGeVector3d &refAxis,
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

AcGePoint3d AcGeCylinder::pointAt(double angle, double height) const
{
    const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
    const AcGeVector3d radial =
        m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
    return m_origin + m_axis * height + radial * m_radius;
}

AcGeVector3d AcGeCylinder::normalAt(double angle, double) const
{
    return (m_refAxis * std::cos(angle) +
            m_axis.crossProduct(m_refAxis) * std::sin(angle))
        .normal();
}

// ---------------------------------------------------------------------------
// AcGeTorus
// ---------------------------------------------------------------------------

void AcGeTorus::setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
{
    m_axis = axis.normal();
    if (m_axis.isZeroLength())
        m_axis = AcGeVector3d::kZAxis;

    m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
    if (m_refAxis.isZeroLength())
        m_refAxis = m_axis.perpVector();
    m_refAxis.normalize();
}

AcGeTorus::AcGeTorus(double majorRadius, double minorRadius,
                     const AcGePoint3d &center, const AcGeVector3d &axis)
    : m_majorRadius(std::abs(majorRadius)),
      m_minorRadius(std::abs(minorRadius)), m_center(center)
{
    setFrame(axis, axis.perpVector());
}

AcGeTorus::AcGeTorus(double majorRadius, double minorRadius,
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

AcGeTorus &AcGeTorus::set(double majorRadius, double minorRadius,
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

AcGeTorus &AcGeTorus::set(double majorRadius, double minorRadius,
                          const AcGePoint3d &center, const AcGeVector3d &axis,
                          const AcGeVector3d &refAxis, double startU,
                          double endU, double startV, double endV)
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

AcGePoint3d AcGeTorus::pointAt(double u, double v) const
{
    const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
    const AcGeVector3d radial =
        m_refAxis * std::cos(u) + sideAxis * std::sin(u);
    const double ringRadius = m_majorRadius + m_minorRadius * std::cos(v);
    return m_center + radial * ringRadius +
           m_axis * (m_minorRadius * std::sin(v));
}

AcGeVector3d AcGeTorus::normalAt(double u, double v) const
{
    const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
    const AcGeVector3d radial =
        m_refAxis * std::cos(u) + sideAxis * std::sin(u);
    return (radial * std::cos(v) + m_axis * std::sin(v)).normal();
}

// ---------------------------------------------------------------------------
// AcGeCone
// ---------------------------------------------------------------------------

void AcGeCone::setFrame(const AcGeVector3d &axis, const AcGeVector3d &reference)
{
    m_axis = axis.normal();
    if (m_axis.isZeroLength())
        m_axis = AcGeVector3d::kZAxis;

    m_refAxis = reference - m_axis * reference.dotProduct(m_axis);
    if (m_refAxis.isZeroLength())
        m_refAxis = m_axis.perpVector();
    m_refAxis.normalize();
}

double AcGeCone::apexHeight() const
{
    const double sine = std::sin(m_halfAngle);
    return std::abs(sine) <= 1.0e-15
               ? 0.0
               : m_baseRadius * std::cos(m_halfAngle) / sine;
}

double AcGeCone::radiusAtHeight(double height) const
{
    return std::max(0.0, m_baseRadius -
                                  height * std::tan(m_halfAngle));
}

AcGeCone::AcGeCone(double cosineAngle, double sineAngle,
                   const AcGePoint3d &baseCenter, double baseRadius,
                   const AcGeVector3d &axis)
    : m_baseCenter(baseCenter), m_baseRadius(std::abs(baseRadius))
{
    setAngle(cosineAngle, sineAngle);
    setFrame(axis, axis.perpVector());
    m_height.set(0.0, apexHeight());
}

AcGeCone::AcGeCone(double cosineAngle, double sineAngle,
                   const AcGePoint3d &baseCenter, double baseRadius,
                   const AcGeVector3d &axis, const AcGeVector3d &refAxis,
                   const AcGeInterval &height, double startAngle,
                   double endAngle)
    : m_baseCenter(baseCenter), m_baseRadius(std::abs(baseRadius)),
      m_height(height), m_startAngle(startAngle), m_endAngle(endAngle)
{
    setAngle(cosineAngle, sineAngle);
    setFrame(axis, refAxis);
}

AcGeCone &AcGeCone::set(double cosineAngle, double sineAngle,
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

AcGeCone &AcGeCone::set(double cosineAngle, double sineAngle,
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

AcGePoint3d AcGeCone::pointAt(double angle, double height) const
{
    const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
    const AcGeVector3d radial =
        m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
    return m_baseCenter + m_axis * height +
           radial * radiusAtHeight(height);
}

AcGeVector3d AcGeCone::normalAt(double angle, double) const
{
    const AcGeVector3d sideAxis = m_axis.crossProduct(m_refAxis);
    const AcGeVector3d radial =
        m_refAxis * std::cos(angle) + sideAxis * std::sin(angle);
    return (radial * std::cos(m_halfAngle) +
            m_axis * std::sin(m_halfAngle))
        .normal();
}
