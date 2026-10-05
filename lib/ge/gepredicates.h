#pragma once

// Robust geometric predicates (namespace ge) — thin wrappers over
// Shewchuk's adaptive exact-sign arithmetic (external/predicates, public
// domain).  orient2d/orient3d return the exact sign of the point
// orientation test (positive = counter-clockwise in 2D), incircle and
// insphere the exact sign of the in-circle/in-sphere test; unlike plain
// double arithmetic the sign is guaranteed correct even for
// near-degenerate inputs at large coordinates (1e7 and beyond), where the
// double rounding error of a cross product reaches ~1e-2 world units.
//
// The wrappers accept the AcGe value types so kernel code can call them
// without unpacking arrays.  exactinit() runs once on first use.
//
// Discipline: these predicates decide SIGNS (branch decisions); they do
// not replace the tolerance-based logic of curve evaluation.  The
// predicates translation unit is compiled with /fp:precise (locked in
// CMakeLists) — fast-math would silently destroy exactness.

#include <glm/glm.hpp>

#include "ge/gepoint.h"

extern "C" {
void exactinit();
double orient2d(double pa[2], double pb[2], double pc[2]);
double orient3d(double pa[3], double pb[3], double pc[3], double pd[3]);
double incircle(double pa[2], double pb[2], double pc[2], double pd[2]);
double insphere(double pa[3], double pb[3], double pc[3], double pd[3],
                double pe[3]);
}

namespace ge
{

namespace detail
{
inline void ensurePredicatesInitialized()
{
    static const bool initialized = [] {
        exactinit();
        return true;
    }();
    (void)initialized;
}
} // namespace detail

// Exact 2D orientation: sign of (b-a) x (c-a).  Positive when c lies to
// the left of a->b, negative to the right, exactly 0.0 when collinear.
inline double orient2d(const AcGePoint2d &pa, const AcGePoint2d &pb,
                       const AcGePoint2d &pc)
{
    detail::ensurePredicatesInitialized();
    double a[2] = {pa.x, pa.y};
    double b[2] = {pb.x, pb.y};
    double c[2] = {pc.x, pc.y};
    return ::orient2d(a, b, c);
}

// Exact 3D orientation: sign of the volume of the tetrahedron
// (a, b, c, d).  Positive when d lies below the plane a->b->c (right-hand
// rule), exactly 0.0 when coplanar.
inline double orient3d(const AcGePoint3d &pa, const AcGePoint3d &pb,
                       const AcGePoint3d &pc, const AcGePoint3d &pd)
{
    detail::ensurePredicatesInitialized();
    double a[3] = {pa.x, pa.y, pa.z};
    double b[3] = {pb.x, pb.y, pb.z};
    double c[3] = {pc.x, pc.y, pc.z};
    double d[3] = {pd.x, pd.y, pd.z};
    return ::orient3d(a, b, c, d);
}

// Exact 2D in-circle: sign of d's position relative to the circle through
// (a, b, c).  Positive inside, negative outside, exactly 0.0 on the
// circle.  Requires a, b, c in counter-clockwise order for the convention
// to hold (use orient2d to orient them first).
inline double incircle(const AcGePoint2d &pa, const AcGePoint2d &pb,
                       const AcGePoint2d &pc, const AcGePoint2d &pd)
{
    detail::ensurePredicatesInitialized();
    double a[2] = {pa.x, pa.y};
    double b[2] = {pb.x, pb.y};
    double c[2] = {pc.x, pc.y};
    double d[2] = {pd.x, pd.y};
    return ::incircle(a, b, c, d);
}

// Exact 3D in-sphere, same convention as incircle (a, b, c, d must be
// positively oriented).
inline double insphere(const AcGePoint3d &pa, const AcGePoint3d &pb,
                       const AcGePoint3d &pc, const AcGePoint3d &pd,
                       const AcGePoint3d &pe)
{
    detail::ensurePredicatesInitialized();
    double a[3] = {pa.x, pa.y, pa.z};
    double b[3] = {pb.x, pb.y, pb.z};
    double c[3] = {pc.x, pc.y, pc.z};
    double d[3] = {pd.x, pd.y, pd.z};
    double e[3] = {pe.x, pe.y, pe.z};
    return ::insphere(a, b, c, d, e);
}

} // namespace ge
