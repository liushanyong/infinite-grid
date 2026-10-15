#include "acge/AcGeCompositeCurve.h"

#include "acge/AcGe2d.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGeCurves2d.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{

template <typename Exception, typename Function>
void expectThrow(Function &&function)
{
    bool caught = false;
    try
    {
        function();
    }
    catch (const Exception &)
    {
        caught = true;
    }
    assert(caught);
}

template <typename Point>
void assertPoint(const Point &actual, const Point &expected)
{
    assert(std::abs(actual.x - expected.x) <= 1.0e-11);
    assert(std::abs(actual.y - expected.y) <= 1.0e-11);
    if constexpr (requires { actual.z; })
        assert(std::abs(actual.z - expected.z) <= 1.0e-11);
}

struct Planar
{
    using Composite = AcGeCompositeCurve2d;
    using Point = AcGePoint2d;
    using Line = AcGeLineSeg2d;
    using Spline = ge::AcGeBsplineCurve2d;
    using Bezier = ge::AcGeBezierCurve2d;
    using Nurbs = ge::AcGeNurbsCurve2d;
    using Fit = ge::AcGeFitSpline2d;
    static Point point(double x, double y) { return {x, y}; }
    static AcGeCircArc2d arc()
    {
        return {point(1.0, 1.0), 1.0, -std::acos(-1.0) / 2.0, 0.0};
    }
};

struct Spatial
{
    using Composite = AcGeCompositeCurve3d;
    using Point = AcGePoint3d;
    using Line = AcGeLineSeg3d;
    using Spline = ge::AcGeBsplineCurve3d;
    using Bezier = ge::AcGeBezierCurve3d;
    using Nurbs = ge::AcGeNurbsCurve3d;
    using Fit = AcGeFitSpline3d;
    static Point point(double x, double y) { return {x, y, 2.0}; }
    static AcGeCircArc3d arc()
    {
        return {point(1.0, 1.0), AcGeVector3d(0.0, 0.0, 1.0),
                1.0, -std::acos(-1.0) / 2.0, 0.0};
    }
};

template <typename Point>
struct ParameterCurve
{
    Point pointAt(double parameter) const
    {
        Point result;
        result.x = parameter;
        return result;
    }
};

template <typename Point>
struct ThrowingCurve
{
    inline static int copiesBeforeThrow = -1;
    Point value;

    explicit ThrowingCurve(Point point) : value(point) {}
    ThrowingCurve(const ThrowingCurve &other) : value(other.value)
    {
        if (copiesBeforeThrow == 0)
            throw std::runtime_error("curve copy failed");
        if (copiesBeforeThrow > 0)
            --copiesBeforeThrow;
    }
    ThrowingCurve(ThrowingCurve &&) noexcept = default;
    Point pointAt(double parameter) const
    {
        if (parameter == 0.5)
            throw std::runtime_error("curve evaluation failed");
        return value;
    }
};

template <typename Point>
struct NonfiniteCurve
{
    bool atEndpoint = false;
    Point pointAt(double parameter) const
    {
        Point result;
        if (atEndpoint || parameter == 0.5)
            result.x = std::numeric_limits<double>::quiet_NaN();
        return result;
    }
};

