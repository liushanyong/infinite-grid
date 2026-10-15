#pragma once

#include "AcGePort.h"

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

#include "AcGeLinear.h"
#include "AcGePoint.h"

struct ACGE_PORT AcGeMatrix3d
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
AcGePoint3d transformBy(const AcGePoint3d &point,
                        const AcGeMatrix3d &matrix);
AcGeVector3d transformBy(const AcGeVector3d &vector,
                         const AcGeMatrix3d &matrix);

struct ACGE_PORT AcGePlane
{
    AcGePoint3d origin{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};

    AcGePlane() = default;
    AcGePlane(const AcGePoint3d &pointOnPlane, const AcGeVector3d &planeNormal)
        : origin(pointOnPlane), normal(planeNormal)
    {
    }
    AcGePlane(const AcGePoint3d &pntU, const AcGePoint3d &org,
              const AcGePoint3d &pntV);
    AcGePlane(const AcGePoint3d &org, const AcGeVector3d &uAxis,
              const AcGeVector3d &vAxis);
    AcGePlane(double a, double b, double c, double d);

    AcGePlane &set(const AcGePoint3d &pointOnPlane,
                   const AcGeVector3d &planeNormal)
    {
        origin = pointOnPlane;
        normal = planeNormal;
        return *this;
    }
    AcGePlane &set(const AcGePoint3d &pntU, const AcGePoint3d &org,
                   const AcGePoint3d &pntV);
    AcGePlane &set(const AcGePoint3d &org, const AcGeVector3d &uAxis,
                   const AcGeVector3d &vAxis);
    AcGePlane &set(double a, double b, double c, double d);

    double distanceTo(const AcGePoint3d &point) const
    {
        return (point - origin).dotProduct(normal);
    }

    double signedDistanceTo(const AcGePoint3d &point) const;
    AcGePoint3d closestPointTo(const AcGePoint3d &point) const;

    bool isOn(const AcGePoint3d &point,
              const AcGeTol &tol = AcGeTol{}) const
    {
        return normal.length() > tol.equalVector() &&
               std::abs(signedDistanceTo(point)) <= tol.equalPoint();
    }

    AcGeVector3d projectVector(const AcGeVector3d &vector) const;
    AcGePoint3d mirrorPoint(const AcGePoint3d &point) const;

    bool isParallelTo(const AcGePlane &other,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        const double lengths = normal.length() * other.normal.length();
        return lengths > 0.0 &&
               normal.crossProduct(other.normal).length() <=
                   std::abs(tol.equalVector()) * lengths;
    }

    bool isPerpendicularTo(const AcGePlane &other,
                           const AcGeTol &tol = AcGeTol{}) const
    {
        const double lengths = normal.length() * other.normal.length();
        return lengths > 0.0 &&
               std::abs(normal.dotProduct(other.normal)) <=
                   std::abs(tol.equalVector()) * lengths;
    }

    bool isCoplanarTo(const AcGePlane &other,
                      const AcGeTol &tol = AcGeTol{}) const
    {
        return isParallelTo(other, tol) &&
               std::abs(signedDistanceTo(other.origin)) <=
                   std::abs(tol.equalPoint());
    }

    bool project(const AcGePoint3d &point, const AcGeVector3d &direction,
                 AcGePoint3d &projected,
                 const AcGeTol &tol = AcGeTol{}) const;
    bool intersectWith(const AcGeLine3d &line, AcGePoint3d &result,
                       const AcGeTol &tol = AcGeTol{}) const;
    bool intersectWith(const AcGePlane &other, AcGeLine3d &result,
                       const AcGeTol &tol = AcGeTol{}) const;
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
