#pragma once

// AcGe-compatible matrix and plane value types (ObjectARX signature subset
// used by the graphics interface).  glm::dmat4 storage; column-major like
// AcGeMatrix3d's entry[0..3][0..3].

#include <glm/glm.hpp>
#include <cmath>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

#include <stdexcept>

#include "gelinear.h"
#include "gepoint.h"

struct AcGeMatrix3d
{
    glm::dmat4 m{1.0};

    AcGeMatrix3d() = default;
    // Boundary conversions: the rendering layer (camera/uniform math)
    // stays in glm; the domain layer carries AcGeMatrix3d.
    explicit AcGeMatrix3d(const glm::dmat4 &matrix) : m(matrix) {}
    explicit AcGeMatrix3d(const glm::mat4 &matrix)
        : m(matrix)
    {
    }
    explicit operator glm::dmat4() const { return m; }
    explicit operator glm::mat4() const
    {
        return glm::mat4(m);
    }

    static AcGeMatrix3d identity() { return AcGeMatrix3d(); }

    static AcGeMatrix3d setToTranslation(const AcGeVector3d &translation)
    {
        AcGeMatrix3d result;
        result.m = glm::translate(glm::dmat4(1.0), glm::dvec3(translation));
        return result;
    }

    static AcGeMatrix3d setToScaling(double factor)
    {
        AcGeMatrix3d result;
        result.m = glm::scale(glm::dmat4(1.0), glm::dvec3(factor));
        return result;
    }

    // ObjectARX setToScaling(const AcGeVector3d&): non-uniform scale.
    static AcGeMatrix3d setToScaling(const AcGeVector3d &factor)
    {
        AcGeMatrix3d result;
        result.m = glm::scale(glm::dmat4(1.0), glm::dvec3(factor));
        return result;
    }

    static AcGeMatrix3d setToScaling(double factor, const AcGePoint3d &center)
    {
        AcGeMatrix3d result;
        result.m = glm::translate(glm::dmat4(1.0), glm::dvec3(center)) *
                   glm::scale(glm::dmat4(1.0), glm::dvec3(factor)) *
                   glm::translate(glm::dmat4(1.0), -glm::dvec3(center));
        return result;
    }

    // Counter-clockwise rotation about an axis through a center point.
    static AcGeMatrix3d setToRotation(double angle, const AcGeVector3d &axis,
                                      const AcGePoint3d &center)
    {
        AcGeMatrix3d result;
        result.m = glm::translate(glm::dmat4(1.0), glm::dvec3(center)) *
                   glm::rotate(glm::dmat4(1.0), angle, glm::dvec3(axis)) *
                   glm::translate(glm::dmat4(1.0), -glm::dvec3(center));
        return result;
    }

    AcGeMatrix3d operator*(const AcGeMatrix3d &other) const
    {
        AcGeMatrix3d result;
        result.m = m * other.m;
        return result;
    }

    AcGeMatrix3d inverted() const
    {
        AcGeMatrix3d result;
        result.m = glm::inverse(m);
        return result;
    }
};

// Point/vector transformBy so entity code can apply matrices like ObjectARX.
inline AcGePoint3d transformBy(const AcGePoint3d &point,
                               const AcGeMatrix3d &matrix)
{
    const glm::dvec4 transformed =
        matrix.m * glm::dvec4(glm::dvec3(point), 1.0);
    return AcGePoint3d(transformed.x / transformed.w, transformed.y / transformed.w,
                       transformed.z / transformed.w);
}

inline AcGeVector3d transformBy(const AcGeVector3d &vector,
                                const AcGeMatrix3d &matrix)
{
    const glm::dvec4 transformed = matrix.m * glm::dvec4(glm::dvec3(vector), 0.0);
    return AcGeVector3d(transformed.x, transformed.y, transformed.z);
}

struct AcGePlane
{
    AcGePoint3d origin{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};

