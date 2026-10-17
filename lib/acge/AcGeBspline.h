#pragma once

#include "AcGePort.h"

// Bézier / B-spline / NURBS curve and surface evaluation for the AcGe
// layer, following "The NURBS Book" 2nd edition algorithm numbering
// (A3.1 CurvePoint, A3.2 CurveDerivs via eq. 3.8 difference curves,
// A3.5 SurfacePoint, A4.1 rational point, A4.2/A4.3 rational derivatives
// via the binomial quotient rule).  Reimplemented from the book's
// pseudocode on top of genurbscore.h; the LNLib reference in external/
// (LGPL) is only consulted for structure.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGePoint.h"
#include "AcGeCurve.h"
#include "AcGeNurbsCore.h"
#include "AcGeKnotVector.h"

namespace ge
{

// Homogeneous coordinate point for rational curves/surfaces.
struct ACGE_PORT AcGePoint4d
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;

    AcGePoint4d() = default;
    AcGePoint4d(double px, double py, double pz, double pw = 1.0)
        : x(px), y(py), z(pz), w(pw)
    {
    }

    AcGePoint4d &operator+=(const AcGePoint4d &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        w += o.w;
        return *this;
    }
    AcGePoint4d operator*(double s) const
    {
        return {x * s, y * s, z * s, w * s};
    }
};

// Drop the first/last knot of a clamped knot vector (NURBS Book eq. 3.8:
// the derivative curve's knot vector).
inline std::vector<double> innerKnotVector(const std::vector<double> &knots)
{
    return std::vector<double>(knots.begin() + 1, knots.end() - 1);
}

// ---------------------------------------------------------------------------
// Bézier curve (de Casteljau).
// ---------------------------------------------------------------------------
class ACGE_PORT AcGeBezierCurve3d : public AcGeCurve3d
{
public:
    std::vector<AcGePoint3d> controlPoints;

    AcGeBezierCurve3d() = default;
    explicit AcGeBezierCurve3d(const std::vector<AcGePoint3d> &points);

    size_t degree() const { return controlPoints.size() - 1; }

    double startParam() const override { return 0.0; }
    double endParam() const override { return 1.0; }
    AcGePoint3d pointAt(double t) const override;

    // First derivative via the degree-(p-1) Bézier of control-point
    // differences (NURBS Book eq. 1.7 / 1.8).
    AcGeVector3d derivativeAt(double t) const;
};

// ---------------------------------------------------------------------------
// B-spline curve (A3.1 / A3.2).
// ---------------------------------------------------------------------------
class ACGE_PORT AcGeBsplineCurve3d : public AcGeCurve3d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint3d> controlPoints;

    AcGeBsplineCurve3d() = default;
    AcGeBsplineCurve3d(std::vector<double> knotVector,
                       std::vector<AcGePoint3d> points, size_t curveDegree);

    void validate() const;

    double minParameter() const { return knots.at(degree); }
    double maxParameter() const { return knots.at(controlPoints.size()); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }

    // A3.1
    AcGePoint3d pointAt(double u) const override;

    // A3.2 via eq. 3.8: the k-th derivative is a degree-(p-k) B-spline
    // whose knots drop the first/last pair and whose control points are
    // scaled differences.  Evaluated with the same A3.1 machinery, which
    // avoids the delicate A2.3 derivative-basis table entirely.
    AcGeBsplineCurve3d differentiated() const;

    // A3.2: derivatives 0..derivativeCount at u.
    std::vector<AcGeVector3d> derivativesAt(double u,
                                            size_t derivativeCount) const;

    // Global interpolation through fit points (A9.1): constructs the
    // control points so the curve passes through every fit point.
    static AcGeBsplineCurve3d interpolate(
        const std::vector<AcGePoint3d> &fitPoints, size_t curveDegree,
        double alpha = 0.0);
};

// ---------------------------------------------------------------------------
// NURBS curve (A4.1 / A4.2).
// ---------------------------------------------------------------------------
class ACGE_PORT AcGeNurbsCurve3d : public AcGeCurve3d
{
public:
    size_t degree = 3;
    std::vector<double> knots;
    std::vector<AcGePoint4d> controlPoints; // homogeneous (w = weight)

    AcGeNurbsCurve3d() = default;
    AcGeNurbsCurve3d(std::vector<double> knotVector,
                     std::vector<AcGePoint4d> points, size_t curveDegree);

