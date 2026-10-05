#pragma once

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

#include "ge/gepoint.h"
#include "ge/geoctree.h" // AcGeBoundBox3d

#include <glm/glm.hpp>

namespace ge
{

struct AcGeBvhNode
{
    AcGeBoundBox3d bounds;
    std::uint32_t leftChild = 0;
    std::uint32_t rightChild = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    bool leaf = false;
};

struct AcGeBvhBuildResult
{
    std::vector<AcGeBvhNode> nodes;
    // Leaf ranges [begin, end) address this permutation: order[i] is the
    // original input index stored at permuted position i.
    std::vector<std::uint32_t> order;
};

class AcGeBoundBvh
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

    void build(std::vector<AcGeBoundBox3d> bounds,
               const Settings &settings = {})
    {
        bounds_ = std::move(bounds);
        std::vector<PrimRef> refs;
        refs.reserve(bounds_.size());
        for (std::uint32_t i = 0; i < bounds_.size(); ++i)
            refs.push_back({bounds_[i], bounds_[i].center(), i});
        nodes_.clear();
        order_.assign(refs.size(), 0);
        partitionRange(refs, 0, std::uint32_t(refs.size()), settings,
                       nodes_, 0);
        for (std::uint32_t i = 0; i < refs.size(); ++i)
            order_[i] = refs[i].index;
    }

    std::size_t size() const { return bounds_.size(); }

