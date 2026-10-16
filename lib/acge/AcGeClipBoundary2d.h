#pragma once

#include <vector>

#include "AcGePoint.h"

// Convex clipping subset; no ObjectARX array or error-status compatibility.
// Both boundary and polygon inputs must be strictly convex, in either winding.
// The tolerance expands each clipping half-plane by equalPoint() world units.
class ACGE_PORT AcGeClipBoundary2d
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

    static void validatePoint(const AcGePoint2d &point);
    static long double distance(const Edge &edge, const AcGePoint2d &point);
    static std::vector<Edge> makeEdges(std::vector<AcGePoint2d> &vertices);
    static AcGePoint2d interpolate(const AcGePoint2d &a,
                                   const AcGePoint2d &b,
                                   long double parameter);
    long double clippingDistance(const Edge &edge,
                                 const AcGePoint2d &point) const;
    static long double crossingParameter(long double a, long double b);
    void requireBoundary() const;
    bool clipParameters(const AcGePoint2d &start, const AcGePoint2d &end,
                        long double &first, long double &last) const;

public:
    AcGeClipBoundary2d();
    explicit AcGeClipBoundary2d(std::vector<AcGePoint2d> vertices,
                                const AcGeTol &tol = AcGeTol{});
    AcGeClipBoundary2d(const AcGePoint2d &minimum, const AcGePoint2d &maximum,
                       const AcGeTol &tol = AcGeTol{});

    AcGeClipBoundary2d &set(std::vector<AcGePoint2d> vertices,
                            const AcGeTol &tol = AcGeTol{});
    AcGeClipBoundary2d &set(const AcGePoint2d &minimum,
                            const AcGePoint2d &maximum,
                            const AcGeTol &tol = AcGeTol{});

    bool isSet() const { return !m_edges.empty(); }
    const std::vector<AcGePoint2d> &vertices() const { return m_vertices; }

    bool containsPoint(const AcGePoint2d &point) const;

    bool clipSegment(const AcGePoint2d &start, const AcGePoint2d &end,
                     AcGePoint2d &clippedStart,
                     AcGePoint2d &clippedEnd) const;

    std::vector<AcGePoint2d> clipPolygon(
        std::vector<AcGePoint2d> polygon) const;

    // Each returned chain is a separate visible part; outside gaps are not joined.
    std::vector<std::vector<AcGePoint2d>> clipPolyline(
        const std::vector<AcGePoint2d> &polyline) const;
};
