#pragma once

// AcGe-compatible 2D/3D point and vector value types, signature-aligned
// with the ObjectARX geometry kernel (see ObjectARX docs: AcGe Overview).
// The storage is hand-rolled doubles (matching the AcGe member layout so
// ported entity code can read .x/.y/.z directly); glm interop is provided
// through implicit conversions for the tessellation bridge.

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <type_traits>

#include "getol.h"

struct AcGeMatrix2d;

struct AcGeVector2d
{
    double x = 0.0;
    double y = 0.0;

    static const AcGeVector2d kIdentity;
    static const AcGeVector2d kXAxis;
    static const AcGeVector2d kYAxis;

    AcGeVector2d() = default;
    AcGeVector2d(double xx, double yy) : x(xx), y(yy) {}
    AcGeVector2d(const glm::dvec2 &v) : x(v.x), y(v.y) {}
    operator glm::dvec2() const { return {x, y}; }

    AcGeVector2d &set(double xx, double yy)
    {
        x = xx;
        y = yy;
        return *this;
    }

    double dotProduct(const AcGeVector2d &other) const
    {
        return x * other.x + y * other.y;
    }

    double lengthSq() const { return x * x + y * y; }
    double lengthSqrd() const { return lengthSq(); }
    double length() const { return std::sqrt(lengthSq()); }
    double angle() const
    {
        double result = std::atan2(y, x);
        return result < 0.0 ? result + 6.2831853071795864769 : result;
    }
    double angleTo(const AcGeVector2d &other) const
    {
        const double denominator = length() * other.length();
        if (denominator == 0.0)
            return 0.0;
        return std::acos(std::clamp(dotProduct(other) / denominator, -1.0, 1.0));
    }
    bool isEqualTo(const AcGeVector2d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return (*this - other).length() <= tol.equalVector();
    }
    bool isZeroLength(const AcGeTol &tol = AcGeTol{}) const
    {
        return length() <= tol.equalVector();
    }
    bool isUnitLength(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(length() - 1.0) <= tol.equalVector();
    }
    bool isParallelTo(const AcGeVector2d &other,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        if (isZeroLength(tol) || other.isZeroLength(tol))
            return false;
        AcGeVector2d normalizedThis = normal();
        AcGeVector2d normalizedOther = other.normal();
        if (normalizedThis.dotProduct(normalizedOther) < 0.0)
            normalizedOther.negate();
        return (normalizedThis - normalizedOther).length() <=
               tol.equalVector();
    }
    bool isPerpendicularTo(const AcGeVector2d &other,
                           const AcGeTol &tol = AcGeTol{}) const
    {
        return !isZeroLength(tol) && !other.isZeroLength(tol) &&
               std::abs(dotProduct(other)) <=
                   tol.equalVector() * length() * other.length();
    }
    bool isCodirectionalTo(const AcGeVector2d &other,
                           const AcGeTol &tol = AcGeTol{}) const
    {
        return isParallelTo(other, tol) && dotProduct(other) > 0.0;
    }
    AcGeVector2d perpVector() const { return {-y, x}; }
    AcGeVector2d &negate()
    {
        x = -x;
        y = -y;
        return *this;
    }

    AcGeVector2d &normalize()
    {
        const double len = length();
        if (len > 0.0)
        {
            x /= len;
            y /= len;
        }
        return *this;
    }

    AcGeVector2d normal() const
    {
        AcGeVector2d copy = *this;
        copy.normalize();
        return copy;
    }

    AcGeVector2d &transformBy(const AcGeMatrix2d &matrix);

    AcGeVector2d operator-() const { return {-x, -y}; }
    AcGeVector2d operator+(const AcGeVector2d &o) const { return {x + o.x, y + o.y}; }
    AcGeVector2d operator-(const AcGeVector2d &o) const { return {x - o.x, y - o.y}; }
    AcGeVector2d operator*(double s) const { return {x * s, y * s}; }
    AcGeVector2d operator/(double s) const { return {x / s, y / s}; }
    AcGeVector2d &operator+=(const AcGeVector2d &o) { x += o.x; y += o.y; return *this; }
    AcGeVector2d &operator-=(const AcGeVector2d &o) { x -= o.x; y -= o.y; return *this; }
    AcGeVector2d &operator*=(double s) { x *= s; y *= s; return *this; }
    bool operator==(const AcGeVector2d &o) const { return x == o.x && y == o.y; }
    bool operator!=(const AcGeVector2d &o) const { return !(*this == o); }
};

