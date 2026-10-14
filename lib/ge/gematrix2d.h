#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "gepoint.h"
#include "geutil.h"

// Source-level AcGeMatrix2d subset based on the ObjectARX 2026 reference.
// The matrix stores the two affine rows [a00 a01 t0] and [a10 a11 t1].
struct AcGeMatrix2d
{
    double entry[2][3]{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};

    AcGeMatrix2d() = default;
    AcGeMatrix2d(double a00, double a01, double t0,
                 double a10, double a11, double t1)
        : entry{{a00, a01, t0}, {a10, a11, t1}}
    {
    }

    double *operator[](unsigned int row) { return entry[row]; }
    const double *operator[](unsigned int row) const { return entry[row]; }
    double &operator()(unsigned int row, unsigned int column)
    {
        return entry[row][column];
    }
    const double &operator()(unsigned int row, unsigned int column) const
    {
        return entry[row][column];
    }

    static AcGeMatrix2d kIdentity() { return {}; }

    AcGeMatrix2d &setToIdentity()
    {
        *this = AcGeMatrix2d{};
        return *this;
    }

    double det() const
    {
        return entry[0][0] * entry[1][1] -
               entry[0][1] * entry[1][0];
    }

    double norm() const
    {
        double largest = 0.0;
        for (const auto &row : entry)
            for (double value : row)
                largest = std::max(largest, std::abs(value));
        return largest;
    }

    AcGeMatrix2d inverse() const
    {
        const double determinant = det();
        if (determinant == 0.0)
            throw std::domain_error("Cannot invert a singular AcGeMatrix2d.");

        const double invDet = 1.0 / determinant;
        const double a00 = entry[0][0];
        const double a01 = entry[0][1];
        const double a10 = entry[1][0];
        const double a11 = entry[1][1];
        const double tx = entry[0][2];
        const double ty = entry[1][2];
        return {a11 * invDet, -a01 * invDet,
                (a01 * ty - a11 * tx) * invDet,
                -a10 * invDet, a00 * invDet,
                (a10 * tx - a00 * ty) * invDet};
    }

    AcGeMatrix2d &invert()
    {
        *this = inverse();
        return *this;
    }

    AcGeMatrix2d operator*(const AcGeMatrix2d &right) const
    {
        AcGeMatrix2d result;
        result.entry[0][0] = entry[0][0] * right.entry[0][0] +
                             entry[0][1] * right.entry[1][0];
        result.entry[0][1] = entry[0][0] * right.entry[0][1] +
                             entry[0][1] * right.entry[1][1];
        result.entry[0][2] = entry[0][0] * right.entry[0][2] +
                             entry[0][1] * right.entry[1][2] +
                             entry[0][2];
        result.entry[1][0] = entry[1][0] * right.entry[0][0] +
                             entry[1][1] * right.entry[1][0];
        result.entry[1][1] = entry[1][0] * right.entry[0][1] +
                             entry[1][1] * right.entry[1][1];
        result.entry[1][2] = entry[1][0] * right.entry[0][2] +
                             entry[1][1] * right.entry[1][2] +
                             entry[1][2];
        return result;
    }

    AcGeMatrix2d &postMultBy(const AcGeMatrix2d &right)
    {
        *this = *this * right;
        return *this;
    }

    AcGeMatrix2d &preMultBy(const AcGeMatrix2d &left)
    {
        *this = left * *this;
        return *this;
    }

    AcGeMatrix2d &setToProduct(const AcGeMatrix2d &left,
                               const AcGeMatrix2d &right)
    {
        *this = left * right;
        return *this;
    }

    AcGeMatrix2d transpose() const
    {
        return {entry[0][0], entry[1][0], 0.0,
                entry[0][1], entry[1][1], 0.0};
    }

    AcGeMatrix2d &transposeIt()
    {
        *this = transpose();
        return *this;
    }

    AcGeVector2d translation() const
    {
        return {entry[0][2], entry[1][2]};
    }

    AcGeMatrix2d &setTranslation(const AcGeVector2d &offset)
    {
        entry[0][2] = offset.x;
        entry[1][2] = offset.y;
        return *this;
    }

    static AcGeMatrix2d translation(const AcGeVector2d &offset)
    {
        AcGeMatrix2d result;
        result.setTranslation(offset);
        return result;
    }

    static AcGeMatrix2d scaling(double factor, const AcGePoint2d &center)
    {
        return {factor, 0.0, center.x * (1.0 - factor),
                0.0, factor, center.y * (1.0 - factor)};
    }

    AcGeMatrix2d &setToScaling(double factor, const AcGePoint2d &center)
    {
        *this = scaling(factor, center);
        return *this;
    }

    static AcGeMatrix2d rotation(double angle, const AcGePoint2d &center)
    {
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        return {c, -s, center.x - c * center.x + s * center.y,
                s, c, center.y - s * center.x - c * center.y};
    }

