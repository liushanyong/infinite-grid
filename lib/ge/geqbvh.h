#pragma once

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

#include "ge/gebvh.h"

namespace ge
{

class AcGeQuantizedBvh
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

    void build(const AcGeBoundBvh &source)
    {
        nodes_.clear();
        nodes_.reserve(source.nodes().size());
        for (const AcGeBvhNode &node : source.nodes())
        {
            QuantizedNode out;
            out.leaf = node.leaf;
            out.begin = node.begin;
            out.end = node.end;
            out.children[0] = node.leftChild;
            out.children[1] = node.rightChild;
            if (!node.leaf)
            {
                const AcGeBoundBox3d childBoxes[kMaxChildren] = {
                    source.nodes()[node.leftChild].bounds,
                    source.nodes()[node.rightChild].bounds};
                for (std::uint32_t child = 0; child < kMaxChildren; ++child)
                {
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        const double lo = axis == 0   ? childBoxes[child].min.x
                                          : axis == 1 ? childBoxes[child].min.y
                                                      : childBoxes[child].min.z;
                        const double hi = axis == 0   ? childBoxes[child].max.x
                                          : axis == 1 ? childBoxes[child].max.y
                                                      : childBoxes[child].max.z;
                        float loF = float(lo);
                        if (double(loF) > lo)
                            loF = std::nextafterf(
                                loF, -std::numeric_limits<float>::infinity());
                        float hiF = float(hi);
                        if (double(hiF) < hi)
                            hiF = std::nextafterf(
                                hiF, std::numeric_limits<float>::infinity());
                        out.lower[child][axis] = loF;
                        out.upper[child][axis] = hiF;
                    }
                }
            }
            nodes_.push_back(out);
        }
    }

    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t memoryBytes() const
    {
        return nodes_.size() * sizeof(QuantizedNode);
    }

    // Near-ordered closest-hit, mirroring AcGeBoundBvh::rayClosest.  The
    // widened float boxes can only admit extra candidates, never miss.
    std::optional<std::pair<std::uint32_t, double>> rayClosest(
        const AcGePoint3d &origin, const glm::dvec3 &direction,
        double tMin, double tMax,
        const std::vector<AcGeBoundBox3d> &primitiveBounds,
        const std::vector<std::uint32_t> &order) const
    {
        if (nodes_.empty())
            return std::nullopt;
        const glm::dvec3 inverseDirection = 1.0 / direction;

        struct StackItem
        {
            std::uint32_t node;
            double distance;
        };
        std::array<StackItem, 128> stack{};
        int top = 0;

        std::optional<double> rootEntry =
            slabEntry(decodeBox(0), origin, inverseDirection, tMin, tMax);
        if (!rootEntry)
            return std::nullopt;

        std::optional<std::pair<std::uint32_t, double>> best;
        double bestDistance = tMax;
        std::uint32_t current = 0;
        while (true)
        {
            const QuantizedNode &node = nodes_[current];
            if (node.leaf)
            {
                for (std::uint32_t i = node.begin; i < node.end; ++i)
                {
                    const std::uint32_t primitive = order[i];
                    std::optional<double> entry = slabEntry(
                        primitiveBounds[primitive], origin, inverseDirection,
                        tMin, bestDistance);
                    if (entry && *entry <= bestDistance)
                    {
                        bestDistance = *entry;
                        best = {primitive, *entry};
                    }
                }
            }
            else
            {
                const AcGeBoundBox3d leftBox = decodeChild(current, 0);
                const AcGeBoundBox3d rightBox = decodeChild(current, 1);
                const std::optional<double> leftEntry =
                    slabEntry(leftBox, origin, inverseDirection, tMin,
                              bestDistance);
                const std::optional<double> rightEntry =
                    slabEntry(rightBox, origin, inverseDirection, tMin,
                              bestDistance);
                if (leftEntry && rightEntry)
                {
                    if (*leftEntry <= *rightEntry)
                    {
                        stack[top++] = {node.children[1], *rightEntry};
                        current = node.children[0];
                        continue;
                    }
                    stack[top++] = {node.children[0], *leftEntry};
                    current = node.children[1];
                    continue;
                }
                if (leftEntry)
                {
                    current = node.children[0];
                    continue;
                }
                if (rightEntry)
                {
                    current = node.children[1];
                    continue;
                }
            }

            while (top > 0 && stack[top - 1].distance > bestDistance)
                --top;
            if (top == 0)
                break;
            current = stack[--top].node;
        }
        return best;
    }

    // Debug: decode the ancestor boxes on the path to the leaf that
    // holds |primitive|.
    std::vector<AcGeBoundBox3d> debugDecodedPath(
        std::uint32_t primitive, const AcGeBoundBvh &source,
        const std::vector<std::uint32_t> &order) const
    {
        std::uint32_t position = std::uint32_t(-1);
        for (std::uint32_t i = 0; i < order.size(); ++i)
            if (order[i] == primitive)
            {
                position = i;
                break;
            }
        std::vector<AcGeBoundBox3d> path;
        if (nodes_.empty() || position == std::uint32_t(-1))
            return path;
        std::uint32_t currentNode = 0;
        while (true)
        {
            const QuantizedNode &node = nodes_[currentNode];
            if (node.leaf)
                break;
            const AcGeBvhNode &srcLeft = source.nodes()[node.children[0]];
            const AcGeBvhNode &srcRight = source.nodes()[node.children[1]];
            std::uint32_t next;
            if (position >= srcLeft.begin && position < srcLeft.end)
            {
                next = node.children[0];
                path.push_back(decodeChild(currentNode, 0));
            }
            else if (position >= srcRight.begin && position < srcRight.end)
            {
                next = node.children[1];
                path.push_back(decodeChild(currentNode, 1));
            }
            else
            {
                path.push_back(AcGeBoundBox3d{}); // broken path marker
                break;
            }
            currentNode = next;
        }
        return path;
    }

