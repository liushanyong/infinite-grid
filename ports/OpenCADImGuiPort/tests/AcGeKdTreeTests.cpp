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

} // namespace

int main()
{
    testNearestAndDeterministicTies();
    testRadiusAndClamping();
    testEmptyAndInvalidInputs();
}