    AcGeMatrix2d &setToRotation(double angle, const AcGePoint2d &center)
    {
        *this = rotation(angle, center);
        return *this;
    }

    AcGeMatrix2d &setToTranslation(const AcGeVector2d &offset)
    {
        setToIdentity();
        return setTranslation(offset);
    }

    AcGeMatrix2d &setCoordSystem(const AcGePoint2d &origin,
                                 const AcGeVector2d &e0,
                                 const AcGeVector2d &e1)
    {
        entry[0][0] = e0.x;
        entry[1][0] = e0.y;
        entry[0][1] = e1.x;
        entry[1][1] = e1.y;
        entry[0][2] = origin.x;
        entry[1][2] = origin.y;
        return *this;
    }

    void getCoordSystem(AcGePoint2d &origin, AcGeVector2d &e0,
                        AcGeVector2d &e1) const
    {
        origin = {entry[0][2], entry[1][2]};
        e0 = {entry[0][0], entry[1][0]};
        e1 = {entry[0][1], entry[1][1]};
    }

    AcGeMatrix2d &setToAlignCoordSys(const AcGePoint2d &fromOrigin,
                                     const AcGeVector2d &fromE0,
                                     const AcGeVector2d &fromE1,
                                     const AcGePoint2d &toOrigin,
                                     const AcGeVector2d &toE0,
                                     const AcGeVector2d &toE1)
    {
        AcGeMatrix2d from, to;
        from.setCoordSystem(fromOrigin, fromE0, fromE1);
        to.setCoordSystem(toOrigin, toE0, toE1);
        *this = to * from.inverse();
        return *this;
    }

    bool isSingular(const AcGeTol &tol = AcGeTol{}) const
    {
        const AcGeVector2d e0{entry[0][0], entry[1][0]};
        const AcGeVector2d e1{entry[0][1], entry[1][1]};
        return e0.length() <= tol.equalVector ||
               e1.length() <= tol.equalVector ||
               std::abs(e0.x * e1.y - e0.y * e1.x) <=
                   tol.equalVector * e0.length() * e1.length();
    }

    bool isScaledOrtho(const AcGeTol &tol = AcGeTol{}) const
    {
        const AcGeVector2d e0{entry[0][0], entry[1][0]};
        const AcGeVector2d e1{entry[0][1], entry[1][1]};
        return std::abs(e0.dotProduct(e1)) <=
               tol.equalVector * e0.length() * e1.length();
    }

    bool isUniScaledOrtho(const AcGeTol &tol = AcGeTol{}) const
    {
        const AcGeVector2d e0{entry[0][0], entry[1][0]};
        const AcGeVector2d e1{entry[0][1], entry[1][1]};
        const double length0 = e0.length();
        const double length1 = e1.length();
        return isScaledOrtho(tol) &&
               std::abs(length0 - length1) <=
                   tol.equalVector * std::max({1.0, length0, length1});
    }

    bool isConformal(double &scale, double &angle, bool &isMirror,
                     AcGeVector2d &reflex,
                     const AcGeTol &tol = AcGeTol{}) const
    {
        const AcGeVector2d e0{entry[0][0], entry[1][0]};
        const AcGeVector2d e1{entry[0][1], entry[1][1]};
        if (!isUniScaledOrtho(tol) || e0.length() <= tol.equalVector)
            return false;
        scale = e0.length();
        angle = std::atan2(e0.y, e0.x);
        isMirror = det() < 0.0;
        reflex = isMirror ? e0.normal() : AcGeVector2d{};
        return true;
    }

    bool isEqualTo(const AcGeMatrix2d &other,
                   const AcGeTol &tol = AcGeTol{}) const
    {
        for (unsigned int row = 0; row < 2; ++row)
            for (unsigned int column = 0; column < 3; ++column)
            {
                const double epsilon = column == 2
                    ? tol.equalPoint
                    : tol.equalVector;
                if (std::abs(entry[row][column] -
                             other.entry[row][column]) > epsilon)
                    return false;
            }
        return true;
    }

    bool operator==(const AcGeMatrix2d &other) const
    {
        return isEqualTo(other, AcGeTol{0.0, 0.0});
    }

    bool operator!=(const AcGeMatrix2d &other) const
    {
        return !(*this == other);
    }
};

inline AcGePoint2d transformBy(const AcGePoint2d &point,
                               const AcGeMatrix2d &matrix)
{
    return {
        matrix.entry[0][0] * point.x +
            matrix.entry[0][1] * point.y + matrix.entry[0][2],
        matrix.entry[1][0] * point.x +
            matrix.entry[1][1] * point.y + matrix.entry[1][2]};
}

inline AcGeVector2d transformBy(const AcGeVector2d &vector,
                                const AcGeMatrix2d &matrix)
{
    return {
        matrix.entry[0][0] * vector.x + matrix.entry[0][1] * vector.y,
        matrix.entry[1][0] * vector.x + matrix.entry[1][1] * vector.y};
}
