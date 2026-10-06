#pragma once

// brep sweep + loft (namespace brep) — milestone 6-a/6-b: the first
// feature-modeling constructions beyond extrude.
//
// sweepAroundAxis follows the path-station idea of the wecad_sdk
// reference (3d_algo/source/KyGe/KyGe/AcGeSolidSweep: place the profile
// at every path station, then sew consecutive stations), but emits real
// topology through the shared position-keyed maps instead of a triangle
// soup: profile edges parallel to the axis sweep cylindrical patches
// (M4 surface type), radial edges sweep planar annular sectors, and the
// two caps are the profile polygon and its rotated copy.
//
// loftProfiles rules two parallel profiles: corresponding corners
// connect with straight lines, so every side face is a planar
// trapezoid — again assembled through the shared maps.

#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

#include "brep/BRepBuild.h"

namespace brep
{

namespace detail
{

inline AcGeVector3d normalizedOr(const AcGeVector3d &v,
                                 const AcGeVector3d &fallback)
{
    const double len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1.0e-12)
        return fallback;
    return AcGeVector3d(v.x / len, v.y / len, v.z / len);
}

} // namespace detail

// Sweeps a planar profile around an axis by |angle| radians (positive
// right-hand around |axisDir|).  The profile must lie in a meridian
// half-plane (every point off-axis, all in one plane through the axis)
// and be listed CCW around theta-hat = axisDir x r-hat (r-hat the
// profile centroid's radial direction).  Profile edges parallel to the
// axis sweep cylindrical patches; radial edges sweep planar annular
// sectors; slanted edges (conical faces) return nullptr until the
// NURBS phase.  Volume satisfies Pappus: area * centroidRadius * angle.
inline Body *sweepAroundAxis(Arena &arena, const AcGePoint3d *profile,
                             std::size_t count,
                             const AcGePoint3d &axisPoint,
                             const AcGeVector3d &axisDir, double angle)
{
    if (profile == nullptr || count < 3 || std::abs(angle) < 1.0e-12 ||
        angle > 6.2831853071795864769)
        return nullptr;
    const AcGeVector3d k = detail::normalizedOr(axisDir, {0.0, 0.0, 1.0});
    const bool flip = angle < 0.0;
    const double sweep = flip ? -angle : angle;
    const AcGeVector3d axis = flip ? k * -1.0 : k;

    auto axialOf = [&](const AcGePoint3d &p) {
        const AcGeVector3d delta = p - axisPoint;
        return delta.dotProduct(axis);
    };
    auto radialOf = [&](const AcGePoint3d &p) {
        const AcGeVector3d delta = p - axisPoint;
        return delta - axis * delta.dotProduct(axis);
    };
    auto rotate = [&](const AcGePoint3d &p, double theta) {
        const AcGeVector3d r = radialOf(p);
        const AcGeVector3d u = detail::normalizedOr(r, {1.0, 0.0, 0.0});
        const AcGeVector3d w = axis.crossProduct(u);
        const double radius = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
        return axisPoint +
               axis * axialOf(p) +
               u * (radius * std::cos(theta)) +
               w * (radius * std::sin(theta));
    };

    // Classify every profile edge; reject slanted ones.
    enum class EdgeSweep { Axial, Radial };
    std::vector<EdgeSweep> kinds(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcGePoint3d &a = profile[i];
        const AcGePoint3d &b = profile[(i + 1) % count];
        const double dz = (b - a).dotProduct(axis);
        const AcGeVector3d ra = radialOf(a);
        const AcGeVector3d rb = radialOf(b);
        const double dr = std::sqrt((rb - ra).dotProduct(rb - ra));
        const bool axial = dr < 1.0e-9 && std::abs(dz) > 1.0e-9;
        const bool radial = std::abs(dz) < 1.0e-9 && dr > 1.0e-9;
        if (!axial && !radial)
            return nullptr; // conical edge: outside the M6 scope
        kinds[i] = axial ? EdgeSweep::Axial : EdgeSweep::Radial;
    }

    // Profile centroid radial direction for theta-hat.
    AcGePoint3d centroid{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < count; ++i)
        centroid = centroid + profile[i];
    centroid = centroid * (1.0 / double(count));
    const AcGeVector3d thetaHat =
        detail::normalizedOr(axis.crossProduct(radialOf(centroid)),
                             {0.0, 1.0, 0.0});

    Builder builder(arena);
    std::map<PosKey, Vertex *> vertices;
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> edgeMap;
    std::map<std::pair<PosKey, PosKey>, Edge *> arcEdgeMap;
    Body *body = builder.createBody();
    Shell *shell = body->shell;

    // Caps: the profile polygon and its rotated copy.
    {
        std::vector<AcGePoint3d> endProfile(count);
        for (std::size_t i = 0; i < count; ++i)
            endProfile[i] = rotate(profile[i], sweep);
        makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                                  profile, count, thetaHat * -1.0);
        // End cap plane normal: theta-hat carried through the sweep.
        const AcGeVector3d endTheta = detail::normalizedOr(
            axis.crossProduct(radialOf(rotate(centroid, sweep))),
            thetaHat);
        makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                                  endProfile.data(), count, endTheta);
    }

    // Side patches: one per profile edge, ring [A, B, B', A'] with the
    // profile traversal A->B — the orientation that proved outward for
    // both the inner and outer surfaces of the worked example.
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcGePoint3d &A = profile[i];
        const AcGePoint3d &B = profile[(i + 1) % count];
        const AcGePoint3d Bp = rotate(B, sweep);
        const AcGePoint3d Ap = rotate(A, sweep);

        RingSide ring[4];
        if (kinds[i] == EdgeSweep::Axial)
        {
            const Cylinder cylinder{
                axisPoint + axis * axialOf(A), axis,
                std::sqrt(radialOf(A).dotProduct(radialOf(A)))};
            ring[0].point = A;
            ring[1].point = B;
            ring[1].arc = true;
            ring[1].arcCenter = axisPoint + axis * axialOf(B);
            ring[1].arcAxis = axis;
            ring[2].point = Bp;
            ring[3].point = Ap;
            ring[3].arc = true;
            ring[3].arcCenter = axisPoint + axis * axialOf(A);
            ring[3].arcAxis = axis;
            makeFaceFromRingSpec(builder, shell, vertices, edgeMap,
                                 arcEdgeMap, ring, 4,
                                 axis, true, cylinder);
        }
        else
        {
            // Planar annular sector in the horizontal plane z(A)=z(B).
            const AcGeVector3d sectorNormal =
                detail::normalizedOr(
                    (B - A).crossProduct(Bp - A), axis);
            ring[0].point = A;
            ring[1].point = B;
            ring[1].arc = true;
            ring[1].arcCenter = axisPoint + axis * axialOf(B);
            ring[1].arcAxis = axis;
            ring[2].point = Bp;
            ring[3].point = Ap;
            ring[3].arc = true;
            ring[3].arcCenter = axisPoint + axis * axialOf(A);
            ring[3].arcAxis = axis;
            makeFaceFromRingSpec(builder, shell, vertices, edgeMap,
                                 arcEdgeMap, ring, 4, sectorNormal);
        }
    }
    return body;
}

