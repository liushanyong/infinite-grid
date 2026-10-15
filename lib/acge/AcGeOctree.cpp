#include "acge/AcGeOctree.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ge
{

AcGeOctree::AcGeOctree(const AcGePoint3d &center, double extent,
                       size_t leafCapacity, double minExtent)
    : root_(std::make_unique<Octant>(center, extent, 0)),
      leafCapacity_(leafCapacity),
      minExtent_(minExtent)
{
    if (!(extent > 0.0))
        throw std::runtime_error("Octree extent must be positive.");
}

void AcGeOctree::insertInto(Octant *octant, uint64_t id,
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

void AcGeOctree::split(Octant *octant, size_t depth)
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

void AcGeOctree::boxSearch(const Octant *octant, const AcGeBoundBox3d &query,
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

void AcGeOctree::radiusSearch(const Octant *octant, const AcGePoint3d &center,
                              double radius,
                              std::vector<uint64_t> &results) const
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

void AcGeOctree::nearestByCenter(const Octant *octant, const AcGePoint3d &point,
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

} // namespace ge
