#pragma once

// Axis-aligned octree for the AcGe layer: stores (id, AABB) pairs for
// range queries, radius queries, and nearest-neighbor seed generation.
// Structure follows the standard loose-octree construction (each octant
// splits when its payload exceeds the leaf capacity and its extent exceeds
// the minimum); the LNLib reference in external/ (LGPL) is only consulted
// for structure.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "gepoint.h"

namespace ge
{

struct AcGeBoundBox3d
{
    AcGePoint3d min{0.0, 0.0, 0.0};
    AcGePoint3d max{0.0, 0.0, 0.0};

    AcGePoint3d center() const
    {
        return AcGePoint3d((min.x + max.x) * 0.5, (min.y + max.y) * 0.5,
                           (min.z + max.z) * 0.5);
    }

    bool intersects(const AcGeBoundBox3d &other, double tolerance) const
    {
        return min.x <= other.max.x + tolerance &&
               max.x >= other.min.x - tolerance &&
               min.y <= other.max.y + tolerance &&
               max.y >= other.min.y - tolerance &&
               min.z <= other.max.z + tolerance &&
               max.z >= other.min.z - tolerance;
    }

    bool containsPoint(const AcGePoint3d &point) const
    {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    static AcGeBoundBox3d aroundPoint(const AcGePoint3d &center,
                                      double extent)
    {
        return AcGeBoundBox3d{
            AcGePoint3d(center.x - extent, center.y - extent,
                        center.z - extent),
            AcGePoint3d(center.x + extent, center.y + extent,
                        center.z + extent)};
    }
};

class AcGeOctree
{
public:
    // extent = half side length of the root cube; minExtent bounds the
    // subdivision depth; leafCapacity triggers octant splitting.
    AcGeOctree(const AcGePoint3d &center, double extent,
               size_t leafCapacity = 16, double minExtent = 1.0e-4)
        : root_(std::make_unique<Octant>(center, extent, 0)),
          leafCapacity_(leafCapacity),
          minExtent_(minExtent)
    {
        if (!(extent > 0.0))
            throw std::runtime_error("Octree extent must be positive.");
    }

    // Insert an item; the item is stored in every octant whose box
    // intersects the item's AABB (loose insertion), so removal queries
    // stay simple.
    void insert(uint64_t id, const AcGeBoundBox3d &bounds)
    {
        insertInto(root_.get(), id, bounds, 0);
        ++size_;
    }

    // All item ids whose AABB intersects the query box.
    std::vector<uint64_t> boxSearch(const AcGeBoundBox3d &query) const
    {
        std::vector<uint64_t> results;
        boxSearch(root_.get(), query, results);
        return results;
    }

    // All item ids whose AABB intersects the query sphere.
    std::vector<uint64_t> radiusSearch(const AcGePoint3d &center,
                                       double radius) const
    {
        std::vector<uint64_t> results;
        radiusSearch(root_.get(), center, radius, results);
        return results;
    }

    // Nearest item center among the stored boxes (coarse: center distance).
    std::optional<uint64_t> nearestByCenter(const AcGePoint3d &point) const
    {
        std::optional<uint64_t> best;
        double bestDistance = 1.0e300;
        nearestByCenter(root_.get(), point, best, bestDistance);
        return best;
    }

    size_t size() const { return size_; }

private:
    struct Octant
    {
        AcGePoint3d center;
        double extent;
        size_t depth;
        std::unique_ptr<Octant> children[8];
        std::vector<std::pair<uint64_t, AcGeBoundBox3d>> items;

        Octant(const AcGePoint3d &octantCenter, double octantExtent,
               size_t octantDepth)
            : center(octantCenter), extent(octantExtent),
              depth(octantDepth)
        {
        }

        bool isLeaf() const
        {
            return !children[0];
        }
    };

    std::unique_ptr<Octant> root_;
    size_t leafCapacity_;
    double minExtent_;
    size_t size_ = 0;

