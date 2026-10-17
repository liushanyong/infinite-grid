#pragma once

#include "AcGePort.h"

#include <vector>

#include "AcGeArrays.h"
#include "AcGeKnotVector.h"
#include "AcGePoint.h"
#include "AcGeTol.h"

// Source-level AcGeCubicSplineCurve2d/3d aligned with the ObjectARX
// reference (gecspl2d.h / gecspl3d.h): interpolation cubic splines that
// pass through every fit point.  The curve is stored as per-interval cubic
// Hermite data (fit points + first derivatives over the knot vector), so
// C2 continuity is solved at construction time:
//
//   (fitPoints, tol)                    -> closed periodic spline
//   (fitPoints, startDeriv, endDeriv)   -> clamped spline
//   (knots, fitPoints, firstDerivs)     -> explicit derivative data
//
// Evaluation follows the ObjectARX parameterization domain [startParam,
// endParam]; periodic curves wrap the parameter around the span.  These
// value types do not implement the AcGeCurve2d/3d inheritance hierarchy,
// curve fitting from a generic curve, or the filer/IO protocols.

class ACGE_PORT AcGeCubicSplineCurve2d
{
public:
    enum class FitMode
    {
        Empty,
        Periodic,
        Clamped,
        Explicit
    };

    AcGeCubicSplineCurve2d() = default;
    AcGeCubicSplineCurve2d(const AcGePoint2dArray &fitPnts,
                           const AcGeTol &tol = AcGeTol{});
    AcGeCubicSplineCurve2d(const AcGePoint2dArray &fitPnts,
                           const AcGeVector2d &startDeriv,
                           const AcGeVector2d &endDeriv,
                           const AcGeTol &tol = AcGeTol{});
    AcGeCubicSplineCurve2d(const AcGeKnotVector &knots,
                           const AcGePoint2dArray &fitPnts,
                           const AcGeVector2dArray &firstDerivs,
                           bool isPeriodic = false);

    int numFitPoints() const;
    AcGePoint2d fitPointAt(int idx) const;
    AcGeCubicSplineCurve2d &setFitPointAt(int idx, const AcGePoint2d &point);
    AcGeVector2d firstDerivAt(int idx) const;
    AcGeCubicSplineCurve2d &setFirstDerivAt(int idx,
                                            const AcGeVector2d &deriv);

    double startParam() const;
    double endParam() const;
    bool isPeriodic() const { return m_periodic; }
    FitMode fitMode() const { return m_mode; }
    const AcGeKnotVector &knots() const { return m_knots; }

    AcGePoint2d pointAt(double param) const;
    AcGeVector2d firstDeriv(double param) const;

private:
    AcGeKnotVector m_knots;
    AcGePoint2dArray m_fitPoints;
    AcGeVector2dArray m_firstDerivs;
    AcGeVector2d m_startDeriv;
    AcGeVector2d m_endDeriv;
    FitMode m_mode = FitMode::Empty;
    bool m_periodic = false;

    void checkFitIndex(int idx) const;
    void solveTangents();
    void wrapParameter(double &param) const;
};

class ACGE_PORT AcGeCubicSplineCurve3d
{
public:
    enum class FitMode
    {
        Empty,
        Periodic,
        Clamped,
        Explicit
    };

    AcGeCubicSplineCurve3d() = default;
    AcGeCubicSplineCurve3d(const AcGePoint3dArray &fitPnts,
                           const AcGeTol &tol = AcGeTol{});
    AcGeCubicSplineCurve3d(const AcGePoint3dArray &fitPnts,
                           const AcGeVector3d &startDeriv,
                           const AcGeVector3d &endDeriv,
                           const AcGeTol &tol = AcGeTol{});
    AcGeCubicSplineCurve3d(const AcGeKnotVector &knots,
                           const AcGePoint3dArray &fitPnts,
                           const AcGeVector3dArray &firstDerivs,
                           bool isPeriodic = false);

    int numFitPoints() const;
    AcGePoint3d fitPointAt(int idx) const;
    AcGeCubicSplineCurve3d &setFitPointAt(int idx, const AcGePoint3d &point);
    AcGeVector3d firstDerivAt(int idx) const;
    AcGeCubicSplineCurve3d &setFirstDerivAt(int idx,
                                            const AcGeVector3d &deriv);

    double startParam() const;
    double endParam() const;
    bool isPeriodic() const { return m_periodic; }
    FitMode fitMode() const { return m_mode; }
    const AcGeKnotVector &knots() const { return m_knots; }

    AcGePoint3d pointAt(double param) const;
    AcGeVector3d firstDeriv(double param) const;

private:
    AcGeKnotVector m_knots;
    AcGePoint3dArray m_fitPoints;
    AcGeVector3dArray m_firstDerivs;
    AcGeVector3d m_startDeriv;
    AcGeVector3d m_endDeriv;
    FitMode m_mode = FitMode::Empty;
    bool m_periodic = false;

    void checkFitIndex(int idx) const;
    void solveTangents();
    void wrapParameter(double &param) const;
};
