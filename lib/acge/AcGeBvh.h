#pragma once

#include "AcGePort.h"

// AcGeBoundBvh (namespace ge) — a binned-SAH bounding volume hierarchy
// over (index, AABB) pairs, the sibling of AcGeOctree for queries that
// want ray determinism and frustum-culling quality.
//
// Construction follows the Embree binned-SAH builder (external/
// embree-master, kernels/builders/heuristic_binning.h + bvh_builder_sah.h):
//   * 32 bins per axis, adaptively reduced to min(bins, 4 + 0.05*N),
//   * centroid-linear bin mapping, per-bin primitive counts and bounds,
//   * the best split minimizes lArea*lCount + rArea*rCount across all
//     three axes (not the longest axis),
//   * leaf when size <= minLeafSize, or when size <= maxLeafSize and
//     leafCost <= splitCost, with a depth-limit fallback that splits
//     medially so degenerate clusters keep the tree bounded,
//   * no valid SAH split (all centroids in one bin) falls back to a
//     median split on the widest centroid axis.
//
// Traversal: near-first ordered stack for closest-hit ray queries
// (Embree bvh_traverser1.h pattern) and predicate collection for
// frustum culling.  All bounds stay double precision — the 1e7
// large-coordinate scene is the primary consumer.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "AcGePoint.h"
#include "AcGeOctree.h" // AcGeBoundBox3d

namespace ge
{

struct ACGE_PORT AcGeBvhNode
{
    AcGeBoundBox3d bounds;
    std::uint32_t leftChild = 0;
    std::uint32_t rightChild = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    bool leaf = false;
};

struct ACGE_PORT AcGeBvhBuildResult
{
    std::vector<AcGeBvhNode> nodes;
    // Leaf ranges [begin, end) address this permutation: order[i] is the
    // original input index stored at permuted position i.
    std::vector<std::uint32_t> order;
};

class ACGE_PORT AcGeBoundBvh
{
public:
    struct Settings
    {
        std::uint32_t minLeafSize = 2;
        std::uint32_t maxLeafSize = 8;
        std::uint32_t maxDepth = 32;
        std::uint32_t numBins = 32;
        double travCost = 1.0;
        double intCost = 1.0;
    };

    struct PrimRef
    {
        AcGeBoundBox3d bounds;
        AcGePoint3d centroid;
        std::uint32_t index;
    };

    // ---- one-shot build (nodes retained for queries) ----

    void build(std::vector<AcGeBoundBox3d> bounds, const Settings &settings = {});

    std::size_t size() const { return bounds_.size(); }

    const AcGeBoundBox3d &bounds(std::uint32_t index) const
    {
        return bounds_[index];
    }
    const std::vector<AcGeBvhNode> &nodes() const { return nodes_; }
    const std::vector<std::uint32_t> &order() const { return order_; }

    // Collects every primitive whose visited node bounds satisfy
    // |visitor| (frustum culling: receive bounds, return visible).
    template <typename Visitor>
    void collect(Visitor &&visitor,
                 std::vector<std::uint32_t> &output) const
    {
        output.clear();
        if (nodes_.empty())
            return;
        std::array<std::uint32_t, 128> stack{};
        int top = 0;
        stack[top++] = 0;
        while (top > 0)
        {
            const AcGeBvhNode &node = nodes_[stack[--top]];
            if (!visitor(node.bounds))
                continue;
            if (node.leaf)
            {
                for (std::uint32_t i = node.begin; i < node.end; ++i)
                    output.push_back(order_[i]);
            }
            else
            {
                stack[top++] = node.rightChild;
                stack[top++] = node.leftChild;
            }
        }
    }

    // Closest ray-AABB hit: primitive index + entry depth.  Nodes pop
    // nearest-first, so the first primitive hit terminates the walk.
    std::optional<std::pair<std::uint32_t, double>> rayClosest(
        const AcGePoint3d &origin, const AcGeVector3d &direction,
        double tMin, double tMax) const;

