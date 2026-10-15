// Standalone assert tests; intentionally independent of the port executable.
#include "acge/AcGeOffsetCurve.h"
#include "acge/AcGe2d.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGeCurves2d.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <numbers>
#include <type_traits>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double inf = std::numeric_limits<double>::infinity();
constexpr double nan = std::numeric_limits<double>::quiet_NaN();

template <class Exception, class Function>
void throws(Function function)
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

void near(double actual, double expected, double tolerance = 1e-11)
{
    assert(std::isfinite(actual));
    if (std::abs(actual - expected) > tolerance)
        std::cerr << "Expected " << expected << ", got " << actual << '\n';
    assert(std::abs(actual - expected) <= tolerance);
}
void near(AcGePoint2d actual, AcGePoint2d expected)
{
    near(actual.x, expected.x);
    near(actual.y, expected.y);
}
void near(AcGePoint3d actual, AcGePoint3d expected)
{
    near(actual.x, expected.x);
    near(actual.y, expected.y);
    near(actual.z, expected.z);
}

struct AnalyticLine2d
{
    AcGePoint2d origin{1, 2};
    AcGeVector2d velocity{3, 4};
    AcGePoint2d pointAt(double t) const { return origin + velocity * t; }
    AcGeVector2d derivativeAt(double) const { return velocity; }
};

struct ThrowingCopyLine2d : AnalyticLine2d
{
    inline static bool failCopies = false;
    ThrowingCopyLine2d() = default;
    ThrowingCopyLine2d(const ThrowingCopyLine2d &other) : AnalyticLine2d(other)
    {
        if (failCopies)
            throw std::runtime_error("source copy failure");
    }
    ThrowingCopyLine2d(ThrowingCopyLine2d &&) = default;
};

void linesAndOwnership()
{
    AnalyticLine2d source;
    AcGeOffsetCurve2d offset(source, -2, 3, 5);
    source.origin = {99, 99};
    source.velocity = {-10, -10};
    near(offset.startPoint(), {-9, -3});
    near(offset.endPoint(), {6, 17});
    near(offset.pointAt(0), {-3, 5});
    near(offset.distance(), 5);
    near(offset.minParameter(), -2);
    near(offset.maxParameter(), 3);

    auto copy = offset;
    offset.setDistance(-5).setDomain(-1, 1);
    near(offset.pointAt(0), {5, -1});
    near(copy.pointAt(0), {-3, 5});
    near(copy.minParameter(), -2);
    copy = offset;
    near(copy.startPoint(), {2, -5});
    offset.setDistance(0);
    near(offset.pointAt(0.5), {2.5, 4});

    AcGeLineSeg2d segment({2, 3}, AcGePoint2d{6, 3});
    auto owned = AcGeOffsetCurve2d(
        segment, [](const AcGeLineSeg2d &line, double) {
            return line.direction();
        }, 0, 1, 2);
    segment.set({-2, -3}, AcGePoint2d{-6, -3});
    near(owned.startPoint(), {2, 5});
    near(owned.endPoint(), {6, 5});
    auto returned = [] {
        return AcGeOffsetCurve2d(AnalyticLine2d{}, -1, 1, 2);
    }();
    near(returned.pointAt(0), {-0.6, 3.2});

    AcGeLineSeg3d line3({1, 2, 3}, AcGePoint3d{1, 2, 7});
    auto tangent3 = [](const AcGeLineSeg3d &line, double) {
        return line.direction();
    };
    AcGeOffsetCurve3d offset3(line3, tangent3, 0, 1, 2, {0, 8, 0});
    line3.set({0, 0, 0}, AcGePoint3d{1, 0, 0});
    near(offset3.startPoint(), {3, 2, 3});
    near(offset3.endPoint(), {3, 2, 7});
    near(offset3.normal().y, 1);
    auto copied3 = offset3;
    offset3.setDistance(-2);
    near(offset3.startPoint(), {-1, 2, 3});
    near(copied3.startPoint(), {3, 2, 3});
    offset3.setNormal({0, -1, 0});
    near(offset3.startPoint(), {3, 2, 3});
    offset3.setDistance(0).setDomain(0.25, 0.75);
    near(offset3.startPoint(), {1, 2, 4});
    near(offset3.endPoint(), {1, 2, 6});
}

