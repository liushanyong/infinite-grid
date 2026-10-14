#pragma once

// AcGe-compatible bound block and tolerance value types.

#include <algorithm>
#include <cstddef>
#include <vector>

#include "gepoint.h"

struct AcGeBoundBlock2d
{
    AcGePoint2d min{0.0, 0.0};
    AcGePoint2d max{0.0, 0.0};
    bool m_empty = true;

    AcGeBoundBlock2d() = default;
    AcGeBoundBlock2d(const AcGePoint2d &point1, const AcGePoint2d &point2)
    {
        set(point1, point2);
    }

    AcGeBoundBlock2d &set(const AcGePoint2d &point1,
                          const AcGePoint2d &point2)
    {
        min = {std::min(point1.x, point2.x), std::min(point1.y, point2.y)};
        max = {std::max(point1.x, point2.x), std::max(point1.y, point2.y)};
        m_empty = false;
        return *this;
    }

    AcGeBoundBlock2d &extend(const AcGePoint2d &point)
    {
        if (m_empty)
        {
            min = max = point;
            m_empty = false;
            return *this;
        }
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        return *this;
    }

    AcGeBoundBlock2d &extend(const AcGePoint2d *points, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
            extend(points[i]);
        return *this;
    }

    AcGeBoundBlock2d &extend(const std::vector<AcGePoint2d> &points)
    {
        return extend(points.data(), points.size());
    }

    AcGeBoundBlock2d &swell(double distance)
    {
        if (!m_empty)
        {
            min.x -= distance;
            min.y -= distance;
            max.x += distance;
            max.y += distance;
        }
        return *this;
    }

    void get(AcGePoint2d &origin, AcGeVector2d &side1,
             AcGeVector2d &side2) const
    {
        origin = min;
        side1 = {max.x - min.x, 0.0};
        side2 = {0.0, max.y - min.y};
    }

    void getMinMaxPoints(AcGePoint2d &point1, AcGePoint2d &point2) const
    {
        point1 = min;
        point2 = max;
    }

    bool contains(const AcGePoint2d &point) const
    {
        return !m_empty && point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y;
    }

    bool isDisjoint(const AcGeBoundBlock2d &block) const
    {
        return m_empty || block.m_empty || max.x < block.min.x ||
               block.max.x < min.x || max.y < block.min.y ||
               block.max.y < min.y;
    }

    bool isBox() const { return true; }
    bool isEmpty() const { return m_empty; }
};

struct AcGeBoundBlock3d
{
    AcGePoint3d min{0.0, 0.0, 0.0};
    AcGePoint3d max{0.0, 0.0, 0.0};
    bool m_empty = true;

    AcGeBoundBlock3d() = default;

    AcGeBoundBlock3d(const AcGePoint3d &point1, const AcGePoint3d &point2)
    {
        set(point1, point2);
    }

    AcGeBoundBlock3d &set(const AcGePoint3d &point1,
                          const AcGePoint3d &point2)
    {
        min = {std::min(point1.x, point2.x), std::min(point1.y, point2.y),
               std::min(point1.z, point2.z)};
        max = {std::max(point1.x, point2.x), std::max(point1.y, point2.y),
               std::max(point1.z, point2.z)};
        m_empty = false;
        return *this;
    }

    AcGeBoundBlock3d &extend(const AcGePoint3d &point)
    {
        if (m_empty)
        {
            min = max = point;
            m_empty = false;
            return *this;
        }
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
        return *this;
    }

    AcGeBoundBlock3d &extend(const AcGePoint3d *points, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
            extend(points[i]);
        return *this;
    }

    AcGeBoundBlock3d &extend(const std::vector<AcGePoint3d> &points)
    {
        return extend(points.data(), points.size());
    }

    AcGeBoundBlock3d &swell(double distance)
    {
        if (!m_empty)
        {
            min.x -= distance;
            min.y -= distance;
            min.z -= distance;
            max.x += distance;
            max.y += distance;
            max.z += distance;
        }
        return *this;
    }

    void get(AcGePoint3d &origin, AcGeVector3d &side1,
             AcGeVector3d &side2, AcGeVector3d &side3) const
    {
        origin = min;
        side1 = {max.x - min.x, 0.0, 0.0};
        side2 = {0.0, max.y - min.y, 0.0};
        side3 = {0.0, 0.0, max.z - min.z};
    }

    void getMinMaxPoints(AcGePoint3d &point1, AcGePoint3d &point2) const
    {
        point1 = min;
        point2 = max;
    }

    bool contains(const AcGePoint3d &point) const
    {
        return !m_empty && point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool isDisjoint(const AcGeBoundBlock3d &block) const
    {
        return m_empty || block.m_empty || max.x < block.min.x ||
               block.max.x < min.x || max.y < block.min.y ||
               block.max.y < min.y || max.z < block.min.z ||
               block.max.z < min.z;
    }

    bool isBox() const { return true; }
    bool isEmpty() const { return m_empty; }
};

struct AcGeTol
{
    double equalPoint = 1.0e-10;
    double equalVector = 1.0e-10;

    void setEqualPoint(double value) { equalPoint = value; }
    void setEqualVector(double value) { equalVector = value; }
};