    void validate() const;

    double minParameter() const { return knots.at(degree); }
    double maxParameter() const { return knots.at(controlPoints.size()); }
    double startParam() const override { return minParameter(); }
    double endParam() const override { return maxParameter(); }

    // A4.1: projective evaluation of the rational curve.
    AcGePoint3d pointAt(double u) const override;

    // A4.2 via scalar difference splines: differentiate the weight and the
    // three homogeneous component splines k times each (degree drops per
    // step, NURBS Book eq. 3.8), evaluate all parts at u, then apply the
    // binomial quotient rule in ascending order.
    std::vector<AcGeVector3d> derivativesAt(double u,
                                            size_t derivativeCount) const;

    double curvatureAt(double u) const;
};

// ---------------------------------------------------------------------------
// B-spline surface (A3.5 / A3.6 difference surfaces).
// ---------------------------------------------------------------------------
class ACGE_PORT AcGeBsplineSurface3d : public AcGeSurface
{
public:
    size_t degreeU = 3;
    size_t degreeV = 3;
    std::vector<double> knotsU;
    std::vector<double> knotsV;
    // Row-major over V: controlPoints[uIndex * vCount + vIndex].
    std::vector<AcGePoint3d> controlPoints;
    size_t controlPointCountU = 0;
    size_t controlPointCountV = 0;

    void validate() const;

    double minParameterU() const override { return knotsU.at(degreeU); }
    double maxParameterU() const override
    {
        return knotsU.at(controlPointCountU);
    }
    double minParameterV() const override { return knotsV.at(degreeV); }
    double maxParameterV() const override
    {
        return knotsV.at(controlPointCountV);
    }

    // A3.5: tensor-product evaluation.
    AcGePoint3d pointAt(double u, double v) const override;

    // Partial derivatives up to dU in u and dV in v via difference surfaces:
    // result[k][l] = d^(k+l) S / du^k dv^l (eq. 3.9, the operations commute).
    std::vector<std::vector<AcGeVector3d>> derivativesAt(
        double u, double v, size_t derivativeCountU,
        size_t derivativeCountV) const;
    AcGeVector3d normalAt(double u, double v) const;
};

// ---------------------------------------------------------------------------
// NURBS surface (A4.3 rational tensor product).
// ---------------------------------------------------------------------------
class ACGE_PORT AcGeNurbsSurface3d : public AcGeSurface
{
public:
    size_t degreeU = 3;
    size_t degreeV = 3;
    std::vector<double> knotsU;
    std::vector<double> knotsV;
    std::vector<AcGePoint4d> controlPoints; // homogeneous grid, row-major U
    size_t controlPointCountU = 0;
    size_t controlPointCountV = 0;

    void validate() const;

    double minParameterU() const override { return knotsU.at(degreeU); }
    double maxParameterU() const override
    {
        return knotsU.at(controlPointCountU);
    }
    double minParameterV() const override { return knotsV.at(degreeV); }
    double maxParameterV() const override
    {
        return knotsV.at(controlPointCountV);
    }

    // A4.3: rational tensor-product evaluation.
    AcGePoint3d pointAt(double u, double v) const override;

    // Partial derivatives up to dU in u and dV in v via homogeneous
    // difference surfaces: for each (k, l) re-derive a working surface
    // differentiated k times in u and l times in v (the operations
    // commute), evaluate its homogeneous parts, then apply the binomial
    // quotient rule in ascending order.
    std::vector<std::vector<AcGeVector3d>> derivativesAt(
        double u, double v, size_t derivativeCountU,
        size_t derivativeCountV) const;

    AcGeVector3d normalAt(double u, double v) const;
    bool isRational() const;
    bool isRationalInU() const;
    bool isRationalInV() const;
    bool isPeriodicInU(double &period,
                       const AcGeTol &tol = AcGeTol{}) const;
    bool isPeriodicInV(double &period,
                       const AcGeTol &tol = AcGeTol{}) const;
    void getDefinitionData(
        size_t &degreeInU, size_t &degreeInV, size_t &controlPointCountInU,
        size_t &controlPointCountInV, bool &rationalInU, bool &rationalInV,
        AcGePoint3dArray &points, AcGeDoubleArray &weights,
        AcGeKnotVector &knotsInU, AcGeKnotVector &knotsInV) const;

    static double binomialCoefficient(size_t n, size_t k);
};

} // namespace ge