void circlesAndRotatedFrames()
{
    const AcGeCircArc2d circle({1, -2}, 4);
    const auto tangent = [](const AcGeCircArc2d &arc, double t) {
        const auto radial = arc.pointAtAngle(t) - arc.center();
        return AcGeVector2d{-radial.y, radial.x};
    };
    for (double distance : {-2.0, 0.0, 1.0, 4.0, 6.0})
    {
        AcGeOffsetCurve2d offset(
            [circle](double t) { return circle.pointAtAngle(t); },
            [circle, tangent](double t) { return tangent(circle, t); },
            0, 2 * pi, distance);
        for (int i = 0; i <= 16; ++i)
        {
            const double t = 2 * pi * i / 16;
            near(offset.pointAt(t),
                 {1 + (4 - distance) * std::cos(t),
                  -2 + (4 - distance) * std::sin(t)});
        }
        near(offset.startPoint(), offset.endPoint());
    }
    AcGeCircArc2d arc({0, 0}, 3, pi / 4, pi);
    AcGeOffsetCurve2d arcOffset(
        [arc](double t) { return arc.pointAtAngle(t); },
        [arc, tangent](double t) { return tangent(arc, t); }, pi / 4, pi, -1);
    near(arcOffset.startPoint(), {std::sqrt(8.0), std::sqrt(8.0)});
    near(arcOffset.endPoint(), {-4, 0});
    // Reverse parameter direction changes which side is left.
    AcGeOffsetCurve2d clockwise(
        [](double t) { return AcGePoint2d{3 * std::cos(t), -3 * std::sin(t)}; },
        [](double t) { return AcGeVector2d{-3 * std::sin(t), -3 * std::cos(t)}; },
        0, pi, 1);
    near(clockwise.pointAt(pi / 2), {0, -4});

    const AcGeVector3d n{1, 2, 3};
    AcGeCircArc3d tilted({2, -1, 4}, n, 5, 0.2, 2.3);
    const auto derivative = [](const AcGeCircArc3d &arc3, double t) {
        const auto u = arc3.referenceAxis();
        const auto v = arc3.normal.normal().crossProduct(u);
        return u * (-arc3.radius * std::sin(t)) +
               v * (arc3.radius * std::cos(t));
    };
    for (double distance : {-2.0, 0.0, 1.5})
    {
        AcGeOffsetCurve3d offset(tilted, derivative, 0.2, 2.3, distance, n);
        for (double t : {0.2, 0.7, 1.6, 2.3})
        {
            const auto radial = tilted.pointAt(t) - tilted.center;
            near(offset.pointAt(t),
                 tilted.center + radial * ((5 - distance) / 5));
            const auto delta = offset.pointAt(t) - tilted.pointAt(t);
            near(delta.length(), std::abs(distance));
            near(delta.dotProduct(n), 0);
            near(delta.dotProduct(derivative(tilted, t)), 0);
        }
    }
}

void splines()
{
    ge::AcGeBezierCurve2d bezier({{0, 0}, {1, 2}, {2, 0}});
    AcGeOffsetCurve2d offset(bezier, 0, 1, 0.5);
    near(offset.pointAt(0.5), {1, 1.5});
    near(offset.startPoint(), {-1 / std::sqrt(5.0), 0.5 / std::sqrt(5.0)});
    bezier.controlPoints[1] = {100, 100};
    near(offset.pointAt(0.5), {1, 1.5});

    ge::AcGeBsplineCurve2d spline(
        {0, 0, 0, 1, 1, 1}, {{0, 0}, {1, 2}, {2, 0}}, 2);
    AcGeOffsetCurve2d bspline(
        spline, [](const ge::AcGeBsplineCurve2d &curve, double t) {
            return curve.derivativesAt(t, 1).at(1);
        }, 0, 1, -0.5);
    near(bspline.pointAt(0.5), {1, 0.5});
    near(bspline.endPoint(), {2 - 1 / std::sqrt(5.0), -0.5 / std::sqrt(5.0)});

    ge::AcGeBezierCurve3d curve3({{0, 0, 2}, {1, 2, 2}, {2, 0, 2}});
    AcGeOffsetCurve3d offset3(curve3, 0, 1, 0.5, {0, 0, 1});
    near(offset3.pointAt(0.5), {1, 1.5, 2});
    curve3.controlPoints.clear();
    near(offset3.pointAt(0.5), {1, 1.5, 2});
}

