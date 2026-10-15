#pragma once

#include "AcGePort.h"

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

#include "AcGePoint.h"

namespace ge
{

struct ACGE_PORT AcGeBoundBox3d
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

class ACGE_PORT AcGeOctree
{
public:
    // extent = half side length of the root cube; minExtent bounds the
    // subdivision depth; leafCapacity triggers octant splitting.
    AcGeOctree(const AcGePoint3d &center, double extent,
               size_t leafCapacity = 16, double minExtent = 1.0e-4);

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
                    const AcGeBoundBox3d &bounds, size_t depth);
    void split(Octant *octant, size_t depth);
    void boxSearch(const Octant *octant, const AcGeBoundBox3d &query,
                   std::vector<uint64_t> &results) const;
    void radiusSearch(const Octant *octant, const AcGePoint3d &center,
                      double radius, std::vector<uint64_t> &results) const;
    void nearestByCenter(const Octant *octant, const AcGePoint3d &point,
                         std::optional<uint64_t> &best,
                         double &bestDistance) const;
};

} // namespace ge
