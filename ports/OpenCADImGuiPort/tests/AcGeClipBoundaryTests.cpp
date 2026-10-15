#include "acge/AcGeClipBoundary2d.h"

#include <cassert>
#include <limits>

namespace
{
template <typename Function>
void invalid(Function function)
{
    bool caught = false;
    try
    {
        function();
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    assert(caught);
}

void near(const AcGePoint2d &point, double x, double y)
{
    assert(std::abs(point.x - x) < 1.0e-8);
    assert(std::abs(point.y - y) < 1.0e-8);
}
}

int main()
{
    AcGeTol exact;
    exact.setEqualPoint(0.0);
    AcGeClipBoundary2d box({0.0, 0.0}, {2.0, 2.0}, exact);
    assert(box.isSet() && box.vertices().size() == 4);
    assert(box.containsPoint({1.0, 1.0}));
    assert(box.containsPoint({0.0, 1.0}));
    assert(!box.containsPoint({-0.01, 1.0}));
    AcGePoint2d a(99.0, 99.0), b(99.0, 99.0);
    assert(box.clipSegment({-1.0, 1.0}, {3.0, 1.0}, a, b));
    near(a, 0.0, 1.0);
    near(b, 2.0, 1.0);
    assert(box.clipSegment({3.0, 1.0}, {-1.0, 1.0}, a, b));
    near(a, 2.0, 1.0);
    near(b, 0.0, 1.0);
    assert(!box.clipSegment({-1.0, -1.0}, {-2.0, -2.0}, a, b));
    near(a, 2.0, 1.0);
    assert(box.clipSegment({-1.0, 1.0}, {1.0, -1.0}, a, b));
    near(a, 0.0, 0.0);
    near(b, 0.0, 0.0);
    assert(box.clipSegment({1.0, 1.0}, {1.0, 1.0}, a, b));
    assert(!box.clipSegment({3.0, 3.0}, {3.0, 3.0}, a, b));
    a = {-1.0, 1.0};
    b = {3.0, 1.0};
    assert(box.clipSegment(a, b, a, b));
    near(a, 0.0, 1.0);
    near(b, 2.0, 1.0);

    const auto polygon = box.clipPolygon(
        {{-1.0, -1.0}, {3.0, -1.0}, {3.0, 3.0}, {-1.0, 3.0}});
    assert(polygon.size() == 4);
    for (const auto &point : polygon)
    {
        assert(point.x >= -1.0e-12 && point.x <= 2.0 + 1.0e-12);
        assert(point.y >= -1.0e-12 && point.y <= 2.0 + 1.0e-12);
    }
    double doubleArea = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        const auto &p = polygon[i];
        const auto &q = polygon[(i + 1) % polygon.size()];
        doubleArea += p.x * q.y - p.y * q.x;
    }
    assert(std::abs(doubleArea - 8.0) < 1.0e-10);
    assert(box.clipPolygon({{3.0, 0.0}, {4.0, 0.0}, {4.0, 1.0}}).empty());
    assert(box.clipPolygon({}).empty());
    assert(box.clipPolyline({}).empty());
    assert(box.clipPolyline({{1.0, 1.0}}).size() == 1);
    assert(box.clipPolyline({{3.0, 1.0}}).empty());
    auto chains = box.clipPolyline(
        {{-1.0, 1.0}, {1.0, 1.0}, {3.0, 1.0}, {1.0, 1.0}});
    assert(chains.size() == 2);
    assert(chains[0].size() == 3);
    near(chains[0].front(), 0.0, 1.0);
    near(chains[0].back(), 2.0, 1.0);
    near(chains[1].front(), 2.0, 1.0);
    near(chains[1].back(), 1.0, 1.0);

    AcGeClipBoundary2d triangle(
        std::vector<AcGePoint2d>{{0.0, 0.0}, {0.0, 2.0}, {2.0, 0.0}, {0.0, 0.0}},
        exact);
    assert(triangle.vertices().size() == 3);
    assert(triangle.containsPoint({0.25, 0.25}));
    assert(!triangle.containsPoint({1.5, 1.5}));
    assert(triangle.clipSegment({0.5, -1.0}, {0.5, 3.0}, a, b));
    near(a, 0.5, 0.0);
    near(b, 0.5, 1.5);
    AcGeTol tolerant;
    tolerant.setEqualPoint(0.1);
    AcGeClipBoundary2d expanded({0.0, 0.0}, {2.0, 2.0}, tolerant);
    assert(expanded.containsPoint({-0.05, 1.0}));
    assert(!expanded.containsPoint({-0.2, 1.0}));
    assert(expanded.clipSegment({-1.0, 1.0}, {3.0, 1.0}, a, b));
    near(a, -0.1, 1.0);
    near(b, 2.1, 1.0);
    AcGeClipBoundary2d translated({1.0e12, 1.0e12},
                                 {1.0e12 + 2.0, 1.0e12 + 2.0}, exact);
    assert(translated.clipSegment({1.0e12 - 1.0, 1.0e12 + 1.0},
                                  {1.0e12 + 3.0, 1.0e12 + 1.0}, a, b));
    near(a, 1.0e12, 1.0e12 + 1.0);
    near(b, 1.0e12 + 2.0, 1.0e12 + 1.0);

    invalid([&] { box.set({0.0, 0.0}, {0.0, 1.0}); });
    invalid([&] { box.set(std::vector<AcGePoint2d>{{0, 0}, {1, 0}}); });
    invalid([&] { box.set(std::vector<AcGePoint2d>{{0, 0}, {1, 0}, {2, 0}}); });
    invalid([&] { box.set(std::vector<AcGePoint2d>{{0, 0}, {2, 0}, {1, 1}, {2, 2}, {0, 2}}); });
    invalid([&] { box.set(std::vector<AcGePoint2d>{{0, 0}, {2, 2}, {0, 2}, {2, 0}}); });
    // A pentagram has consistent local turn signs, but is not convex/simple.
    invalid([&] { box.set(std::vector<AcGePoint2d>{{0, 3}, {2, -3}, {-3, 1}, {3, 1}, {-2, -3}}); });
    invalid([&] { box.clipPolygon({{0, 0}, {1, 0}}); });
    const double nan = std::numeric_limits<double>::quiet_NaN();
    invalid([&] { box.containsPoint({nan, 0.0}); });
    invalid([&] { box.clipPolyline({{0, 0}, {nan, 0}}); });
    tolerant.setEqualPoint(-1.0);
    invalid([&] { box.set({0, 0}, {1, 1}, tolerant); });
    assert(box.containsPoint({1.0, 1.0})); // Failed setters preserve the boundary.
    AcGeClipBoundary2d unset;
    bool caught = false;
    try
    {
        unset.containsPoint({0.0, 0.0});
    }
    catch (const std::logic_error &)
    {
        caught = true;
    }
    assert(caught);
}