void invalidTypedSources()
{
    throws<std::invalid_argument>([] {
        AcGeOffsetCurve2d bad(ge::AcGeBezierCurve2d{}, 0, 1, 0);
    });
    throws<std::invalid_argument>([] {
        AcGeOffsetCurve3d bad(ge::AcGeBezierCurve3d{}, 0, 1, 0, {0, 0, 1});
    });
    auto reject2d = [](auto curve) {
        throws<std::invalid_argument>([&] {
            AcGeOffsetCurve2d bad(
                curve, [](const auto &, double) { return AcGeVector2d{1, 0}; },
                0, 1, 0);
        });
    };
    auto reject3d = [](auto curve) {
        throws<std::invalid_argument>([&] {
            AcGeOffsetCurve3d bad(
                curve, [](const auto &, double) { return AcGeVector3d{1, 0, 0}; },
                0, 1, 0, {0, 0, 1});
        });
    };
    reject2d(ge::AcGeBsplineCurve2d{});
    reject2d(ge::AcGeNurbsCurve2d{});
    reject2d(ge::AcGeFitSpline2d{});
    reject3d(ge::AcGeBsplineCurve3d{});
    reject3d(ge::AcGeNurbsCurve3d{});
    reject3d(AcGeFitSpline3d{});

    ge::AcGeBsplineCurve3d malformed;
    malformed.controlPoints = {{0, 0, 0}};
    malformed.knots = {0, 0, 0, 1, 1};
    malformed.degree = 3;
    // The existing validator accepts this count equality, but evaluation
    // would index beyond the control points; the adapter must reject it.
    malformed.validate();
    reject3d(malformed);
    malformed.controlPoints = {{0, 0, 0}, {1, 0, 0}};
    malformed.knots = {0, 0, 1, 1};
    malformed.degree = 1;
    malformed.knots[2] = nan;
    reject3d(malformed);
    malformed.knots[2] = inf;
    reject3d(malformed);
    malformed.knots = {0, 1, 0, 1};
    reject3d(malformed);
    malformed.knots = {0, 0, 0, 0};
    reject3d(malformed);
    malformed.knots.clear();
    reject3d(malformed);
    malformed.degree = std::numeric_limits<std::size_t>::max();
    reject3d(malformed);

    ge::AcGeBsplineCurve2d malformed2(
        {0, 0, 1, 1}, {{0, 0}, {1, 0}}, 1);
    malformed2.knots.pop_back();
    reject2d(malformed2);
    ge::AcGeNurbsCurve3d rational(
        {0, 0, 1, 1}, {{0, 0, 0, 1}, {1, 0, 0, 1}}, 1);
    rational.controlPoints.clear();
    reject3d(rational);

    struct ValidatingLine : AnalyticLine2d
    {
        void validate() const { throw std::runtime_error("source validation"); }
    };
    throws<std::runtime_error>([] {
        AcGeOffsetCurve2d bad(ValidatingLine{}, 0, 1, 0);
    });
}