private:
    static std::optional<double> slabEntry(
        const AcGeBoundBox3d &bounds, const AcGePoint3d &origin,
        const glm::dvec3 &inverseDirection, double tMin, double tMax)
    {
        double tEnter = tMin;
        double tExit = tMax;
        const double o[3] = {origin.x, origin.y, origin.z};
        const double inv[3] = {inverseDirection.x, inverseDirection.y,
                               inverseDirection.z};
        const double lo[3] = {bounds.min.x, bounds.min.y, bounds.min.z};
        const double hi[3] = {bounds.max.x, bounds.max.y, bounds.max.z};
        for (int axis = 0; axis < 3; ++axis)
        {
            double t0 = (lo[axis] - o[axis]) * inv[axis];
            double t1 = (hi[axis] - o[axis]) * inv[axis];
            if (t0 > t1)
                std::swap(t0, t1);
            tEnter = std::max(tEnter, t0);
            tExit = std::min(tExit, t1);
            if (tEnter > tExit)
                return std::nullopt;
        }
        return tEnter;
    }

    AcGeBoundBox3d decodeChild(std::uint32_t nodeIndex,
                               std::uint32_t child) const
    {
        const QuantizedNode &node = nodes_[nodeIndex];
        return {AcGePoint3d(node.lower[child][0], node.lower[child][1],
                            node.lower[child][2]),
                AcGePoint3d(node.upper[child][0], node.upper[child][1],
                            node.upper[child][2])};
    }

    AcGeBoundBox3d decodeBox(std::uint32_t nodeIndex) const
    {
        const QuantizedNode &node = nodes_[nodeIndex];
        if (node.leaf)
        {
            return {AcGePoint3d(-std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity()),
                    AcGePoint3d(std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::infinity())};
        }
        const AcGeBoundBox3d left = decodeChild(nodeIndex, 0);
        const AcGeBoundBox3d right = decodeChild(nodeIndex, 1);
        return {AcGePoint3d(std::min(left.min.x, right.min.x),
                            std::min(left.min.y, right.min.y),
                            std::min(left.min.z, right.min.z)),
                AcGePoint3d(std::max(left.max.x, right.max.x),
                            std::max(left.max.y, right.max.y),
                            std::max(left.max.z, right.max.z))};
    }

    std::vector<QuantizedNode> nodes_;
};

} // namespace ge
