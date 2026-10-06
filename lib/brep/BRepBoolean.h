#pragma once

// brep::boolean (namespace brep) — milestone 3-b: planar polyhedron
// boolean operations on convex solid shells, following the phase
// structure of the CZY3DKernel reference (external/CZY3DKernel-main/
// CAD/Boolean): section-segment computation, face splitting, piece
// classification (In/Out against the other solid), operation-rule
// selection, and result assembly.
//
// The result is assembled through makePlanarFaceFromCorners with SHARED
// vertex/edge maps, so the section curves shared between an A-piece and
// a B-piece weld into single edges with radial coedge pairs — this is
// the "connectivity correct" requirement of the milestone.
//
// Scope (first milestone): convex solids, no coplanar face pairs
// (On-classification and solid-angle In/Out for untouched faces arrive
// with the general phase).

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "brep/BRepBuild.h"
#include "ge/gepredicates.h"

namespace brep
{

enum class BoolOp
{
    Union,
    Intersect,
    Subtract // A minus B
};

// A convex solid: outward planes + the topology's face rings.
struct BoolSolid
{
    struct PlaneData
    {
        AcGeVector3d normal; // outward, unit
        AcGePoint3d point;
    };

    struct FaceData
    {
        AcGeVector3d normal;
        std::vector<AcGePoint3d> ring; // geometric order, CCW around normal
    };

    std::vector<PlaneData> planes;
    std::vector<FaceData> faces;

    static BoolSolid fromBody(const Body *body)
    {
        BoolSolid solid;
        for (const Face *face = body->shell->firstFace; face;
             face = face->next)
        {
            solid.planes.push_back({face->surface.normal,
                                    face->surface.origin});
            FaceData fd;
            fd.normal = face->surface.normal;
            const CoEdge *first = face->outerLoop->first;
            const CoEdge *ce = first;
            do
            {
                fd.ring.push_back(ce->forward ? ce->edge->end->point
                                              : ce->edge->start->point);
                ce = ce->next;
            } while (ce != first);
            solid.faces.push_back(std::move(fd));
        }
        return solid;
    }

    // Point in (or on) the convex solid: all outward plane distances <= eps.
    bool containsPoint(const AcGePoint3d &point) const
    {
        for (const PlaneData &plane : planes)
            if (plane.normal.dotProduct(point - plane.point) > 1.0e-9)
                return false;
        return true;
    }
};

// A planar piece of a split face.
struct FacePiece
{
    std::vector<AcGePoint2d> ring2d; // in the face's (u, v) frame
    AcGePoint3d origin;              // frame origin (world)
    AcGeVector3d u, v, n;            // frame (right-handed)
    int source;                      // 0 = shell A, 1 = shell B

    AcGePoint3d toWorld(const AcGePoint2d &p) const
    {
        return AcGePoint3d(origin.x + p.x * u.x + p.y * v.x,
                           origin.y + p.x * u.y + p.y * v.y,
                           origin.z + p.x * u.z + p.y * v.z);
    }
    AcGePoint2d toPlane(const AcGePoint3d &p) const
    {
        const AcGeVector3d d = p - origin;
        return {d.dotProduct(u), d.dotProduct(v)};
    }
    AcGePoint3d centroid() const
    {
        AcGePoint3d c{0.0, 0.0, 0.0};
        for (const AcGePoint2d &p : ring2d)
            c = c + toWorld(p);
        return AcGePoint3d(c.x / double(ring2d.size()),
                           c.y / double(ring2d.size()),
                           c.z / double(ring2d.size()));
    }
};

// Splits a convex polygon (CCW in 2D) by the infinite line through
// point p with direction d.  Returns the left and right pieces
// (left = CCW side of d).  Either may be empty when the line misses.
inline void splitConvexByLine(const std::vector<AcGePoint2d> &poly,
                              const AcGePoint2d &p,
                              const AcGeVector2d &d,
                              std::vector<AcGePoint2d> &left,
                              std::vector<AcGePoint2d> &right)
{
    left.clear();
    right.clear();
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const AcGePoint2d &cur = poly[i];
        const AcGePoint2d &next = poly[(i + 1) % n];
        const double sideCur = d.y * (cur.x - p.x) - d.x * (cur.y - p.y);
        const double sideNext = d.y * (next.x - p.x) - d.x * (next.y - p.y);
        if (sideCur >= 0.0)
            left.push_back(cur);
        if (sideCur <= 0.0)
            right.push_back(cur);
        if ((sideCur > 0.0) != (sideNext > 0.0) &&
            std::abs(sideCur - sideNext) > 1.0e-12)
        {
            const double t = sideCur / (sideCur - sideNext);
            const AcGePoint2d crossPt{
                cur.x + (next.x - cur.x) * t,
                cur.y + (next.y - cur.y) * t};
            left.push_back(crossPt);
            right.push_back(crossPt);
        }
    }
}

