#include "acge/AcGeUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// ---------------------------------------------------------------------------
// AcGeBoundBlock2d
//
// An oriented block is stored as a base corner plus three (2D: two) edge
// vectors and per-edge parameter ranges, i.e.
//
//     { m_base + sum ti*m_diri,  ti in [m_tMini, m_tMaxi] }
//
// extend() and swell() grow the parameter ranges so the oriented edges are
// preserved; the coordinate extents in min/max always track the block
// corners through updateExtents().
// ---------------------------------------------------------------------------

namespace
{
// Parameter tolerance used for boundary-inclusive containment tests.
constexpr double kParamTol = 1.0e-12;
// Determinant threshold under which an oriented basis is considered
// degenerate (zero-volume / collinear edges).
constexpr double kDetTol = 1.0e-15;
} // namespace

void AcGeBoundBlock2d::updateExtents()
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

AcGeBoundBlock2d::AcGeBoundBlock2d(const AcGePoint2d &point1,
                                   const AcGePoint2d &point2)
{
    set(point1, point2);
}

AcGeBoundBlock2d::AcGeBoundBlock2d(const AcGePoint2d &base,
                                   const AcGeVector2d &dir1,
                                   const AcGeVector2d &dir2)
{
    set(base, dir1, dir2);
}

AcGeBoundBlock2d &AcGeBoundBlock2d::set(const AcGePoint2d &point1,
                                        const AcGePoint2d &point2)
{
    min = {std::min(point1.x, point2.x), std::min(point1.y, point2.y)};
    max = {std::max(point1.x, point2.x), std::max(point1.y, point2.y)};
    m_empty = false;
    m_oriented = false;
    m_tMin = {0.0, 0.0};
    m_tMax = {1.0, 1.0};
    return *this;
}

AcGeBoundBlock2d &AcGeBoundBlock2d::set(const AcGePoint2d &base,
                                        const AcGeVector2d &dir1,
                                        const AcGeVector2d &dir2)
{
    m_base = base;
    m_dir1 = dir1;
    m_dir2 = dir2;
    m_empty = false;
    m_oriented = true;
    m_tMin = {0.0, 0.0};
    m_tMax = {1.0, 1.0};
    updateExtents();
    return *this;
}

