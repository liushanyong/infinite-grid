#include "acge/AcGeQuadtree.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ge
{

// ---------------------------------------------------------------------------
// AcGeBoundBox2d
// ---------------------------------------------------------------------------

AcGeBoundBox2d::AcGeBoundBox2d(const AcGePoint2d &point1,
                               const AcGePoint2d &point2)
{
    set(point1, point2);
}

AcGeBoundBox2d &AcGeBoundBox2d::set(const AcGePoint2d &point1,
                                    const AcGePoint2d &point2)
{
    if (!std::isfinite(point1.x) || !std::isfinite(point1.y) ||
        !std::isfinite(point2.x) || !std::isfinite(point2.y))
        throw std::invalid_argument(
            "AcGeBoundBox2d points must have finite coordinates.");
    min = {std::min(point1.x, point2.x), std::min(point1.y, point2.y)};
    max = {std::max(point1.x, point2.x), std::max(point1.y, point2.y)};
    m_empty = false;
    return *this;
}

AcGeBoundBox2d &AcGeBoundBox2d::extend(const AcGePoint2d &point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        throw std::invalid_argument(
            "AcGeBoundBox2d points must have finite coordinates.");
    if (m_empty)
    {
        min = max = point;
        m_empty = false;
        return *this;
    }
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    return *this;
}

std::optional<AcGeBoundBox2d> AcGeBoundBox2d::intersection(
    const AcGeBoundBox2d &box) const
{
    if (!intersects(box))
        return std::nullopt;
    return AcGeBoundBox2d{
        {std::max(min.x, box.min.x), std::max(min.y, box.min.y)},
        {std::min(max.x, box.max.x), std::min(max.y, box.max.y)}};
}

AcGeBoundBox2d AcGeBoundBox2d::aroundPoint(const AcGePoint2d &center,
                                           double extent)
{
    if (!std::isfinite(extent) || extent < 0.0)
        throw std::invalid_argument(
            "AcGeBoundBox2d extent must be finite and nonnegative.");
    return {{center.x - extent, center.y - extent},
            {center.x + extent, center.y + extent}};
}

// ---------------------------------------------------------------------------
// AcGeQuadtree
// ---------------------------------------------------------------------------

AcGeQuadtree::AcGeQuadtree(const AcGePoint2d &center, double extent,
                           std::size_t leafCapacity, double minExtent)
    : root_(std::make_unique<Quadrant>(center, extent, 0)),
      leafCapacity_(leafCapacity), minExtent_(minExtent),
      center_(center), extent_(extent)
{
    if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
        !std::isfinite(extent) || !(extent > 0.0))
        throw std::invalid_argument(
            "Quadtree center must be finite and extent must be positive.");
    if (leafCapacity_ == 0 || !std::isfinite(minExtent_) ||
        !(minExtent_ > 0.0))
        throw std::invalid_argument(
            "Quadtree capacity and minimum extent must be positive.");
}

void AcGeQuadtree::insert(std::uint64_t id, const AcGeBoundBox2d &bounds)
{
    if (bounds.isEmpty())
        throw std::invalid_argument("Cannot insert an empty quadtree bound.");
    if (!rootBounds().contains(bounds))
        throw std::out_of_range(
            "Quadtree item bounds must be inside the root bounds.");
    insertInto(root_.get(), id, bounds);
    ++size_;
}

std::vector<std::uint64_t> AcGeQuadtree::boxSearch(
    const AcGeBoundBox2d &query) const
{
    std::vector<std::uint64_t> results;
    if (!query.isEmpty() && rootBounds().intersects(query))
        boxSearch(root_.get(), query, results);
    sortUnique(results);
    return results;
}

std::vector<std::uint64_t> AcGeQuadtree::radiusSearch(
    const AcGePoint2d &center, double radius) const
{
    if (!std::isfinite(radius) || radius < 0.0)
        throw std::invalid_argument(
            "Quadtree search radius must be finite and nonnegative.");
    std::vector<std::uint64_t> results;
    const AcGeBoundBox2d query =
        AcGeBoundBox2d::aroundPoint(center, radius);
    if (rootBounds().intersects(query))
        radiusSearch(root_.get(), center, radius, results);
    sortUnique(results);
    return results;
}

void AcGeQuadtree::sortUnique(std::vector<std::uint64_t> &ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

void AcGeQuadtree::insertInto(Quadrant *quadrant, std::uint64_t id,
                              const AcGeBoundBox2d &bounds)
{
    quadrant->items.emplace_back(id, bounds);
    const bool splittable = quadrant->extent * 0.5 >= minExtent_;
    if (quadrant->items.size() > leafCapacity_ && splittable &&
        quadrant->isLeaf())
        split(quadrant);
    if (!quadrant->isLeaf())
        for (auto &child : quadrant->children)
            if (intersectsQuadrant(bounds, *child))
                insertInto(child.get(), id, bounds);
}

void AcGeQuadtree::split(Quadrant *quadrant)
{
    const double childExtent = quadrant->extent * 0.5;
    for (std::size_t i = 0; i < quadrant->children.size(); ++i)
    {
        const AcGePoint2d childCenter{
            quadrant->center.x + (i & 1 ? childExtent : -childExtent),
            quadrant->center.y + (i & 2 ? childExtent : -childExtent)};
        quadrant->children[i] = std::make_unique<Quadrant>(
            childCenter, childExtent, quadrant->depth + 1);
    }

    const auto existing = quadrant->items;
    for (const auto &item : existing)
        for (auto &child : quadrant->children)
            if (intersectsQuadrant(item.second, *child))
                insertInto(child.get(), item.first, item.second);
}

void AcGeQuadtree::boxSearch(const Quadrant *quadrant,
                             const AcGeBoundBox2d &query,
                             std::vector<std::uint64_t> &results) const
{
    if (!intersectsQuadrant(query, *quadrant))
        return;
    for (const auto &item : quadrant->items)
        if (item.second.intersects(query))
            results.push_back(item.first);
    if (!quadrant->isLeaf())
        for (const auto &child : quadrant->children)
            boxSearch(child.get(), query, results);
}

void AcGeQuadtree::radiusSearch(const Quadrant *quadrant,
                                const AcGePoint2d &center, double radius,
                                std::vector<std::uint64_t> &results) const
{
    const AcGeBoundBox2d searchBounds =
        AcGeBoundBox2d::aroundPoint(center, radius);
    if (!intersectsQuadrant(searchBounds, *quadrant))
        return;
    const double radiusSquared = radius * radius;
    for (const auto &item : quadrant->items)
    {
        const AcGePoint2d closest = item.second.closestPointTo(center);
        if ((closest - center).lengthSq() <= radiusSquared)
            results.push_back(item.first);
    }
    if (!quadrant->isLeaf())
        for (const auto &child : quadrant->children)
            radiusSearch(child.get(), center, radius, results);
}

void AcGeQuadtree::nearestByCenter(const Quadrant *quadrant,
                                   const AcGePoint2d &point,
                                   std::optional<std::uint64_t> &best,
                                   double &bestDistanceSquared) const
{
    for (const auto &item : quadrant->items)
    {
        const AcGePoint2d center = item.second.center();
        const double distanceSquared = (center - point).lengthSq();
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            best = item.first;
        }
    }
    if (!quadrant->isLeaf())
        for (const auto &child : quadrant->children)
            nearestByCenter(child.get(), point, best,
                            bestDistanceSquared);
}

} // namespace ge
