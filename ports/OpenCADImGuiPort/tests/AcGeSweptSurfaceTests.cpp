#include "acge/AcGeSweptSurface.h"
#include "acge/AcGeCurve3d.h"
#include "acge/AcGeBspline.h"
#include "acge/AcGeNurbsSpline.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
const AcGeInterval unitDomain{0.0, 1.0};
const AcGeInterval angles{-pi, pi};

struct Line
{
    AcGePoint3d origin;
    AcGeVector3d tangent;
    AcGePoint3d pointAt(double u) const { return origin + tangent * u; }
    AcGeVector3d derivativeAt(double) const { return tangent; }
};

struct Circle
{
    AcGePoint3d pointAt(double u) const { return {2 * std::cos(u), 2 * std::sin(u), 0}; }
    AcGeVector3d derivativeAt(double u) const { return {-2 * std::sin(u), 2 * std::cos(u), 0}; }
};

template <class Exception, class F>
void throws(F &&operation)
{
    bool caught = false;
    try { operation(); }
    catch (const Exception &) { caught = true; }
    assert(caught);
}

void near(const AcGeVector3d &actual, const AcGeVector3d &expected, double tol = 1e-9)
{
    assert(std::hypot(actual.x - expected.x, actual.y - expected.y,
                      actual.z - expected.z) < tol);
}

void near(const AcGePoint3d &actual, const AcGePoint3d &expected, double tol = 1e-9)
{
    near(actual - expected, AcGeVector3d{}, tol);
}

template <class Surface>
void derivatives(const Surface &surface, double u, double v)
{
    constexpr double h = 1e-6;
    const auto du = surface.partialU(u, v);
    const auto dv = surface.partialV(u, v);
    near(du, (surface.pointAt(u + h, v) - surface.pointAt(u - h, v)) / (2 * h), 1e-8);
    near(dv, (surface.pointAt(u, v + h) - surface.pointAt(u, v - h)) / (2 * h), 1e-8);
    const auto normal = surface.normalAt(u, v);
    near(normal, du.crossProduct(dv).normal());
    assert(std::abs(normal.dotProduct(du)) < 1e-10);
    assert(std::abs(normal.dotProduct(dv)) < 1e-10);
}

AcGeSweptCurve3d line(AcGePoint3d origin, AcGeVector3d tangent,
                      const AcGeInterval &domain = unitDomain)
{
    return {Line{origin, tangent}, domain};
}

void geometry()
{
    auto a = line({0, 0, 0}, {2, 0, 0});
    auto b = line({0, 3, 0}, {2, 0, 0});
    AcGeRuledSurface3d ruled(a, b, unitDomain);
    near(ruled.pointAt(0.25, 0.5), {0.5, 1.5, 0});
    near(ruled.pointAt(0, 0), a.pointAt(0));
    near(ruled.pointAt(1, 1), b.pointAt(1));
    near(ruled.normalAt(0.4, 0.6), AcGeVector3d{0, 0, 1});
    derivatives(ruled, 0.4, 0.6);
    derivatives(AcGeRuledSurface3d(a, line({0, 1, 1}, {3, 2, 1}), unitDomain), 0.2, 0.7);
    AcGeRuledSurface3d reversed(b, a, unitDomain);
    near(reversed.normalAt(0.4, 0.6), AcGeVector3d{0, 0, -1});

    AcGeExtrudedSurface3d plane(a, {0, 3, 0}, {-2.0, 4.0});
    near(plane.pointAt(0.25, -1), {0.5, -3, 0});
    near(plane.normalAt(0.4, 0.6), AcGeVector3d{0, 0, 1});
    near(plane.direction(), AcGeVector3d{0, 3, 0});
    derivatives(plane, 0.4, 0.6);
    AcGeExtrudedSurface3d cylinder({Circle{}, angles}, {0, 0, 3}, {-1.0, 2.0});
    near(cylinder.pointAt(pi / 2, 1), {0, 2, 3});
    near(cylinder.normalAt(0, 1), AcGeVector3d{1, 0, 0});
    derivatives(cylinder, 0.4, 0.6);

    AcGeRevolvedSurface3d revolution(line({2, 0, 0}, {0, 0, 3}), {}, {0, 0, 7}, angles);
    near(revolution.pointAt(0.5, pi / 2), {0, 2, 1.5});
    near(revolution.normalAt(0.5, 0), AcGeVector3d{-1, 0, 0});
    derivatives(revolution, 0.4, 0.6);
    AcGeRevolvedSurface3d cone(line({}, {1, 0, 2}), {}, {0, 0, 1}, angles);
    near(cone.pointAt(0.5, pi / 2), {0, 0.5, 1});
    near(cone.normalAt(0.5, 0), AcGeVector3d{-2, 0, 1}.normal());
    derivatives(cone, 0.4, 0.6);

    const AcGePoint3d origin{3, -4, 2};
    const auto axis = AcGeVector3d{1, 2, 3}.normal();
    const auto radial = axis.perpVector();
    const auto side = axis.crossProduct(radial);
    AcGeRevolvedSurface3d arbitrary(
        line(origin + radial * 2, axis * 3), origin, axis * 19, angles);
    near(arbitrary.axisOrigin(), origin);
    near(arbitrary.axisDirection(), axis);
    near(arbitrary.pointAt(0.5, pi / 2), origin + side * 2 + axis * 1.5);
    derivatives(arbitrary, 0.2, -0.8);
    AcGeRevolvedSurface3d negative(
        line(origin + radial * 2, axis * 3), origin, -axis, angles);
    near(negative.pointAt(0.5, -pi / 2), arbitrary.pointAt(0.5, pi / 2));

    assert(ruled.uDomain().lowerBound() == 0 && ruled.vDomain().upperBound() == 1);
    assert(plane.vDomain().lowerBound() == -2 && cylinder.uDomain().lowerBound() == -pi);
    near(ruled.firstCurve().pointAt(0.4), a.pointAt(0.4));
    near(ruled.secondCurve().pointAt(0.4), b.pointAt(0.4));
    near(plane.sourceCurve().derivativeAt(0.4), AcGeVector3d{2, 0, 0});
    near(revolution.sourceCurve().pointAt(0), {2, 0, 0});
}

