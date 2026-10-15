#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "AcGePoint.h"

// Convex clipping subset; no ObjectARX array or error-status compatibility.
// Both boundary and polygon inputs must be strictly convex, in either winding.
// The tolerance expands each clipping half-plane by equalPoint() world units.
class AcGeClipBoundary2d
{
    struct Edge
    {
        AcGePoint2d origin;
        long double nx;
        long double ny;
    };

    std::vector<AcGePoint2d> m_vertices;
    std::vector<Edge> m_edges;
    double m_tolerance = 1.0e-10;

    static void validatePoint(const AcGePoint2d &point)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            throw std::invalid_argument("Clipping coordinates must be finite.");
    }

    static long double distance(const Edge &edge, const AcGePoint2d &point)
    {
        const long double result =
            (static_cast<long double>(point.x) - edge.origin.x) * edge.nx +
            (static_cast<long double>(point.y) - edge.origin.y) * edge.ny;
        if (!std::isfinite(result))
            throw std::overflow_error("Clipping distance overflow.");
        return result;
    }

    static std::vector<Edge> makeEdges(std::vector<AcGePoint2d> &vertices)
    {
        for (const auto &point : vertices)
            validatePoint(point);
        if (vertices.size() > 1 && vertices.front() == vertices.back())
            vertices.pop_back();
        if (vertices.size() < 3)
            throw std::invalid_argument("Clipping requires a convex polygon.");

        std::vector<Edge> edges;
        edges.reserve(vertices.size());
        int winding = 0;
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const auto &a = vertices[i];
            const auto &b = vertices[(i + 1) % vertices.size()];
            const long double dx = static_cast<long double>(b.x) - a.x;
            const long double dy = static_cast<long double>(b.y) - a.y;
            const long double length = std::hypot(dx, dy);
            if (!std::isfinite(length) || length == 0.0L)
                throw std::invalid_argument("Clipping edges must be nonzero and finite.");
            Edge edge{a, -dy / length, dx / length};
            const long double turn =
                distance(edge, vertices[(i + 2) % vertices.size()]);
            if (turn == 0.0L)
                throw std::invalid_argument("Clipping polygons must be strictly convex.");
            const int sign = turn > 0.0L ? 1 : -1;
            if (winding != 0 && winding != sign)
                throw std::invalid_argument("Clipping polygons must be convex.");
            winding = sign;
            edges.push_back(edge);
        }
        for (auto &edge : edges)
        {
            edge.nx *= winding;
            edge.ny *= winding;
        }
        // Checking all vertices also rejects self-intersecting star polygons.
        for (size_t i = 0; i < edges.size(); ++i)
            for (size_t j = 0; j < vertices.size(); ++j)
                if (j != i && j != (i + 1) % vertices.size() &&
                    distance(edges[i], vertices[j]) <= 0.0L)
                    throw std::invalid_argument("Clipping polygons must be simple and strictly convex.");
        return edges;
    }

    static AcGePoint2d interpolate(const AcGePoint2d &a, const AcGePoint2d &b,
                                   long double parameter)
    {
        return {
            static_cast<double>((1.0L - parameter) * a.x + parameter * b.x),
            static_cast<double>((1.0L - parameter) * a.y + parameter * b.y)};
    }

    long double clippingDistance(const Edge &edge, const AcGePoint2d &point) const
    {
        const long double value = distance(edge, point) + m_tolerance;
        if (!std::isfinite(value))
            throw std::overflow_error("Clipping tolerance expansion overflow.");
        return value;
    }

    static long double crossingParameter(long double a, long double b)
    {
        const long double scale = std::max(std::abs(a), std::abs(b));
        return (a / scale) / (a / scale - b / scale);
    }

    void requireBoundary() const
    {
        if (m_edges.empty())
            throw std::logic_error("Clipping boundary has not been set.");
    }

    bool clipParameters(const AcGePoint2d &start, const AcGePoint2d &end,
                        long double &first, long double &last) const
    {
        first = 0.0L;
        last = 1.0L;
        for (const auto &edge : m_edges)
        {
            const long double a = clippingDistance(edge, start);
            const long double b = clippingDistance(edge, end);
            if (a < 0.0L && b < 0.0L)
                return false;
            if ((a < 0.0L) != (b < 0.0L))
            {
                const long double parameter = crossingParameter(a, b);
                if (a < 0.0L)
                    first = std::max(first, parameter);
                else
                    last = std::min(last, parameter);
                if (first > last)
                    return false;
            }
        }
        return true;
    }