    // Near-ordered traversal visiting every primitive whose box the ray
    // enters, nearest entry first.  |callback|(primitiveIndex, entryDepth)
    // returns false to stop the walk (once an exact test has found a hit
    // nearer than the remaining entries, nothing better can appear).
    template <typename Callback>
    void rayTraverse(const AcGePoint3d &origin, const AcGeVector3d &direction,
                     double tMin, double tMax, Callback &&callback) const
    {
        if (nodes_.empty())
            return;
        const AcGeVector3d inverseDirection{1.0 / direction.x,
                                            1.0 / direction.y,
                                            1.0 / direction.z};

        struct StackItem
        {
            std::uint32_t node;
            double distance;
        };
        std::array<StackItem, 128> stack{};
        int top = 0;

        std::optional<double> rootEntry =
            slabEntry(nodes_[0].bounds, origin, inverseDirection, tMin, tMax);
        if (!rootEntry)
            return;

        std::uint32_t current = 0;
        double bestDistance = tMax;
        while (true)
        {
            const AcGeBvhNode &node = nodes_[current];
            if (node.leaf)
            {
                for (std::uint32_t i = node.begin; i < node.end; ++i)
                {
                    const std::uint32_t primitive = order_[i];
                    std::optional<double> entry = slabEntry(
                        bounds_[primitive], origin, inverseDirection, tMin,
                        bestDistance);
                    if (entry && !callback(primitive, *entry))
                        return; // callback stopped the walk
                }
            }
            else
            {
                const std::optional<double> leftEntry = slabEntry(
                    nodes_[node.leftChild].bounds, origin, inverseDirection,
                    tMin, bestDistance);
                const std::optional<double> rightEntry = slabEntry(
                    nodes_[node.rightChild].bounds, origin, inverseDirection,
                    tMin, bestDistance);
                if (leftEntry && rightEntry)
                {
                    if (*leftEntry <= *rightEntry)
                    {
                        stack[top++] = {node.rightChild, *rightEntry};
                        current = node.leftChild;
                        continue;
                    }
                    stack[top++] = {node.leftChild, *leftEntry};
                    current = node.rightChild;
                    continue;
                }
                if (leftEntry)
                {
                    current = node.leftChild;
                    continue;
                }
                if (rightEntry)
                {
                    current = node.rightChild;
                    continue;
                }
            }

            // The stack is not distance-sorted after mixed descents, so
            // skip (do not abandon) entries beyond the best hit.
            while (top > 0 && stack[top - 1].distance > bestDistance)
                --top;
            if (top == 0)
                break;
            current = stack[--top].node;
        }
    }

    // ---- dynamic refit: entity-level local update ----

    // After an entity's bounds change (an edit), recompute the leaf and
    // every ancestor bound on its path without reordering anything
    // (ObjectARX-style refit: cheap, no rebuild; tree quality degrades
    // gracefully until the next full build).
    void refitPrimitive(std::uint32_t primitive,
                        const AcGeBoundBox3d &newBounds);

    // ---- reusable partition (adapters build their own node types) ----

    static AcGeBvhBuildResult partition(
        const std::vector<AcGeBoundBox3d> &bounds, const Settings &settings = {});

private:
    static double halfArea(const AcGeBoundBox3d &b);
    static AcGeBoundBox3d merge(const AcGeBoundBox3d &a,
                                const AcGeBoundBox3d &b);
    static std::optional<double> slabEntry(
        const AcGeBoundBox3d &bounds, const AcGePoint3d &origin,
        const AcGeVector3d &inverseDirection, double tMin, double tMax);

    static constexpr std::uint32_t kMaxBins = 32;

    static void partitionRange(std::vector<PrimRef> &refs,
                               std::uint32_t begin, std::uint32_t end,
                               const Settings &settings,
                               std::vector<AcGeBvhNode> &nodes,
                               std::uint32_t depth);

    std::vector<AcGeBoundBox3d> bounds_;
    std::vector<AcGeBvhNode> nodes_;
    std::vector<std::uint32_t> order_;
    std::vector<std::uint32_t> positionOf_; // primitive -> permuted slot
};

} // namespace ge
