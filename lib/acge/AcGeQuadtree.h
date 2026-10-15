#pragma once

#include "AcGePort.h"

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

#include "AcGePoint.h"

namespace ge
{

struct ACGE_PORT AcGeBoundBox2d
{
    AcGePoint2d min{0.0, 0.0};
    AcGePoint2d max{0.0, 0.0};
    bool m_empty = true;

    AcGeBoundBox2d() = default;
    AcGeBoundBox2d(const AcGePoint2d &point1, const AcGePoint2d &point2);

    AcGeBoundBox2d &set(const AcGePoint2d &point1, const AcGePoint2d &point2);
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

    AcGeBoundBox2d &extend(const AcGePoint2d &point);
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
        const AcGeBoundBox2d &box) const;

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
                                      double extent);
};

class ACGE_PORT AcGeQuadtree
{
public:
    AcGeQuadtree(const AcGePoint2d &center, double extent,
                 std::size_t leafCapacity = 16, double minExtent = 1.0e-4);

    void insert(std::uint64_t id, const AcGeBoundBox2d &bounds);
    std::vector<std::uint64_t> boxSearch(const AcGeBoundBox2d &query) const;
    std::vector<std::uint64_t> radiusSearch(const AcGePoint2d &center,
                                            double radius) const;

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

    static void sortUnique(std::vector<std::uint64_t> &ids);

    void insertInto(Quadrant *quadrant, std::uint64_t id,
                    const AcGeBoundBox2d &bounds);
    void split(Quadrant *quadrant);
    void boxSearch(const Quadrant *quadrant, const AcGeBoundBox2d &query,
                   std::vector<std::uint64_t> &results) const;
    void radiusSearch(const Quadrant *quadrant, const AcGePoint2d &center,
                      double radius, std::vector<std::uint64_t> &results) const;
    void nearestByCenter(const Quadrant *quadrant, const AcGePoint2d &point,
                         std::optional<std::uint64_t> &best,
                         double &bestDistanceSquared) const;
};

} // namespace ge