inline const AcGeVector2d AcGeVector2d::kIdentity{1.0, 1.0};
inline const AcGeVector2d AcGeVector2d::kXAxis{1.0, 0.0};
inline const AcGeVector2d AcGeVector2d::kYAxis{0.0, 1.0};

struct AcGeVector3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    static const AcGeVector3d kIdentity;
    static const AcGeVector3d kXAxis;
    static const AcGeVector3d kYAxis;
    static const AcGeVector3d kZAxis;

    AcGeVector3d() = default;
    AcGeVector3d(double xx, double yy, double zz) : x(xx), y(yy), z(zz) {}
    AcGeVector3d(const glm::dvec3 &v) : x(v.x), y(v.y), z(v.z) {}
    operator glm::dvec3() const { return {x, y, z}; }

    AcGeVector3d &set(double xx, double yy, double zz)
    {
        x = xx;
        y = yy;
        z = zz;
        return *this;
    }

    double dotProduct(const AcGeVector3d &other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }

    AcGeVector3d crossProduct(const AcGeVector3d &other) const
    {
        return {y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x};
    }

    double lengthSq() const { return dotProduct(*this); }
    double lengthSqrd() const { return lengthSq(); }
    double length() const { return std::sqrt(lengthSq()); }
    double angleTo(const AcGeVector3d &other) const
    {
        const double denominator = length() * other.length();
        if (denominator == 0.0)
            return 0.0;
        return std::acos(std::clamp(dotProduct(other) / denominator, -1.0, 1.0));
    }
    bool isEqualTo(const AcGeVector3d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return (*this - other).length() <= tol.equalVector();
    }
    bool isZeroLength(const AcGeTol &tol = AcGeTol{}) const
    {
        return length() <= tol.equalVector();
    }
    bool isUnitLength(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(length() - 1.0) <= tol.equalVector();
    }
    bool isParallelTo(const AcGeVector3d &other,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        if (isZeroLength(tol) || other.isZeroLength(tol))
            return false;
        AcGeVector3d normalizedThis = normal();
        AcGeVector3d normalizedOther = other.normal();
        if (normalizedThis.dotProduct(normalizedOther) < 0.0)
            normalizedOther.negate();
        return (normalizedThis - normalizedOther).length() <=
               tol.equalVector();
    }
    bool isPerpendicularTo(const AcGeVector3d &other,
                           const AcGeTol &tol = AcGeTol{}) const
    {
        return !isZeroLength(tol) && !other.isZeroLength(tol) &&
               std::abs(dotProduct(other)) <=
                   tol.equalVector() * length() * other.length();
    }
    bool isCodirectionalTo(const AcGeVector3d &other,
                           const AcGeTol &tol = AcGeTol{}) const
    {
        return isParallelTo(other, tol) && dotProduct(other) > 0.0;
    }
    AcGeVector3d perpVector() const
    {
        const AcGeVector3d seed =
            std::abs(x) < std::abs(y)
                ? (std::abs(x) < std::abs(z) ? AcGeVector3d{1.0, 0.0, 0.0}
                                             : AcGeVector3d{0.0, 0.0, 1.0})
                : (std::abs(y) < std::abs(z) ? AcGeVector3d{0.0, 1.0, 0.0}
                                             : AcGeVector3d{0.0, 0.0, 1.0});
        return crossProduct(seed);
    }
    AcGeVector3d &negate()
    {
        x = -x;
        y = -y;
        z = -z;
        return *this;
    }

    AcGeVector3d &normalize()
    {
        const double len = length();
        if (len > 0.0)
        {
            x /= len;
            y /= len;
            z /= len;
        }
        return *this;
    }

    AcGeVector3d normal() const
    {
        AcGeVector3d copy = *this;
        copy.normalize();
        return copy;
    }

    AcGeVector3d operator-() const { return {-x, -y, -z}; }
    AcGeVector3d operator+(const AcGeVector3d &o) const { return {x + o.x, y + o.y, z + o.z}; }
    AcGeVector3d operator-(const AcGeVector3d &o) const { return {x - o.x, y - o.y, z - o.z}; }
    AcGeVector3d operator*(double s) const { return {x * s, y * s, z * s}; }
    AcGeVector3d operator/(double s) const { return {x / s, y / s, z / s}; }
    AcGeVector3d &operator+=(const AcGeVector3d &o) { x += o.x; y += o.y; z += o.z; return *this; }
    AcGeVector3d &operator-=(const AcGeVector3d &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    AcGeVector3d &operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const AcGeVector3d &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const AcGeVector3d &o) const { return !(*this == o); }
};

