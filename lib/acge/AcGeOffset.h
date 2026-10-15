#pragma once

#include "AcGePort.h"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "AcGePolyline.h"

namespace ge::detail
{
std::vector<AcGePoint2d> offsetPolylineVertices(
    const std::vector<AcGePoint2d> &vertices, bool closed, double distance,
    const AcGeTol &tol);
} // namespace ge::detail

// Miter-joined parallel offset of a 2D polyline. Positive distance is to the
// left of each directed segment. Self-intersections are preserved, not
// trimmed.
struct ACGE_PORT AcGeOffsetPolyline2d
{
private:
    AcGePolyline2d m_polyline;
    double m_distance = 0.0;

public:
    AcGeOffsetPolyline2d() = default;
    AcGeOffsetPolyline2d(const AcGePolyline2d &polyline, double distance,
                         const AcGeTol &tol = AcGeTol{})
    {
        set(polyline, distance, tol);
    }

    AcGeOffsetPolyline2d &set(const AcGePolyline2d &polyline, double distance,
                              const AcGeTol &tol = AcGeTol{});

    const AcGePolyline2d &polyline() const { return m_polyline; }
    const std::vector<AcGePoint2d> &vertices() const
    {
        return m_polyline.vertices();
    }
    double distance() const { return m_distance; }
    bool isClosed() const { return m_polyline.isClosed(); }
};

// Miter-joined planar 3D polyline offset. The supplied normal fixes the
// plane and the positive side is normal x segment direction. Input vertices
// must be coplanar within the point tolerance.
struct ACGE_PORT AcGeOffsetPolyline3d
{
private:
    AcGePolyline3d m_polyline;
    double m_distance = 0.0;
    AcGeVector3d m_normal{0.0, 0.0, 1.0};

public:
    AcGeOffsetPolyline3d() = default;
    AcGeOffsetPolyline3d(const AcGePolyline3d &polyline,
                         const AcGeVector3d &normal, double distance,
                         const AcGeTol &tol = AcGeTol{})
    {
        set(polyline, normal, distance, tol);
    }

    AcGeOffsetPolyline3d &set(const AcGePolyline3d &polyline,
                              const AcGeVector3d &normal, double distance,
                              const AcGeTol &tol = AcGeTol{});

    const AcGePolyline3d &polyline() const { return m_polyline; }
    const std::vector<AcGePoint3d> &vertices() const
    {
        return m_polyline.vertices();
    }
    double distance() const { return m_distance; }
    AcGeVector3d normal() const { return m_normal; }
    bool isClosed() const { return m_polyline.isClosed(); }
};
