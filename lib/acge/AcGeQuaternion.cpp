#include "acge/AcGeQuaternion.h"

AcGeQuaternion3d AcGeQuaternion3d::fromAxisAngle(const AcGeVector3d &axis,
                                                 double angle)
{
    AcGeQuaternion3d result;
    result.setToRotation(axis, angle);
    return result;
}

AcGeQuaternion3d &AcGeQuaternion3d::setToRotation(
    const AcGeVector3d &axis, double angle)
{
    const AcGeVector3d unitAxis = axis.normal();
    if (unitAxis.isZeroLength())
    {
        x = y = z = 0.0;
        w = 1.0;
        return *this;
    }
    const double halfAngle = angle * 0.5;
    const double sine = std::sin(halfAngle);
    x = unitAxis.x * sine;
    y = unitAxis.y * sine;
    z = unitAxis.z * sine;
    w = std::cos(halfAngle);
    return *this;
}

AcGeVector3d AcGeQuaternion3d::axis() const
{
    const AcGeQuaternion3d unit = normal();
    const double sine = std::sqrt(std::max(0.0, 1.0 - unit.w * unit.w));
    if (sine <= 1.0e-12)
        return AcGeVector3d::kXAxis;
    return AcGeVector3d{unit.x / sine, unit.y / sine, unit.z / sine};
}

AcGeVector3d AcGeQuaternion3d::rotateVector(
    const AcGeVector3d &vector) const
{
    const AcGeQuaternion3d unit = normal();
    const AcGeVector3d imaginary{unit.x, unit.y, unit.z};
    const AcGeVector3d twiceCross = imaginary.crossProduct(vector) * 2.0;
    return vector + twiceCross * unit.w +
           imaginary.crossProduct(twiceCross);
}

AcGeMatrix3d AcGeQuaternion3d::toMatrix() const
{
    const AcGeQuaternion3d q = normal();
    const double xx = q.x * q.x;
    const double yy = q.y * q.y;
    const double zz = q.z * q.z;
    const double xy = q.x * q.y;
    const double xz = q.x * q.z;
    const double yz = q.y * q.z;
    const double wx = q.w * q.x;
    const double wy = q.w * q.y;
    const double wz = q.w * q.z;

    AcGeMatrix3d result;
    result.m[0][0] = 1.0 - 2.0 * (yy + zz);
    result.m[0][1] = 2.0 * (xy + wz);
    result.m[0][2] = 2.0 * (xz - wy);
    result.m[1][0] = 2.0 * (xy - wz);
    result.m[1][1] = 1.0 - 2.0 * (xx + zz);
    result.m[1][2] = 2.0 * (yz + wx);
    result.m[2][0] = 2.0 * (xz + wy);
    result.m[2][1] = 2.0 * (yz - wx);
    result.m[2][2] = 1.0 - 2.0 * (xx + yy);
    return result;
}

AcGeQuaternion3d AcGeQuaternion3d::slerp(
    const AcGeQuaternion3d &from, const AcGeQuaternion3d &to, double t)
{
    AcGeQuaternion3d a = from.normal();
    AcGeQuaternion3d b = to.normal();
    double cosine = a.dotProduct(b);
    if (cosine < 0.0)
    {
        b = {-b.x, -b.y, -b.z, -b.w};
        cosine = -cosine;
    }
    if (cosine > 0.9995)
    {
        return AcGeQuaternion3d{
                   a.x + t * (b.x - a.x), a.y + t * (b.y - a.y),
                   a.z + t * (b.z - a.z), a.w + t * (b.w - a.w)}
            .normal();
    }

    const double angle = std::acos(std::clamp(cosine, -1.0, 1.0));
    const double sine = std::sin(angle);
    const double weightA = std::sin((1.0 - t) * angle) / sine;
    const double weightB = std::sin(t * angle) / sine;
    return {a.x * weightA + b.x * weightB,
            a.y * weightA + b.y * weightB,
            a.z * weightA + b.z * weightB,
            a.w * weightA + b.w * weightB};
}