void invalidConfigurationAndStrongSetters()
{
    const auto p2 = [](double t) { return AcGePoint2d{t, 0}; };
    const auto t2 = [](double) { return AcGeVector2d{1, 0}; };
    for (const auto &[from, to] : {
             std::pair{0.0, 0.0}, {2.0, 1.0}, {-inf, 1.0}, {0.0, inf},
             {nan, 1.0}, {0.0, nan}})
        throws<std::invalid_argument>([&] {
            AcGeOffsetCurve2d bad(p2, t2, from, to, 1);
        });
    for (double distance : {nan, inf, -inf})
        throws<std::invalid_argument>([&] {
            AcGeOffsetCurve2d bad(p2, t2, 0, 1, distance);
        });
    throws<std::invalid_argument>([&] {
        AcGeOffsetCurve2d bad(AcGeOffsetCurve2d::PointFunction{}, t2, 0, 1, 0);
    });
    throws<std::invalid_argument>([&] {
        AcGeOffsetCurve2d bad(p2, AcGeOffsetCurve2d::TangentFunction{}, 0, 1, 0);
    });
    AcGeOffsetCurve2d offset(p2, t2, 0, 1, 2);
    throws<std::invalid_argument>([&] { offset.setDistance(nan); });
    throws<std::invalid_argument>([&] { offset.setDomain(3, 2); });
    near(offset.distance(), 2);
    near(offset.minParameter(), 0);
    near(offset.maxParameter(), 1);
    near(offset.startPoint(), {0, 2});
    for (double t : {-0.001, 1.001, nan, inf, -inf})
        throws<std::out_of_range>([&] { (void)offset.pointAt(t); });

    const auto p3 = [](double t) { return AcGePoint3d{t, 0, 0}; };
    const auto t3 = [](double) { return AcGeVector3d{1, 0, 0}; };
    for (AcGeVector3d normal : {
             AcGeVector3d{}, {nan, 0, 1}, {0, inf, 1}, {1, 0, -inf}})
        throws<std::invalid_argument>([&] {
            AcGeOffsetCurve3d bad(p3, t3, 0, 1, 0, normal);
        });
    AcGeOffsetCurve3d offset3(p3, t3, 0, 1, 2, {0, 0, 1});
    throws<std::invalid_argument>([&] { offset3.setNormal({0, 0, 0}); });
    throws<std::invalid_argument>([&] { offset3.setDistance(inf); });
    throws<std::invalid_argument>([&] { offset3.setDomain(-inf, 2); });
    near(offset3.distance(), 2);
    near(offset3.normal().z, 1);
    near(offset3.startPoint(), {0, 2, 0});
    for (double t : {-1.0, 2.0, nan, inf})
        throws<std::out_of_range>([&] { (void)offset3.pointAt(t); });
    throws<std::invalid_argument>([&] {
        AcGeOffsetCurve3d bad(p3, t3, 1, 0, 2, {0, 0, 1});
    });
    throws<std::invalid_argument>([&] {
        AcGeOffsetCurve3d bad(p3, t3, 0, 1, nan, {0, 0, 1});
    });
    throws<std::invalid_argument>([&] {
        AcGeOffsetCurve3d bad(p3, {}, 0, 1, 0, {0, 0, 1});
    });
}

void degeneracyAndExtremes()
{
    const auto p2 = [](double t) { return AcGePoint2d{t, 0}; };
    for (AcGeVector2d tangent : {AcGeVector2d{}, {nan, 1}, {1, inf}})
    {
        AcGeOffsetCurve2d offset(
            p2, [tangent](double) { return tangent; }, 0, 1, 1);
        throws<std::domain_error>([&] { (void)offset.startPoint(); });
        offset.setDistance(0);
        near(offset.pointAt(0.5), {0.5, 0});
    }
    AcGeOffsetCurve2d stationary(
        [](double t) { return AcGePoint2d{t * t, 0}; },
        [](double t) { return AcGeVector2d{2 * t, 0}; }, -1, 1, 1);
    near(stationary.pointAt(-0.5), {0.25, -1});
    near(stationary.pointAt(0.5), {0.25, 1});
    throws<std::domain_error>([&] { (void)stationary.pointAt(0); });
    const double huge = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    for (double scale : {huge, tiny})
    {
        AcGeOffsetCurve2d extreme(
            p2, [scale](double) { return AcGeVector2d{scale, scale}; }, 0, 1, 1);
        near(extreme.startPoint(), {-std::sqrt(0.5), std::sqrt(0.5)});
        AcGeOffsetCurve3d extreme3(
            [](double) { return AcGePoint3d{}; },
            [scale](double) { return AcGeVector3d{scale, 0, 0}; },
            0, 1, 2, {0, 0, scale});
        near(extreme3.startPoint(), {0, 2, 0});
    }
    AcGeOffsetCurve2d overflow(
        [huge](double) { return AcGePoint2d{huge, 0}; },
        [](double) { return AcGeVector2d{0, -1}; }, 0, 1, huge);
    throws<std::domain_error>([&] { (void)overflow.startPoint(); });
    AcGeOffsetCurve2d badPoint(
        [](double) { return AcGePoint2d{nan, 0}; },
        [](double) { return AcGeVector2d{1, 0}; }, 0, 1, 0);
    throws<std::domain_error>([&] { (void)badPoint.startPoint(); });

    const auto p3 = [](double) { return AcGePoint3d{1, 2, 3}; };
    for (AcGeVector3d tangent : {
             AcGeVector3d{}, {0, 0, 1}, {0, 0, -1},
             {1e-16, 0, 1}, {nan, 0, 1}, {1, inf, 0}})
    {
        AcGeOffsetCurve3d offset(
            p3, [tangent](double) { return tangent; }, 0, 1, 1, {0, 0, 1});
        throws<std::domain_error>([&] { (void)offset.startPoint(); });
        offset.setDistance(0);
        near(offset.startPoint(), {1, 2, 3});
    }
    AcGeOffsetCurve3d nonplanar(
        [](double t) { return AcGePoint3d{t, 0, t}; },
        [](double) { return AcGeVector3d{1, 0, 1}; }, 0, 1, 2, {0, 0, 1});
    near(nonplanar.endPoint(), {1, 2, 1});
    AcGeOffsetCurve3d bad3(
        [](double) { return AcGePoint3d{0, inf, 0}; },
        [](double) { return AcGeVector3d{1, 0, 0}; }, 0, 1, 0, {0, 0, 1});
    throws<std::domain_error>([&] { (void)bad3.startPoint(); });
    AcGeOffsetCurve3d overflow3(
        [huge](double) { return AcGePoint3d{0, huge, 0}; },
        [](double) { return AcGeVector3d{1, 0, 0}; }, 0, 1, huge, {0, 0, 1});
    throws<std::domain_error>([&] { (void)overflow3.startPoint(); });
}

