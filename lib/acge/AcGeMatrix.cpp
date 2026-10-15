#include "acge/AcGeMatrix.h"

#include <cmath>
#include <stdexcept>

// ---------------------------------------------------------------------------
// AcGePlane
// ---------------------------------------------------------------------------

AcGePlane::AcGePlane(const AcGePoint3d &pntU, const AcGePoint3d &org,
                     const AcGePoint3d &pntV)
    : origin(org),
      normal((pntU - org).crossProduct(pntV - org))
{
    if (normal.isZeroLength())
        throw std::invalid_argument("AcGePlane points must not be collinear.");
}

AcGePlane::AcGePlane(const AcGePoint3d &org, const AcGeVector3d &uAxis,
                     const AcGeVector3d &vAxis)
    : origin(org), normal(uAxis.crossProduct(vAxis))
{
    if (normal.isZeroLength())
        throw std::invalid_argument("AcGePlane axes must not be collinear.");
}

AcGePlane::AcGePlane(double a, double b, double c, double d)
{
    set(a, b, c, d);
}

AcGePlane &AcGePlane::set(const AcGePoint3d &pntU, const AcGePoint3d &org,
                          const AcGePoint3d &pntV)
{
    const AcGeVector3d newNormal =
        (pntU - org).crossProduct(pntV - org);
    if (newNormal.isZeroLength())
        throw std::invalid_argument("AcGePlane points must not be collinear.");
    origin = org;
    normal = newNormal;
    return *this;
}

AcGePlane &AcGePlane::set(const AcGePoint3d &org, const AcGeVector3d &uAxis,
                          const AcGeVector3d &vAxis)
{
    const AcGeVector3d newNormal = uAxis.crossProduct(vAxis);
    if (newNormal.isZeroLength())
        throw std::invalid_argument("AcGePlane axes must not be collinear.");
    origin = org;
    normal = newNormal;
    return *this;
}

AcGePlane &AcGePlane::set(double a, double b, double c, double d)
{
    const AcGeVector3d equationNormal{a, b, c};
    const double normalLengthSq = equationNormal.lengthSq();
    if (normalLengthSq == 0.0)
        throw std::invalid_argument("AcGePlane equation normal must be nonzero.");
    normal = equationNormal;
    origin = AcGePoint3d::kOrigin +
             equationNormal * (-d / normalLengthSq);
    return *this;
}

double AcGePlane::signedDistanceTo(const AcGePoint3d &point) const
{
    const double normalLength = normal.length();
    return normalLength == 0.0 ? 0.0 : distanceTo(point) / normalLength;
}

AcGePoint3d AcGePlane::closestPointTo(const AcGePoint3d &point) const
{
    const double normalLength = normal.length();
    if (normalLength == 0.0)
        return point;
    const AcGeVector3d unitNormal = normal / normalLength;
    return point - unitNormal * signedDistanceTo(point);
}

AcGeVector3d AcGePlane::projectVector(const AcGeVector3d &vector) const
{
    const double normalLength = normal.length();
    if (normalLength == 0.0)
        return vector;
    const AcGeVector3d unitNormal = normal / normalLength;
    return vector - unitNormal * vector.dotProduct(unitNormal);
}

AcGePoint3d AcGePlane::mirrorPoint(const AcGePoint3d &point) const
{
    const double normalLength = normal.length();
    if (normalLength == 0.0)
        return point;
    const AcGeVector3d unitNormal = normal / normalLength;
    return point - unitNormal * (2.0 * signedDistanceTo(point));
}

bool AcGePlane::project(const AcGePoint3d &point, const AcGeVector3d &direction,
                        AcGePoint3d &projected, const AcGeTol &tol) const
{
    const double denominator = direction.dotProduct(normal);
    if (std::abs(denominator) <=
        tol.equalVector() * direction.length() * normal.length())
        return false;
    const double parameter = -distanceTo(point) / denominator;
    projected = point + direction * parameter;
    return true;
}

bool AcGePlane::intersectWith(const AcGeLine3d &line, AcGePoint3d &result,
                              const AcGeTol &tol) const
{
    const double denominator = normal.dotProduct(line.direction);
    if (std::abs(denominator) <=
        tol.equalVector() * normal.length() * line.direction.length())
        return false;
    const double parameter =
        normal.dotProduct(origin - line.basePoint) / denominator;
    result = line.pointAt(parameter);
    return true;
}

bool AcGePlane::intersectWith(const AcGePlane &other, AcGeLine3d &result,
                              const AcGeTol &tol) const
{
    const AcGeVector3d direction = normal.crossProduct(other.normal);
    const double directionLengthSq = direction.lengthSq();
    if (directionLengthSq <=
        std::pow(tol.equalVector() * normal.length() * other.normal.length(),
                 2.0))
        return false;
    const double thisOffset = normal.dotProduct(origin.asVector());
    const double otherOffset = other.normal.dotProduct(other.origin.asVector());
    const AcGePoint3d intersection =
        AcGePoint3d::kOrigin +
        (other.normal.crossProduct(direction) * thisOffset +
         direction.crossProduct(normal) * otherOffset) /
            directionLengthSq;
    result.set(intersection, direction);
    return true;
}

// ---------------------------------------------------------------------------
// Free transformBy functions
// ---------------------------------------------------------------------------

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