void ownershipAndCallbacks()
{
    Line source{{1, 2, 3}, {2, 0, 0}};
    AcGeSweptCurve3d saved(source, unitDomain);
    source.origin = {100, 200, 300};
    near(saved.pointAt(0.5), {2, 2, 3});
    auto surface = [] {
        const std::vector<double> data{2, 3};
        AcGeSweptCurve3d owned(
            [data](double u) { return AcGePoint3d{data[0] * u, data[1], 0}; },
            unitDomain, [data](double) { return AcGeVector3d{data[0], 0, 0}; });
        return AcGeExtrudedSurface3d(owned, {0, 0, 4}, unitDomain);
    }();
    auto copy = surface;
    surface.set(line({}, {1, 0, 0}), {0, 1, 0}, unitDomain);
    near(copy.pointAt(0.5, 0.5), {1, 3, 2});
    derivatives(copy, 0.4, 0.6);

    const AcGeLineSeg3d segment(AcGePoint3d{0, 0, 0}, AcGePoint3d{2, 0, 0});
    AcGeExtrudedSurface3d pointOnly({segment, unitDomain}, {0, 1, 0}, unitDomain);
    near(pointOnly.pointAt(0.5, 0.5), {1, 0.5, 0});
    assert(!pointOnly.sourceCurve().hasDerivative());
    throws<std::logic_error>([&] { pointOnly.partialU(0.5, 0.5); });
    throws<std::logic_error>([&] { pointOnly.normalAt(0.5, 0.5); });
    AcGeExtrudedSurface3d adapted(
        {[segment](double u) { return segment.pointAt(u); }, unitDomain,
         [segment](double) { return segment.direction(); }},
        {0, 1, 0}, unitDomain);
    derivatives(adapted, 0.4, 0.6);
}

