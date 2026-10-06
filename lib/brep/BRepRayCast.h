#pragma once

// brep rayCast (namespace brep) — milestone 4: picking against a body.
// Faces are tessellated (planar rings fan out; arc sides subdivide),
// triangle bounds go through ge::AcGeBoundBvh for candidate pruning,
// and candidates resolve through exact Möller-Trumbore tests so the
// reported hit is the true nearest surface point, not a bound entry.

#include <optional>
#include <vector>

#include "ge/gebvh.h"

#include "brep/BRep.h"
#include "brep/BRepBlend.h" // faceRingPoints

namespace brep
{

struct RayHit
{
    const Face *face = nullptr;
    AcGePoint3d point{0.0, 0.0, 0.0};
    AcGeVector3d normal{0.0, 0.0, 1.0}; // outward geometric normal
    double distance = 0.0;
};

// Nearest ray-body hit along |direction| (need not be unit), or
// nullopt when the ray misses.
inline std::optional<RayHit> rayCast(const Body *body,
                                     const AcGePoint3d &origin,
                                     const AcGeVector3d &direction)
{
    if (body == nullptr || body->shell == nullptr)
        return std::nullopt;

    // Tessellate: triangle soup with a face index per triangle.  Planar
    // rings fan out; a cylindrical patch strips between its two arc
    // polylines (a blind ring fan would hang triangles through the
    // patch interior, since the patch ring is not planar).  Arc sides
    // subdivide into 16 chords so curved patches pick cleanly.
    struct Tri
    {
        AcGePoint3d a, b, c;
        const Face *face;
    };
    std::vector<Tri> triangles;
    constexpr int kArcSlices = 16;
    auto arcPolyline = [](const Edge *edge, bool forward, int slices) {
        // Traversal-direction polyline along the arc, endpoints included.
        const AcGeCircArc3d &arc = edge->arc;
        const AcGeVector3d u = arc.referenceAxis();
        const AcGeVector3d w = arc.normal.crossProduct(u);
        const double phiStart = forward ? arc.startAngle : arc.endAngle;
        const double span = forward ? arc.endAngle - arc.startAngle
                                    : arc.startAngle - arc.endAngle;
        std::vector<AcGePoint3d> points;
        points.reserve(std::size_t(slices) + 1);
        for (int s = 0; s <= slices; ++s)
        {
            const double phi = phiStart + span * (double(s) / slices);
            points.push_back(arc.center +
                             u * (arc.radius * std::cos(phi)) +
                             w * (arc.radius * std::sin(phi)));
        }
        return points;
    };
    for (const Face *face = body->shell->firstFace; face != nullptr;
         face = face->next)
    {
        if (face->outerLoop == nullptr)
            continue;

        if (face->cylindrical)
        {
            // Collect the two arc polylines, then orient both in the
            // same angular direction before strip-pairing (the patch
            // ring walks them oppositely).
            std::vector<AcGePoint3d> lines[2];
            int lineCount = 0;
            const CoEdge *first = face->outerLoop->first;
            const CoEdge *coedge = first;
            int guard = 0;
            while (guard++ < 256 && lineCount < 2)
            {
                if (coedge->edge->isArc)
                    lines[lineCount++] = arcPolyline(
                        coedge->edge, coedge->forward, kArcSlices);
                coedge = coedge->next;
                if (coedge == first)
                    break;
            }
            if (lineCount == 2)
            {
                const AcGePoint3d &axisOrigin = face->cylinder.origin;
                const AcGeVector3d &axis = face->cylinder.axis;
                auto radial = [&](const AcGePoint3d &p) {
                    const AcGeVector3d delta = p - axisOrigin;
                    return delta - axis * delta.dotProduct(axis);
                };
                auto sense = [&](const std::vector<AcGePoint3d> &pts) {
                    return radial(pts[0])
                        .crossProduct(radial(pts[1]))
                        .dotProduct(axis);
                };
                for (std::vector<AcGePoint3d> &line : lines)
                    if (sense(line) < 0.0)
                        std::reverse(line.begin(), line.end());
                for (int i = 0; i < kArcSlices; ++i)
                {
                    triangles.push_back({lines[0][i], lines[0][i + 1],
                                         lines[1][i + 1], face});
                    triangles.push_back({lines[0][i], lines[1][i + 1],
                                         lines[1][i], face});
                }
            }
            continue;
        }

        std::vector<AcGePoint3d> ring;
        const CoEdge *first = face->outerLoop->first;
        const CoEdge *coedge = first;
        int guard = 0;
        while (guard++ < 256)
        {
            const Edge *edge = coedge->edge;
            const AcGePoint3d &start = coedge->forward
                ? edge->start->point
                : edge->end->point;
            ring.push_back(start);
            if (edge->isArc)
            {
                const AcGeCircArc3d &arc = edge->arc;
                const AcGeVector3d u = arc.referenceAxis();
                const AcGeVector3d w = arc.normal.crossProduct(u);
                double span = arc.endAngle - arc.startAngle;
                if (!coedge->forward)
                    span = -span;
                const double phiStart =
                    coedge->forward ? arc.startAngle : arc.endAngle;
                for (int s = 1; s < kArcSlices; ++s)
                {
                    const double phi =
                        phiStart + span * (double(s) / kArcSlices);
                    ring.push_back(arc.center +
                                   u * (arc.radius * std::cos(phi)) +
                                   w * (arc.radius * std::sin(phi)));
                }
            }
            coedge = coedge->next;
            if (coedge == first)
                break;
        }
        for (std::size_t i = 1; i + 1 < ring.size(); ++i)
            triangles.push_back({ring[0], ring[i], ring[i + 1], face});
    }
    if (triangles.empty())
        return std::nullopt;

    std::vector<ge::AcGeBoundBox3d> bounds;
    bounds.reserve(triangles.size());
    for (const Tri &tri : triangles)
    {
        ge::AcGeBoundBox3d box;
        box.min = AcGePoint3d(std::min(tri.a.x, std::min(tri.b.x, tri.c.x)),
                              std::min(tri.a.y, std::min(tri.b.y, tri.c.y)),
                              std::min(tri.a.z, std::min(tri.b.z, tri.c.z)));
        box.max = AcGePoint3d(std::max(tri.a.x, std::max(tri.b.x, tri.c.x)),
                              std::max(tri.a.y, std::max(tri.b.y, tri.c.y)),
                              std::max(tri.a.z, std::max(tri.b.z, tri.c.z)));
        bounds.push_back(box);
    }

    ge::AcGeBoundBvh bvh;
    bvh.build(std::move(bounds));

    // Candidate gather: ray-AABB slab predicate, then exact triangle
    // intersection for the nearest hit.
    const AcGeVector3d dir = direction.normal();
    const glm::dvec3 d(dir.x, dir.y, dir.z);
    auto slabHit = [&](const ge::AcGeBoundBox3d &box) {
        double tEnter = -std::numeric_limits<double>::infinity();
        double tExit = std::numeric_limits<double>::infinity();
        const double p[3] = {origin.x, origin.y, origin.z};
        const double lo[3] = {box.min.x, box.min.y, box.min.z};
        const double hi[3] = {box.max.x, box.max.y, box.max.z};
        const double dv[3] = {dir.x, dir.y, dir.z};
        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(dv[axis]) < 1.0e-12)
            {
                if (p[axis] < lo[axis] || p[axis] > hi[axis])
                    return false;
                continue;
            }
            double t0 = (lo[axis] - p[axis]) / dv[axis];
            double t1 = (hi[axis] - p[axis]) / dv[axis];
            if (t0 > t1)
                std::swap(t0, t1);
            tEnter = std::max(tEnter, t0);
            tExit = std::min(tExit, t1);
            if (tEnter > tExit)
                return false;
        }
        return tExit >= 0.0;
    };

    std::vector<std::uint32_t> candidates;
    bvh.collect(slabHit, candidates);

    std::optional<RayHit> best;
    for (std::uint32_t index : candidates)
    {
        const Tri &tri = triangles[index];
        // Möller-Trumbore.
        const AcGeVector3d e1 = tri.b - tri.a;
        const AcGeVector3d e2 = tri.c - tri.a;
        const AcGeVector3d pvec = dir.crossProduct(e2);
        const double det = e1.dotProduct(pvec);
        if (std::abs(det) < 1.0e-12)
            continue;
        const AcGeVector3d tvec = origin - tri.a;
        const double u = tvec.dotProduct(pvec) / det;
        if (u < -1.0e-9 || u > 1.0 + 1.0e-9)
            continue;
        const AcGeVector3d qvec = tvec.crossProduct(e1);
        const double v = dir.dotProduct(qvec) / det;
        if (v < -1.0e-9 || u + v > 1.0 + 1.0e-9)
            continue;
        const double t = e2.dotProduct(qvec) / det;
        if (t < 1.0e-9)
            continue;
        if (best.has_value() && t >= best->distance)
            continue;

        RayHit hit;
        hit.face = tri.face;
        hit.distance = t;
        hit.point = origin + dir * t;
        if (tri.face->cylindrical)
        {
            // Outward radial normal.
            const AcGeVector3d radial = hit.point - tri.face->cylinder.origin;
            const AcGeVector3d &axis = tri.face->cylinder.axis;
            const AcGeVector3d axial = axis * radial.dotProduct(axis);
            hit.normal = (radial - axial).normal();
        }
        else
        {
            hit.normal = tri.face->surface.normal;
        }
        best = std::move(hit);
    }
    return best;
}

} // namespace brep
