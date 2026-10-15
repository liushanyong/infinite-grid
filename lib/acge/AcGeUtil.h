#pragma once

// AcGe-compatible bound block and tolerance value types.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "AcGePoint.h"
#include "AcGeTol.h"

struct AcGeBoundBlock2d
{
    AcGePoint2d min{0.0, 0.0};
    AcGePoint2d max{0.0, 0.0};
    bool m_empty = true;
    bool m_oriented = false;
    AcGePoint2d m_base{0.0, 0.0};
    AcGeVector2d m_dir1{0.0, 0.0};
    AcGeVector2d m_dir2{0.0, 0.0};

    std::array<AcGePoint2d, 4> corners() const
    {
        if (m_oriented)
            return {m_base, m_base + m_dir1, m_base + m_dir2,
                    m_base + m_dir1 + m_dir2};
        return {AcGePoint2d{min.x, min.y}, AcGePoint2d{max.x, min.y},
                AcGePoint2d{min.x, max.y}, AcGePoint2d{max.x, max.y}};
    }

    void updateExtents()
    {
        const auto points = corners();
        min = max = points.front();
        for (const auto &point : points)
        {
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
        }
    }

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
        m_oriented = false;
        return *this;
    }

    AcGeBoundBlock2d(const AcGePoint2d &base, const AcGeVector2d &dir1,
                     const AcGeVector2d &dir2)
    {
        set(base, dir1, dir2);
    }

    AcGeBoundBlock2d &set(const AcGePoint2d &base,
                          const AcGeVector2d &dir1,
                          const AcGeVector2d &dir2)
    {
        m_base = base;
        m_dir1 = dir1;
        m_dir2 = dir2;
        m_empty = false;
        m_oriented = true;
        updateExtents();
        return *this;
    }

    AcGeBoundBlock2d &extend(const AcGePoint2d &point)
    {
        if (m_oriented)
        {
            const AcGePoint2d oldMin = min;
            const AcGePoint2d oldMax = max;
            m_oriented = false;
            set(oldMin, oldMax);
        }
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
            if (m_oriented)
            {
                const AcGePoint2d oldMin = min;
                const AcGePoint2d oldMax = max;
                m_oriented = false;
                set(oldMin, oldMax);
            }
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
        if (m_oriented)
        {
            origin = m_base;
            side1 = m_dir1;
            side2 = m_dir2;
        }
        else
        {
            origin = min;
            side1 = {max.x - min.x, 0.0};
            side2 = {0.0, max.y - min.y};
        }
    }

    void getMinMaxPoints(AcGePoint2d &point1, AcGePoint2d &point2) const
    {
        point1 = min;
        point2 = max;
    }

    bool contains(const AcGePoint2d &point) const
    {
        if (m_empty)
            return false;
        if (!m_oriented)
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y;
        const double determinant =
            m_dir1.x * m_dir2.y - m_dir1.y * m_dir2.x;
        if (std::abs(determinant) <= 1.0e-15)
            return false;
        const AcGeVector2d offset{point.x - m_base.x, point.y - m_base.y};
        const double u =
            (offset.x * m_dir2.y - offset.y * m_dir2.x) / determinant;
        const double v =
            (m_dir1.x * offset.y - m_dir1.y * offset.x) / determinant;
        constexpr double tol = 1.0e-12;
        return u >= -tol && u <= 1.0 + tol && v >= -tol &&
               v <= 1.0 + tol;
    }

    bool isDisjoint(const AcGeBoundBlock2d &block) const
    {
        if (m_empty || block.m_empty)
            return true;
        const auto a = corners();
        const auto b = block.corners();
        const auto separated = [&](const AcGeVector2d &axis)
        {
            if (axis.lengthSq() <= 1.0e-30)
                return false;
            double amin = a[0].x * axis.x + a[0].y * axis.y;
            double amax = amin;
            double bmin = b[0].x * axis.x + b[0].y * axis.y;
            double bmax = bmin;
            for (std::size_t i = 1; i < a.size(); ++i)
            {
                const double ap = a[i].x * axis.x + a[i].y * axis.y;
                const double bp = b[i].x * axis.x + b[i].y * axis.y;
                amin = std::min(amin, ap);
                amax = std::max(amax, ap);
                bmin = std::min(bmin, bp);
                bmax = std::max(bmax, bp);
            }
            return amax < bmin || bmax < amin;
        };
        const auto axesFor = [](const AcGeBoundBlock2d &value)
        {
            if (value.m_oriented)
                return std::array<AcGeVector2d, 2>{
                    value.m_dir1.perpVector(), value.m_dir2.perpVector()};
            return std::array<AcGeVector2d, 2>{
                AcGeVector2d{1.0, 0.0}, AcGeVector2d{0.0, 1.0}};
        };
        for (const auto &axis : axesFor(*this))
            if (separated(axis))
                return true;
        for (const auto &axis : axesFor(block))
            if (separated(axis))
                return true;
        return false;
    }

    bool isBox(const AcGeTol &tol = AcGeTol{}) const
    {
        if (m_empty || !m_oriented)
            return true;
        int axes = 0;
        for (const auto &direction : {m_dir1, m_dir2})
        {
            if (direction.isZeroLength(tol))
                continue;
            const bool xAxis = std::abs(direction.y) <= tol.equalVector();
            const bool yAxis = std::abs(direction.x) <= tol.equalVector();
            if (xAxis == yAxis)
                return false;
            const int bit = xAxis ? 1 : 2;
            if ((axes & bit) != 0)
                return false;
            axes |= bit;
        }
        return true;
    }
    bool isEmpty() const { return m_empty; }
};