    AcGePlane() = default;
    AcGePlane(const AcGePoint3d &pointOnPlane, const AcGeVector3d &planeNormal)
        : origin(pointOnPlane), normal(planeNormal)
    {
    }
    AcGePlane(const AcGePoint3d &pntU, const AcGePoint3d &org,
              const AcGePoint3d &pntV)
        : origin(org),
          normal((pntU - org).crossProduct(pntV - org))
    {
        if (normal.isZeroLength())
            throw std::invalid_argument("AcGePlane points must not be collinear.");
    }
    AcGePlane(const AcGePoint3d &org, const AcGeVector3d &uAxis,
              const AcGeVector3d &vAxis)
        : origin(org), normal(uAxis.crossProduct(vAxis))
    {
        if (normal.isZeroLength())
            throw std::invalid_argument("AcGePlane axes must not be collinear.");
    }
    AcGePlane(double a, double b, double c, double d)
    {
        set(a, b, c, d);
    }

    AcGePlane &set(const AcGePoint3d &pointOnPlane,
                   const AcGeVector3d &planeNormal)
    {
        origin = pointOnPlane;
        normal = planeNormal;
        return *this;
    }
    AcGePlane &set(const AcGePoint3d &pntU, const AcGePoint3d &org,
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
    AcGePlane &set(const AcGePoint3d &org, const AcGeVector3d &uAxis,
                   const AcGeVector3d &vAxis)
    {
        const AcGeVector3d newNormal = uAxis.crossProduct(vAxis);
        if (newNormal.isZeroLength())
            throw std::invalid_argument("AcGePlane axes must not be collinear.");
        origin = org;
        normal = newNormal;
        return *this;
    }
    AcGePlane &set(double a, double b, double c, double d)
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

    double distanceTo(const AcGePoint3d &point) const
    {
        return (point - origin).dotProduct(normal);
    }

    double signedDistanceTo(const AcGePoint3d &point) const
    {
        const double normalLength = normal.length();
        return normalLength == 0.0 ? 0.0 : distanceTo(point) / normalLength;
    }

    AcGePoint3d closestPointTo(const AcGePoint3d &point) const
    {
        const double normalLength = normal.length();
        if (normalLength == 0.0)
            return point;
        const AcGeVector3d unitNormal = normal / normalLength;
        return point - unitNormal * signedDistanceTo(point);
    }

    bool isOn(const AcGePoint3d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return normal.length() > tol.equalVector() &&
               std::abs(signedDistanceTo(point)) <= tol.equalPoint();
    }

    bool project(const AcGePoint3d &point, const AcGeVector3d &direction,
                 AcGePoint3d &projected,
                 const AcGeTol &tol = AcGeTol{}) const
    {
        const double denominator = direction.dotProduct(normal);
        if (std::abs(denominator) <=
            tol.equalVector() * direction.length() * normal.length())
            return false;
        const double parameter = -distanceTo(point) / denominator;
        projected = point + direction * parameter;
        return true;
    }

    bool intersectWith(const AcGeLine3d &line, AcGePoint3d &result,
                       const AcGeTol &tol = AcGeTol{}) const
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

    bool intersectWith(const AcGePlane &other, AcGeLine3d &result,
                       const AcGeTol &tol = AcGeTol{}) const
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
};

// AutoCAD arbitrary axis algorithm: the OCS x-axis derived from an
// extrusion normal.  Near-world-Z normals (|Nx| and |Ny| below 1/64) seed
// from the world Y axis; everything else seeds from world Z.  DWG entities
// store planar geometry in this OCS, so libredwg extrusion fields require
// exactly this rule for correct angle bases.
inline AcGeVector3d arbitraryAxis(const AcGeVector3d &normal)
{
    const AcGeVector3d n = normal.normal();
    if (std::abs(n.x) < 1.0 / 64.0 && std::abs(n.y) < 1.0 / 64.0)
        return AcGeVector3d(0.0, 1.0, 0.0).crossProduct(n).normal();
    return AcGeVector3d(0.0, 0.0, 1.0).crossProduct(n).normal();
}

// OCS-to-world transform for an extrusion normal: the basis matrix whose
// columns are the arbitrary axis, the derived y, and the normal itself.
inline AcGeMatrix3d setToPlaneToWorld(const AcGeVector3d &normal)
{
    const AcGeVector3d n = normal.normal();
    const AcGeVector3d ax = arbitraryAxis(n);
    const AcGeVector3d ay = n.crossProduct(ax);
    AcGeMatrix3d result;
    result.m = glm::dmat4(
        glm::dvec4(glm::dvec3(ax), 0.0), glm::dvec4(glm::dvec3(ay), 0.0),
        glm::dvec4(glm::dvec3(n), 0.0), glm::dvec4(0.0, 0.0, 0.0, 1.0));
    return result;
}
