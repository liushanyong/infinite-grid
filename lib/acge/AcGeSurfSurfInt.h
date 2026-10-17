#pragma once

#include "AcGePort.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGeInterval.h"
#include "AcGePoint.h"
#include "AcGeTol.h"

// Sampled AcGeSurfSurfInt subset. Input surfaces are copied into point
// evaluators; intersection curves are returned as sampled polylines because
// this value-type port deliberately has no AcGeEntity3d/AcGeCurve3d hierarchy.
// The marching grid detects transverse intersections of finite parameter
// domains. Coincident regions and tangent components are not classified.
class ACGE_PORT AcGeSurfSurfInt
{
public:
    AcGeSurfSurfInt() = default;

    template <class Surface1, class Surface2>
    AcGeSurfSurfInt(const Surface1 &surface1, const Surface2 &surface2,
                    const AcGeTol &tol = AcGeTol{})
    {
        set(surface1, surface2, tol);
    }

    template <class Surface1, class Surface2>
    AcGeSurfSurfInt &set(const Surface1 &surface1, const Surface2 &surface2,
                         const AcGeTol &tol = AcGeTol{})
    {
        m_surface1 = {};
        m_surface2 = {};
        m_results.clear();
        m_surface1.point = [surface1](double u, double v)
        { return AcGePoint3d(surface1.pointAt(u, v)); };
        m_surface2.point = [surface2](double u, double v)
        { return AcGePoint3d(surface2.pointAt(u, v)); };
        domainOf(surface1, m_domain1);
        domainOf(surface2, m_domain2);
        const double requestedTolerance = tol.equalPoint();
        m_tolerance = std::isfinite(requestedTolerance) &&
                              requestedTolerance > 0.0
                          ? requestedTolerance
                          : 1.0e-7;
        validateDomains();
        compute();
        return *this;
    }

    int numResults() const { return static_cast<int>(m_results.size()); }

    int getDimension(int index) const
    {
        return checkedResult(index).dimension;
    }

    AcGePoint3d intPoint(int index) const
    {
        const Result &result = checkedResult(index);
        if (result.dimension != 0)
            throw std::logic_error(
                "SurfSurfInt result is not a point intersection.");
        return result.point;
    }

    std::vector<AcGePoint3d> intCurve(int index) const
    {
        const Result &result = checkedResult(index);
        if (result.dimension != 1)
            throw std::logic_error(
                "SurfSurfInt result is not a curve intersection.");
        return result.curve;
    }

    std::vector<AcGePoint2d> intParamCurve(int index, bool isFirst) const
    {
        const Result &result = checkedResult(index);
        if (result.dimension != 1)
            throw std::logic_error(
                "SurfSurfInt result is not a curve intersection.");
        return isFirst ? result.params1 : result.params2;
    }

    AcGePoint2d intPointParam1(int index) const
    {
        return checkedResult(index).param1;
    }

    AcGePoint2d intPointParam2(int index) const
    {
        return checkedResult(index).param2;
    }

    double tolerance() const { return m_tolerance; }

private:
    static constexpr int kGridSize = 48;

    struct Domain
    {
        double uLow = 0.0;
        double uHigh = 1.0;
        double vLow = 0.0;
        double vHigh = 1.0;
    };

    struct Sample
    {
        AcGePoint3d point{};
        AcGePoint2d param1{};
        AcGePoint2d param2{};
        double signedDistance = 0.0;
        double distance = 0.0;
        bool valid = false;
    };

    struct Vertex
    {
        AcGePoint3d point{};
        AcGePoint2d param1{};
        AcGePoint2d param2{};
    };

    struct Result
    {
        int dimension = 1;
        AcGePoint3d point{};
        AcGePoint2d param1{};
        AcGePoint2d param2{};
        std::vector<AcGePoint3d> curve;
        std::vector<AcGePoint2d> params1;
        std::vector<AcGePoint2d> params2;
    };

    struct SurfaceView
    {
        std::function<AcGePoint3d(double, double)> point;
        AcGePoint3d at(double u, double v) const { return point(u, v); }
    };

    SurfaceView m_surface1;
    SurfaceView m_surface2;
    Domain m_domain1{};
    Domain m_domain2{};
    std::vector<Result> m_results;
    double m_tolerance = 0.0;

