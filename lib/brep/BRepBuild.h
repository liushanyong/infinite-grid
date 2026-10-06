#pragma once

// brep::makeBox — the milestone-3 seed (box extrude will generate exactly
// this shape from a profile + height) and the validate-test fixture.

#include <algorithm>
#include <cmath>
#include <map>

#include "brep/BRep.h"

namespace brep
{

// Creates a planar face through |corners| (any order): sorts them
// counter-clockwise around |normal|, reuses or creates the ring edges,
// and builds the loop.  Reuse key = unordered vertex pair.
struct PosKey
{
    std::int64_t v[3];

    bool operator<(const PosKey &other) const
    {
        for (int i = 0; i < 3; ++i)
            if (v[i] != other.v[i])
                return v[i] < other.v[i];
        return false;
    }
};

inline PosKey positionKey(const AcGePoint3d &point)
{
    return {std::llround(point.x * 1.0e6),
            std::llround(point.y * 1.0e6),
            std::llround(point.z * 1.0e6)};
}

inline Vertex *vertexAt(Builder &builder,
                        std::map<PosKey, Vertex *> &vertices,
                        const AcGePoint3d &point)
{
    const PosKey key = positionKey(point);
    auto found = vertices.find(key);
    if (found != vertices.end())
        return found->second;
    Vertex *vertex = builder.createVertex(point);
    vertices[key] = vertex;
    return vertex;
}

// Finds or creates the undirected edge a-b; |forward| reports whether
// coedge[0] traverses a->b.
inline bool edgeAt(Builder &builder,
                   std::map<std::pair<PosKey, PosKey>,
                            std::pair<Edge *, bool>> &edgeMap,
                   Vertex *a, Vertex *b, Edge *&outEdge, bool &forward)
{
    const PosKey keyA = positionKey(a->point);
    const PosKey keyB = positionKey(b->point);
    const std::pair<PosKey, PosKey> key =
        keyA < keyB ? std::make_pair(keyA, keyB)
                    : std::make_pair(keyB, keyA);
    auto found = edgeMap.find(key);
    if (found != edgeMap.end())
    {
        outEdge = found->second.first;
        // Direction is relative to the CALLER's ring: the stored flag
        // only records the creation-time order.  Vertices are deduped
        // objects, so pointer comparison against the caller's start is
        // exact.
        forward = outEdge->start == a;
        return true;
    }
    outEdge = builder.createEdge(a, b);
    forward = outEdge->start == a;
    edgeMap[key] = {outEdge, forward};
    return false;
}

inline Face *makePlanarFaceFromCorners(
    Builder &builder, Shell *shell,
    std::map<PosKey, Vertex *> &vertices,
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> &edgeMap,
    const AcGePoint3d *corners, std::size_t count,
    const AcGeVector3d &normal)
{
    // Face frame: u/v tangents orthogonal to the unit normal.
    AcGeVector3d n = normal;
    {
        const double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        n = AcGeVector3d(n.x / len, n.y / len, n.z / len);
    }
    AcGeVector3d u = std::abs(n.z) < 0.9 ? AcGeVector3d(0.0, 0.0, 1.0)
                                         : AcGeVector3d(1.0, 0.0, 0.0);
    {
        // u = normalize(n x u)
        const AcGeVector3d cross = n.crossProduct(u);
        const double len = std::sqrt(cross.x * cross.x + cross.y * cross.y +
                                     cross.z * cross.z);
        u = AcGeVector3d(cross.x / len, cross.y / len, cross.z / len);
    }
    const AcGeVector3d v = n.crossProduct(u);

    // Sort around the CENTROID, not corners[0]: the centroid of a convex
    // face is strictly inside, so every corner gets a distinct angle.
    // Sorting around a corner gives the degenerate zero delta for that
    // corner (atan2(0,0)), which can tie with the opposite corner and
    // flip the ring into a bowtie with zero signed area.
    AcGePoint3d centroid{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < count; ++i)
        centroid = centroid + corners[i];
    centroid = AcGePoint3d(centroid.x / double(count),
                           centroid.y / double(count),
                           centroid.z / double(count));

    std::vector<std::pair<double, std::size_t>> angleOrder;
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcGeVector3d delta = corners[i] - centroid;
        angleOrder.push_back(
            {std::atan2(delta.dotProduct(v), delta.dotProduct(u)), i});
    }
    std::sort(angleOrder.begin(), angleOrder.end(),
              [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

    // Vertices are deduplicated by QUANTIZED POSITION (micron grid),
    // not by pointer: adjacent faces must share the same vertex and
    // edge objects for the radial coedge pairing to be real topology.
    std::vector<Vertex *> ring;
    for (const auto &angleEntry : angleOrder)
        ring.push_back(vertexAt(builder, vertices,
                                corners[angleEntry.second]));

    // Create or reuse the ring edges; collect the traversal coedges.
    std::vector<CoEdge *> traversal;
    for (std::size_t i = 0; i < ring.size(); ++i)
    {
        Vertex *a = ring[i];
        Vertex *b = ring[(i + 1) % ring.size()];
        Edge *edge;
        bool forward;
        if (!edgeAt(builder, edgeMap, a, b, edge, forward))
            forward = true;
        traversal.push_back(forward ? edge->coedge[0] : edge->coedge[1]);
    }


    Face *face = builder.createFace(shell, {corners[0], n});
    face->outerLoop = builder.createLoop(face, traversal.data(),
                                         traversal.size());
    return face;
}

// Axis-aligned box [0,size]^3 offset by |origin| with outward normals.
inline Body *makeBox(Arena &arena, const AcGePoint3d &origin, double size)
{
    Builder builder(arena);
    std::map<PosKey, Vertex *> vertices;
    std::map<std::pair<PosKey, PosKey>, std::pair<Edge *, bool>> edgeMap;
    Body *body = builder.createBody();
    Shell *shell = body->shell;

    const AcGePoint3d p[2][2][2] = {
        {{origin,
          {origin.x, origin.y, origin.z + size}},
         {{origin.x, origin.y + size, origin.z},
          {origin.x, origin.y + size, origin.z + size}}},
        {{{origin.x + size, origin.y, origin.z},
          {origin.x + size, origin.y, origin.z + size}},
         {{origin.x + size, origin.y + size, origin.z},
          {origin.x + size, origin.y + size, origin.z + size}}}};

    const AcGeVector3d nx(1.0, 0.0, 0.0);
    const AcGeVector3d ny(0.0, 1.0, 0.0);
    const AcGeVector3d nz(0.0, 0.0, 1.0);

    const AcGePoint3d top[4] = {p[0][0][1], p[1][0][1], p[1][1][1], p[0][1][1]};
    const AcGePoint3d bottom[4] = {p[0][0][0], p[0][1][0], p[1][1][0],
                                   p[1][0][0]};
    const AcGePoint3d px[4] = {p[1][0][0], p[1][1][0], p[1][1][1], p[1][0][1]};
    const AcGePoint3d mx[4] = {p[0][0][0], p[0][0][1], p[0][1][1], p[0][1][0]};
    const AcGePoint3d py[4] = {p[0][1][0], p[0][1][1], p[1][1][1], p[1][1][0]};
    const AcGePoint3d my[4] = {p[0][0][0], p[1][0][0], p[1][0][1], p[0][0][1]};

    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, top, 4, nz);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, bottom, 4, -nz);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, px, 4, nx);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, mx, 4, -nx);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, py, 4, ny);
    makePlanarFaceFromCorners(builder, shell, vertices, edgeMap, my, 4, -ny);
    return body;
}

} // namespace brep