// Rules two profiles with the same corner count into a solid: straight
// connections between corresponding corners give planar trapezoid side
// faces (the parallel corresponding-edge condition is enforced), the
// caps are the profiles.  Both profiles must be listed CCW around the
// direction from the first to the second profile.  |outNormal|
// receives the first profile's Newell normal.
inline Body *loftProfiles(Arena &arena, const AcGePoint3d *profileA,
                          const AcGePoint3d *profileB, std::size_t count,
                          AcGeVector3d &outNormal)
{
    if (profileA == nullptr || profileB == nullptr || count < 3)
        return nullptr;

    // Newell normals.
    auto newell = [&](const AcGePoint3d *profile) {
        AcGeVector3d n{0.0, 0.0, 0.0};
        for (std::size_t i = 0; i < count; ++i)
        {
            const AcGePoint3d &a = profile[i];
            const AcGePoint3d &b = profile[(i + 1) % count];
            n = n + AcGeVector3d((a.y - b.y) * (a.z + b.z),
                                 (a.z - b.z) * (a.x + b.x),
                                 (a.x - b.x) * (a.y + b.y));
        }
        return detail::normalizedOr(n, {0.0, 0.0, 1.0});
    };
    const AcGeVector3d nA = newell(profileA);
    const AcGeVector3d nB = newell(profileB);

    // Parallel corresponding edges => planar trapezoid sides; and the
    // profiles must face the same way.
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcGeVector3d ea =
            profileA[(i + 1) % count] - profileA[i];
        const AcGeVector3d eb =
            profileB[(i + 1) % count] - profileB[i];
        const AcGeVector3d cross = ea.crossProduct(eb);
        if (cross.lengthSq() > 1.0e-9 * ea.lengthSq() * eb.lengthSq())
            return nullptr; // twisted ruling: side face non-planar
    }
    outNormal = nA;

    Builder builder(arena);
    std::map<PosKey, Vertex *> vertices;
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> edgeMap;
    Body *body = builder.createBody();
    Shell *shell = body->shell;

    // Caps: material lies from A toward B; orient the caps outward.
    AcGePoint3d cA{0.0, 0.0, 0.0};
    AcGePoint3d cB{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < count; ++i)
    {
        cA = cA + profileA[i];
        cB = cB + profileB[i];
    }
    cA = cA * (1.0 / double(count));
    cB = cB * (1.0 / double(count));
    const double towardB = (cB - cA).dotProduct(nA);
    if (std::abs(towardB) < 1.0e-9 || nA.dotProduct(nB) <= 0.0)
        return nullptr; // degenerate or flipped pairing
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                              profileA, count,
                              towardB > 0.0 ? nA * -1.0 : nA);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                              profileB, count,
                              towardB > 0.0 ? nB : nB * -1.0);

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t j = (i + 1) % count;
        const AcGePoint3d quad[4] = {profileA[i], profileA[j],
                                     profileB[j], profileB[i]};
        const AcGeVector3d sideNormal = detail::normalizedOr(
            (profileA[j] - profileA[i]).crossProduct(profileB[i] -
                                                     profileA[i]),
            nA);
        makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                                  quad, 4, sideNormal);
    }
    return body;
}

} // namespace brep