    const AcGeBoundBox3d &bounds(std::uint32_t index) const
    {
        return bounds_[index];
    }
    const std::vector<AcGeBvhNode> &nodes() const { return nodes_; }

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
        const AcGePoint3d &origin, const glm::dvec3 &direction,
        double tMin, double tMax) const
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
            slabEntry(nodes_[0].bounds, origin, inverseDirection, tMin, tMax);
        if (!rootEntry)
            return std::nullopt;

        std::optional<std::pair<std::uint32_t, double>> best;
        double bestDistance = tMax;

        std::uint32_t current = 0;
        while (true)
        {
            const AcGeBvhNode &node = nodes_[current];
            if (node.leaf)
            {
                for (std::uint32_t i = node.begin; i < node.end; ++i)
                {
                    const AcGeBoundBox3d &bounds = bounds_[order_[i]];
                    std::optional<double> entry = slabEntry(
                        bounds, origin, inverseDirection, tMin, bestDistance);
                    if (entry && *entry <= bestDistance)
                    {
                        bestDistance = *entry;
                        best = {order_[i], *entry};
                    }
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

            if (top == 0)
                break;
            // The stack is not distance-sorted after mixed descents,
            // so skip (do not abandon) entries beyond the best hit.
            while (top > 0 && stack[top - 1].distance > bestDistance)
                --top;
            if (top == 0)
                break;
            current = stack[--top].node;
        }
        return best;
    }

    // ---- reusable partition (adapters build their own node types) ----

    static AcGeBvhBuildResult partition(
        const std::vector<AcGeBoundBox3d> &bounds,
        const Settings &settings = {})
    {
        AcGeBvhBuildResult result;
        std::vector<PrimRef> refs;
        refs.reserve(bounds.size());
        for (std::uint32_t i = 0; i < bounds.size(); ++i)
            refs.push_back({bounds[i], bounds[i].center(), i});
        result.order.assign(refs.size(), 0);
        partitionRange(refs, 0, std::uint32_t(refs.size()), settings,
                       result.nodes, 0);
        for (std::uint32_t i = 0; i < refs.size(); ++i)
            result.order[i] = refs[i].index;
        return result;
    }

private:
    static double halfArea(const AcGeBoundBox3d &b)
    {
        const double dx = b.max.x - b.min.x;
        const double dy = b.max.y - b.min.y;
        const double dz = b.max.z - b.min.z;
        return dx * dy + dy * dz + dz * dx;
    }

    static AcGeBoundBox3d merge(const AcGeBoundBox3d &a,
                                const AcGeBoundBox3d &b)
    {
        return {AcGePoint3d(std::min(a.min.x, b.min.x),
                            std::min(a.min.y, b.min.y),
                            std::min(a.min.z, b.min.z)),
                AcGePoint3d(std::max(a.max.x, b.max.x),
                            std::max(a.max.y, b.max.y),
                            std::max(a.max.z, b.max.z))};
    }

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

    static constexpr std::uint32_t kMaxBins = 32;

    static void partitionRange(std::vector<PrimRef> &refs,
                               std::uint32_t begin, std::uint32_t end,
                               const Settings &settings,
                               std::vector<AcGeBvhNode> &nodes,
                               std::uint32_t depth)
    {
        AcGeBoundBox3d bounds = refs[begin].bounds;
        for (std::uint32_t i = begin + 1; i < end; ++i)
            bounds = merge(bounds, refs[i].bounds);

        const std::uint32_t nodeIndex = std::uint32_t(nodes.size());
        nodes.push_back({bounds, 0, 0, begin, end, false});
        const std::uint32_t count = end - begin;

        if (count <= settings.minLeafSize)
        {
            nodes[nodeIndex].leaf = true;
            return;
        }

        // Centroid bounds drive the bin mapping.
        auto extendPoint = [](AcGeBoundBox3d box, const AcGePoint3d &p) {
            box.min = AcGePoint3d(std::min(box.min.x, p.x),
                                  std::min(box.min.y, p.y),
                                  std::min(box.min.z, p.z));
            box.max = AcGePoint3d(std::max(box.max.x, p.x),
                                  std::max(box.max.y, p.y),
                                  std::max(box.max.z, p.z));
            return box;
        };
        AcGeBoundBox3d centroidBounds =
            extendPoint({refs[begin].centroid, refs[begin].centroid},
                        refs[begin].centroid);
        for (std::uint32_t i = begin + 1; i < end; ++i)
            centroidBounds = extendPoint(centroidBounds, refs[i].centroid);
        const glm::dvec3 centroidExtent{
            centroidBounds.max.x - centroidBounds.min.x,
            centroidBounds.max.y - centroidBounds.min.y,
            centroidBounds.max.z - centroidBounds.min.z};

        const std::uint32_t numBins = std::min(
            settings.numBins,
            std::max<std::uint32_t>(
                4, std::uint32_t(4.0 + 0.05 * double(count))));

        // Best split over all three axes.
        int bestAxis = -1;
        std::uint32_t bestSplitBin = 0;
        double bestSah = std::numeric_limits<double>::infinity();
        for (int axis = 0; axis < 3; ++axis)
        {
            const double extent = axis == 0   ? centroidExtent.x
                                  : axis == 1 ? centroidExtent.y
                                              : centroidExtent.z;
            if (!(extent > 0.0))
                continue;
            const double cMin = axis == 0   ? centroidBounds.min.x
                                : axis == 1 ? centroidBounds.min.y
                                            : centroidBounds.min.z;
            const double scale =
                double(numBins) / extent;

            struct Bin
            {
                AcGeBoundBox3d bounds = 
                    {AcGePoint3d(0.0, 0.0, 0.0),
                     AcGePoint3d(0.0, 0.0, 0.0)};
                std::uint32_t count = 0;
                bool used = false;
            };
            std::array<Bin, kMaxBins> bins{};

            for (std::uint32_t i = begin; i < end; ++i)
            {
                const double c = axis == 0   ? refs[i].centroid.x
                                 : axis == 1 ? refs[i].centroid.y
                                             : refs[i].centroid.z;
                std::uint32_t bin = std::uint32_t((c - cMin) * scale);
                bin = std::min(bin, numBins - 1);
                Bin &binRef = bins[bin];
                binRef.bounds =
                    binRef.used ? merge(binRef.bounds, refs[i].bounds)
                                : refs[i].bounds;
                binRef.used = true;
                ++binRef.count;
            }

            // Right-to-left running bounds/counts.
            std::array<AcGeBoundBox3d, kMaxBins> rightBounds{};
            std::array<std::uint32_t, kMaxBins> rightCounts{};
            AcGeBoundBox3d running{AcGePoint3d(0.0,0.0,0.0), AcGePoint3d(0.0,0.0,0.0)};
            std::uint32_t runningCount = 0;
            for (std::uint32_t bin = numBins; bin > 0; --bin)
            {
                if (bins[bin - 1].used)
                {
                    running = runningCount == 0
                                  ? bins[bin - 1].bounds
                                  : merge(running, bins[bin - 1].bounds);
                    runningCount += bins[bin - 1].count;
                }
                rightBounds[bin - 1] = running;
                rightCounts[bin - 1] = runningCount;
            }

            // Left-to-right sweep evaluating split after each bin.
            AcGeBoundBox3d leftRunning = refs[begin].bounds;
            std::uint32_t leftCount = 0;
            bool leftUsed = false;
            for (std::uint32_t split = 0; split + 1 < numBins; ++split)
            {
                if (bins[split].used)
                {
                    leftRunning = leftUsed ? merge(leftRunning,
                                                   bins[split].bounds)
                                           : bins[split].bounds;
                    leftUsed = true;
                    leftCount += bins[split].count;
                }
                const std::uint32_t rightCount =
                    (split + 1 < numBins) ? rightCounts[split + 1] : 0;
                if (leftCount == 0 || rightCount == 0)
                    continue;
                const AcGeBoundBox3d &rightOfSplit = rightBounds[split + 1];
                const double sah =
                    halfArea(leftRunning) * double(leftCount) +
                    halfArea(rightOfSplit) * double(rightCount);
                if (sah < bestSah)
                {
                    bestSah = sah;
                    bestAxis = axis;
                    bestSplitBin = split;
                }
            }
        }

        // Leaf decisions (Embree bvh_builder_sah.h:235).
        const double parentArea = halfArea(bounds);
        const double leafCost = settings.intCost * parentArea *
                                double(count);
        const double splitCost = settings.travCost * parentArea + bestSah;
        const bool depthForced = depth + 8 >= settings.maxDepth;

        if (bestAxis < 0)
        {
            nodes[nodeIndex].leaf = true; // all centroids coincide
            return;
        }
        if (count <= settings.minLeafSize ||
            (count <= settings.maxLeafSize && leafCost <= splitCost &&
             !depthForced))
        {
            nodes[nodeIndex].leaf = true;
            return;
        }

        // Partition by the best split; fall back to a median split when
        // every centroid landed in one bin or the depth limit forces a
        // balanced large leaf.
        const double cMin = bestAxis == 0   ? centroidBounds.min.x
                            : bestAxis == 1 ? centroidBounds.min.y
                                            : centroidBounds.min.z;
        const double extent = bestAxis == 0   ? centroidExtent.x
                              : bestAxis == 1 ? centroidExtent.y
                                              : centroidExtent.z;
        const double scale = double(numBins) / extent;

        std::uint32_t middle = 0;
        if (depthForced || bestSah == std::numeric_limits<double>::infinity())
        {
            middle = begin + count / 2;
            std::nth_element(refs.begin() + begin, refs.begin() + middle,
                             refs.begin() + end,
                             [bestAxis](const PrimRef &lhs,
                                        const PrimRef &rhs) {
                                 const double lc = bestAxis == 0 ? lhs.centroid.x
                                                   : bestAxis == 1
                                                       ? lhs.centroid.y
                                                       : lhs.centroid.z;
                                 const double rc = bestAxis == 0 ? rhs.centroid.x
                                                   : bestAxis == 1
                                                       ? rhs.centroid.y
                                                       : rhs.centroid.z;
                                 return lc < rc;
                             });
        }
        else
        {
            auto splitIt = std::stable_partition(
                refs.begin() + begin, refs.begin() + end,
                [&](const PrimRef &ref) {
                    const double c = bestAxis == 0   ? ref.centroid.x
                                     : bestAxis == 1 ? ref.centroid.y
                                                     : ref.centroid.z;
                    std::uint32_t bin = std::uint32_t((c - cMin) * scale);
                    bin = std::min(bin, numBins - 1);
                    return bin <= bestSplitBin;
                });
            middle = std::uint32_t(splitIt - refs.begin());
            if (middle == begin || middle == end)
                middle = begin + count / 2;
        }

        // Child bounds are refreshed by the recursive calls; buildRange
        // style index-stability (children append after the parent).
        const std::uint32_t leftChild = std::uint32_t(nodes.size());
        partitionRange(refs, begin, middle, settings, nodes, depth + 1);
        nodes[nodeIndex].leftChild = leftChild;
        const std::uint32_t rightChild = std::uint32_t(nodes.size());
        partitionRange(refs, middle, end, settings, nodes, depth + 1);
        nodes[nodeIndex].rightChild = rightChild;
    }

    std::vector<AcGeBoundBox3d> bounds_;
    std::vector<AcGeBvhNode> nodes_;
    std::vector<std::uint32_t> order_;
};

} // namespace ge
