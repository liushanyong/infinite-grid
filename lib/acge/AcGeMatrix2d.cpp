#include "acge/AcGeMatrix2d.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

AcGeMatrix2d AcGeMatrix2d::inverse() const
{
    const double determinant = det();
    if (determinant == 0.0)
        throw std::domain_error("Cannot invert a singular AcGeMatrix2d.");

    const double invDet = 1.0 / determinant;
    AcGeMatrix2d result;
    result.entry[0][0] =
        (entry[1][1] * entry[2][2] - entry[1][2] * entry[2][1]) *
        invDet;
    result.entry[0][1] =
        (entry[0][2] * entry[2][1] - entry[0][1] * entry[2][2]) *
        invDet;
    result.entry[0][2] =
        (entry[0][1] * entry[1][2] - entry[0][2] * entry[1][1]) *
        invDet;
    result.entry[1][0] =
        (entry[1][2] * entry[2][0] - entry[1][0] * entry[2][2]) *
        invDet;
    result.entry[1][1] =
        (entry[0][0] * entry[2][2] - entry[0][2] * entry[2][0]) *
        invDet;
    result.entry[1][2] =
        (entry[0][2] * entry[1][0] - entry[0][0] * entry[1][2]) *
        invDet;
    result.entry[2][0] =
        (entry[1][0] * entry[2][1] - entry[1][1] * entry[2][0]) *
        invDet;
    result.entry[2][1] =
        (entry[0][1] * entry[2][0] - entry[0][0] * entry[2][1]) *
        invDet;
    result.entry[2][2] =
        (entry[0][0] * entry[1][1] - entry[0][1] * entry[1][0]) *
        invDet;
    return result;
}

AcGeMatrix2d AcGeMatrix2d::operator*(const AcGeMatrix2d &right) const
{
    AcGeMatrix2d result;
    for (unsigned int row = 0; row < 3; ++row)
        for (unsigned int column = 0; column < 3; ++column)
        {
            result.entry[row][column] = 0.0;
            for (unsigned int k = 0; k < 3; ++k)
                result.entry[row][column] +=
                    entry[row][k] * right.entry[k][column];
        }
    return result;
}

AcGeMatrix2d AcGeMatrix2d::transpose() const
{
    AcGeMatrix2d result;
    for (unsigned int row = 0; row < 3; ++row)
        for (unsigned int column = 0; column < 3; ++column)
            result.entry[row][column] = entry[column][row];
    return result;
}

AcGeMatrix2d AcGeMatrix2d::alignCoordSys(const AcGePoint2d &fromOrigin,
                                         const AcGeVector2d &fromE0,
                                         const AcGeVector2d &fromE1,
                                         const AcGePoint2d &toOrigin,
                                         const AcGeVector2d &toE0,
                                         const AcGeVector2d &toE1)
{
    AcGeMatrix2d result;
    result.setToAlignCoordSys(fromOrigin, fromE0, fromE1,
                              toOrigin, toE0, toE1);
    return result;
}

AcGeMatrix2d &AcGeMatrix2d::setToAlignCoordSys(const AcGePoint2d &fromOrigin,
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

bool AcGeMatrix2d::isConformal(double &scale, double &angle, bool &isMirror,
                               AcGeVector2d &reflex) const
{
    const AcGeVector2d e0{entry[0][0], entry[1][0]};
    const AcGeTol tol;
    if (!isUniScaledOrtho(tol) || e0.length() <= tol.equalVector())
        return false;
    scale = e0.length();
    angle = std::atan2(e0.y, e0.x);
    isMirror = det() < 0.0;
    reflex = isMirror ? e0.normal() : AcGeVector2d{};
    return true;
}

bool AcGeMatrix2d::isEqualTo(const AcGeMatrix2d &other,
                             const AcGeTol &tol) const
{
    for (unsigned int row = 0; row < 3; ++row)
        for (unsigned int column = 0; column < 3; ++column)
        {
            const double epsilon = column == 2
                ? tol.equalPoint()
                : tol.equalVector();
            if (std::abs(entry[row][column] -
                         other.entry[row][column]) > epsilon)
                return false;
        }
    return true;
}

AcGePoint2d transformBy(const AcGePoint2d &point,
                        const AcGeMatrix2d &matrix)
{
    return {
        matrix.entry[0][0] * point.x +
            matrix.entry[0][1] * point.y + matrix.entry[0][2],
        matrix.entry[1][0] * point.x +
            matrix.entry[1][1] * point.y + matrix.entry[1][2]};
}

AcGeVector2d transformBy(const AcGeVector2d &vector,
                         const AcGeMatrix2d &matrix)
{
    return {
        matrix.entry[0][0] * vector.x + matrix.entry[0][1] * vector.y,
        matrix.entry[1][0] * vector.x + matrix.entry[1][1] * vector.y};
}