void invalidBuiltInSources()
{
    throws<std::invalid_argument>([] {
        AcGeSweptCurve3d bad(ge::AcGeBezierCurve3d{}, unitDomain);
    });
    ge::AcGeBezierCurve3d bezier({{0, 0, 0}, {2, 0, 0}});
    AcGeSweptCurve3d owned(bezier, unitDomain);
    bezier.controlPoints.clear();
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(bezier, unitDomain); });
    near(owned.pointAt(0.5), {1, 0, 0});
    near(owned.derivativeAt(0.5), AcGeVector3d{2, 0, 0});
    throws<std::invalid_argument>([] {
        AcGeSweptCurve3d bad(ge::AcGeBsplineCurve3d{}, unitDomain);
    });
    throws<std::invalid_argument>([] {
        AcGeSweptCurve3d bad(ge::AcGeNurbsCurve3d{}, unitDomain);
    });
    throws<std::invalid_argument>([] {
        AcGeSweptCurve3d bad(AcGeFitSpline3d{}, unitDomain);
    });
    ge::AcGeBsplineCurve3d spline({0, 0, 1, 1}, {{0, 0, 0}, {2, 0, 0}}, 1);
    AcGeSweptCurve3d valid(spline, unitDomain);
    near(valid.pointAt(0.5), {1, 0, 0});
    spline.knots.clear();
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(spline, unitDomain); });
    ge::AcGeNurbsCurve3d nurbs({0, 0, 1, 1}, {{0, 0, 0, 1}, {2, 0, 0, 1}}, 1);
    nurbs.knots.clear();
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(nurbs, unitDomain); });

    ge::AcGeBsplineCurve3d tooFew;
    tooFew.degree = 3;
    tooFew.controlPoints = {{0, 0, 0}};
    tooFew.knots = {0, 0, 0, 1, 1};
    tooFew.validate(); // The old built-in validator accepts this unsafe shape.
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(tooFew, unitDomain); });
    ge::AcGeNurbsCurve3d rationalTooFew;
    rationalTooFew.degree = 3;
    rationalTooFew.controlPoints = {{0, 0, 0, 1}};
    rationalTooFew.knots = tooFew.knots;
    rationalTooFew.validate();
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(rationalTooFew, unitDomain); });

    const auto validSpline = ge::AcGeBsplineCurve3d(
        {0, 0, 1, 1}, {{0, 0, 0}, {2, 0, 0}}, 1);
    for (const std::vector<double> &knots : {
             std::vector<double>{0, 1, 0, 1}, std::vector<double>{0, 0, 0, 0},
             std::vector<double>{0, 0, 1, std::numeric_limits<double>::infinity()},
             std::vector<double>{0, 0, std::numeric_limits<double>::quiet_NaN(), 1}})
    {
        auto malformed = validSpline;
        malformed.knots = knots;
        throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(malformed, unitDomain); });
    }
    throws<std::invalid_argument>([&] {
        AcGeSweptCurve3d bad(validSpline, {-0.5, 1.0});
    });
    throws<std::invalid_argument>([&] {
        AcGeSweptCurve3d bad(validSpline, {0.0, 1.5});
    });
}

struct ThrowingDerivative
{
    std::shared_ptr<bool> fail;
    explicit ThrowingDerivative(std::shared_ptr<bool> flag) : fail(std::move(flag)) {}
    ThrowingDerivative(const ThrowingDerivative &other) : fail(other.fail)
    {
        if (*fail)
            throw std::runtime_error("Intentional derivative copy failure.");
    }
    AcGeVector3d operator()(double) const { return {3, 0, 0}; }
};

void transactionalAssignments()
{
    auto fail = std::make_shared<bool>(false);
    AcGeSweptCurve3d source(
        [](double u) { return AcGePoint3d{3 * u, 4, 5}; },
        {0.0, 2.0}, ThrowingDerivative(fail));
    auto original = line({1, 2, 3}, {2, 0, 0});
    auto target = original;
    AcGeExtrudedSurface3d extrusion(original, {0, 1, 0}, unitDomain);
    AcGeExtrudedSurface3d extrusionSource(source, {0, 0, 7}, {-1.0, 2.0});
    AcGeRevolvedSurface3d revolution(original, {}, {0, 0, 1}, unitDomain);
    AcGeRevolvedSurface3d revolutionSource(source, {1, 2, 3}, {0, 1, 0}, angles);
    AcGeRuledSurface3d ruled(original, line({1, 3, 3}, {2, 0, 0}), unitDomain);
    AcGeRuledSurface3d ruledSource(
        line({8, 9, 10}, {1, 0, 0}, {0.0, 2.0}), source, {0.2, 0.8});
    *fail = true;
    throws<std::runtime_error>([&] { target = source; });
    throws<std::runtime_error>([&] { extrusion = extrusionSource; });
    throws<std::runtime_error>([&] { revolution = revolutionSource; });
    // The first ruled source copies successfully before its second throws.
    throws<std::runtime_error>([&] { ruled = ruledSource; });
    near(target.pointAt(0.5), {2, 2, 3});
    near(target.derivativeAt(0.5), AcGeVector3d{2, 0, 0});
    assert(target.domain().upperBound() == 1);
    near(extrusion.pointAt(0.5, 0.5), {2, 2.5, 3});
    near(extrusion.partialU(0.5, 0.5), AcGeVector3d{2, 0, 0});
    near(extrusion.direction(), AcGeVector3d{0, 1, 0});
    assert(extrusion.uDomain().upperBound() == 1 && extrusion.vDomain().lowerBound() == 0);
    near(revolution.pointAt(0.5, 0), {2, 2, 3});
    near(revolution.partialU(0.5, 0), AcGeVector3d{2, 0, 0});
    near(revolution.axisOrigin(), AcGePoint3d{});
    near(revolution.axisDirection(), AcGeVector3d{0, 0, 1});
    assert(revolution.uDomain().upperBound() == 1 && revolution.vDomain().lowerBound() == 0);
    near(ruled.pointAt(0.5, 0.5), {2, 2.5, 3});
    near(ruled.partialU(0.5, 0.5), AcGeVector3d{2, 0, 0});
    assert(ruled.uDomain().upperBound() == 1 && ruled.vDomain().lowerBound() == 0);
    *fail = false;
    target = source;
    near(target.pointAt(1.5), source.pointAt(1.5));
    near(target.derivativeAt(1.5), source.derivativeAt(1.5));
    target = target;
    extrusion = extrusionSource;
    revolution = revolutionSource;
    ruled = ruledSource;
    near(extrusion.pointAt(1.5, 0.5), extrusionSource.pointAt(1.5, 0.5));
    near(revolution.pointAt(1.5, 0.5), revolutionSource.pointAt(1.5, 0.5));
    near(ruled.pointAt(1.5, 0.5), ruledSource.pointAt(1.5, 0.5));
}

