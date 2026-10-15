#include "acge/AcGeScale.h"

#include <cmath>
#include <stdexcept>

AcGeScale2d AcGeScale2d::inverse() const
{
    if (sx == 0.0 || sy == 0.0)
        throw std::domain_error("Cannot invert a zero AcGeScale2d component.");
    return {1.0 / sx, 1.0 / sy};
}

AcGeScale3d AcGeScale3d::inverse() const
{
    if (sx == 0.0 || sy == 0.0 || sz == 0.0)
        throw std::domain_error("Cannot invert a zero AcGeScale3d component.");
    return {1.0 / sx, 1.0 / sy, 1.0 / sz};
}

AcGeMatrix3d AcGeScale3d::getMatrix() const
{
    AcGeMatrix3d result;
    result.m = glm::scale(glm::dmat4(1.0), glm::dvec3(sx, sy, sz));
    return result;
}