template <typename Types>
void testMixedAndOwnership()
{
    using Composite = typename Types::Composite;
    using Segment = typename Composite::Segment;
    typename Types::Line line(Types::point(0.0, 0.0), Types::point(1.0, 0.0));
    auto arc = Types::arc();
    typename Types::Spline spline(
        {4.0, 4.0, 4.0, 8.0, 8.0, 8.0},
        {Types::point(2.0, 1.0), Types::point(3.0, 2.0), Types::point(4.0, 1.0)}, 2);
    const double halfPi = std::acos(-1.0) / 2.0;
    Composite composite({
        Segment(line, AcGeInterval(0.0, 1.0)),
        Segment(arc, AcGeInterval(-halfPi, 0.0)),
        Segment(spline, AcGeInterval(4.0, 8.0))});

    assert(!composite.isEmpty() && composite.numSegments() == 3);
    assert(composite.domain().lowerBound() == 0.0);
    assert(composite.domain().upperBound() == 3.0);
    assert(composite.minParameter() == 0.0 && composite.maxParameter() == 3.0);
    assertPoint(composite.startPoint(), Types::point(0.0, 0.0));
    assertPoint(composite.endPoint(), Types::point(4.0, 1.0));
    assertPoint(composite.pointAt(0.5), Types::point(0.5, 0.0));
    const double diagonal = std::sqrt(0.5);
    assertPoint(composite.pointAt(1.5), Types::point(1.0 + diagonal, 1.0 - diagonal));
    assertPoint(composite.pointAt(2.5), spline.pointAt(6.0));
    assertPoint(composite.pointAt(1.0), Types::point(1.0, 0.0));
    assertPoint(composite.pointAt(2.0), Types::point(2.0, 1.0));
    assertPoint(composite.pointAt(3.0), composite.endPoint());
    assert(composite.isContinuous() && !composite.isClosed());
    assert(composite.segment(2).domain().lowerBound() == 4.0);

    std::size_t index = 99;
    assert(composite.globalToLocalParam(0.0, index) == 0.0 && index == 0);
    assert(composite.globalToLocalParam(1.0, index) == -halfPi && index == 1);
    assert(composite.globalToLocalParam(2.0, index) == 4.0 && index == 2);
    assert(composite.globalToLocalParam(3.0, index) == 8.0 && index == 2);
    assert(composite.globalToLocalParam(2.5, index) == 6.0 && index == 2);
    assert(composite.localToGlobalParam(1.0, 0) == 1.0);
    assert(composite.localToGlobalParam(-halfPi, 1) == 1.0);
    assert(composite.localToGlobalParam(6.0, 2) == 2.5);
    assert(composite.localToGlobalParam(8.0, 2) == 3.0);
    for (double global : {0.0, 0.125, 0.999, 1.0, 1.75, 2.0, 2.9, 3.0})
    {
        const double local = composite.globalToLocalParam(global, index);
        assert(std::abs(composite.localToGlobalParam(local, index) - global) < 1.0e-14);
    }
    const double below = std::nextafter(1.0, 0.0);
    const double above = std::nextafter(1.0, 2.0);
    (void)composite.globalToLocalParam(below, index);
    assert(index == 0);
    (void)composite.globalToLocalParam(above, index);
    assert(index == 1);

    line.set(Types::point(20.0, 0.0), Types::point(21.0, 0.0));
    arc.setCenter(Types::point(40.0, 40.0));
    spline.controlPoints[0] = Types::point(80.0, 80.0);
    assertPoint(composite.startPoint(), Types::point(0.0, 0.0));
    assertPoint(composite.pointAt(1.0), Types::point(1.0, 0.0));
    assertPoint(composite.pointAt(2.0), Types::point(2.0, 1.0));

    Composite copy = composite;
    copy.setSegment(0, line, AcGeInterval(0.0, 1.0));
    assertPoint(copy.startPoint(), Types::point(20.0, 0.0));
    assertPoint(composite.startPoint(), Types::point(0.0, 0.0));
    Composite assigned;
    assigned = composite;
    composite.clear();
    assertPoint(assigned.endPoint(), Types::point(4.0, 1.0));
    assert(composite.isEmpty());

    Composite owned;
    {
        typename Types::Line temporary(Types::point(5.0, 6.0), Types::point(7.0, 8.0));
        owned.addSegment(temporary, AcGeInterval(0.0, 1.0));
    }
    assertPoint(owned.pointAt(0.5), Types::point(6.0, 7.0));
    owned.set({Segment(line, AcGeInterval(0.0, 1.0))});
    assertPoint(owned.startPoint(), line.startPoint());
}

template <typename Types>
void testConnectivity()
{
    using Composite = typename Types::Composite;
    const auto makeLine = [](double a, double b) {
        return typename Types::Line(Types::point(a, 0.0), Types::point(b, 0.0));
    };
    Composite disconnected(makeLine(0.0, 1.0), AcGeInterval(0.0, 1.0));
    disconnected.addSegment(makeLine(5.0, 0.0), AcGeInterval(0.0, 1.0));
    assert(!disconnected.isContinuous());
    // Matching first/last endpoints alone does not make a closed composite.
    assert(!disconnected.isClosed());
    assertPoint(disconnected.segment(0).endPoint(), Types::point(1.0, 0.0));
    assertPoint(disconnected.pointAt(1.0), Types::point(5.0, 0.0));
    disconnected.setSegment(1, makeLine(1.0, 0.0), AcGeInterval(0.0, 1.0));
    assert(disconnected.isContinuous() && disconnected.isClosed());

    disconnected.setSegment(1, makeLine(1.0 + 1.0e-7, 0.0), AcGeInterval(0.0, 1.0));
    assert(!disconnected.isContinuous());
    AcGeTol tolerance;
    tolerance.setEqualPoint(1.0e-6);
    assert(disconnected.isContinuous(tolerance) && disconnected.isClosed(tolerance));
    for (double value : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity()})
    {
        tolerance.setEqualPoint(value);
        expectThrow<std::invalid_argument>([&] { (void)disconnected.isContinuous(tolerance); });
        expectThrow<std::invalid_argument>([&] { (void)disconnected.isClosed(tolerance); });
    }
    Composite circle(Types::arc(), AcGeInterval(0.0, 2.0 * std::acos(-1.0)));
    assert(circle.isContinuous() && circle.isClosed());

    if constexpr (std::is_same_v<Types, Spatial>)
    {
        Composite spatial(AcGeLineSeg3d(AcGePoint3d(0, 0, 0), AcGePoint3d(1, 0, 0)),
                          AcGeInterval(0.0, 1.0));
        spatial.addSegment(AcGeLineSeg3d(AcGePoint3d(1, 0, 1), AcGePoint3d(0, 0, 0)),
                           AcGeInterval(0.0, 1.0));
        assert(!spatial.isContinuous() && !spatial.isClosed());
    }
}

