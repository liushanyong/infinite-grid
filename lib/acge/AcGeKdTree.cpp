#include "AcGeKdTree.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace ge
{
namespace
{

double coordinate(const AcGePoint3d &point, std::uint8_t axis)
{
    return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
}

void validatePoint(const AcGePoint3d &point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        !std::isfinite(point.z))
        throw std::invalid_argument("Kd-tree points and queries must be finite.");
}

double distanceSquared(const AcGePoint3d &a, const AcGePoint3d &b)
{
    const double x = a.x - b.x;
    const double y = a.y - b.y;
    const double z = a.z - b.z;
    return x * x + y * y + z * z;
}

bool neighborLess(const AcGeKdTree::Neighbor &a,
                  const AcGeKdTree::Neighbor &b)
{
    if (a.distanceSquared != b.distanceSquared)
        return a.distanceSquared < b.distanceSquared;
    return a.index < b.index;
}

} // namespace

AcGeKdTree::AcGeKdTree(std::vector<AcGePoint3d> points)
{
    build(std::move(points));
}

void AcGeKdTree::build(std::vector<AcGePoint3d> points)
{
    if (points.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Kd-tree input has too many points.");
    for (const AcGePoint3d &point : points)
        validatePoint(point);

    points_ = std::move(points);
    nodes_.clear();
    nodes_.reserve(points_.size());
    root_ = kNoNode;
    if (points_.empty())
        return;

    std::vector<std::uint32_t> indices(points_.size());
    for (std::uint32_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    root_ = buildRange(indices, 0, indices.size());
}

std::uint32_t AcGeKdTree::buildRange(std::vector<std::uint32_t> &indices,
                                     std::size_t begin, std::size_t end)
{
    std::array<double, 3> minimum{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()};
    std::array<double, 3> maximum{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()};
    for (std::size_t i = begin; i < end; ++i)
    {
        const AcGePoint3d &point = points_[indices[i]];
        for (std::uint8_t axis = 0; axis < 3; ++axis)
        {
            const double value = coordinate(point, axis);
            minimum[axis] = std::min(minimum[axis], value);
            maximum[axis] = std::max(maximum[axis], value);
        }
    }
    std::uint8_t axis = 0;
    for (std::uint8_t candidate = 1; candidate < 3; ++candidate)
        if (maximum[candidate] - minimum[candidate] >
            maximum[axis] - minimum[axis])
            axis = candidate;

    const std::size_t middle = begin + (end - begin) / 2;
    std::nth_element(
        indices.begin() + begin, indices.begin() + middle,
        indices.begin() + end, [&](std::uint32_t a, std::uint32_t b) {
            const double ca = coordinate(points_[a], axis);
            const double cb = coordinate(points_[b], axis);
            return ca == cb ? a < b : ca < cb;
        });

    const std::uint32_t nodeIndex =
        static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back({indices[middle], kNoNode, kNoNode, axis});
    if (begin < middle)
        nodes_[nodeIndex].left = buildRange(indices, begin, middle);
    if (middle + 1 < end)
        nodes_[nodeIndex].right = buildRange(indices, middle + 1, end);
    return nodeIndex;
}

std::optional<AcGeKdTree::Neighbor>
AcGeKdTree::nearest(const AcGePoint3d &query) const
{
    validatePoint(query);
    if (root_ == kNoNode)
        return std::nullopt;

    Neighbor best{0, std::numeric_limits<double>::infinity()};
    bool hasBest = false;
    auto visit = [&](auto &&self, std::uint32_t nodeIndex) -> void {
        if (nodeIndex == kNoNode)
            return;
        const Node &node = nodes_[nodeIndex];
        const AcGePoint3d &point = points_[node.pointIndex];
        const Neighbor candidate{node.pointIndex,
                                 distanceSquared(point, query)};
        if (!hasBest || neighborLess(candidate, best))
        {
            best = candidate;
            hasBest = true;
        }

        const double delta =
            coordinate(query, node.axis) - coordinate(point, node.axis);
        const std::uint32_t nearChild = delta <= 0.0 ? node.left : node.right;
        const std::uint32_t farChild = delta <= 0.0 ? node.right : node.left;
        self(self, nearChild);
        if (delta * delta <= best.distanceSquared)
            self(self, farChild);
    };
    visit(visit, root_);
    return best;
}

std::vector<AcGeKdTree::Neighbor>
AcGeKdTree::kNearest(const AcGePoint3d &query, std::size_t count) const
{
    validatePoint(query);
    if (count == 0 || root_ == kNoNode)
        return {};
    count = std::min(count, points_.size());

    auto worseFirst = [](const Neighbor &a, const Neighbor &b) {
        return neighborLess(a, b);
    };
    std::priority_queue<Neighbor, std::vector<Neighbor>,
                        decltype(worseFirst)>
        candidates(worseFirst);
    auto visit = [&](auto &&self, std::uint32_t nodeIndex) -> void {
        if (nodeIndex == kNoNode)
            return;
        const Node &node = nodes_[nodeIndex];
        const AcGePoint3d &point = points_[node.pointIndex];
        const Neighbor candidate{node.pointIndex,
                                 distanceSquared(point, query)};
        if (candidates.size() < count)
            candidates.push(candidate);
        else if (neighborLess(candidate, candidates.top()))
        {
            candidates.pop();
            candidates.push(candidate);
        }

        const double delta =
            coordinate(query, node.axis) - coordinate(point, node.axis);
        const std::uint32_t nearChild = delta <= 0.0 ? node.left : node.right;
        const std::uint32_t farChild = delta <= 0.0 ? node.right : node.left;
        self(self, nearChild);
        const double worstDistance =
            candidates.size() < count
                ? std::numeric_limits<double>::infinity()
                : candidates.top().distanceSquared;
        if (delta * delta <= worstDistance)
            self(self, farChild);
    };
    visit(visit, root_);

    std::vector<Neighbor> result;
    result.reserve(candidates.size());
    while (!candidates.empty())
    {
        result.push_back(candidates.top());
        candidates.pop();
    }
    std::sort(result.begin(), result.end(), neighborLess);
    return result;
}

std::vector<AcGeKdTree::Neighbor>
AcGeKdTree::radiusSearch(const AcGePoint3d &query, double radius) const
{
    validatePoint(query);
    if (!std::isfinite(radius) || radius < 0.0)
        throw std::invalid_argument("Kd-tree radius must be finite and non-negative.");
    if (root_ == kNoNode)
        return {};

    const double radiusSquared = radius * radius;
    std::vector<Neighbor> result;
    auto visit = [&](auto &&self, std::uint32_t nodeIndex) -> void {
        if (nodeIndex == kNoNode)
            return;
        const Node &node = nodes_[nodeIndex];
        const AcGePoint3d &point = points_[node.pointIndex];
        const double distance = distanceSquared(point, query);
        if (distance <= radiusSquared)
            result.push_back({node.pointIndex, distance});

        const double delta =
            coordinate(query, node.axis) - coordinate(point, node.axis);
        const std::uint32_t nearChild = delta <= 0.0 ? node.left : node.right;
        const std::uint32_t farChild = delta <= 0.0 ? node.right : node.left;
        self(self, nearChild);
        if (delta * delta <= radiusSquared)
            self(self, farChild);
    };
    visit(visit, root_);
    std::sort(result.begin(), result.end(), neighborLess);
    return result;
}

} // namespace ge
