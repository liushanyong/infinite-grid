#pragma once

#include "AcGePort.h"

// AcGeQuantizedBvh (namespace ge) — memory-compact copy of an
// AcGeBoundBvh: every node's child bounds are quantized to one byte per
// axis against the node's own extent (Embree QuantizedBaseNode_t,
// external/embree-master kernels/bvh/bvh_node_qaabb.h: per-node
// start + scale per axis, child boundaries stored as unsigned char, the
// lower bound ceil-ed and the upper bound floor-ed so the decoded box is
// always conservative, scale clamped away from zero when a span
// collapses).
//
// Use when millions of nodes make the double-precision AcGeBoundBvh too
// large (roughly 6x smaller per node).  Ray traversal decodes bounds on
// the fly; the query results match the source BVH up to quantization
// slop, which only ever WIDENS a box — never misses.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <vector>

#include "AcGeBvh.h"

namespace ge
{

class ACGE_PORT AcGeQuantizedBvh
{
public:
    static constexpr std::uint32_t kMaxChildren = 2;

    // float32 child bounds, widened by one ulp on encode so the decoded
    // box always CONTAINS the double-precision original (lo rounds toward
    // -inf, hi toward +inf).  Node cost: 2 children x 24 bytes + links.
    struct QuantizedNode
    {
        float lower[kMaxChildren][3];
        float upper[kMaxChildren][3];
        std::uint32_t children[kMaxChildren];
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
        bool leaf = false;
    };

    void build(const AcGeBoundBvh &source);

    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t memoryBytes() const
    {
        return nodes_.size() * sizeof(QuantizedNode);
    }

    // Near-ordered closest-hit, mirroring AcGeBoundBvh::rayClosest.  The
    // widened float boxes can only admit extra candidates, never miss.
    std::optional<std::pair<std::uint32_t, double>> rayClosest(
        const AcGePoint3d &origin, const AcGeVector3d &direction,
        double tMin, double tMax,
        const std::vector<AcGeBoundBox3d> &primitiveBounds,
        const std::vector<std::uint32_t> &order) const;

    // Debug: decode the ancestor boxes on the path to the leaf that
    // holds |primitive|.
    std::vector<AcGeBoundBox3d> debugDecodedPath(
        std::uint32_t primitive, const AcGeBoundBvh &source,
        const std::vector<std::uint32_t> &order) const;

private:
    static std::optional<double> slabEntry(
        const AcGeBoundBox3d &bounds, const AcGePoint3d &origin,
        const AcGeVector3d &inverseDirection, double tMin, double tMax);
    AcGeBoundBox3d decodeChild(std::uint32_t nodeIndex,
                               std::uint32_t child) const;
    AcGeBoundBox3d decodeBox(std::uint32_t nodeIndex) const;

    std::vector<QuantizedNode> nodes_;
};

} // namespace ge
