#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "gepoint.h"

namespace ge
{

struct AcGeBoundBox2d
{
    AcGePoint2d min{0.0, 0.0};
    AcGePoint2d max{0.0, 0.0};
    bool m_empty = true;

    AcGeBoundBox2d() = default;
    AcGeBoundBox2d(const AcGePoint2d &point1, const AcGePoint2d &point2)
    {
        set(point1, point2);
    }

    AcGeBoundBox2d &set(const AcGePoint2d &point1,
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

    AcGeBoundBox2d &clear()
    {
        min = max = {};
        m_empty = true;
        return *this;
    }

    bool isEmpty() const { return m_empty; }

    AcGePoint2d center() const
    {
        return m_empty ? AcGePoint2d{} :
                         AcGePoint2d{(min.x + max.x) * 0.5,
                                     (min.y + max.y) * 0.5};
    }

    AcGeVector2d size() const
    {
        return m_empty ? AcGeVector2d{} :
                         AcGeVector2d{max.x - min.x, max.y - min.y};
    }

    double area() const
    {
        const AcGeVector2d extent = size();
        return extent.x * extent.y;
    }

    double perimeter() const
    {
        const AcGeVector2d extent = size();
        return 2.0 * (extent.x + extent.y);
    }

    AcGeBoundBox2d &extend(const AcGePoint2d &point)
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

    AcGeBoundBox2d &extend(const AcGeBoundBox2d &box)
    {
        if (!box.m_empty)
        {
            extend(box.min);
            extend(box.max);
        }
        return *this;
    }

    bool containsPoint(const AcGePoint2d &point,
                       double tolerance = 0.0) const
    {
        const double tol = std::abs(tolerance);
        return !m_empty && point.x >= min.x - tol &&
               point.x <= max.x + tol && point.y >= min.y - tol &&
               point.y <= max.y + tol;
    }

    bool contains(const AcGeBoundBox2d &box,
                  double tolerance = 0.0) const
    {
        const double tol = std::abs(tolerance);
        return !m_empty && (box.m_empty ||
                            (box.min.x >= min.x - tol &&
                             box.max.x <= max.x + tol &&
                             box.min.y >= min.y - tol &&
                             box.max.y <= max.y + tol));
    }

    bool intersects(const AcGeBoundBox2d &box,
                    double tolerance = 0.0) const
    {
        const double tol = std::abs(tolerance);
        return !m_empty && !box.m_empty &&
               min.x <= box.max.x + tol && max.x >= box.min.x - tol &&
               min.y <= box.max.y + tol && max.y >= box.min.y - tol;
    }

    std::optional<AcGeBoundBox2d> intersection(
        const AcGeBoundBox2d &box) const
    {
        if (!intersects(box))
            return std::nullopt;
        return AcGeBoundBox2d{
            {std::max(min.x, box.min.x), std::max(min.y, box.min.y)},
            {std::min(max.x, box.max.x), std::min(max.y, box.max.y)}};
    }

    AcGePoint2d closestPointTo(const AcGePoint2d &point) const
    {
        if (m_empty)
            return {};
        return {std::clamp(point.x, min.x, max.x),
                std::clamp(point.y, min.y, max.y)};
    }

    double distanceTo(const AcGePoint2d &point) const
    {
        return m_empty ? std::numeric_limits<double>::infinity()
                       : closestPointTo(point).distanceTo(point);
    }

    static AcGeBoundBox2d aroundPoint(const AcGePoint2d &center,
                                      double extent)
    {
        if (!std::isfinite(extent) || extent < 0.0)
            throw std::invalid_argument(
                "AcGeBoundBox2d extent must be finite and nonnegative.");
        return {{center.x - extent, center.y - extent},
                {center.x + extent, center.y + extent}};
    }
};

class AcGeQuadtree
{
public:
    AcGeQuadtree(const AcGePoint2d &center, double extent,
                 std::size_t leafCapacity = 16, double minExtent = 1.0e-4)
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

    void insert(std::uint64_t id, const AcGeBoundBox2d &bounds)
    {
        if (bounds.isEmpty())
            throw std::invalid_argument("Cannot insert an empty quadtree bound.");
        if (!rootBounds().contains(bounds))
            throw std::out_of_range(
                "Quadtree item bounds must be inside the root bounds.");
        insertInto(root_.get(), id, bounds);
        ++size_;
    }

    std::vector<std::uint64_t> boxSearch(
        const AcGeBoundBox2d &query) const
    {
        std::vector<std::uint64_t> results;
        if (!query.isEmpty() && rootBounds().intersects(query))
            boxSearch(root_.get(), query, results);
        sortUnique(results);
        return results;
    }

    std::vector<std::uint64_t> radiusSearch(const AcGePoint2d &center,
                                            double radius) const
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

    std::optional<std::uint64_t> nearestByCenter(
        const AcGePoint2d &point) const
    {
        std::optional<std::uint64_t> best;
        double bestDistanceSquared = std::numeric_limits<double>::infinity();
        nearestByCenter(root_.get(), point, best, bestDistanceSquared);
        return best;
    }

    std::size_t size() const { return size_; }

    void clear()
    {
        root_ = std::make_unique<Quadrant>(center_, extent_, 0);
        size_ = 0;
    }

private:
    struct Quadrant
    {
        AcGePoint2d center;
        double extent;
        std::size_t depth;
        std::array<std::unique_ptr<Quadrant>, 4> children;
        std::vector<std::pair<std::uint64_t, AcGeBoundBox2d>> items;

        Quadrant(const AcGePoint2d &quadrantCenter, double quadrantExtent,
                 std::size_t quadrantDepth)
            : center(quadrantCenter), extent(quadrantExtent),
              depth(quadrantDepth)
        {
        }

        bool isLeaf() const { return !children[0]; }
    };

    std::unique_ptr<Quadrant> root_;
    std::size_t leafCapacity_;
    double minExtent_;
    std::size_t size_ = 0;
    AcGePoint2d center_{};
    double extent_ = 0.0;

    AcGeBoundBox2d rootBounds() const
    {
        return AcGeBoundBox2d::aroundPoint(center_, extent_);
    }

    static bool intersectsQuadrant(const AcGeBoundBox2d &bounds,
                                   const Quadrant &quadrant)
    {
        return bounds.intersects(AcGeBoundBox2d::aroundPoint(
            quadrant.center, quadrant.extent));
    }

    static void sortUnique(std::vector<std::uint64_t> &ids)
    {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    }

    void insertInto(Quadrant *quadrant, std::uint64_t id,
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

    void split(Quadrant *quadrant)
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

    void boxSearch(const Quadrant *quadrant, const AcGeBoundBox2d &query,
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

    void radiusSearch(const Quadrant *quadrant, const AcGePoint2d &center,
                      double radius, std::vector<std::uint64_t> &results) const
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

    void nearestByCenter(const Quadrant *quadrant, const AcGePoint2d &point,
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
};

} // namespace ge