AcGeBoundBlock2d &AcGeBoundBlock2d::extend(const AcGePoint2d &point)
{
    if (m_oriented)
    {
        const AcGeVector2d offset{point.x - m_base.x, point.y - m_base.y};
        const double determinant =
            m_dir1.x * m_dir2.y - m_dir1.y * m_dir2.x;
        if (std::abs(determinant) > kDetTol)
        {
            const double t1 =
                (offset.x * m_dir2.y - offset.y * m_dir2.x) / determinant;
            const double t2 =
                (m_dir1.x * offset.y - m_dir1.y * offset.x) / determinant;
            m_tMin[0] = std::min(m_tMin[0], t1);
            m_tMax[0] = std::max(m_tMax[0], t1);
            m_tMin[1] = std::min(m_tMin[1], t2);
            m_tMax[1] = std::max(m_tMax[1], t2);
            updateExtents();
            return *this;
        }
        // Degenerate edges: fall back to the coordinate box of the
        // current block plus the new point.
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

AcGeBoundBlock2d &AcGeBoundBlock2d::extend(const AcGePoint2d *points,
                                           std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        extend(points[i]);
    return *this;
}

AcGeBoundBlock2d &AcGeBoundBlock2d::swell(double distance)
{
    if (m_empty || distance == 0.0)
        return *this;
    if (m_oriented)
    {
        bool grew = false;
        const AcGeVector2d dirs[2] = {m_dir1, m_dir2};
        for (int i = 0; i < 2; ++i)
        {
            const double len = dirs[i].length();
            if (len > kDetTol)
            {
                const double delta = distance / len;
                m_tMin[i] -= delta;
                m_tMax[i] += delta;
                grew = true;
            }
        }
        if (grew)
        {
            updateExtents();
            return *this;
        }
        // Fully degenerate block: swell the coordinate box instead.
        const AcGePoint2d oldMin = min;
        const AcGePoint2d oldMax = max;
        m_oriented = false;
        set(oldMin, oldMax);
    }
    min.x -= distance;
    min.y -= distance;
    max.x += distance;
    max.y += distance;
    return *this;
}

// ARX AcGeBoundBlock2d::setToBox: sets the block to the coordinate-aligned
// box of its corner points. The Boolean requests the conversion; kFalse is
// treated as a no-op (the reference HTML leaves the parameter undocumented).
AcGeBoundBlock2d &AcGeBoundBlock2d::setToBox(bool convert)
{
    if (!convert || m_empty || !m_oriented)
        return *this;
    return set(min, max);
}

void AcGeBoundBlock2d::get(AcGePoint2d &origin, AcGeVector2d &side1,
                           AcGeVector2d &side2) const
{
    if (m_oriented)
    {
        origin = cornerAt(0, 0);
        side1 = m_dir1 * (m_tMax[0] - m_tMin[0]);
        side2 = m_dir2 * (m_tMax[1] - m_tMin[1]);
    }
    else
    {
        origin = min;
        side1 = {max.x - min.x, 0.0};
        side2 = {0.0, max.y - min.y};
    }
}

bool AcGeBoundBlock2d::contains(const AcGePoint2d &point) const
{
    if (m_empty)
        return false;
    if (!m_oriented)
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y;
    const double determinant =
        m_dir1.x * m_dir2.y - m_dir1.y * m_dir2.x;
    if (std::abs(determinant) <= kDetTol)
        return false;
    const AcGeVector2d offset{point.x - m_base.x, point.y - m_base.y};
    const double u =
        (offset.x * m_dir2.y - offset.y * m_dir2.x) / determinant;
    const double v =
        (m_dir1.x * offset.y - m_dir1.y * offset.x) / determinant;
    return u >= m_tMin[0] - kParamTol && u <= m_tMax[0] + kParamTol &&
           v >= m_tMin[1] - kParamTol && v <= m_tMax[1] + kParamTol;
}

bool AcGeBoundBlock2d::isDisjoint(const AcGeBoundBlock2d &block) const
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

bool AcGeBoundBlock2d::isBox(const AcGeTol &tol) const
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

// ---------------------------------------------------------------------------
// AcGeBoundBlock3d
// ---------------------------------------------------------------------------

void AcGeBoundBlock3d::updateExtents()
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

AcGeBoundBlock3d::AcGeBoundBlock3d(const AcGePoint3d &point1,
                                   const AcGePoint3d &point2)
{
    set(point1, point2);
}

AcGeBoundBlock3d::AcGeBoundBlock3d(const AcGePoint3d &base,
                                   const AcGeVector3d &dir1,
                                   const AcGeVector3d &dir2,
                                   const AcGeVector3d &dir3)
{
    set(base, dir1, dir2, dir3);
}

AcGeBoundBlock3d &AcGeBoundBlock3d::set(const AcGePoint3d &point1,
                                        const AcGePoint3d &point2)
{
    min = {std::min(point1.x, point2.x), std::min(point1.y, point2.y),
           std::min(point1.z, point2.z)};
    max = {std::max(point1.x, point2.x), std::max(point1.y, point2.y),
           std::max(point1.z, point2.z)};
    m_empty = false;
    m_oriented = false;
    m_tMin = {0.0, 0.0, 0.0};
    m_tMax = {1.0, 1.0, 1.0};
    return *this;
}

AcGeBoundBlock3d &AcGeBoundBlock3d::set(const AcGePoint3d &base,
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
    m_tMin = {0.0, 0.0, 0.0};
    m_tMax = {1.0, 1.0, 1.0};
    updateExtents();
    return *this;
}

AcGeBoundBlock3d &AcGeBoundBlock3d::extend(const AcGePoint3d &point)
{
    if (m_oriented)
    {
        const AcGeVector3d offset = point - m_base;
        const double determinant =
            m_dir1.dotProduct(m_dir2.crossProduct(m_dir3));
        if (std::abs(determinant) > kDetTol)
        {
            const double t1 =
                offset.dotProduct(m_dir2.crossProduct(m_dir3)) / determinant;
            const double t2 =
                m_dir1.dotProduct(offset.crossProduct(m_dir3)) / determinant;
            const double t3 =
                m_dir1.dotProduct(m_dir2.crossProduct(offset)) / determinant;
            m_tMin[0] = std::min(m_tMin[0], t1);
            m_tMax[0] = std::max(m_tMax[0], t1);
            m_tMin[1] = std::min(m_tMin[1], t2);
            m_tMax[1] = std::max(m_tMax[1], t2);
            m_tMin[2] = std::min(m_tMin[2], t3);
            m_tMax[2] = std::max(m_tMax[2], t3);
            updateExtents();
            return *this;
        }
        // Degenerate edges: fall back to the coordinate box of the
        // current block plus the new point.
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

AcGeBoundBlock3d &AcGeBoundBlock3d::extend(const AcGePoint3d *points,
                                           std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        extend(points[i]);
    return *this;
}

AcGeBoundBlock3d &AcGeBoundBlock3d::swell(double distance)
{
    if (m_empty || distance == 0.0)
        return *this;
    if (m_oriented)
    {
        bool grew = false;
        const AcGeVector3d dirs[3] = {m_dir1, m_dir2, m_dir3};
        for (int i = 0; i < 3; ++i)
        {
            const double len = dirs[i].length();
            if (len > kDetTol)
            {
                const double delta = distance / len;
                m_tMin[i] -= delta;
                m_tMax[i] += delta;
                grew = true;
            }
        }
        if (grew)
        {
            updateExtents();
            return *this;
        }
        // Fully degenerate block: swell the coordinate box instead.
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
    return *this;
}

// ARX AcGeBoundBlock3d::setToBox: sets the block to the coordinate-aligned
// box of its corner points. The Boolean requests the conversion; kFalse is
// treated as a no-op (the reference HTML leaves the parameter undocumented).
AcGeBoundBlock3d &AcGeBoundBlock3d::setToBox(bool convert)
{
    if (!convert || m_empty || !m_oriented)
        return *this;
    return set(min, max);
}

void AcGeBoundBlock3d::get(AcGePoint3d &origin, AcGeVector3d &side1,
                           AcGeVector3d &side2, AcGeVector3d &side3) const
{
    if (m_oriented)
    {
        origin = cornerAt(0, 0, 0);
        side1 = m_dir1 * (m_tMax[0] - m_tMin[0]);
        side2 = m_dir2 * (m_tMax[1] - m_tMin[1]);
        side3 = m_dir3 * (m_tMax[2] - m_tMin[2]);
    }
    else
    {
        origin = min;
        side1 = {max.x - min.x, 0.0, 0.0};
        side2 = {0.0, max.y - min.y, 0.0};
        side3 = {0.0, 0.0, max.z - min.z};
    }
}

bool AcGeBoundBlock3d::contains(const AcGePoint3d &point) const
{
    if (m_empty)
        return false;
    if (!m_oriented)
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    const double determinant =
        m_dir1.dotProduct(m_dir2.crossProduct(m_dir3));
    if (std::abs(determinant) <= kDetTol)
        return false;
    const AcGeVector3d offset = point - m_base;
    const double u =
        offset.dotProduct(m_dir2.crossProduct(m_dir3)) / determinant;
    const double v =
        m_dir1.dotProduct(offset.crossProduct(m_dir3)) / determinant;
    const double w =
        m_dir1.dotProduct(m_dir2.crossProduct(offset)) / determinant;
    return u >= m_tMin[0] - kParamTol && u <= m_tMax[0] + kParamTol &&
           v >= m_tMin[1] - kParamTol && v <= m_tMax[1] + kParamTol &&
           w >= m_tMin[2] - kParamTol && w <= m_tMax[2] + kParamTol;
}

bool AcGeBoundBlock3d::isDisjoint(const AcGeBoundBlock3d &block) const
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

bool AcGeBoundBlock3d::isBox(const AcGeTol &tol) const
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
