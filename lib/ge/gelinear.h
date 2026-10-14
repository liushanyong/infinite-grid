#pragma once

#include <algorithm>

#include "gepoint.h"

// Source-level AcGe linear entity subset: infinite lines and rays in 2D/3D.
// Directions are not implicitly normalized; parameters use the supplied vector.
struct AcGeLine2d
{
    AcGePoint2d basePoint{0.0, 0.0};
    AcGeVector2d direction{1.0, 0.0};

    static const AcGeLine2d kXAxis;
    static const AcGeLine2d kYAxis;

    AcGeLine2d() = default;
    AcGeLine2d(const AcGePoint2d &point, const AcGeVector2d &vector)
        : basePoint(point), direction(vector) {}
    AcGeLine2d(const AcGePoint2d &first, const AcGePoint2d &second)
        : basePoint(first), direction(second - first) {}

    AcGeLine2d &set(const AcGePoint2d &point, const AcGeVector2d &vector)
    {
        basePoint = point;
        direction = vector;
        return *this;
    }
    AcGeLine2d &set(const AcGePoint2d &first, const AcGePoint2d &second)
    {
        return set(first, second - first);
    }
    AcGePoint2d pointAt(double parameter) const
    {
        return basePoint + direction * parameter;
    }
};

inline const AcGeLine2d AcGeLine2d::kXAxis{
    AcGePoint2d{0.0, 0.0}, AcGeVector2d{1.0, 0.0}};
inline const AcGeLine2d AcGeLine2d::kYAxis{
    AcGePoint2d{0.0, 0.0}, AcGeVector2d{0.0, 1.0}};

struct AcGeLine3d
{
    AcGePoint3d basePoint{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};

    static const AcGeLine3d kXAxis;
    static const AcGeLine3d kYAxis;
    static const AcGeLine3d kZAxis;

    AcGeLine3d() = default;
    AcGeLine3d(const AcGePoint3d &point, const AcGeVector3d &vector)
        : basePoint(point), direction(vector) {}
    AcGeLine3d(const AcGePoint3d &first, const AcGePoint3d &second)
        : basePoint(first), direction(second - first) {}

    AcGeLine3d &set(const AcGePoint3d &point, const AcGeVector3d &vector)
    {
        basePoint = point;
        direction = vector;
        return *this;
    }
    AcGeLine3d &set(const AcGePoint3d &first, const AcGePoint3d &second)
    {
        return set(first, second - first);
    }
    AcGePoint3d pointAt(double parameter) const
    {
        return basePoint + direction * parameter;
    }
};

inline const AcGeLine3d AcGeLine3d::kXAxis{
    AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{1.0, 0.0, 0.0}};
inline const AcGeLine3d AcGeLine3d::kYAxis{
    AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 1.0, 0.0}};
inline const AcGeLine3d AcGeLine3d::kZAxis{
    AcGePoint3d{0.0, 0.0, 0.0}, AcGeVector3d{0.0, 0.0, 1.0}};

struct AcGeRay2d
{
    AcGePoint2d basePoint{0.0, 0.0};
    AcGeVector2d direction{1.0, 0.0};

    AcGeRay2d() = default;
    AcGeRay2d(const AcGePoint2d &point, const AcGeVector2d &vector)
        : basePoint(point), direction(vector) {}
    AcGeRay2d(const AcGePoint2d &start, const AcGePoint2d &through)
        : basePoint(start), direction(through - start) {}

    AcGeRay2d &set(const AcGePoint2d &point, const AcGeVector2d &vector)
    {
        basePoint = point;
        direction = vector;
        return *this;
    }
    AcGeRay2d &set(const AcGePoint2d &start, const AcGePoint2d &through)
    {
        return set(start, through - start);
    }
    AcGePoint2d pointAt(double parameter) const
    {
        return basePoint + direction * std::max(0.0, parameter);
    }
};

struct AcGeRay3d
{
    AcGePoint3d basePoint{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};

    AcGeRay3d() = default;
    AcGeRay3d(const AcGePoint3d &point, const AcGeVector3d &vector)
        : basePoint(point), direction(vector) {}
    AcGeRay3d(const AcGePoint3d &start, const AcGePoint3d &through)
        : basePoint(start), direction(through - start) {}

    AcGeRay3d &set(const AcGePoint3d &point, const AcGeVector3d &vector)
    {
        basePoint = point;
        direction = vector;
        return *this;
    }
    AcGeRay3d &set(const AcGePoint3d &start, const AcGePoint3d &through)
    {
        return set(start, through - start);
    }
    AcGePoint3d pointAt(double parameter) const
    {
        return basePoint + direction * std::max(0.0, parameter);
    }
};