template <typename Types>
void testInvalidAndEmpty()
{
    using Composite = typename Types::Composite;
    using Segment = typename Composite::Segment;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    Composite empty;
    assert(empty.isEmpty() && empty.numSegments() == 0);
    assert(!empty.isContinuous() && !empty.isClosed());
    std::size_t index = 42;
    expectThrow<std::logic_error>([&] { (void)empty.domain(); });
    expectThrow<std::logic_error>([&] { (void)empty.minParameter(); });
    expectThrow<std::logic_error>([&] { (void)empty.maxParameter(); });
    expectThrow<std::logic_error>([&] { (void)empty.startPoint(); });
    expectThrow<std::logic_error>([&] { (void)empty.endPoint(); });
    expectThrow<std::logic_error>([&] { (void)empty.pointAt(0.0); });
    expectThrow<std::logic_error>([&] { (void)empty.globalToLocalParam(0.0, index); });
    expectThrow<std::logic_error>([&] { (void)empty.localToGlobalParam(0.0, 0); });
    expectThrow<std::out_of_range>([&] { (void)empty.segment(0); });
    assert(index == 42);

    typename Types::Line line(Types::point(0.0, 0.0), Types::point(1.0, 0.0));
    Composite composite(line, AcGeInterval(0.0, 1.0, 10.0));
    for (const AcGeInterval &domain : {
             AcGeInterval(), AcGeInterval(true, 0.0), AcGeInterval(false, 0.0),
             AcGeInterval(1.0, 1.0), AcGeInterval(nan, 1.0), AcGeInterval(0.0, nan),
             AcGeInterval(-inf, 1.0), AcGeInterval(0.0, inf),
             AcGeInterval(0.0, 1.0, nan), AcGeInterval(0.0, 1.0, inf)})
    {
        expectThrow<std::invalid_argument>([&] { composite.addSegment(line, domain); });
        expectThrow<std::invalid_argument>([&] { composite.setSegment(0, line, domain); });
        assert(composite.numSegments() == 1);
        assertPoint(composite.endPoint(), Types::point(1.0, 0.0));
    }
    AcGeInterval inverted(0.0, 1.0);
    expectThrow<std::invalid_argument>([&] { inverted.set(2.0, 1.0); });
    expectThrow<std::invalid_argument>([&] { (void)Segment(line, inverted); });

    for (double parameter : {nan, inf, -inf})
    {
        expectThrow<std::invalid_argument>([&] { (void)composite.pointAt(parameter); });
        expectThrow<std::invalid_argument>([&] { (void)composite.globalToLocalParam(parameter, index); });
        expectThrow<std::invalid_argument>([&] { (void)composite.localToGlobalParam(parameter, 0); });
        expectThrow<std::invalid_argument>([&] { (void)composite.segment(0).pointAt(parameter); });
    }
    for (double parameter : {-1.0, 2.0, std::nextafter(0.0, -1.0),
                              std::nextafter(1.0, 2.0)})
    {
        expectThrow<std::out_of_range>([&] { (void)composite.pointAt(parameter); });
        expectThrow<std::out_of_range>([&] { (void)composite.globalToLocalParam(parameter, index); });
        expectThrow<std::out_of_range>([&] { (void)composite.localToGlobalParam(parameter, 0); });
        expectThrow<std::out_of_range>([&] { (void)composite.segment(0).pointAt(parameter); });
    }
    assert(index == 42);
    for (std::size_t badIndex : {std::size_t(1), std::numeric_limits<std::size_t>::max()})
    {
        expectThrow<std::out_of_range>([&] { (void)composite.segment(badIndex); });
        expectThrow<std::out_of_range>([&] { (void)composite.localToGlobalParam(0.0, badIndex); });
        expectThrow<std::out_of_range>([&] { composite.setSegment(badIndex, line, AcGeInterval(0.0, 1.0)); });
    }
    expectThrow<std::invalid_argument>([&] { (void)Segment(typename Types::Bezier{}, AcGeInterval(0.0, 1.0)); });
    expectThrow<std::invalid_argument>([&] { (void)Segment(typename Types::Spline{}, AcGeInterval(0.0, 1.0)); });
    expectThrow<std::invalid_argument>([&] { (void)Segment(typename Types::Nurbs{}, AcGeInterval(0.0, 1.0)); });
    expectThrow<std::invalid_argument>([&] { (void)Segment(typename Types::Fit{}, AcGeInterval(0.0, 1.0)); });
    typename Types::Spline invalidSpline;
    invalidSpline.controlPoints = {Types::point(0.0, 0.0)};
    expectThrow<std::invalid_argument>([&] { (void)Segment(invalidSpline, AcGeInterval(0.0, 1.0)); });
    composite.set({});
    assert(composite.isEmpty());
}

