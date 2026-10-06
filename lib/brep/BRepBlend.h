#pragma once

// brep blend operations (namespace brep) — milestone 4: chamfer, fillet,
// and thicken, following the rebuild-through-shared-maps pattern proven
// by the boolean milestone: every operation produces a NEW body whose
// faces are assembled through position-deduplicated vertex/edge maps, so
// connectivity (radial coedge pairs along section and blend curves) is
// welded by construction rather than patched in place.
//
// Scope (first milestone): convex shells, straight edges, planar face
// pairs; filletEdge further requires the 90-degree dihedral of box-like
// solids (general dihedrals cut elliptical arcs into the third faces,
// which wait for the NURBS phase).

#include <cmath>
#include <map>
#include <vector>

#include "brep/BRepBuild.h"

namespace brep
{

// Traversal start points of a face's ring, CCW around the outward
// normal (the invariant every builder in this library maintains).
inline std::vector<AcGePoint3d> faceRingPoints(const Face *face)
{
    std::vector<AcGePoint3d> ring;
    const CoEdge *first = face->outerLoop->first;
    const CoEdge *coedge = first;
    int guard = 0;
    while (guard++ < 256)
    {
        ring.push_back(coedge->forward ? coedge->edge->start->point
                                       : coedge->edge->end->point);
        coedge = coedge->next;
        if (coedge == first)
            break;
    }
    return ring;
}

// Clips a convex CCW ring by the half-space m.(x - origin) <= 0
// (Sutherland-Hodgman, single plane).  Intersection points encountered
// on crossing sides are appended to |crossings| when non-null.  Returns
// the clipped ring (CCW preserved); empty when the ring lies entirely
// outside.
inline std::vector<AcGePoint3d> clipRingByHalfSpace(
    const std::vector<AcGePoint3d> &ring, const AcGeVector3d &m,
    const AcGePoint3d &origin, std::vector<AcGePoint3d> *crossings)
{
    std::vector<AcGePoint3d> out;
    const std::size_t n = ring.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const AcGePoint3d &a = ring[i];
        const AcGePoint3d &b = ring[(i + 1) % n];
        const double sideA = m.dotProduct(a - origin);
        const double sideB = m.dotProduct(b - origin);
        const bool inA = sideA <= 1.0e-9;
        const bool inB = sideB <= 1.0e-9;
        if (inA)
            out.push_back(a);
        if (inA != inB)
        {
            const double t = sideA / (sideA - sideB);
            const AcGePoint3d cross(a.x + (b.x - a.x) * t,
                                    a.y + (b.y - a.y) * t,
                                    a.z + (b.z - a.z) * t);
            out.push_back(cross);
            if (crossings)
                crossings->push_back(cross);
        }
    }
    return out;
}

namespace detail
{

struct SharedMaps
{
    std::map<PosKey, Vertex *> vertices;
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> edges;
    std::map<std::pair<PosKey, PosKey>, Edge *> arcEdges;
};

// Unit inward normal of |edge| as seen from |face| (perpendicular to
// the edge, inside the face plane, pointing into the face polygon).
inline AcGeVector3d edgeInwardNormal(const Face *face, const Edge *edge,
                                     const CoEdge *faceCoedge)
{
    const AcGeVector3d start(edge->start->point.x, edge->start->point.y,
                             edge->start->point.z);
    const AcGeVector3d end(edge->end->point.x, edge->end->point.y,
                           edge->end->point.z);
    AcGeVector3d tangent = faceCoedge->forward ? end - start : start - end;
    {
        const double len = std::sqrt(tangent.x * tangent.x +
                                     tangent.y * tangent.y +
                                     tangent.z * tangent.z);
        tangent = AcGeVector3d(tangent.x / len, tangent.y / len,
                               tangent.z / len);
    }
    AcGeVector3d inward = face->surface.normal.crossProduct(tangent);
    const double len = std::sqrt(inward.x * inward.x + inward.y * inward.y +
                                 inward.z * inward.z);
    return AcGeVector3d(inward.x / len, inward.y / len, inward.z / len);
}

} // namespace detail

