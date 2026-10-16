#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "acge/AcGe.h"

namespace
{

void testNearestAndDeterministicTies()
{
    const ge::AcGeKdTree tree(
        {{-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 3.0, 0.0}});
    const auto nearest = tree.nearest({0.0, 0.0, 0.0});
    assert(nearest);
    assert(nearest->index == 0);
    assert(nearest->distanceSquared == 1.0);

    const auto two = tree.kNearest({0.0, 0.0, 0.0}, 2);
    assert(two.size() == 2);
    assert(two[0].index == 0);
    assert(two[1].index == 1);
}

void testRadiusAndClamping()
{
    const ge::AcGeKdTree tree(
        {{0.0, 0.0, 0.0}, {3.0, 4.0, 0.0}, {0.0, 0.0, 2.0}});
    const auto within = tree.radiusSearch({0.0, 0.0, 0.0}, 2.0);
    assert(within.size() == 2);
    assert(within[0].index == 0);
    assert(within[1].index == 2);
    assert(within[1].distanceSquared == 4.0);

    const auto all = tree.kNearest({0.0, 0.0, 0.0}, 100);
    assert(all.size() == tree.size());
    assert(tree.kNearest({0.0, 0.0, 0.0}, 0).empty());
}

void testEmptyAndInvalidInputs()
{
    const ge::AcGeKdTree empty;
    assert(empty.empty());
    assert(!empty.nearest({0.0, 0.0, 0.0}));
    assert(empty.radiusSearch({0.0, 0.0, 0.0}, 1.0).empty());

    bool rejectedPoint = false;
    try
    {
        ge::AcGeKdTree invalid({{0.0, 0.0, std::numeric_limits<double>::infinity()}});
    }
    catch (const std::invalid_argument &)
    {
        rejectedPoint = true;
    }
    assert(rejectedPoint);

    bool rejectedRadius = false;
    try
    {
        empty.radiusSearch({0.0, 0.0, 0.0}, -1.0);
    }
    catch (const std::invalid_argument &)
    {
        rejectedRadius = true;
    }
    assert(rejectedRadius);

    bool rejectedQuery = false;
    try
    {
        empty.nearest({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    }
    catch (const std::invalid_argument &)
    {
        rejectedQuery = true;
    }
    assert(rejectedQuery);
}

void testAgainstBruteForce()
{
    std::vector<AcGePoint3d> points;
    for (int x = -4; x <= 4; ++x)
        for (int y = -3; y <= 3; ++y)
            for (int z = -2; z <= 2; ++z)
                points.emplace_back(x * 0.7, y * 1.1, z * 1.3);
    const ge::AcGeKdTree tree(points);

    for (int x = -3; x <= 3; ++x)
    {
        const AcGePoint3d query{x * 0.37, x * -0.29, x * 0.61};
        std::vector<ge::AcGeKdTree::Neighbor> expected;
        for (std::uint32_t i = 0; i < points.size(); ++i)
        {
            const double dx = points[i].x - query.x;
            const double dy = points[i].y - query.y;
            const double dz = points[i].z - query.z;
            expected.push_back({i, dx * dx + dy * dy + dz * dz});
        }
        std::sort(expected.begin(), expected.end(),
                  [](const auto &a, const auto &b) {
                      if (a.distanceSquared != b.distanceSquared)
                          return a.distanceSquared < b.distanceSquared;
                      return a.index < b.index;
                  });

        const auto actual = tree.kNearest(query, 7);
        assert(actual.size() == 7);
        for (std::size_t i = 0; i < actual.size(); ++i)
        {
            assert(actual[i].index == expected[i].index);
            assert(actual[i].distanceSquared == expected[i].distanceSquared);
        }
        const auto within = tree.radiusSearch(query, 2.5);
        const auto expectedEnd =
            std::upper_bound(expected.begin(), expected.end(), 2.5 * 2.5,
                             [](double radiusSquared, const auto &neighbor) {
                                 return radiusSquared < neighbor.distanceSquared;
                             });
        assert(within.size() ==
               static_cast<std::size_t>(expectedEnd - expected.begin()));
        for (std::size_t i = 0; i < within.size(); ++i)
            assert(within[i].index == expected[i].index);
    }
}

} // namespace

int main()
{
    testNearestAndDeterministicTies();
    testRadiusAndClamping();
    testEmptyAndInvalidInputs();
    testAgainstBruteForce();
}
