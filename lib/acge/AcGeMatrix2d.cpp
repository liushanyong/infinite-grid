#include "acge/AcGeMatrix2d.h"

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