// Clips the line (point p, direction d) to a convex polygon ring (CCW).
// Slab-style interval accumulation; returns [tEnter, tExit], nullopt if
// the line misses.  Parallel edges outside the half-plane reject.
inline std::optional<std::pair<double, double>> clipLineToPolygon(
    const AcGePoint2d &p, const AcGeVector2d &d,
    const std::vector<AcGePoint2d> &poly)
{
    double tEnter = -std::numeric_limits<double>::infinity();
    double tExit = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < poly.size(); ++i)
    {
        const AcGePoint2d &a = poly[i];
        const AcGeVector2d edge{poly[(i + 1) % poly.size()].x - a.x,
                                poly[(i + 1) % poly.size()].y - a.y};
        const double crossD = d.x * edge.y - d.y * edge.x;
        const double offset = (a.x - p.x) * edge.y - (a.y - p.y) * edge.x;
        if (std::abs(crossD) < 1.0e-12)
        {
            if (offset < -1.0e-9) // line outside this edge's half-plane
                return std::nullopt;
            continue;
        }
        const double t = offset / crossD;
        if (crossD > 0.0)
            tEnter = std::max(tEnter, t);
        else
            tExit = std::min(tExit, t);
        if (tEnter > tExit)
            return std::nullopt;
    }
    if (tExit <= tEnter)
        return std::nullopt;
    return std::make_pair(tEnter, tExit);
}

// Splits every face of |solid| by all non-parallel faces of |other|,
// producing convex pieces classified later.
inline std::vector<FacePiece> splitFaces(
    const BoolSolid &solid, const BoolSolid &other, int source)
{
    std::vector<FacePiece> pieces;
    for (const BoolSolid::FaceData &face : solid.faces)
    {
        // Face frame: origin at ring[0], u/v orthogonal to the normal.
        FacePiece base;
        base.origin = face.ring[0];
        base.n = face.normal;
        base.u = std::abs(face.normal.z) < 0.9
                     ? AcGeVector3d(0.0, 0.0, 1.0)
                     : AcGeVector3d(1.0, 0.0, 0.0);
        base.u = face.normal.crossProduct(base.u);
        {
            const double len = std::sqrt(base.u.x * base.u.x +
                                         base.u.y * base.u.y +
                                         base.u.z * base.u.z);
            base.u = AcGeVector3d(base.u.x / len, base.u.y / len,
                                  base.u.z / len);
        }
        base.v = face.normal.crossProduct(base.u);
        base.source = source;
        for (const AcGePoint3d &p : face.ring)
            base.ring2d.push_back(base.toPlane(p));

        std::vector<FacePiece> current{base};

        // Section segments: for every non-parallel face of the other
        // solid, the 3D line (planeA x planeB) clipped to BOTH this
        // face's ring and the other solid's half-spaces is a
        // splitting segment on this face's plane.  All clipping is
        // done in 3D — projections would degenerate for
        // perpendicular face pairs.
        for (const BoolSolid::FaceData &otherFace : other.faces)
        {
            const AcGeVector3d nA = face.normal;
            const AcGeVector3d nB = otherFace.normal;
            const AcGeVector3d sectionDir = nA.crossProduct(nB);
            if (sectionDir.lengthSq() < 1.0e-24)
                continue; // parallel planes

            // Point on the section line (standard two-plane formula,
            // unit normals: p = ((dA - dB*cos)*nA + (dB - dA*cos)*nB)
            // / sin^2).
            const double cosAB = nA.dotProduct(nB);
            const double sin2 = 1.0 - cosAB * cosAB;
            const AcGePoint3d b0(otherFace.ring[0].x,
                                 otherFace.ring[0].y,
                                 otherFace.ring[0].z);
            const AcGeVector3d b0v(b0.x, b0.y, b0.z);
            const double dB = nB.dotProduct(b0v);
            const AcGePoint3d a0(face.ring[0].x, face.ring[0].y,
                                 face.ring[0].z);
            const AcGeVector3d a0v(a0.x, a0.y, a0.z);
            const double dA = nA.dotProduct(a0v);
            const AcGeVector3d point3 =
                AcGeVector3d(
                    ((dA - dB * cosAB) * nA.x +
                      (dB - dA * cosAB) * nB.x) / sin2,
                    ((dA - dB * cosAB) * nA.y +
                      (dB - dA * cosAB) * nB.y) / sin2,
                    ((dA - dB * cosAB) * nA.z +
                      (dB - dA * cosAB) * nB.z) / sin2);
            const AcGeVector3d ld3 = sectionDir;

            // Interval inside OUR face ring: per ring edge half-space
            // m = edge x nA (outward for the CCW ring).
            double tA0 = -std::numeric_limits<double>::infinity();
            double tA1 = std::numeric_limits<double>::infinity();
            bool okA = true;
            for (std::size_t i = 0; i < face.ring.size(); ++i)
            {
                const AcGePoint3d &pa = face.ring[i];
                const AcGePoint3d &pb = face.ring[(i + 1) % face.ring.size()];
                const AcGeVector3d m = (pb - pa).crossProduct(nA);
                const double md = m.dotProduct(ld3);
                const double ma = m.dotProduct(
                    AcGeVector3d(pa.x, pa.y, pa.z) - point3);
                if (std::abs(md) < 1.0e-12)
                {
                    if (ma < -1.0e-9) { okA = false; break; }
                    continue;
                }
                const double t = ma / md;
                if (md > 0.0) tA1 = std::min(tA1, t);
                else tA0 = std::max(tA0, t);
                if (tA0 > tA1) { okA = false; break; }
            }
            if (!okA)
                continue;

            // Interval inside the OTHER solid: its outward planes
            // (convex = intersection of half-spaces, exact).
            double tB0 = -std::numeric_limits<double>::infinity();
            double tB1 = std::numeric_limits<double>::infinity();
            for (const BoolSolid::PlaneData &plane : other.planes)
            {
                const double md = plane.normal.dotProduct(ld3);
                const AcGeVector3d pref(plane.point.x, plane.point.y,
                                       plane.point.z);
                const double ma = plane.normal.dotProduct(pref - point3);
                if (std::abs(md) < 1.0e-12)
                {
                    if (ma < -1.0e-9) { okA = false; break; }
                    continue;
                }
                const double t = ma / md;
                if (md > 0.0) tB1 = std::min(tB1, t);
                else tB0 = std::max(tB0, t);
                if (tB0 > tB1) { okA = false; break; }
            }
            if (!okA)
                continue;

            const double sEnter = std::max(tA0, tB0);
            const double sExit = std::min(tA1, tB1);
            if (sEnter >= sExit)
                continue;

            const AcGeVector3d segV0 = point3 + ld3 * sEnter;
            const AcGeVector3d segV1 = point3 + ld3 * sExit;
            const AcGePoint3d segStart(segV0.x, segV0.y, segV0.z);
            const AcGePoint3d segEnd(segV1.x, segV1.y, segV1.z);

            std::vector<FacePiece> next;
            for (const FacePiece &piece : current)
            {
                std::vector<AcGePoint2d> left, right;
                const AcGePoint2d s2a = base.toPlane(segStart);
                const AcGePoint2d s2b = base.toPlane(segEnd);
                splitConvexByLine(piece.ring2d, s2a,
                                  {s2b.x - s2a.x, s2b.y - s2a.y},
                                  left, right);
                if (left.size() >= 3)
                {
                    FacePiece half = piece;
                    half.ring2d = left;
                    next.push_back(std::move(half));
                }
                if (right.size() >= 3)
                {
                    FacePiece half = piece;
                    half.ring2d = right;
                    next.push_back(std::move(half));
                }
            }
            if (!next.empty())
                current = std::move(next);
        }
        for (FacePiece &piece : current)
        {
            piece.source = source;
            pieces.push_back(std::move(piece));
        }
    }
    return pieces;
}