template <typename Types, typename Spline>
void testMalformedSpline()
{
    using Composite = typename Types::Composite;
    using Segment = typename Composite::Segment;
    const AcGeInterval domain(0.0, 1.0);
    typename Types::Line line(Types::point(0.0, 0.0), Types::point(1.0, 0.0));
    Composite target(line, domain);
    const auto reject = [&](const Spline &spline) {
        expectThrow<std::invalid_argument>([&] { (void)Segment(spline, domain); });
        expectThrow<std::invalid_argument>([&] { target.addSegment(spline, domain); });
        expectThrow<std::invalid_argument>([&] { target.setSegment(0, spline, domain); });
        assert(target.numSegments() == 1);
        assertPoint(target.startPoint(), line.startPoint());
        assertPoint(target.endPoint(), line.endPoint());
    };

    // This malformed state passes the existing spline validator, but its
    // endpoint evaluation previously underflowed a basis-function knot index.
    Spline spline;
    spline.degree = 3;
    spline.controlPoints.resize(1);
    spline.knots = {0.0, 0.0, 0.0, 1.0, 1.0};
    spline.validate();
    reject(spline);
    spline.degree = std::numeric_limits<std::size_t>::max();
    reject(spline);

    spline.degree = 1;
    spline.controlPoints.resize(2);
    spline.knots = {0.0, 0.0, 1.0};
    reject(spline);
    spline.knots = {0.0, 0.0};
    reject(spline);
    spline.knots.clear();
    reject(spline);
    spline.knots = {0.0, 0.0, 1.0, 1.0, 1.0};
    reject(spline);
    for (const std::vector<double> &knots : {
             std::vector<double>{0.0, 1.0, 0.0, 1.0},
             std::vector<double>{0.0, 0.0, 0.0, 0.0},
             std::vector<double>{0.0, 0.0, std::numeric_limits<double>::quiet_NaN(), 1.0},
             std::vector<double>{0.0, 0.0, 1.0, std::numeric_limits<double>::infinity()}})
    {
        spline.knots = knots;
        reject(spline);
    }
    spline.degree = 2;
    spline.controlPoints.resize(3);
    spline.knots = {-1.0, -1.0, 0.0, 0.0, 1.0, 1.0};
    reject(spline);

    spline.knots = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    const Segment valid(spline, AcGeInterval(0.25, 0.75));
    assert(valid.domain().lowerBound() == 0.25);
    expectThrow<std::invalid_argument>([&] {
        (void)Segment(spline, AcGeInterval(-0.01, 1.0));
    });
    expectThrow<std::invalid_argument>([&] {
        (void)Segment(spline, AcGeInterval(0.0, 1.01));
    });
}

