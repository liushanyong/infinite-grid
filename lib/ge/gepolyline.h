#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#include "gepoint.h"

// Source-level AcGe polyline subset. Parameters are segment indices with a
// fractional position along each segment; closed polylines wrap parameters.
template <typename Point, typename Vector>
class AcGePolylineBase
{
protected:
    std::vector<Point> m_vertices;
    bool m_closed = false;

    std::size_t segmentCount() const
    {
        if (m_vertices.size() < 2)
            return 0;
        return m_closed ? m_vertices.size() : m_vertices.size() - 1;
    }

public:
    AcGePolylineBase() = default;
    explicit AcGePolylineBase(const std::vector<Point> &vertices,
                              bool closed = false)
        : m_vertices(vertices), m_closed(closed)
    {
    }
    AcGePolylineBase(std::initializer_list<Point> vertices,
                     bool closed = false)
        : m_vertices(vertices), m_closed(closed)
    {
    }

    std::size_t numVerts() const { return m_vertices.size(); }
    bool isClosed() const { return m_closed; }
    bool isEmpty() const { return m_vertices.empty(); }
    const std::vector<Point> &vertices() const { return m_vertices; }

    AcGePolylineBase &setClosed(bool closed)
    {
        m_closed = closed;
        return *this;
    }
    AcGePolylineBase &setVertexAt(std::size_t index, const Point &point)
    {
        m_vertices.at(index) = point;
        return *this;
    }
    Point vertexAt(std::size_t index) const { return m_vertices.at(index); }
    AcGePolylineBase &appendVertex(const Point &point)
    {
        m_vertices.push_back(point);
        return *this;
    }
    AcGePolylineBase &insertVertexAt(std::size_t index, const Point &point)
    {
        if (index > m_vertices.size())
            throw std::out_of_range("AcGePolyline vertex index is out of range.");
        m_vertices.insert(m_vertices.begin() + index, point);
        return *this;
    }
    AcGePolylineBase &removeVertexAt(std::size_t index)
    {
        if (index >= m_vertices.size())
            throw std::out_of_range("AcGePolyline vertex index is out of range.");
        m_vertices.erase(m_vertices.begin() + index);
        return *this;
    }
    AcGePolylineBase &reverse()
    {
        std::reverse(m_vertices.begin(), m_vertices.end());
        return *this;
    }
    AcGePolylineBase &clear()
    {
        m_vertices.clear();
        m_closed = false;
        return *this;
    }

    double length() const
    {
        double result = 0.0;
        for (std::size_t i = 0; i < segmentCount(); ++i)
        {
            const std::size_t next = (i + 1) % m_vertices.size();
            result += m_vertices[i].distanceTo(m_vertices[next]);
        }
        return result;
    }

    Point pointAt(double parameter) const
    {
        if (m_vertices.empty())
            return {};
        if (m_vertices.size() == 1)
            return m_vertices.front();
        if (!std::isfinite(parameter))
            return m_vertices.front();

        const std::size_t segments = segmentCount();
        if (segments == 0)
            return m_vertices.back();
        if (m_closed)
        {
            parameter = std::fmod(parameter, double(segments));
            if (parameter < 0.0)
                parameter += double(segments);
        }
        else
        {
            parameter = std::clamp(parameter, 0.0, double(segments));
            if (parameter >= double(segments))
                return m_vertices.back();
        }

        const auto index = static_cast<std::size_t>(std::floor(parameter));
        const double fraction = parameter - double(index);
        const std::size_t next = (index + 1) % m_vertices.size();
        return m_vertices[index] + (m_vertices[next] - m_vertices[index]) *
                                        fraction;
    }

    Point closestPointTo(const Point &point, double &parameter) const
    {
        if (m_vertices.empty())
        {
            parameter = 0.0;
            return {};
        }
        if (m_vertices.size() == 1 || segmentCount() == 0)
        {
            parameter = 0.0;
            return m_vertices.front();
        }

        Point closest = m_vertices.front();
        double bestDistanceSquared = (point - closest).lengthSq();
        parameter = 0.0;
        for (std::size_t i = 0; i < segmentCount(); ++i)
        {
            const std::size_t next = (i + 1) % m_vertices.size();
            const Vector direction = m_vertices[next] - m_vertices[i];
            const double lengthSquared = direction.lengthSq();
            const double fraction =
                lengthSquared == 0.0
                    ? 0.0
                    : std::clamp((point - m_vertices[i]).dotProduct(direction) /
                                     lengthSquared,
                                 0.0, 1.0);
            const Point candidate = m_vertices[i] + direction * fraction;
            const double distanceSquared = (point - candidate).lengthSq();
            if (distanceSquared < bestDistanceSquared)
            {
                closest = candidate;
                bestDistanceSquared = distanceSquared;
                parameter = double(i) + fraction;
            }
        }
        return closest;
    }

    Point closestPointTo(const Point &point) const
    {
        double ignoredParameter = 0.0;
        return closestPointTo(point, ignoredParameter);
    }

    double distanceTo(const Point &point) const
    {
        return point.distanceTo(closestPointTo(point));
    }

    bool isOn(const Point &point, double &parameter,
              double tolerance = 1.0e-9) const
    {
        if (m_vertices.empty())
        {
            parameter = 0.0;
            return false;
        }
        const Point closest = closestPointTo(point, parameter);
        return closest.distanceTo(point) <= std::abs(tolerance);
    }
};

struct AcGePolyline2d : AcGePolylineBase<AcGePoint2d, AcGeVector2d>
{
    using AcGePolylineBase::AcGePolylineBase;

    double signedArea() const
    {
        if (!m_closed || m_vertices.size() < 3)
            return 0.0;
        double twiceArea = 0.0;
        const std::size_t count = m_vertices.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            const AcGePoint2d &a = m_vertices[i];
            const AcGePoint2d &b = m_vertices[(i + 1) % count];
            twiceArea += a.x * b.y - b.x * a.y;
        }
        return twiceArea * 0.5;
    }

    double area() const { return std::abs(signedArea()); }
};

struct AcGePolyline3d : AcGePolylineBase<AcGePoint3d, AcGeVector3d>
{
    using AcGePolylineBase::AcGePolylineBase;
};