// Classification of a piece against the OTHER solid via centroid
// (convex half-space test).
inline bool pieceInside(const FacePiece &piece, const BoolSolid &other)
{
    return other.containsPoint(piece.centroid());
}

// Operation rule from the CZY3DKernel GroupValidition truth table:
// INTERSECT keeps In, UNION keeps Out, SUBTRACT keeps Out for A and
// In for B.  (On-classification arrives with coplanar-face support.)
inline bool opKeeps(int source, bool inside, BoolOp op)
{
    if (op == BoolOp::Intersect)
        return inside;
    if (op == BoolOp::Union)
        return !inside;
    return source == 0 ? !inside : inside;
}

// Performs the boolean and assembles the result as a new body with
// shared maps: pieces from both solids weld along identical section
// curves (position-keyed dedup), which is the connectivity guarantee.
inline Body *performBoolean(const Body *bodyA, const Body *bodyB,
                            BoolOp op)
{
    const BoolSolid solidA = BoolSolid::fromBody(bodyA);
    const BoolSolid solidB = BoolSolid::fromBody(bodyB);
    std::vector<FacePiece> pieces = splitFaces(solidA, solidB, 0);
    const std::vector<FacePiece> piecesB = splitFaces(solidB, solidA, 1);
    pieces.insert(pieces.end(), piecesB.begin(), piecesB.end());

    // The result topology lives in this arena; the arena itself is
    // intentionally heap-anchored so the returned body stays valid.
    Arena &arena = *(new Arena());
    Builder builder(arena);
    std::map<PosKey, Vertex *> vertices;
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> edgeMap;
    Body *result = builder.createBody();
    Shell *shell = result->shell;

    for (const FacePiece &piece : pieces)
    {
        const bool inside = pieceInside(
            piece, piece.source == 0 ? solidB : solidA);
        if (!opKeeps(piece.source, inside, op))
            continue;

        std::vector<AcGePoint3d> ring;
        for (const AcGePoint2d &p2 : piece.ring2d)
            ring.push_back(piece.toWorld(p2));
        AcGeVector3d n = piece.n;
        if (op == BoolOp::Subtract && piece.source == 1)
            n = AcGeVector3d(-n.x, -n.y, -n.z);
        makePlanarFaceFromCorners(builder, shell, vertices, edgeMap,
                                  ring.data(), ring.size(), n);
    }
    return result;
}
} // namespace brep
