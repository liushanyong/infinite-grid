#pragma once

// AcGe-compatible 2D/3D point and vector value types, signature-aligned
// with the ObjectARX geometry kernel (see ObjectARX docs: AcGe Overview).
// The storage is hand-rolled doubles (matching the AcGe member layout so
// ported entity code can read .x/.y/.z directly); glm interop is provided
// through implicit conversions for the tessellation bridge.

#include <glm/glm.hpp>
#include <type_traits>

struct AcGeVector2d
{
    double x = 0.0;
    double y = 0.0;

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
    double length() const { return std::sqrt(lengthSq()); }

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

struct AcGeVector3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

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
    double length() const { return std::sqrt(lengthSq()); }

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

struct AcGePoint2d
{
    double x = 0.0;
    double y = 0.0;

    AcGePoint2d() = default;
    AcGePoint2d(double xx, double yy) : x(xx), y(yy) {}
    AcGePoint2d(const glm::dvec2 &v) : x(v.x), y(v.y) {}
    operator glm::dvec2() const { return {x, y}; }

    double distanceTo(const AcGePoint2d &other) const
    {
        return AcGeVector2d(x - other.x, y - other.y).length();
    }

    AcGeVector2d asVector() const { return {x, y}; }
    AcGePoint2d operator+(const AcGeVector2d &o) const { return {x + o.x, y + o.y}; }
    AcGePoint2d operator-(const AcGeVector2d &o) const { return {x - o.x, y - o.y}; }
    AcGeVector2d operator-(const AcGePoint2d &o) const { return {x - o.x, y - o.y}; }
    AcGePoint2d &operator+=(const AcGeVector2d &o) { x += o.x; y += o.y; return *this; }
    AcGePoint2d &operator-=(const AcGeVector2d &o) { x -= o.x; y -= o.y; return *this; }
    bool operator==(const AcGePoint2d &o) const { return x == o.x && y == o.y; }
    bool operator!=(const AcGePoint2d &o) const { return !(*this == o); }
};

struct AcGePoint3d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    AcGePoint3d() = default;
    AcGePoint3d(double xx, double yy, double zz) : x(xx), y(yy), z(zz) {}
    AcGePoint3d(const glm::dvec3 &v) : x(v.x), y(v.y), z(v.z) {}
    operator glm::dvec3() const { return {x, y, z}; }

    double distanceTo(const AcGePoint3d &other) const
    {
        return AcGeVector3d(x - other.x, y - other.y, z - other.z).length();
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