template <typename Types>
void testNumericalDomains()
{
    using Composite = typename Types::Composite;
    using Point = typename Types::Point;
    const double largest = std::numeric_limits<double>::max();
    Composite wide(ParameterCurve<Point>{}, AcGeInterval(-largest, largest));
    std::size_t index = 99;
    assert(wide.globalToLocalParam(0.0, index) == -largest && index == 0);
    assert(wide.globalToLocalParam(0.5, index) == 0.0);
    assert(wide.globalToLocalParam(1.0, index) == largest);
    assert(wide.localToGlobalParam(-largest, 0) == 0.0);
    assert(wide.localToGlobalParam(0.0, 0) == 0.5);
    assert(wide.localToGlobalParam(largest, 0) == 1.0);
    assert(wide.pointAt(0.5).x == 0.0);
    assert(std::abs(wide.pointAt(0.25).x / largest + 0.5) < 1.0e-15);
    assert(wide.localToGlobalParam(-largest * 0.5, 0) == 0.25);

    const double tiny = std::numeric_limits<double>::denorm_min();
    Composite narrow(ParameterCurve<Point>{}, AcGeInterval(tiny, 3.0 * tiny));
    assert(narrow.globalToLocalParam(0.5, index) == 2.0 * tiny);
    assert(narrow.localToGlobalParam(2.0 * tiny, 0) == 0.5);
    Composite adjacent(ParameterCurve<Point>{},
                       AcGeInterval(largest * 0.5, std::nextafter(largest * 0.5, largest)));
    assert(adjacent.localToGlobalParam(adjacent.segment(0).domain().lowerBound(), 0) == 0.0);
    assert(adjacent.localToGlobalParam(adjacent.segment(0).domain().upperBound(), 0) == 1.0);
}

template <typename Types>
void testExceptionSafety()
{
    using Composite = typename Types::Composite;
    using Segment = typename Composite::Segment;
    using Point = typename Types::Point;
    using Throwing = ThrowingCurve<Point>;
    typename Types::Line line(Types::point(0.0, 0.0), Types::point(1.0, 0.0));
    Composite target(line, AcGeInterval(0.0, 1.0));
    Composite source(Throwing(Types::point(5.0, 6.0)), AcGeInterval(0.0, 1.0));
    expectThrow<std::runtime_error>([&] { (void)source.pointAt(0.5); });
    assertPoint(source.endPoint(), Types::point(5.0, 6.0));
    Throwing::copiesBeforeThrow = 0;
    expectThrow<std::runtime_error>([&] { target = source; });
    expectThrow<std::runtime_error>([&] { target.addSegment(source.segment(0)); });
    expectThrow<std::runtime_error>([&] { target.setSegment(0, source.segment(0)); });
    expectThrow<std::runtime_error>([&] { target.set({source.segment(0)}); });
    assert(target.numSegments() == 1);
    assertPoint(target.startPoint(), Types::point(0.0, 0.0));
    Throwing::copiesBeforeThrow = -1;

    // Fail after copying one segment: assignment must not partially replace it.
    source.addSegment(source.segment(0));
    Throwing::copiesBeforeThrow = 1;
    expectThrow<std::runtime_error>([&] { target = source; });
    assert(target.numSegments() == 1);
    assertPoint(target.endPoint(), Types::point(1.0, 0.0));
    Throwing::copiesBeforeThrow = -1;
    target = source;
    assert(target.numSegments() == 2);
    assertPoint(target.endPoint(), Types::point(5.0, 6.0));

    Segment moved(line, AcGeInterval(0.0, 1.0));
    Segment destination(std::move(moved));
    assertPoint(destination.endPoint(), Types::point(1.0, 0.0));
    // std::function's moved-from value is valid but unspecified.
    try
    {
        (void)moved.startPoint();
    }
    catch (const std::logic_error &)
    {
        expectThrow<std::logic_error>([&] { target.addSegment(moved); });
        expectThrow<std::logic_error>([&] { target.set({moved}); });
        assert(target.numSegments() == 2);
    }

    expectThrow<std::domain_error>([&] {
        target.addSegment(NonfiniteCurve<Point>{true}, AcGeInterval(0.0, 1.0));
    });
    expectThrow<std::domain_error>([&] {
        target.setSegment(0, NonfiniteCurve<Point>{true}, AcGeInterval(0.0, 1.0));
    });
    assertPoint(target.startPoint(), Types::point(5.0, 6.0));
    Composite nonfinite(NonfiniteCurve<Point>{}, AcGeInterval(0.0, 1.0));
    expectThrow<std::domain_error>([&] { (void)nonfinite.pointAt(0.5); });
    assert(nonfinite.pointAt(0.0).x == 0.0);
}

template <typename Types>
void runTests()
{
    testMixedAndOwnership<Types>();
    testConnectivity<Types>();
    testInvalidAndEmpty<Types>();
    testMalformedSpline<Types, typename Types::Spline>();
    testMalformedSpline<Types, typename Types::Nurbs>();
    testNumericalDomains<Types>();
    testExceptionSafety<Types>();
}

} // namespace

int main()
{
    runTests<Planar>();
    runTests<Spatial>();
    std::cout << "AcGeCompositeCurve tests passed (2d and 3d).\n";
}