struct AcGeBoundBlock3d
{
    AcGePoint3d min{0.0, 0.0, 0.0};
    AcGePoint3d max{0.0, 0.0, 0.0};
    bool m_empty = true;
    bool m_oriented = false;
    AcGePoint3d m_base{0.0, 0.0, 0.0};
    AcGeVector3d m_dir1{0.0, 0.0, 0.0};
    AcGeVector3d m_dir2{0.0, 0.0, 0.0};
    AcGeVector3d m_dir3{0.0, 0.0, 0.0};

    std::array<AcGePoint3d, 8> corners() const
    {
        if (m_oriented)
            return {m_base,
                    m_base + m_dir1,
                    m_base + m_dir2,
                    m_base + m_dir3,
                    m_base + m_dir1 + m_dir2,
                    m_base + m_dir1 + m_dir3,
                    m_base + m_dir2 + m_dir3,
                    m_base + m_dir1 + m_dir2 + m_dir3};
        return {AcGePoint3d{min.x, min.y, min.z},
                AcGePoint3d{max.x, min.y, min.z},
                AcGePoint3d{min.x, max.y, min.z},
                AcGePoint3d{min.x, min.y, max.z},
                AcGePoint3d{max.x, max.y, min.z},
                AcGePoint3d{max.x, min.y, max.z},
                AcGePoint3d{min.x, max.y, max.z},
                AcGePoint3d{max.x, max.y, max.z}};
    }

