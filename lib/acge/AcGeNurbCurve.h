#pragma once

#include "AcGePort.h"

#include <array>
#include <cstddef>
#include <vector>

#include "AcGeArrays.h"
#include "AcGeKnotVector.h"
#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGeNurbCurve2d/3d aligned with the ObjectARX reference
// (genurb2d.h / genurb3d.h): non-uniform rational B-spline curves.
//
// Non-periodic curves use the standard B-spline form
//   knot count == control point count + degree + 1.
// Periodic curves follow the documented ObjectARX wrapped format:
//   * format 1 — knot count == control point count + 1 with distinct
//     (uniform) knots; the first and last control points differ;
//   * format 2 — knot count == control point count and the first/last
//     control points are identical (normalized to format 1).
// Wrapped periodic data is unrolled to an unclamped uniform curve by
// repeating control points modulo N, the same convention LNLib/OCC use
// for their stored periodic form (structure consulted from
// external/LNLib, LGPL; implementation is original NURBS Book A2.x/A4.x
// machinery on the existing AcGeNurbsCore).
//
// Evaluation supports derivatives up to the degree (A4.2 quotient rule).
// These value types do not implement the AcGeCurve2d/3d hierarchy,
// fitting from a generic curve, or the filer/IO protocols. Structural
// editing (knot insertion, exact removal, degree elevation, hard trim,
// makeRational, joinWith) is provided for non-periodic curves.

namespace ge
{
namespace detail
{

// Dimension-agnostic homogeneous NURBS curve evaluator shared by the 2D
// and 3D public classes.
struct ACGE_PORT NurbsCurveCore
{
    int degree = 0;
    bool periodic = false;
    bool rational = false;
    // Documented definition data.
    std::vector<double> knots;
    std::vector<double> cx, cy, cz; // control coordinates (z == 0 in 2D)
    std::vector<double> weights;    // empty when non-rational
    // Unrolled evaluation data.
    std::vector<double> evalKnots;
    std::vector<double> ex, ey, ez, ew;
    double u0 = 0.0;
    double u1 = 0.0;

    void build(int degree, const std::vector<double> &knots,
               const std::vector<double> &px, const std::vector<double> &py,
               const std::vector<double> &pz, const std::vector<double> &w,
               bool periodic, bool rational);

    void setControlPoint(size_t i, double x, double y, double z);
    void setWeight(size_t i, double w);

    // out[k] = k-th derivative of the homogeneous curve (x, y, z, w).
    void homogeneousDerivatives(
        double u, size_t derivativeCount,
        std::vector<std::array<double, 4>> &out) const;

    // --- structural editing (NURBS Book A5.1 / A5.2 / A5.6-equivalent) ---
    // Non-periodic curves only; every operation preserves the geometry
    // exactly (within a tight relative tolerance).
    // Number of exact-value occurrences of u in the knot vector.
    size_t knotMultiplicity(double u) const;
    // A5.1: insert one occurrence of u, keeping the shape unchanged.
    void insertKnot(double u);
    // A5.2-style removal of one occurrence of u; returns false when the
    // removal would change the curve shape (continuity check).
    bool removeKnot(double u, double tol);
    // Degree elevation by times: decompose to Bezier spans, elevate each
    // span, then merge junctions back to (original multiplicity + times).
    void elevateDegree(int times);
    // Hard trim: clamp newStart/newEnd to full multiplicity and slice the
    // control polygon; the domain becomes [newStart, newEnd].
    void hardTrim(double newStart, double newEnd);
    // Assign the weight to every control point (make rational) or scale
    // all existing weights (already rational).
    void makeRational(double weight);
    // C0 join: append a compatible clamped curve of equal degree whose
    // start point matches this curve's endpoint (within tol).
    void join(const NurbsCurveCore &other, double tol);

private:
    void rebuildEvalData();
};

} // namespace detail
} // namespace ge

class ACGE_PORT AcGeNurbCurve2d
{
public:
    AcGeNurbCurve2d() = default;
    AcGeNurbCurve2d(int degree, const AcGeKnotVector &knots,
                    const AcGePoint2dArray &controlPoints,
                    bool isPeriodic = false);
    AcGeNurbCurve2d(int degree, const AcGeKnotVector &knots,
                    const AcGePoint2dArray &controlPoints,
                    const AcGeDoubleArray &weights, bool isPeriodic = false);

    int degree() const { return m_core.degree; }
    int order() const { return m_core.degree + 1; }
    bool isRational() const { return m_core.rational; }
    bool isPeriodic() const { return m_core.periodic; }

