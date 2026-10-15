#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "AcGeMatrix2d.h"
#include "AcGeMatrix.h"

// Source-level AcGe scale-vector subset aligned with the ObjectARX 2026
// reference. Scale components are exposed as in the documented value types.
struct ACGE_PORT AcGeScale2d
{
    double sx = 1.0;
    double sy = 1.0;

    static const AcGeScale2d kIdentity;

    AcGeScale2d() = default;
    explicit AcGeScale2d(double uniformScale) : sx(uniformScale), sy(uniformScale) {}
    AcGeScale2d(double xScale, double yScale) : sx(xScale), sy(yScale) {}

    AcGeScale2d &set(double xScale, double yScale)
    {
        sx = xScale;
        sy = yScale;
        return *this;
    }
    AcGeScale2d inverse() const;
    AcGeScale2d &invert()
    {
        *this = inverse();
        return *this;
    }
    AcGeMatrix2d getMatrix() const
    {
        return {sx, 0.0, 0.0, 0.0, sy, 0.0};
    }
    bool isEqualTo(const AcGeScale2d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(sx - other.sx) <= tol.equalVector() &&
               std::abs(sy - other.sy) <= tol.equalVector();
    }
    bool isProportional(const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(sx - sy) <=
               tol.equalVector() * std::max({1.0, std::abs(sx), std::abs(sy)});
    }
    AcGeScale2d &postMultBy(const AcGeScale2d &other)
    {
        sx *= other.sx;
        sy *= other.sy;
        return *this;
    }
    AcGeScale2d &preMultBy(const AcGeScale2d &other)
    {
        return postMultBy(other);
    }
    AcGeScale2d &setToProduct(const AcGeScale2d &left,
                              const AcGeScale2d &right)
    {
        sx = left.sx * right.sx;
        sy = left.sy * right.sy;
        return *this;
    }
    double &operator[](unsigned int index) { return index == 0 ? sx : sy; }
    const double &operator[](unsigned int index) const
    {
        return index == 0 ? sx : sy;
    }
    AcGeScale2d operator*(const AcGeScale2d &other) const
    {
        return {sx * other.sx, sy * other.sy};
    }
    AcGeScale2d &operator*=(const AcGeScale2d &other)
    {
        return postMultBy(other);
    }
    bool operator==(const AcGeScale2d &other) const { return isEqualTo(other); }
    bool operator!=(const AcGeScale2d &other) const { return !(*this == other); }
};

inline const AcGeScale2d AcGeScale2d::kIdentity{1.0, 1.0};

struct ACGE_PORT AcGeScale3d
{
    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;

    static const AcGeScale3d kIdentity;

    AcGeScale3d() = default;
    explicit AcGeScale3d(double uniformScale)
        : sx(uniformScale), sy(uniformScale), sz(uniformScale) {}
    AcGeScale3d(double xScale, double yScale, double zScale)
        : sx(xScale), sy(yScale), sz(zScale) {}
    AcGeScale3d(const AcGeScale2d &scale)
        : sx(scale.sx), sy(scale.sy), sz(1.0) {}

    AcGeScale3d &set(double xScale, double yScale, double zScale)
    {
        sx = xScale;
        sy = yScale;
        sz = zScale;
        return *this;
    }
    AcGeScale3d inverse() const;
    AcGeScale3d &invert()
    {
        *this = inverse();
        return *this;
    }
    AcGeMatrix3d getMatrix() const;
    bool isEqualTo(const AcGeScale3d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        return std::abs(sx - other.sx) <= tol.equalVector() &&
               std::abs(sy - other.sy) <= tol.equalVector() &&
               std::abs(sz - other.sz) <= tol.equalVector();
    }
    bool isProportional(const AcGeTol &tol = AcGeTol{}) const
    {
        const double magnitude = std::max({1.0, std::abs(sx), std::abs(sy),
                                           std::abs(sz)});
        return std::abs(sx - sy) <= tol.equalVector() * magnitude &&
               std::abs(sx - sz) <= tol.equalVector() * magnitude;
    }
    AcGeScale3d &postMultBy(const AcGeScale3d &other)
    {
        sx *= other.sx;
        sy *= other.sy;
        sz *= other.sz;
        return *this;
    }
    AcGeScale3d &preMultBy(const AcGeScale3d &other)
    {
        return postMultBy(other);
    }
    AcGeScale3d &setToProduct(const AcGeScale3d &left,
                              const AcGeScale3d &right)
    {
        sx = left.sx * right.sx;
        sy = left.sy * right.sy;
        sz = left.sz * right.sz;
        return *this;
    }
    double &operator[](unsigned int index)
    {
        return index == 0 ? sx : (index == 1 ? sy : sz);
    }
    const double &operator[](unsigned int index) const
    {
        return index == 0 ? sx : (index == 1 ? sy : sz);
    }
    AcGeScale3d operator*(const AcGeScale3d &other) const
    {
        return {sx * other.sx, sy * other.sy, sz * other.sz};
    }
    AcGeScale3d &operator*=(const AcGeScale3d &other)
    {
        return postMultBy(other);
    }
    bool operator==(const AcGeScale3d &other) const { return isEqualTo(other); }
    bool operator!=(const AcGeScale3d &other) const { return !(*this == other); }
};

inline const AcGeScale3d AcGeScale3d::kIdentity{1.0, 1.0, 1.0};