    template <class Surface>
    static void domainOf(const Surface &surface, Domain &domain)
    {
        if constexpr (requires {
                          { surface.minParameterU() } -> std::convertible_to<double>;
                          { surface.maxParameterU() } -> std::convertible_to<double>;
                          { surface.minParameterV() } -> std::convertible_to<double>;
                          { surface.maxParameterV() } -> std::convertible_to<double>;
                      })
        {
            domain = {surface.minParameterU(), surface.maxParameterU(),
                      surface.minParameterV(), surface.maxParameterV()};
        }
        else if constexpr (requires {
                               { surface.uRange().lowerBound() } -> std::convertible_to<double>;
                               { surface.uRange().upperBound() } -> std::convertible_to<double>;
                               { surface.vRange().lowerBound() } -> std::convertible_to<double>;
                               { surface.vRange().upperBound() } -> std::convertible_to<double>;
                           })
        {
            const auto uRange = surface.uRange();
            const auto vRange = surface.vRange();
            domain = {uRange.lowerBound(), uRange.upperBound(),
                      vRange.lowerBound(), vRange.upperBound()};
        }
        else if constexpr (requires {
                               surface.getAnglesInU(std::declval<double &>(),
                                                    std::declval<double &>());
                               surface.getAnglesInV(std::declval<double &>(),
                                                    std::declval<double &>());
                           })
        {
            surface.getAnglesInU(domain.uLow, domain.uHigh);
            surface.getAnglesInV(domain.vLow, domain.vHigh);
        }
        else if constexpr (requires {
                               surface.getAngles(std::declval<double &>(),
                                                 std::declval<double &>());
                               surface.getHeight(std::declval<AcGeInterval &>());
                           })
        {
            surface.getAngles(domain.uLow, domain.uHigh);
            AcGeInterval height;
            surface.getHeight(height);
            domain.vLow = height.lowerBound();
            domain.vHigh = height.upperBound();
        }
    }

    void validateDomains() const
    {
        const Domain domains[] = {m_domain1, m_domain2};
        for (const Domain &domain : domains)
        {
            if (!std::isfinite(domain.uLow) || !std::isfinite(domain.uHigh) ||
                !std::isfinite(domain.vLow) || !std::isfinite(domain.vHigh) ||
                !(domain.uLow < domain.uHigh) ||
                !(domain.vLow < domain.vHigh))
                throw std::invalid_argument(
                    "SurfSurfInt requires finite increasing parameter ranges.");
        }
    }

    static double step(double u, double v)
    {
        return 1.0e-5 * std::max({1.0, std::abs(u), std::abs(v)});
    }

    static void partials(const SurfaceView &surface, double u, double v,
                         AcGeVector3d &su, AcGeVector3d &sv)
    {
        const double h = step(u, v);
        su = (surface.at(u + h, v) - surface.at(u - h, v)) * (0.5 / h);
        sv = (surface.at(u, v + h) - surface.at(u, v - h)) * (0.5 / h);
    }

    double project(const AcGePoint3d &point, double &u, double &v) const
    {
        for (int iteration = 0; iteration < 24; ++iteration)
        {
            AcGeVector3d su;
            AcGeVector3d sv;
            partials(m_surface2, u, v, su, sv);
            const AcGeVector3d delta = m_surface2.at(u, v) - point;
            const double g11 = su.dotProduct(su);
            const double g12 = su.dotProduct(sv);
            const double g22 = sv.dotProduct(sv);
            const double determinant = g11 * g22 - g12 * g12;
            if (!(determinant > 1.0e-30))
                break;
            const double b1 = delta.dotProduct(su);
            const double b2 = delta.dotProduct(sv);
            const double du = (g12 * b2 - g22 * b1) / determinant;
            const double dv = (g12 * b1 - g11 * b2) / determinant;
            if (!std::isfinite(du) || !std::isfinite(dv))
                break;
            const double nextU = std::clamp(u + du, m_domain2.uLow,
                                            m_domain2.uHigh);
            const double nextV = std::clamp(v + dv, m_domain2.vLow,
                                            m_domain2.vHigh);
            if (nextU == u && nextV == v)
                break;
            u = nextU;
            v = nextV;
            if (std::abs(du) + std::abs(dv) <= 1.0e-12)
                break;
        }
        return (m_surface2.at(u, v) - point).length();
    }

    Sample evaluate(double u, double v) const
    {
        Sample sample;
        sample.param1 = {u, v};
        sample.point = m_surface1.at(u, v);
        const double uFraction =
            (u - m_domain1.uLow) / (m_domain1.uHigh - m_domain1.uLow);
        const double vFraction =
            (v - m_domain1.vLow) / (m_domain1.vHigh - m_domain1.vLow);
        double u2 = m_domain2.uLow +
                    uFraction * (m_domain2.uHigh - m_domain2.uLow);
        double v2 = m_domain2.vLow +
                    vFraction * (m_domain2.vHigh - m_domain2.vLow);
        sample.distance = project(sample.point, u2, v2);
        AcGeVector3d su;
        AcGeVector3d sv;
        partials(m_surface2, u2, v2, su, sv);
        const AcGeVector3d normal = su.crossProduct(sv);
        const double normalLength = normal.length();
        if (!(normalLength > 1.0e-14) || !std::isfinite(normalLength) ||
            !std::isfinite(sample.distance))
            return sample;
        sample.param2 = {u2, v2};
        sample.signedDistance =
            (sample.point - m_surface2.at(u2, v2)).dotProduct(normal) /
            normalLength;
        sample.valid = std::isfinite(sample.signedDistance);
        return sample;
    }

