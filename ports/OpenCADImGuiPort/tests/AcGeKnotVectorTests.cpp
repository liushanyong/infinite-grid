#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "acge/AcGe.h"

namespace
{

constexpr double kEps = 1.0e-9;

void testConstructionAndIndexing()
{
    const double raw[] = {0.0, 1.0, 2.0, 3.0};
    const AcGeKnotVector knots(4, raw, 1.0e-10);
    assert(knots.length() == 4);
    assert(knots[0] == 0.0 && knots[3] == 3.0);
    assert(knots.startParam() == 0.0);
    assert(knots.endParam() == 3.0);
    assert(!knots.isEmpty());
    assert(knots.contains(2.0));
    assert(!knots.contains(3.5));
    assert(knots.isOn(2.0));
    assert(!knots.isOn(2.5));
    assert(knots.numIntervals() == 3);
    assert(knots.tolerance() == 1.0e-10);

    AcGeKnotVector uniform;
    uniform.set(5, 0.0, 4.0);
    assert(uniform.length() == 5);
    assert(std::abs(uniform[1] - 1.0) < kEps);
    assert(uniform.numIntervals() == 4);
}

void testMultiplicityAndInsert()
{
    const double raw[] = {0.0, 1.0, 1.0, 1.0, 2.0};
    const AcGeKnotVector knots(5, raw);
    assert(knots.multiplicityAt(1) == 3);
    assert(knots.multiplicityAt(3) == 3);
    assert(knots.multiplicityAt(0) == 1);

    AcGeKnotVector single(AcGeDoubleArray{0.0, 1.0, 2.0});
    single.insert(1.5);
    assert(single.length() == 4);
    assert(std::abs(single[2] - 1.5) < kEps);

    AcGeKnotVector dup(AcGeDoubleArray{0.0, 1.0, 2.0});
    dup.insert(1.0 + 1.0e-12); // within tolerance -> multiplicity grows
    assert(dup.length() == 4);
    assert(dup.multiplicityAt(1) == 2);

    AcGeKnotVector at(AcGeDoubleArray{0.0, 1.0, 2.0});
    at.insertAt(1, 0.5, 2);
    assert(at.length() == 5);
    assert(at[1] == 0.5 && at[2] == 0.5);
}

void testDistinctKnotsRemoveAndReverse()
{
    const AcGeKnotVector knots(AcGeDoubleArray{0.0, 1.0, 1.0, 2.0, 3.0, 3.0, 3.0});
    AcGeDoubleArray distinct;
    knots.getDistinctKnots(distinct);
    assert(distinct.size() == 4);
    assert(distinct[1] == 1.0 && distinct[3] == 3.0);

    AcGeKnotVector removed(knots);
    removed.removeSubVector(1, 2);
    assert(removed.length() == 5);
    assert(removed[1] == 2.0);

    AcGeKnotVector reversed(AcGeDoubleArray{0.0, 1.0, 3.0});
    reversed.reverse();
    // Boundaries stay; the first new knot is the image of the largest.
    assert(std::abs(reversed[0] - 0.0) < kEps);
    assert(std::abs(reversed[1] - 2.0) < kEps);
    assert(std::abs(reversed[2] - 3.0) < kEps);
}

void testSetRangeSplitAndAppend()
{
    AcGeKnotVector knots(AcGeDoubleArray{0.0, 1.0, 2.0});
    knots.setRange(0.0, 6.0);
    assert(std::abs(knots[1] - 3.0) < kEps);

    AcGeKnotVector source(AcGeDoubleArray{0.0, 1.0, 2.0, 3.0, 4.0});
    AcGeKnotVector head, tail;
    source.split(2.0, &head, 2, &tail, 3);
    assert(head.length() == 4); // {0, 1, 2, 2}
    assert(std::abs(head[3] - 2.0) < kEps);
    assert(tail.length() == 5); // {2, 2, 2, 3, 4}
    assert(std::abs(tail[2] - 2.0) < kEps);

    AcGeKnotVector headPart(AcGeDoubleArray{0.0, 1.0});
    AcGeKnotVector tailPart(AcGeDoubleArray{0.0, 1.0});
    headPart.append(tailPart);
    assert(headPart.length() == 4);
    assert(std::abs(headPart[2] - 1.0) < kEps);
}

void testPlusMultAndEquality()
{
    const AcGeKnotVector src(AcGeDoubleArray{0.0, 1.0, 2.0});
    const AcGeKnotVector raised(2, src);
    assert(raised.length() == 9); // each knot repeated 1 + 2 times
    assert(raised.multiplicityAt(0) == 3);

    const AcGeKnotVector same(AcGeDoubleArray{0.0, 1.0, 2.0});
    const AcGeKnotVector other(AcGeDoubleArray{0.0, 1.0, 2.5});
    assert(src.isEqualTo(same));
    assert(!src.isEqualTo(other));

    bool threw = false;
    try
    {
        const AcGeKnotVector bad(AcGeDoubleArray{0.0, 2.0, 1.0});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

void testGetInterval()
{
    const AcGeKnotVector knots(AcGeDoubleArray{0.0, 0.0, 1.0, 2.0, 3.0, 3.0});
    AcGeInterval interval;
    const int idx = knots.getInterval(2, 1.5, interval);
    assert(idx == 2);
    assert(interval.lowerBound() == 1.0);
    assert(interval.upperBound() == 2.0);
}

} // namespace

int main()
{
    testConstructionAndIndexing();
    testMultiplicityAndInsert();
    testDistinctKnotsRemoveAndReverse();
    testSetRangeSplitAndAppend();
    testPlusMultAndEquality();
    testGetInterval();
    return 0;
}