// Milestone 4-a: equal-setback chamfer of one straight edge between two
// planar faces.  Every face ring of the body is clipped by the chamfer
// plane (normal = the bisector n1 + n2 of the two face normals,
// through the setback point edge start + distance * inward1); the
// collected plane-crossing points are exactly the chamfer face's
// corners.  Returns a new body; nullptr when the operation does not
// apply (curved faces, a ring clipped away entirely).
inline Body *chamferEdge(const Body *body, const Edge *edge,
                         double distance)
{
    if (body == nullptr || edge == nullptr || edge->coedge[0] == nullptr ||
        edge->coedge[1] == nullptr)
        return nullptr;
    const Face *face1 = edge->coedge[0]->loop->face;
    const Face *face2 = edge->coedge[1]->loop->face;
    if (face1->cylindrical || face2->cylindrical)
        return nullptr;

    const AcGeVector3d inward1 =
        detail::edgeInwardNormal(face1, edge, edge->coedge[0]);
    const AcGeVector3d inward2 =
        detail::edgeInwardNormal(face2, edge, edge->coedge[1]);
    AcGeVector3d bisector = face1->surface.normal + face2->surface.normal;
    {
        const double len = std::sqrt(bisector.x * bisector.x +
                                     bisector.y * bisector.y +
                                     bisector.z * bisector.z);
        if (len < 1.0e-12)
            return nullptr; // opposite faces cannot be chamfered
        bisector = AcGeVector3d(bisector.x / len, bisector.y / len,
                                bisector.z / len);
    }
    const AcGePoint3d planePoint =
        edge->start->point + inward1 * distance;

    std::vector<AcGePoint3d> crossings;
    std::vector<std::pair<std::vector<AcGePoint3d>, AcGeVector3d>> clipped;
    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->cylindrical)
            return nullptr;
        std::vector<AcGePoint3d> ring =
            clipRingByHalfSpace(faceRingPoints(face), bisector,
                                planePoint, &crossings);
        if (ring.size() < 3)
            return nullptr; // a face vanished: setback too large here
        clipped.emplace_back(std::move(ring), face->surface.normal);
    }

    // Deduplicated crossing points form the chamfer face corners.
    std::vector<AcGePoint3d> corners;
    std::map<PosKey, bool> seen;
    for (const AcGePoint3d &point : crossings)
    {
        const PosKey key = positionKey(point);
        if (seen.emplace(key, true).second)
            corners.push_back(point);
    }
    if (corners.size() < 3)
        return nullptr;

    Arena &arena = *(new Arena());
    Builder builder(arena);
    detail::SharedMaps maps;
    Body *result = builder.createBody();
    Shell *shell = result->shell;
    for (const auto &faceRing : clipped)
        makePlanarFaceFromCorners(builder, shell, maps.vertices,
                                  maps.edges, faceRing.first.data(),
                                  faceRing.first.size(),
                                  faceRing.second);
    makePlanarFaceFromCorners(builder, shell, maps.vertices, maps.edges,
                              corners.data(), corners.size(), bisector);
    return result;
}

namespace detail
{

// The face sharing |corner| that is neither of the two edge faces.
inline const Face *thirdFaceAt(const Body *body, const Face *face1,
                               const Face *face2,
                               const AcGePoint3d &corner)
{
    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face == face1 || face == face2 || face->cylindrical)
            continue;
        for (const AcGePoint3d &point : faceRingPoints(face))
            if (point.distanceTo(corner) < 1.0e-9)
                return face;
    }
    return nullptr;
}

// Rebuilds |face|'s ring with |corner| replaced by the pair
// tangent-on-face2, arc, tangent-on-face1 — ordered by which neighbor
// ring side at the corner is shared with face1.  The arc side carries
// |arcCenter| and the face normal as its axis.
inline bool cornerReplacedRing(const Face *face, const Face *face1,
                               const AcGePoint3d &corner,
                               const AcGePoint3d &tangent1,
                               const AcGePoint3d &tangent2,
                               const AcGePoint3d &arcCenter,
                               std::vector<RingSide> &out)
{
    const std::vector<AcGePoint3d> ring = faceRingPoints(face);
    const std::size_t n = ring.size();
    std::size_t cornerIndex = n;
    for (std::size_t i = 0; i < n; ++i)
        if (ring[i].distanceTo(corner) < 1.0e-9)
        {
            cornerIndex = i;
            break;
        }
    if (cornerIndex == n)
        return false;

    // Ring side i runs from ring[i] to ring[i+1] and belongs to the
    // i-th coedge of the loop walk.  The side ending at cornerIndex is
    // coedge cornerIndex-1; its radial partner's face tells whether
    // the approach runs along face1's tangent edge or face2's.
    const CoEdge *first = face->outerLoop->first;
    const CoEdge *incomingCoedge = nullptr;
    const CoEdge *coedge = first;
    std::size_t index = 0;
    int guard = 0;
    while (guard++ < 256)
    {
        if ((index + 1) % n == cornerIndex)
            incomingCoedge = coedge;
        coedge = coedge->next;
        ++index;
        if (coedge == first)
            break;
    }
    if (incomingCoedge == nullptr)
        return false;
    const Face *incomingNeighbor =
        incomingCoedge->edge->coedge[0]->loop->face == face
            ? incomingCoedge->edge->coedge[1]->loop->face
            : incomingCoedge->edge->coedge[0]->loop->face;
    const bool incomingAlongFace1 = incomingNeighbor == face1;

    out.reserve(n + 2);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (i == cornerIndex)
        {
            if (incomingAlongFace1)
            {
                RingSide arc;
                arc.point = tangent1;
                arc.arc = true;
                arc.arcCenter = arcCenter;
                arc.arcAxis = face->surface.normal;
                out.push_back(arc);
                RingSide t2;
                t2.point = tangent2;
                out.push_back(t2);
            }
            else
            {
                RingSide arc;
                arc.point = tangent2;
                arc.arc = true;
                arc.arcCenter = arcCenter;
                arc.arcAxis = face->surface.normal;
                out.push_back(arc);
                RingSide t1;
                t1.point = tangent1;
                out.push_back(t1);
            }
            continue;
        }
        RingSide side;
        side.point = ring[i];
        out.push_back(side);
    }
    return true;
}

} // namespace detail

