#pragma once

// AcGe-compatible bound block and tolerance value types.

#include <vector>

#include "gepoint.h"

struct AcGeBoundBlock3d
{
    AcGePoint3d min{0.0, 0.0, 0.0};
    AcGePoint3d max{0.0, 0.0, 0.0};
    bool m_empty = true;

    AcGeBoundBlock3d() = default;

    void extend(const AcGePoint3d &point)
    {
        if (m_empty)
        {
            min = max = point;
            m_empty = false;
            return;
        }
        if (point.x < min.x) min.x = point.x;
        if (point.y < min.y) min.y = point.y;
        if (point.z < min.z) min.z = point.z;
        if (point.x > max.x) max.x = point.x;
        if (point.y > max.y) max.y = point.y;
        if (point.z > max.z) max.z = point.z;
    }

    void extend(const AcGePoint3d *points, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            extend(points[i]);
    }

    void extend(const std::vector<AcGePoint3d> &points)
    {
        extend(points.data(), points.size());
    }

    bool isEmpty() const { return m_empty; }
};

struct AcGeTol
{
    double equalPoint = 1.0e-10;
    double equalVector = 1.0e-10;

    void setEqualPoint(double value) { equalPoint = value; }
    void setEqualVector(double value) { equalVector = value; }
};