    static Vertex interpolate(const Sample &a, const Sample &b)
    {
        const double denominator = a.signedDistance - b.signedDistance;
        const double t = denominator == 0.0
                             ? 0.5
                             : std::clamp(a.signedDistance / denominator,
                                          0.0, 1.0);
        const AcGePoint2d p1 = a.param1 + (b.param1 - a.param1) * t;
        const AcGePoint2d p2 = a.param2 + (b.param2 - a.param2) * t;
        const AcGePoint3d point1 = a.point + (b.point - a.point) * t;
        return {point1, p1, p2};
    }

    void compute()
    {
        const int side = kGridSize + 1;
        std::vector<Sample> samples(static_cast<std::size_t>(side * side));
        const auto index = [side](int u, int v)
        { return static_cast<std::size_t>(v * side + u); };

        for (int v = 0; v <= kGridSize; ++v)
        {
            const double paramV =
                m_domain1.vLow + (m_domain1.vHigh - m_domain1.vLow) *
                                     static_cast<double>(v) / kGridSize;
            for (int u = 0; u <= kGridSize; ++u)
            {
                const double paramU =
                    m_domain1.uLow + (m_domain1.uHigh - m_domain1.uLow) *
                                         static_cast<double>(u) / kGridSize;
                samples[index(u, v)] = evaluate(paramU, paramV);
            }
        }

        std::vector<std::pair<Vertex, Vertex>> segments;
        for (int v = 0; v < kGridSize; ++v)
        {
            for (int u = 0; u < kGridSize; ++u)
            {
                const Sample corners[] = {
                    samples[index(u, v)], samples[index(u + 1, v)],
                    samples[index(u + 1, v + 1)], samples[index(u, v + 1)]};
                if (std::any_of(std::begin(corners), std::end(corners),
                                [](const Sample &sample) { return !sample.valid; }))
                    continue;
                std::vector<Vertex> crossings;
                for (int edge = 0; edge < 4; ++edge)
                {
                    const Sample &a = corners[edge];
                    const Sample &b = corners[(edge + 1) % 4];
                    if ((a.signedDistance < 0.0) !=
                        (b.signedDistance < 0.0))
                        crossings.push_back(interpolate(a, b));
                }
                if (crossings.size() == 2)
                    segments.emplace_back(crossings[0], crossings[1]);
                else if (crossings.size() == 4)
                {
                    segments.emplace_back(crossings[0], crossings[1]);
                    segments.emplace_back(crossings[2], crossings[3]);
                }
            }
        }
        joinSegments(segments);
    }

    void joinSegments(std::vector<std::pair<Vertex, Vertex>> &segments)
    {
        const double joinTolerance = std::max(m_tolerance * 8.0, 1.0e-10);
        std::vector<bool> used(segments.size(), false);
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (used[i])
                continue;
            used[i] = true;
            std::vector<Vertex> chain{segments[i].first, segments[i].second};
            bool extended = true;
            while (extended)
            {
                extended = false;
                for (std::size_t j = 0; j < segments.size(); ++j)
                {
                    if (used[j])
                        continue;
                    if ((chain.back().point - segments[j].first.point).length() <=
                        joinTolerance)
                        chain.push_back(segments[j].second);
                    else if ((chain.back().point - segments[j].second.point)
                                 .length() <= joinTolerance)
                        chain.push_back(segments[j].first);
                    else if ((chain.front().point - segments[j].second.point)
                                 .length() <= joinTolerance)
                        chain.insert(chain.begin(), segments[j].first);
                    else if ((chain.front().point - segments[j].first.point)
                                 .length() <= joinTolerance)
                        chain.insert(chain.begin(), segments[j].second);
                    else
                        continue;
                    used[j] = true;
                    extended = true;
                }
            }
            Result result;
            result.dimension = 1;
            for (const Vertex &vertex : chain)
            {
                if (!result.curve.empty() &&
                    (result.curve.back() - vertex.point).length() <=
                        joinTolerance)
                    continue;
                result.curve.push_back(vertex.point);
                result.params1.push_back(vertex.param1);
                result.params2.push_back(vertex.param2);
            }
            if (result.curve.size() >= 2)
                m_results.push_back(std::move(result));
        }
    }

    const Result &checkedResult(int index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_results.size())
            throw std::out_of_range(
                "SurfSurfInt intersection index is out of range.");
        return m_results[static_cast<std::size_t>(index)];
    }
};