void invalidAndSingular()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const double huge = std::numeric_limits<double>::max();
    auto a = line({}, {1, 0, 0});
    auto b = line({0, 1, 0}, {1, 0, 0});
    for (const auto &domain : {AcGeInterval{}, AcGeInterval{0.0, 0.0},
                              AcGeInterval{nan, 1.0}, AcGeInterval{0.0, inf},
                              AcGeInterval{-huge, huge}, AcGeInterval{0.0, 1.0, nan}})
    {
        throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(Line{{}, {1, 0, 0}}, domain); });
        throws<std::invalid_argument>([&] { AcGeExtrudedSurface3d bad(a, {0, 1, 0}, domain); });
        throws<std::invalid_argument>([&] { AcGeRevolvedSurface3d bad(a, {}, {0, 0, 1}, domain); });
        throws<std::invalid_argument>([&] { AcGeRuledSurface3d bad(a, b, domain); });
    }
    for (const AcGeVector3d vector : {AcGeVector3d{}, AcGeVector3d{nan, 1, 0},
                                      AcGeVector3d{0, inf, 0}})
    {
        throws<std::invalid_argument>([&] { AcGeExtrudedSurface3d bad(a, vector, unitDomain); });
        throws<std::invalid_argument>([&] { AcGeRevolvedSurface3d bad(a, {}, vector, angles); });
    }
    throws<std::invalid_argument>([&] { AcGeRevolvedSurface3d bad(a, {inf, 0, 0}, {0, 0, 1}, angles); });
    throws<std::invalid_argument>([&] { AcGeRuledSurface3d bad(a, b, {-1.0, 1.0}); });
    throws<std::invalid_argument>([&] { AcGeRuledSurface3d bad(a, line({}, {1, 0, 0}, {0.0, 2.0}), unitDomain); });
    throws<std::invalid_argument>([&] { AcGeSweptCurve3d bad(AcGeSweptCurve3d::PointFunction{}, unitDomain); });
    throws<std::domain_error>([&] { line({nan, 0, 0}, {1, 0, 0}); });
    throws<std::domain_error>([&] { line({}, {inf, 0, 0}); });
    throws<std::domain_error>([&] {
        AcGeSweptCurve3d bad([](double) { return AcGePoint3d{}; }, unitDomain,
                            [nan](double) { return AcGeVector3d{nan, 0, 0}; });
    });
    AcGeSweptCurve3d lateBad([nan](double u) {
        return AcGePoint3d{u == 0.25 ? nan : u, 0, 0};
    }, unitDomain);
    throws<std::domain_error>([&] { lateBad.pointAt(0.25); });
    AcGeSweptCurve3d lateBadDerivative(
        [](double u) { return AcGePoint3d{u, 0, 0}; }, unitDomain,
        [nan](double u) { return AcGeVector3d{u == 0.25 ? nan : 1, 0, 0}; });
    throws<std::domain_error>([&] { lateBadDerivative.derivativeAt(0.25); });

    AcGeExtrudedSurface3d extrusion(a, {0, 1, 0}, unitDomain);
    AcGeRuledSurface3d ruled(a, b, unitDomain);
    AcGeRevolvedSurface3d revolved(b, {}, {1, 0, 0}, unitDomain);
    for (double invalid : {nan, inf, -inf, -0.1, 1.1})
    {
        auto check = [&](const auto &surface) {
            throws<std::out_of_range>([&] { surface.pointAt(invalid, 0.5); });
            throws<std::out_of_range>([&] { surface.pointAt(0.5, invalid); });
            throws<std::out_of_range>([&] { surface.partialU(invalid, 0.5); });
            throws<std::out_of_range>([&] { surface.partialU(0.5, invalid); });
            throws<std::out_of_range>([&] { surface.partialV(invalid, 0.5); });
            throws<std::out_of_range>([&] { surface.partialV(0.5, invalid); });
            throws<std::out_of_range>([&] { surface.normalAt(invalid, 0.5); });
            throws<std::out_of_range>([&] { surface.normalAt(0.5, invalid); });
        };
        check(extrusion); check(ruled); check(revolved);
    }
    throws<std::invalid_argument>([&] { extrusion.set(b, {}, unitDomain); });
    throws<std::invalid_argument>([&] { ruled.set(b, a, {-1.0, 1.0}); });
    throws<std::invalid_argument>([&] { revolved.set(a, {}, {}, angles); });
    near(extrusion.pointAt(0.5, 0.5), {0.5, 0.5, 0});
    near(ruled.pointAt(0.5, 0.5), {0.5, 0.5, 0});
    near(revolved.pointAt(0.5, 0), {0.5, 1, 0});
    ruled.set(b, a, unitDomain);
    near(ruled.normalAt(0.5, 0.5), AcGeVector3d{0, 0, -1});
    revolved.set(line({2, 0, 0}, {0, 0, 1}), {}, {0, 0, 1}, angles);
    near(revolved.pointAt(1, pi), {-2, 0, 1});

    throws<std::domain_error>([&] { AcGeRuledSurface3d(a, a, unitDomain).normalAt(0.5, 0.5); });
    throws<std::domain_error>([&] { AcGeExtrudedSurface3d(a, {2, 0, 0}, unitDomain).normalAt(0.5, 0.5); });
    throws<std::domain_error>([&] { AcGeExtrudedSurface3d(line({}, {}), {0, 1, 0}, unitDomain).normalAt(0.5, 0.5); });
    throws<std::domain_error>([&] { AcGeRevolvedSurface3d(a, {}, {0, 0, 1}, angles).normalAt(0, 0); });
    throws<std::domain_error>([&] {
        AcGeRevolvedSurface3d(line({}, {0, 0, 1}), {}, {0, 0, 1}, angles).normalAt(0.5, 0.5);
    });
    const AcGePoint3d shifted{3, -4, 5};
    const AcGeVector3d arbitraryAxis{1, 2, 3};
    throws<std::domain_error>([&] {
        AcGeRevolvedSurface3d(line(shifted, arbitraryAxis), shifted, arbitraryAxis, angles)
            .normalAt(0.5, 0.7);
    });
    throws<std::domain_error>([&] {
        AcGeRevolvedSurface3d(line({2, 0, 0}, {}), {}, {0, 0, 1}, angles).normalAt(0.5, 0.5);
    });
    throws<std::domain_error>([&] {
        AcGeRuledSurface3d(a, line({}, {-1, 0, 0}), unitDomain).normalAt(0.5, 0.5);
    });
    throws<std::domain_error>([&] {
        AcGeExtrudedSurface3d(a, {1, 1e-16, 0}, unitDomain).normalAt(0.5, 0.5);
    });
    for (double scale : {1e-300, 1e300})
    {
        AcGeExtrudedSurface3d scaled(line({}, {scale, 0, 0}), {0, scale, 0}, unitDomain);
        near(scaled.normalAt(0.5, 0.5), AcGeVector3d{0, 0, 1});
        AcGeRevolvedSurface3d axisScale(b, {}, {scale, 0, 0}, angles);
        near(axisScale.axisDirection(), AcGeVector3d{1, 0, 0});
    }
    AcGeExtrudedSurface3d overflow(line({huge, 0, 0}, {}), {huge, 0, 0}, {0.0, 2.0});
    throws<std::domain_error>([&] { overflow.pointAt(0.5, 2); });
}
}

int main()
{
    geometry();
    ownershipAndCallbacks();
    invalidBuiltInSources();
    transactionalAssignments();
    invalidAndSingular();
    std::cout << "AcGeSweptSurfaceTests: all assertions passed\n";
}