void callbackContracts()
{
    int pointCalls = 0;
    int tangentCalls = 0;
    AcGeOffsetCurve2d offset(
        [&pointCalls](double t) {
            ++pointCalls;
            return AcGePoint2d{t, 0};
        },
        [&tangentCalls](double) -> AcGeVector2d {
            ++tangentCalls;
            throw std::runtime_error("source tangent failure");
        }, 0, 1, 0);
    assert(pointCalls == 0 && tangentCalls == 0);
    throws<std::out_of_range>([&] { (void)offset.pointAt(2); });
    assert(pointCalls == 0 && tangentCalls == 0);
    near(offset.startPoint(), {0, 0});
    assert(pointCalls == 1 && tangentCalls == 0);
    offset.setDistance(1);
    throws<std::runtime_error>([&] { (void)offset.startPoint(); });
    near(offset.distance(), 1);
    assert(pointCalls == 2 && tangentCalls == 1);
    offset.setDistance(0);
    near(offset.endPoint(), {1, 0});
    auto moved = std::move(offset);
    near(moved.endPoint(), {1, 0});

    AcGeOffsetCurve2d source(ThrowingCopyLine2d{}, -2, 2, 4);
    AcGeOffsetCurve2d target(AnalyticLine2d{}, 0, 1, 0);
    ThrowingCopyLine2d::failCopies = true;
    throws<std::runtime_error>([&] { target = source; });
    ThrowingCopyLine2d::failCopies = false;
    near(target.distance(), 0);
    near(target.minParameter(), 0);
    near(target.maxParameter(), 1);
    near(target.startPoint(), {1, 2});
    near(source.startPoint(), {-8.2, -3.6});

    int tangent3Calls = 0;
    AcGeOffsetCurve3d zero3(
        [](double t) { return AcGePoint3d{t, 2, 3}; },
        [&tangent3Calls](double) -> AcGeVector3d {
            ++tangent3Calls;
            throw std::runtime_error("source tangent failure");
        }, 0, 1, 0, {0, 0, 1});
    near(zero3.endPoint(), {1, 2, 3});
    assert(tangent3Calls == 0);
    AcGeOffsetCurve2d throwingPoint(
        [](double) -> AcGePoint2d {
            throw std::runtime_error("source point failure");
        },
        [](double) { return AcGeVector2d{1, 0}; }, 0, 1, 1);
    throws<std::runtime_error>([&] { (void)throwingPoint.startPoint(); });
}
} // namespace

int main()
{
    static_assert(!std::is_default_constructible_v<AcGeOffsetCurve2d>);
    static_assert(!std::is_default_constructible_v<AcGeOffsetCurve3d>);
    linesAndOwnership();
    circlesAndRotatedFrames();
    splines();
    invalidTypedSources();
    invalidConfigurationAndStrongSetters();
    degeneracyAndExtremes();
    callbackContracts();
    std::cout << "AcGeOffsetCurve tests passed\n";
}
