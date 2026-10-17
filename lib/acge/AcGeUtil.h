#pragma once

#include "AcGePort.h"

// AcGe-compatible bound block and tolerance value types.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "AcGePoint.h"
#include "AcGeTol.h"

struct ACGE_PORT AcGeBoundBlock2d
{
    AcGePoint2d min{0.0, 0.0};
    AcGePoint2d max{0.0, 0.0};
    bool m_empty = true;
    bool m_oriented = false;
    AcGePoint2d m_base{0.0, 0.0};
    AcGeVector2d m_dir1{0.0, 0.0};
    AcGeVector2d m_dir2{0.0, 0.0};
    // Parameter ranges of the block along each oriented edge; the block
    // is { m_base + sum ti*m_diri, ti in [m_tMini, m_tMaxi] }.
    std::array<double, 2> m_tMin{0.0, 0.0};
    std::array<double, 2> m_tMax{1.0, 1.0};

    std::array<AcGePoint2d, 4> corners() const
    {
        if (m_oriented)
            return {cornerAt(0, 0), cornerAt(1, 0), cornerAt(0, 1),
                    cornerAt(1, 1)};
        return {AcGePoint2d{min.x, min.y}, AcGePoint2d{max.x, min.y},
                AcGePoint2d{min.x, max.y}, AcGePoint2d{max.x, max.y}};
    }

    void updateExtents();

    AcGeBoundBlock2d() = default;
    AcGeBoundBlock2d(const AcGePoint2d &point1, const AcGePoint2d &point2);
    AcGeBoundBlock2d(const AcGePoint2d &base, const AcGeVector2d &dir1,
                     const AcGeVector2d &dir2);

    AcGeBoundBlock2d &set(const AcGePoint2d &point1,
                          const AcGePoint2d &point2);
    AcGeBoundBlock2d &set(const AcGePoint2d &base,
                          const AcGeVector2d &dir1,
                          const AcGeVector2d &dir2);
    AcGeBoundBlock2d &extend(const AcGePoint2d &point);
    AcGeBoundBlock2d &extend(const AcGePoint2d *points, std::size_t count);
    AcGeBoundBlock2d &extend(const std::vector<AcGePoint2d> &points)
    {
        return extend(points.data(), points.size());
    }
    AcGeBoundBlock2d &swell(double distance);
    // ARX AcGeBoundBlock2d::setToBox: converts the block to the
    // coordinate-aligned box of its corner points. kFalse is a no-op.
    AcGeBoundBlock2d &setToBox(bool convert = true);
    void get(AcGePoint2d &origin, AcGeVector2d &side1,
             AcGeVector2d &side2) const;
    void getMinMaxPoints(AcGePoint2d &point1, AcGePoint2d &point2) const
    {
        point1 = min;
        point2 = max;
    }
    bool contains(const AcGePoint2d &point) const;
    bool isDisjoint(const AcGeBoundBlock2d &block) const;
    bool isBox(const AcGeTol &tol = AcGeTol{}) const;
    bool isEmpty() const { return m_empty; }

private:
    AcGePoint2d cornerAt(int iAtMax, int jAtMax) const
    {
        const double ti = iAtMax ? m_tMax[0] : m_tMin[0];
        const double tj = jAtMax ? m_tMax[1] : m_tMin[1];
        return {m_base.x + ti * m_dir1.x + tj * m_dir2.x,
                m_base.y + ti * m_dir1.y + tj * m_dir2.y};
    }
};

struct ACGE_PORT AcGeBoundBlock3d
{
    AcGePoint3d min{0.0, 0.0, 0.0};
    AcGePoint3d max{0.0, 0.0, 0.0};
    bool m_empty = true;
    bool m_oriented = false;
    AcGePoint3d m_base{0.0, 0.0, 0.0};
    AcGeVector3d m_dir1{0.0, 0.0, 0.0};
    AcGeVector3d m_dir2{0.0, 0.0, 0.0};
    AcGeVector3d m_dir3{0.0, 0.0, 0.0};
    // Parameter ranges of the block along each oriented edge; the block
    // is { m_base + sum ti*m_diri, ti in [m_tMini, m_tMaxi] }.
    std::array<double, 3> m_tMin{0.0, 0.0, 0.0};
    std::array<double, 3> m_tMax{1.0, 1.0, 1.0};

    std::array<AcGePoint3d, 8> corners() const
    {
        if (m_oriented)
            return {cornerAt(0, 0, 0), cornerAt(1, 0, 0), cornerAt(0, 1, 0),
                    cornerAt(0, 0, 1), cornerAt(1, 1, 0), cornerAt(1, 0, 1),
                    cornerAt(0, 1, 1), cornerAt(1, 1, 1)};
        return {AcGePoint3d{min.x, min.y, min.z},
                AcGePoint3d{max.x, min.y, min.z},
                AcGePoint3d{min.x, max.y, min.z},
                AcGePoint3d{min.x, min.y, max.z},
                AcGePoint3d{max.x, max.y, min.z},
                AcGePoint3d{max.x, min.y, max.z},
                AcGePoint3d{min.x, max.y, max.z},
                AcGePoint3d{max.x, max.y, max.z}};
    }

    void updateExtents();

    AcGeBoundBlock3d() = default;
    AcGeBoundBlock3d(const AcGePoint3d &point1, const AcGePoint3d &point2);
    AcGeBoundBlock3d(const AcGePoint3d &base, const AcGeVector3d &dir1,
                     const AcGeVector3d &dir2, const AcGeVector3d &dir3);

    AcGeBoundBlock3d &set(const AcGePoint3d &point1,
                          const AcGePoint3d &point2);
    AcGeBoundBlock3d &set(const AcGePoint3d &base,
                          const AcGeVector3d &dir1,
                          const AcGeVector3d &dir2,
                          const AcGeVector3d &dir3);
    AcGeBoundBlock3d &extend(const AcGePoint3d &point);
    AcGeBoundBlock3d &extend(const AcGePoint3d *points, std::size_t count);
    AcGeBoundBlock3d &extend(const std::vector<AcGePoint3d> &points)
    {
        return extend(points.data(), points.size());
    }
    AcGeBoundBlock3d &swell(double distance);
    // ARX AcGeBoundBlock3d::setToBox: converts the block to the
    // coordinate-aligned box of its corner points. kFalse is a no-op.
    AcGeBoundBlock3d &setToBox(bool convert = true);
    void get(AcGePoint3d &origin, AcGeVector3d &side1,
             AcGeVector3d &side2, AcGeVector3d &side3) const;
    void getMinMaxPoints(AcGePoint3d &point1, AcGePoint3d &point2) const
    {
        point1 = min;
        point2 = max;
    }
    bool contains(const AcGePoint3d &point) const;
    bool isDisjoint(const AcGeBoundBlock3d &block) const;
    bool isBox(const AcGeTol &tol = AcGeTol{}) const;
    bool isEmpty() const { return m_empty; }

private:
    AcGePoint3d cornerAt(int iAtMax, int jAtMax, int kAtMax) const
    {
        const double ti = iAtMax ? m_tMax[0] : m_tMin[0];
        const double tj = jAtMax ? m_tMax[1] : m_tMin[1];
        const double tk = kAtMax ? m_tMax[2] : m_tMin[2];
        return {m_base.x + ti * m_dir1.x + tj * m_dir2.x + tk * m_dir3.x,
                m_base.y + ti * m_dir1.y + tj * m_dir2.y + tk * m_dir3.y,
                m_base.z + ti * m_dir1.z + tj * m_dir2.z + tk * m_dir3.z};
    }
};