    static bool boxIntersectsOctant(const AcGeBoundBox3d &bounds,
                                    const Octant &octant)
    {
        return bounds.intersects(
            AcGeBoundBox3d::aroundPoint(octant.center, octant.extent),
            0.0);
    }

    void insertInto(Octant *octant, uint64_t id,
                    const AcGeBoundBox3d &bounds, size_t depth)
    {
        octant->items.emplace_back(id, bounds);
        const bool splittable =
            octant->extent * 0.5 >= minExtent_;
        if (octant->items.size() > leafCapacity_ && splittable &&
            octant->isLeaf())
            split(octant, depth);
        if (!octant->isLeaf())
        {
            // Re-distribute this item into the child octants it touches.
            for (auto &child : octant->children)
            {
                if (child && boxIntersectsOctant(bounds, *child))
                    insertInto(child.get(), id, bounds, depth + 1);
            }
        }
    }

    void split(Octant *octant, size_t depth)
    {
        const double childExtent = octant->extent * 0.5;
        for (int i = 0; i < 8; ++i)
        {
            const AcGePoint3d childCenter(
                octant->center.x + (i & 1 ? childExtent : -childExtent),
                octant->center.y + (i & 2 ? childExtent : -childExtent),
                octant->center.z + (i & 4 ? childExtent : -childExtent));
            octant->children[i] =
                std::make_unique<Octant>(childCenter, childExtent,
                                         depth + 1);
        }
        // Items stay in the parent (loose storage: parents reference every
        // box they intersect); children receive copies at the next insert.
        // To make existing items reach children immediately, re-insert the
        // parent's items once into the children.
        std::vector<std::pair<uint64_t, AcGeBoundBox3d>> items =
            std::move(octant->items);
        octant->items.clear();
        for (const auto &item : items)
        {
            octant->items.push_back(item);
            for (auto &child : octant->children)
            {
                if (child && boxIntersectsOctant(item.second, *child))
                    insertInto(child.get(), item.first, item.second,
                               depth + 1);
            }
        }
    }

    void boxSearch(const Octant *octant, const AcGeBoundBox3d &query,
                   std::vector<uint64_t> &results) const
    {
        if (!boxIntersectsOctant(query, *octant))
            return;
        for (const auto &item : octant->items)
            if (item.second.intersects(query, 0.0))
                results.push_back(item.first);
        if (!octant->isLeaf())
            for (const auto &child : octant->children)
                if (child)
                    boxSearch(child.get(), query, results);
    }

    void radiusSearch(const Octant *octant, const AcGePoint3d &center,
                      double radius, std::vector<uint64_t> &results) const
    {
        const AcGeBoundBox3d sphereBounds =
            AcGeBoundBox3d::aroundPoint(center, radius);
        if (!boxIntersectsOctant(sphereBounds, *octant))
            return;
        for (const auto &item : octant->items)
        {
            const AcGePoint3d clamped(
                std::clamp(center.x, item.second.min.x, item.second.max.x),
                std::clamp(center.y, item.second.min.y, item.second.max.y),
                std::clamp(center.z, item.second.min.z, item.second.max.z));
            const double dx = clamped.x - center.x;
            const double dy = clamped.y - center.y;
            const double dz = clamped.z - center.z;
            if (dx * dx + dy * dy + dz * dz <= radius * radius)
                results.push_back(item.first);
        }
        if (!octant->isLeaf())
            for (const auto &child : octant->children)
                if (child)
                    radiusSearch(child.get(), center, radius, results);
    }

    void nearestByCenter(const Octant *octant, const AcGePoint3d &point,
                         std::optional<uint64_t> &best,
                         double &bestDistance) const
    {
        for (const auto &item : octant->items)
        {
            const AcGePoint3d center = item.second.center();
            const double dx = center.x - point.x;
            const double dy = center.y - point.y;
            const double dz = center.z - point.z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = item.first;
            }
        }
        if (octant->isLeaf())
            return;
        // Visit the child containing the point first, then the rest.
        for (const auto &child : octant->children)
            if (child)
                nearestByCenter(child.get(), point, best, bestDistance);
    }
};

} // namespace ge
