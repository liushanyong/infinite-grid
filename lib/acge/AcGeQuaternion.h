#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>

#include "AcGeMatrix.h"

// AcGe-compatible quaternion for 3D rotations. Components use the common
// vector-first (x, y, z, w) layout while multiplication follows Hamilton order.
struct ACGE_PORT AcGeQuaternion3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;

    AcGeQuaternion3d() = default;
    AcGeQuaternion3d(double xx, double yy, double zz, double ww)
        : x(xx), y(yy), z(zz), w(ww)
    {
    }
    AcGeQuaternion3d(const AcGeVector3d &axis, double angle)
    {
        setToRotation(axis, angle);
    }

    static AcGeQuaternion3d identity() { return {}; }

    static AcGeQuaternion3d fromAxisAngle(const AcGeVector3d &axis,
                                          double angle);
    AcGeQuaternion3d &setToRotation(const AcGeVector3d &axis, double angle);

    double normSq() const { return x * x + y * y + z * z + w * w; }
    double norm() const { return std::sqrt(normSq()); }
    double dotProduct(const AcGeQuaternion3d &other) const
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    AcGeQuaternion3d &normalize()
    {
        const double length = norm();
        if (length > 0.0)
        {
            x /= length;
            y /= length;
            z /= length;
            w /= length;
        }
        return *this;
    }

    AcGeQuaternion3d normal() const
    {
        AcGeQuaternion3d result = *this;
        result.normalize();
        return result;
    }

    AcGeQuaternion3d conjugate() const { return {-x, -y, -z, w}; }

    AcGeQuaternion3d inverse() const
    {
        const double squaredLength = normSq();
        if (squaredLength == 0.0)
            return identity();
        const AcGeQuaternion3d conjugated = conjugate();
        return {conjugated.x / squaredLength, conjugated.y / squaredLength,
                conjugated.z / squaredLength, conjugated.w / squaredLength};
    }

    double angle() const
    {
        const AcGeQuaternion3d unit = normal();
        return 2.0 * std::acos(std::clamp(std::abs(unit.w), 0.0, 1.0));
    }

    AcGeVector3d axis() const;
    AcGeVector3d rotateVector(const AcGeVector3d &vector) const;
    AcGeMatrix3d toMatrix() const;

    static AcGeQuaternion3d slerp(const AcGeQuaternion3d &from,
                                  const AcGeQuaternion3d &to, double t);

    AcGeQuaternion3d operator-() const { return {-x, -y, -z, -w}; }
    AcGeQuaternion3d operator+(const AcGeQuaternion3d &other) const
    {
        return {x + other.x, y + other.y, z + other.z, w + other.w};
    }
    AcGeQuaternion3d operator*(double scale) const
    {
        return {x * scale, y * scale, z * scale, w * scale};
    }
    AcGeQuaternion3d operator*(const AcGeQuaternion3d &other) const
    {
        return {w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w,
                w * other.w - x * other.x - y * other.y - z * other.z};
    }
};

inline AcGeQuaternion3d operator*(double scale,
                                  const AcGeQuaternion3d &quaternion)
{
    return quaternion * scale;
}
