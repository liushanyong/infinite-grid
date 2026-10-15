#include "acge/AcGePoint.h"

#include "acge/AcGeMatrix2d.h"

AcGePoint2d &AcGePoint2d::transformBy(const AcGeMatrix2d &matrix)
{
    *this = ::transformBy(*this, matrix);
    return *this;
}

AcGeVector2d &AcGeVector2d::transformBy(const AcGeMatrix2d &matrix)
{
    *this = ::transformBy(*this, matrix);
    return *this;
}
