#include "acge/AcGeMatrix.h"

AcGePoint3d transformBy(const AcGePoint3d &point,
                        const AcGeMatrix3d &matrix)
{
    const glm::dvec4 transformed =
        matrix.m * glm::dvec4(glm::dvec3(point), 1.0);
    return AcGePoint3d(transformed.x / transformed.w,
                       transformed.y / transformed.w,
                       transformed.z / transformed.w);
}

AcGeVector3d transformBy(const AcGeVector3d &vector,
                         const AcGeMatrix3d &matrix)
{
    const glm::dvec4 transformed =
        matrix.m * glm::dvec4(glm::dvec3(vector), 0.0);
    return AcGeVector3d(transformed.x, transformed.y, transformed.z);
}
