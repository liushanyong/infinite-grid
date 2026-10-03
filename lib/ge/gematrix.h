#pragma once

// AcGe-compatible matrix and plane value types (ObjectARX signature subset
// used by the graphics interface).  glm::dmat4 storage; column-major like
// AcGeMatrix3d's entry[0..3][0..3].

#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

#include "gepoint.h"

struct AcGeMatrix3d
{
    glm::dmat4 m{1.0};

    AcGeMatrix3d() = default;

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

    double distanceTo(const AcGePoint3d &point) const
    {
        return (point - origin).dotProduct(normal);
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