// Milestone 4-c: rolling-ball fillet of one straight edge with a 90
// degree dihedral (box-like solids).  The two edge faces lose a strip
// of width |radius| (half-space clips); the third face at each edge
// end swaps its corner for a quarter-circle arc side; the new
// quarter-cylinder patch closes the shell.  Arc edges canonicalize
// through the shared arc-edge map, so the patches and the clipped
// third faces weld along single shared arc edges.  Returns a new body;
// nullptr when the edge is not a straight 90-degree box edge.
inline Body *filletEdge(const Body *body, const Edge *edge, double radius)
{
    if (body == nullptr || edge == nullptr || edge->coedge[0] == nullptr ||
        edge->coedge[1] == nullptr || edge->isArc)
        return nullptr;
    const Face *face1 = edge->coedge[0]->loop->face;
    const Face *face2 = edge->coedge[1]->loop->face;
    if (face1->cylindrical || face2->cylindrical)
        return nullptr;
    if (std::abs(face1->surface.normal.dotProduct(face2->surface.normal)) >
        1.0e-9)
        return nullptr; // not a 90-degree dihedral

    const AcGeVector3d inward1 =
        detail::edgeInwardNormal(face1, edge, edge->coedge[0]);
    const AcGeVector3d inward2 =
        detail::edgeInwardNormal(face2, edge, edge->coedge[1]);
    const AcGePoint3d &S = edge->start->point;
    const AcGePoint3d &T = edge->end->point;
    AcGeVector3d axisDir = T - S;
    {
        const double len = std::sqrt(axisDir.x * axisDir.x +
                                     axisDir.y * axisDir.y +
                                     axisDir.z * axisDir.z);
        if (len < 1.0e-12)
            return nullptr;
        axisDir = AcGeVector3d(axisDir.x / len, axisDir.y / len,
                               axisDir.z / len);
    }
    if (radius <= 0.0 ||
        radius >= S.distanceTo(T))
        return nullptr;

    // Tangent points on the two edge faces, and the rolling-ball axis
    // anchor at each end (the arc center in the third face's plane).
    const AcGePoint3d S1 = S + inward1 * radius;
    const AcGePoint3d S2 = S + inward2 * radius;
    const AcGePoint3d T1 = T + inward1 * radius;
    const AcGePoint3d T2 = T + inward2 * radius;
    const AcGePoint3d axisS = S + (inward1 + inward2) * radius;
    const AcGePoint3d axisT = T + (inward1 + inward2) * radius;

    const Face *face3 = detail::thirdFaceAt(body, face1, face2, S);
    const Face *face4 = detail::thirdFaceAt(body, face1, face2, T);
    if (face3 == nullptr || face4 == nullptr || face3 == face4)
        return nullptr;

    std::vector<RingSide> ring3;
    std::vector<RingSide> ring4;
    if (!detail::cornerReplacedRing(face3, face1, S, S1, S2, axisS, ring3))
        return nullptr;
    if (!detail::cornerReplacedRing(face4, face1, T, T1, T2, axisT, ring4))
        return nullptr;

    // face1/face2: strip clips (keep inward1.(x - S) >= radius).
    const AcGePoint3d keep1 = S + inward1 * radius;
    const AcGePoint3d keep2 = S + inward2 * radius;
    const AcGeVector3d m1 = inward1 * -1.0;
    const AcGeVector3d m2 = inward2 * -1.0;
    std::vector<AcGePoint3d> ring1 =
        clipRingByHalfSpace(faceRingPoints(face1), m1, keep1, nullptr);
    std::vector<AcGePoint3d> ring2 =
        clipRingByHalfSpace(faceRingPoints(face2), m2, keep2, nullptr);
    if (ring1.size() < 3 || ring2.size() < 3)
        return nullptr;

    Arena &arena = *(new Arena());
    Builder builder(arena);
    detail::SharedMaps maps;
    Body *result = builder.createBody();
    Shell *shell = result->shell;

    const Cylinder cylinder{axisS, axisDir, radius};

    auto lineSpec = [](const std::vector<AcGePoint3d> &points) {
        std::vector<RingSide> spec;
        spec.reserve(points.size());
        for (const AcGePoint3d &p : points)
        {
            RingSide side;
            side.point = p;
            spec.push_back(side);
        }
        return spec;
    };

    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face == face1)
        {
            const std::vector<RingSide> spec = lineSpec(ring1);
            makeFaceFromRingSpec(builder, shell, maps.vertices,
                                 maps.edges, maps.arcEdges, spec.data(),
                                 spec.size(), face->surface.normal);
            continue;
        }
        if (face == face2)
        {
            const std::vector<RingSide> spec = lineSpec(ring2);
            makeFaceFromRingSpec(builder, shell, maps.vertices,
                                 maps.edges, maps.arcEdges, spec.data(),
                                 spec.size(), face->surface.normal);
            continue;
        }
        if (face == face3)
        {
            makeFaceFromRingSpec(builder, shell, maps.vertices,
                                 maps.edges, maps.arcEdges, ring3.data(),
                                 ring3.size(), face->surface.normal);
            continue;
        }
        if (face == face4)
        {
            makeFaceFromRingSpec(builder, shell, maps.vertices,
                                 maps.edges, maps.arcEdges, ring4.data(),
                                 ring4.size(), face->surface.normal);
            continue;
        }
        // Untouched face: rebuilt verbatim (straight sides only in the
        // M4 scope).
        std::vector<AcGePoint3d> ring = faceRingPoints(face);
        bool hasArcs = false;
        {
            const CoEdge *first = face->outerLoop->first;
            const CoEdge *ce = first;
            int guard = 0;
            do
            {
                hasArcs = hasArcs || ce->edge->isArc;
                ce = ce->next;
            } while (ce != first && guard++ < 256);
        }
        if (hasArcs)
            return nullptr; // curved input beyond the M4 scope
        const std::vector<RingSide> spec = lineSpec(ring);
        makeFaceFromRingSpec(builder, shell, maps.vertices, maps.edges,
                             maps.arcEdges, spec.data(), spec.size(),
                             face->surface.normal);
    }

    // Quarter-cylinder patch: CCW around outward radial normals the
    // ring walks T2 -> bottom arc -> T1 -> straight -> S1 -> top arc
    // -> S2 -> straight -> close.  Arc sides carry the cylinder axis;
    // the ring builder canonicalizes spans, so patch and third faces
    // share single arc edges with opposite traversal flags.
    RingSide patch[4];
    patch[0].point = T2;
    patch[0].arc = true;
    patch[0].arcCenter = axisT;
    patch[0].arcAxis = axisDir;
    patch[1].point = T1;
    patch[2].point = S1;
    patch[2].arc = true;
    patch[2].arcCenter = axisS;
    patch[2].arcAxis = axisDir;
    patch[3].point = S2;
    makeFaceFromRingSpec(builder, shell, maps.vertices, maps.edges,
                         maps.arcEdges, patch, 4,
                         AcGeVector3d(0.0, 0.0, 1.0), true, cylinder);
    return result;
}

// Milestone 4-b: thickens a one-face planar sheet body into a slab by
// extruding along the sheet's outward normal (the AutoCAD THICKEN
// direction).  |outNormal| receives the sheet normal.
inline Body *thickenSheet(Arena &arena, const Body *sheet,
                          double thickness, AcGeVector3d &outNormal)
{
    if (sheet == nullptr || sheet->shell == nullptr ||
        thickness <= 0.0)
        return nullptr;
    const Face *face = sheet->shell->firstFace;
    if (face == nullptr || face->next != nullptr || face->cylindrical)
        return nullptr;
    const std::vector<AcGePoint3d> ring = faceRingPoints(face);
    if (ring.size() < 3)
        return nullptr;
    outNormal = face->surface.normal;
    const AcGeVector3d height = outNormal * thickness;
    return extrudePolygon(arena, ring.data(), ring.size(), height,
                          outNormal);
}

} // namespace brep
