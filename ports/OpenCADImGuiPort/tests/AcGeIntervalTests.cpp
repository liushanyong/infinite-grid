#include "acge/AcGeInterval.h"

#include <cassert>
#include <cmath>

namespace
{
void assertInterval(const AcGeInterval &interval, double lower, double upper)
{
    assert(interval.isBounded());
    assert(std::abs(interval.lowerBound() - lower) <= 1.0e-12);
    assert(std::abs(interval.upperBound() - upper) <= 1.0e-12);
}
} // namespace

int main()
{
    AcGeInterval left;
    AcGeInterval right;

    const AcGeInterval bounded(0.0, 10.0);
    assert(bounded.subtract(AcGeInterval(3.0, 7.0), left, right) == 2);
    assertInterval(left, 0.0, 3.0);
    assertInterval(right, 7.0, 10.0);

    assert(bounded.subtract(AcGeInterval(10.0, 12.0), left, right) == 1);
    assert(left == bounded);
    assert(bounded.subtract(AcGeInterval(-2.0, 12.0), left, right) == 0);

    assert(bounded.subtract(bounded, left, right) == 2);
    assertInterval(left, 0.0, 0.0);
    assertInterval(right, 10.0, 10.0);
    assert(bounded.subtract(AcGeInterval(0.0, 4.0), left, right) == 2);
    assertInterval(left, 0.0, 0.0);
    assertInterval(right, 4.0, 10.0);
    assert(bounded.subtract(AcGeInterval(6.0, 10.0), left, right) == 2);
    assertInterval(left, 0.0, 6.0);
    assertInterval(right, 10.0, 10.0);

    const AcGeInterval unboundedBelow(false, 5.0);
    assert(unboundedBelow.subtract(AcGeInterval(2.0, 4.0), left, right) ==
           2);
    assert(!left.isBoundedBelow() && left.isBoundedAbove());
    assert(std::abs(left.upperBound() - 2.0) <= 1.0e-12);
    assertInterval(right, 4.0, 5.0);
    assert(unboundedBelow.subtract(AcGeInterval(false, 2.0), left, right) ==
           1);
    assertInterval(left, 2.0, 5.0);

    AcGeInterval unbounded;
    assert(unbounded.subtract(AcGeInterval(2.0, 4.0), left, right) == 2);
    assert(!left.isBoundedBelow() && left.isBoundedAbove());
    assert(right.isBoundedBelow() && !right.isBoundedAbove());

    const AcGeInterval singleton(5.0, 5.0);
    assert(singleton.subtract(AcGeInterval(0.0, 10.0), left, right) == 0);
    assert(singleton.subtract(AcGeInterval(5.0, 10.0), left, right) == 1);
    assert(left == singleton);

    AcGeInterval overlap;
    assert(AcGeInterval(0.0, 5.0)
               .isOverlapAtUpper(AcGeInterval(3.0, 8.0), overlap));
    assertInterval(overlap, 3.0, 5.0);
    assert(AcGeInterval(0.0, 5.0)
               .isOverlapAtUpper(AcGeInterval(5.0, 8.0), overlap));
    assertInterval(overlap, 5.0, 5.0);
    assert(!AcGeInterval(0.0, 5.0)
                .isOverlapAtUpper(AcGeInterval(0.0, 8.0), overlap));
    assert(!AcGeInterval(0.0, 5.0)
                .isOverlapAtUpper(AcGeInterval(3.0, 4.0), overlap));
}