inline const AcGeVector3d AcGeVector3d::kIdentity{1.0, 1.0, 1.0};
inline const AcGeVector3d AcGeVector3d::kXAxis{1.0, 0.0, 0.0};
inline const AcGeVector3d AcGeVector3d::kYAxis{0.0, 1.0, 0.0};
inline const AcGeVector3d AcGeVector3d::kZAxis{0.0, 0.0, 1.0};

struct AcGePoint2d
{
    double x = 0.0;
    double y = 0.0;

    static const AcGePoint2d kOrigin;

    AcGePoint2d() = default;
    AcGePoint2d(double xx, double yy) : x(xx), y(yy) {}
    AcGePoint2d(const glm::dvec2 &v) : x(v.x), y(v.y) {}
    operator glm::dvec2() const { return {x, y}; }

    double distanceTo(const AcGePoint2d &other) const
    {
        return AcGeVector2d(x - other.x, y - other.y).length();
    }
    bool isEqualTo(const AcGePoint2d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return distanceTo(other) <= tol.equalPoint();
    }

    AcGeVector2d asVector() const { return {x, y}; }
    AcGePoint2d &transformBy(const AcGeMatrix2d &matrix);
    AcGePoint2d operator+(const AcGeVector2d &o) const { return {x + o.x, y + o.y}; }
    AcGePoint2d operator-(const AcGeVector2d &o) const { return {x - o.x, y - o.y}; }
    AcGeVector2d operator-(const AcGePoint2d &o) const { return {x - o.x, y - o.y}; }
    AcGePoint2d &operator+=(const AcGeVector2d &o) { x += o.x; y += o.y; return *this; }
    AcGePoint2d &operator-=(const AcGeVector2d &o) { x -= o.x; y -= o.y; return *this; }
    bool operator==(const AcGePoint2d &o) const { return x == o.x && y == o.y; }
    bool operator!=(const AcGePoint2d &o) const { return !(*this == o); }
};

inline const AcGePoint2d AcGePoint2d::kOrigin{0.0, 0.0};

struct AcGePoint3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    static const AcGePoint3d kOrigin;

    AcGePoint3d() = default;
    AcGePoint3d(double xx, double yy, double zz) : x(xx), y(yy), z(zz) {}
    AcGePoint3d(const glm::dvec3 &v) : x(v.x), y(v.y), z(v.z) {}
    operator glm::dvec3() const { return {x, y, z}; }

    double distanceTo(const AcGePoint3d &other) const
    {
        return AcGeVector3d(x - other.x, y - other.y, z - other.z).length();
    }
    bool isEqualTo(const AcGePoint3d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return distanceTo(other) <= tol.equalPoint();
    }

    AcGeVector3d asVector() const { return {x, y, z}; }
    AcGePoint3d operator*(double s) const
    {
        return {x * s, y * s, z * s};
    }
    // Component-wise point addition (ObjectARX keeps point+point too).
    AcGePoint3d operator+(const AcGePoint3d &o) const
    {
        return {x + o.x, y + o.y, z + o.z};
    }
    // The vector overloads are templates constrained to AcGeVector3d so a
    // bare glm::dvec3 operand resolves uniquely to the point overload
    // (otherwise point + dvec3 would be ambiguous between the two).
    template <typename T,
              typename = std::enable_if_t<std::is_same_v<T, AcGeVector3d>>>
    AcGePoint3d operator+(const T &o) const
    {
        return {x + o.x, y + o.y, z + o.z};
    }
    template <typename T,
              typename = std::enable_if_t<std::is_same_v<T, AcGeVector3d>>>
    AcGePoint3d operator-(const T &o) const
    {
        return {x - o.x, y - o.y, z - o.z};
    }
    // AcGePoint3d::operator+=/-=(const AcGeVector3d&).
    AcGePoint3d &operator+=(const AcGeVector3d &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    AcGePoint3d &operator-=(const AcGeVector3d &o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    AcGeVector3d operator-(const AcGePoint3d &o) const
    {
        return {x - o.x, y - o.y, z - o.z};
    }
    template <typename T,
              typename = std::enable_if_t<std::is_same_v<T, AcGeVector3d>>>
    AcGePoint3d &operator+=(const T &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    template <typename T,
              typename = std::enable_if_t<std::is_same_v<T, AcGeVector3d>>>
    AcGePoint3d &operator-=(const T &o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    bool operator==(const AcGePoint3d &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const AcGePoint3d &o) const { return !(*this == o); }
};

inline const AcGePoint3d AcGePoint3d::kOrigin{0.0, 0.0, 0.0};
