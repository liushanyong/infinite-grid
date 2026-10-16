#pragma once

#include "AcGePort.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "AcGePoint.h"

namespace ge
{

// Point index for deterministic nearest-neighbor queries in 3D.
class ACGE_PORT AcGeKdTree
{
public:
    struct Neighbor
    {
        std::uint32_t index;
        double distanceSquared;
    };

    AcGeKdTree() = default;
    explicit AcGeKdTree(std::vector<AcGePoint3d> points);

    void build(std::vector<AcGePoint3d> points);

    std::size_t size() const noexcept { return points_.size(); }
    bool empty() const noexcept { return points_.empty(); }
    const std::vector<AcGePoint3d> &points() const noexcept { return points_; }

    std::optional<Neighbor> nearest(const AcGePoint3d &query) const;
    std::vector<Neighbor> kNearest(const AcGePoint3d &query,
                                   std::size_t count) const;
    std::vector<Neighbor> radiusSearch(const AcGePoint3d &query,
                                      double radius) const;

private:
    struct Node
    {
        std::uint32_t pointIndex = 0;
        std::uint32_t left = UINT32_MAX;
        std::uint32_t right = UINT32_MAX;
        std::uint8_t axis = 0;
    };

    static constexpr std::uint32_t kNoNode = UINT32_MAX;

    std::vector<AcGePoint3d> points_;
    std::vector<Node> nodes_;
    std::uint32_t root_ = kNoNode;

    std::uint32_t buildRange(std::vector<std::uint32_t> &indices,
                             std::size_t begin, std::size_t end);
};

} // namespace ge