    int numKnots() const;
    double knotAt(int i) const;
    int numControlPoints() const;
    AcGePoint2d controlPointAt(int i) const;
    AcGeNurbCurve2d &setControlPointAt(int i, const AcGePoint2d &point);
    int numWeights() const;
    double weightAt(int i) const;
    AcGeNurbCurve2d &setWeightAt(int i, double weight);

    double startParam() const { return m_core.u0; }
    double endParam() const { return m_core.u1; }
    AcGePoint2d startPoint() const { return pointAt(m_core.u0); }
    AcGePoint2d endPoint() const { return pointAt(m_core.u1); }

    void getDefinitionData(int &degree, bool &rational, bool &periodic,
                           AcGeKnotVector &knots,
                           AcGePoint2dArray &controlPoints,
                           AcGeDoubleArray &weights) const;

    AcGePoint2d pointAt(double param) const;
    AcGeVector2d firstDeriv(double param) const;
    double length() const { return length(startParam(), endParam()); }
    double length(double fromParam, double toParam,
                  double tol = AcGeTol{}.equalPoint()) const;
    std::vector<AcGeVector2d> derivativesAt(
        double param, size_t derivativeCount) const;
    double curvatureAt(double param) const;
    bool isClosed(const AcGeTol &tol = AcGeTol{}) const;

    // Structural editing (ObjectARX genurb2d API; non-periodic only).
    AcGeNurbCurve2d &insertKnot(double param);
    AcGeNurbCurve2d &addKnot(double param);
    bool deleteControlPointAt(int index);
    AcGeNurbCurve2d &elevateDegree(int plusDegree);
    AcGeNurbCurve2d &makeRational(double weight = 1.0);
    AcGeNurbCurve2d &hardTrimByParams(double newStartParam,
                                      double newEndParam);
    AcGeNurbCurve2d &joinWith(const AcGeNurbCurve2d &curve);

private:
    ge::detail::NurbsCurveCore m_core;
};

class ACGE_PORT AcGeNurbCurve3d
{
public:
    AcGeNurbCurve3d() = default;
    AcGeNurbCurve3d(int degree, const AcGeKnotVector &knots,
                    const AcGePoint3dArray &controlPoints,
                    bool isPeriodic = false);
    AcGeNurbCurve3d(int degree, const AcGeKnotVector &knots,
                    const AcGePoint3dArray &controlPoints,
                    const AcGeDoubleArray &weights, bool isPeriodic = false);

    int degree() const { return m_core.degree; }
    int order() const { return m_core.degree + 1; }
    bool isRational() const { return m_core.rational; }
    bool isPeriodic() const { return m_core.periodic; }

    int numKnots() const;
    double knotAt(int i) const;
    int numControlPoints() const;
    AcGePoint3d controlPointAt(int i) const;
    AcGeNurbCurve3d &setControlPointAt(int i, const AcGePoint3d &point);
    int numWeights() const;
    double weightAt(int i) const;
    AcGeNurbCurve3d &setWeightAt(int i, double weight);

    double startParam() const { return m_core.u0; }
    double endParam() const { return m_core.u1; }
    AcGePoint3d startPoint() const { return pointAt(m_core.u0); }
    AcGePoint3d endPoint() const { return pointAt(m_core.u1); }

    void getDefinitionData(int &degree, bool &rational, bool &periodic,
                           AcGeKnotVector &knots,
                           AcGePoint3dArray &controlPoints,
                           AcGeDoubleArray &weights) const;

    AcGePoint3d pointAt(double param) const;
    AcGeVector3d firstDeriv(double param) const;
    double length() const { return length(startParam(), endParam()); }
    double length(double fromParam, double toParam,
                  double tol = AcGeTol{}.equalPoint()) const;
    std::vector<AcGeVector3d> derivativesAt(
        double param, size_t derivativeCount) const;
    double curvatureAt(double param) const;
    bool isClosed(const AcGeTol &tol = AcGeTol{}) const;

    // Structural editing (ObjectARX genurb3d API; non-periodic only).
    AcGeNurbCurve3d &insertKnot(double param);
    AcGeNurbCurve3d &addKnot(double param);
    bool deleteControlPointAt(int index);
    AcGeNurbCurve3d &elevateDegree(int plusDegree);
    AcGeNurbCurve3d &makeRational(double weight = 1.0);
    AcGeNurbCurve3d &hardTrimByParams(double newStartParam,
                                      double newEndParam);
    AcGeNurbCurve3d &joinWith(const AcGeNurbCurve3d &curve);

private:
    ge::detail::NurbsCurveCore m_core;
};