    void updateExtents()
    {
        const auto points = corners();
        min = max = points.front();
        for (const auto &point : points)
        {
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            min.z = std::min(min.z, point.z);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
            max.z = std::max(max.z, point.z);
        }
    }

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
        m_oriented = false;
        return *this;
    }

    AcGeBoundBlock3d(const AcGePoint3d &base, const AcGeVector3d &dir1,
                     const AcGeVector3d &dir2, const AcGeVector3d &dir3)
    {
        set(base, dir1, dir2, dir3);
    }

    AcGeBoundBlock3d &set(const AcGePoint3d &base,
                          const AcGeVector3d &dir1,
                          const AcGeVector3d &dir2,
                          const AcGeVector3d &dir3)
    {
        m_base = base;
        m_dir1 = dir1;
        m_dir2 = dir2;
        m_dir3 = dir3;
        m_empty = false;
        m_oriented = true;
        updateExtents();
        return *this;
    }

    AcGeBoundBlock3d &extend(const AcGePoint3d &point)
    {
        if (m_oriented)
        {
            const AcGePoint3d oldMin = min;
            const AcGePoint3d oldMax = max;
            m_oriented = false;
            set(oldMin, oldMax);
        }
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
            if (m_oriented)
            {
                const AcGePoint3d oldMin = min;
                const AcGePoint3d oldMax = max;
                m_oriented = false;
                set(oldMin, oldMax);
            }
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
        if (m_oriented)
        {
            origin = m_base;
            side1 = m_dir1;
            side2 = m_dir2;
            side3 = m_dir3;
        }
        else
        {
            origin = min;
            side1 = {max.x - min.x, 0.0, 0.0};
            side2 = {0.0, max.y - min.y, 0.0};
            side3 = {0.0, 0.0, max.z - min.z};
        }
    }

    void getMinMaxPoints(AcGePoint3d &point1, AcGePoint3d &point2) const
    {
        point1 = min;
        point2 = max;
    }

    bool contains(const AcGePoint3d &point) const
    {
        if (m_empty)
            return false;
        if (!m_oriented)
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        const double determinant =
            m_dir1.dotProduct(m_dir2.crossProduct(m_dir3));
        if (std::abs(determinant) <= 1.0e-15)
            return false;
        const AcGeVector3d offset = point - m_base;
        const double u =
            offset.dotProduct(m_dir2.crossProduct(m_dir3)) / determinant;
        const double v =
            m_dir1.dotProduct(offset.crossProduct(m_dir3)) / determinant;
        const double w =
            m_dir1.dotProduct(m_dir2.crossProduct(offset)) / determinant;
        constexpr double tol = 1.0e-12;
        return u >= -tol && u <= 1.0 + tol && v >= -tol &&
               v <= 1.0 + tol && w >= -tol && w <= 1.0 + tol;
    }

    bool isDisjoint(const AcGeBoundBlock3d &block) const
    {
        if (m_empty || block.m_empty)
            return true;
        const auto a = corners();
        const auto b = block.corners();
        const auto separated = [&](const AcGeVector3d &axis)
        {
            if (axis.lengthSq() <= 1.0e-30)
                return false;
            double amin = a[0].asVector().dotProduct(axis);
            double amax = amin;
            double bmin = b[0].asVector().dotProduct(axis);
            double bmax = bmin;
            for (std::size_t i = 1; i < a.size(); ++i)
            {
                const double ap = a[i].asVector().dotProduct(axis);
                const double bp = b[i].asVector().dotProduct(axis);
                amin = std::min(amin, ap);
                amax = std::max(amax, ap);
                bmin = std::min(bmin, bp);
                bmax = std::max(bmax, bp);
            }
            return amax < bmin || bmax < amin;
        };
        const std::array<AcGeVector3d, 3> edgesA =
            m_oriented
                ? std::array<AcGeVector3d, 3>{m_dir1, m_dir2, m_dir3}
                : std::array<AcGeVector3d, 3>{
                      AcGeVector3d{max.x - min.x, 0.0, 0.0},
                      AcGeVector3d{0.0, max.y - min.y, 0.0},
                      AcGeVector3d{0.0, 0.0, max.z - min.z}};
        const std::array<AcGeVector3d, 3> edgesB =
            block.m_oriented
                ? std::array<AcGeVector3d, 3>{block.m_dir1, block.m_dir2,
                                              block.m_dir3}
                : std::array<AcGeVector3d, 3>{
                      AcGeVector3d{block.max.x - block.min.x, 0.0, 0.0},
                      AcGeVector3d{0.0, block.max.y - block.min.y, 0.0},
                      AcGeVector3d{0.0, 0.0, block.max.z - block.min.z}};
        for (const auto &edges : {edgesA, edgesB})
        {
            if (separated(edges[0].crossProduct(edges[1])) ||
                separated(edges[1].crossProduct(edges[2])) ||
                separated(edges[2].crossProduct(edges[0])))
                return true;
        }
        for (const auto &aEdge : edgesA)
            for (const auto &bEdge : edgesB)
                if (separated(aEdge.crossProduct(bEdge)))
                    return true;
        return false;
    }

    bool isBox(const AcGeTol &tol = AcGeTol{}) const
    {
        if (m_empty || !m_oriented)
            return true;
        int axes = 0;
        for (const auto &direction : {m_dir1, m_dir2, m_dir3})
        {
            if (direction.isZeroLength(tol))
                continue;
            const bool xAxis =
                std::abs(direction.y) <= tol.equalVector() &&
                std::abs(direction.z) <= tol.equalVector();
            const bool yAxis =
                std::abs(direction.x) <= tol.equalVector() &&
                std::abs(direction.z) <= tol.equalVector();
            const bool zAxis =
                std::abs(direction.x) <= tol.equalVector() &&
                std::abs(direction.y) <= tol.equalVector();
            const int matches = int(xAxis) + int(yAxis) + int(zAxis);
            if (matches != 1)
                return false;
            const int bit = xAxis ? 1 : (yAxis ? 2 : 4);
            if ((axes & bit) != 0)
                return false;
            axes |= bit;
        }
        return true;
    }
    bool isEmpty() const { return m_empty; }
};