public:
    AcGeClipBoundary2d() = default;
    explicit AcGeClipBoundary2d(std::vector<AcGePoint2d> vertices,
                                const AcGeTol &tol = AcGeTol{})
    {
        set(std::move(vertices), tol);
    }
    AcGeClipBoundary2d(const AcGePoint2d &minimum, const AcGePoint2d &maximum,
                       const AcGeTol &tol = AcGeTol{})
    {
        set(minimum, maximum, tol);
    }

    AcGeClipBoundary2d &set(std::vector<AcGePoint2d> vertices,
                            const AcGeTol &tol = AcGeTol{})
    {
        if (!std::isfinite(tol.equalPoint()) || tol.equalPoint() < 0.0)
            throw std::invalid_argument("Clipping tolerance must be finite and nonnegative.");
        auto edges = makeEdges(vertices);
        m_vertices = std::move(vertices);
        m_edges = std::move(edges);
        m_tolerance = tol.equalPoint();
        return *this;
    }
    AcGeClipBoundary2d &set(const AcGePoint2d &minimum,
                            const AcGePoint2d &maximum,
                            const AcGeTol &tol = AcGeTol{})
    {
        validatePoint(minimum);
        validatePoint(maximum);
        if (minimum.x >= maximum.x || minimum.y >= maximum.y)
            throw std::invalid_argument("Clipping rectangle must have positive extents.");
        return set({minimum, {maximum.x, minimum.y}, maximum,
                    {minimum.x, maximum.y}}, tol);
    }

    bool isSet() const { return !m_edges.empty(); }
    const std::vector<AcGePoint2d> &vertices() const { return m_vertices; }

    bool containsPoint(const AcGePoint2d &point) const
    {
        requireBoundary();
        validatePoint(point);
        for (const auto &edge : m_edges)
            if (distance(edge, point) < -m_tolerance)
                return false;
        return true;
    }

    bool clipSegment(const AcGePoint2d &start, const AcGePoint2d &end,
                     AcGePoint2d &clippedStart, AcGePoint2d &clippedEnd) const
    {
        requireBoundary();
        validatePoint(start);
        validatePoint(end);
        long double first;
        long double last;
        if (!clipParameters(start, end, first, last))
            return false;
        const auto a = interpolate(start, end, first);
        const auto b = interpolate(start, end, last);
        clippedStart = a;
        clippedEnd = b;
        return true;
    }

    std::vector<AcGePoint2d> clipPolygon(std::vector<AcGePoint2d> polygon) const
    {
        requireBoundary();
        if (polygon.empty())
            return {};
        (void)makeEdges(polygon);
        for (const auto &edge : m_edges)
        {
            if (polygon.empty())
                break;
            std::vector<AcGePoint2d> output;
            auto previous = polygon.back();
            long double a = clippingDistance(edge, previous);
            for (const auto &current : polygon)
            {
                const long double b = clippingDistance(edge, current);
                if ((a < 0.0L) != (b < 0.0L))
                    output.push_back(interpolate(previous, current, crossingParameter(a, b)));
                if (b >= 0.0L)
                    output.push_back(current);
                previous = current;
                a = b;
            }
            polygon = std::move(output);
        }
        polygon.erase(std::unique(polygon.begin(), polygon.end()), polygon.end());
        if (polygon.size() > 1 && polygon.front() == polygon.back())
            polygon.pop_back();
        return polygon;
    }

    // Each returned chain is a separate visible part; outside gaps are not joined.
    std::vector<std::vector<AcGePoint2d>> clipPolyline(
        const std::vector<AcGePoint2d> &polyline) const
    {
        requireBoundary();
        for (const auto &point : polyline)
            validatePoint(point);
        std::vector<std::vector<AcGePoint2d>> result;
        if (polyline.size() == 1 && containsPoint(polyline.front()))
            result.push_back(polyline);
        bool previousReachedVertex = false;
        for (size_t i = 1; i < polyline.size(); ++i)
        {
            long double first;
            long double last;
            if (!clipParameters(polyline[i - 1], polyline[i], first, last))
            {
                previousReachedVertex = false;
                continue;
            }
            const auto a = interpolate(polyline[i - 1], polyline[i], first);
            const auto b = interpolate(polyline[i - 1], polyline[i], last);
            if (!previousReachedVertex || first != 0.0L)
                result.push_back({a});
            if (result.back().back() != b)
                result.back().push_back(b);
            previousReachedVertex = last == 1.0L;
        }
        return result;
    }
};
