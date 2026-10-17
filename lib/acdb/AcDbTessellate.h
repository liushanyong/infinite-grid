#pragma once

#include "AcDbPort.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/norm.hpp>

#include "acdb/AcDbArc.h"
#include "acdb/AcDbBlockReference.h"
#include "acdb/AcDbCircle.h"
#include "acdb/AcDbEllipse.h"
#include "acdb/AcDbCore.h"
#include "acdb/AcDbHatch.h"
#include "acdb/AcDbLight.h"
#include "acdb/AcDbLine.h"
#include "acdb/AcDbPolyline.h"
#include "acdb/AcDbMText.h"
#include "acdb/AcDbPolyFaceMesh.h"
#include "acdb/AcDbMline.h"
#include "acdb/AcDbPoint.h"
#include "acdb/AcDb3dPolyline.h"
#include "acdb/AcDbRay.h"
#include "acdb/AcDbSolid.h"
#include "acdb/AcDb3dSolid.h"
#include "acdb/AcDbSpline.h"
#include "acdb/AcDbText.h"
#include "acdb/AcDbXline.h"
#include "../acge/AcGeNurbsSpline.h"

namespace acdb
{

struct TesselationOptions
{
    // OpenCADStudio samples analytic curves with kernel DEFAULT_ANGLE. Its
    // direct arc-band paths use 24 segments per full turn, which is pi/12.
    double angleStep = glm::pi<double>() / 12.0;
    double minSegmentLength = 1.0e-9;
    int minSegments = 4;
    int maxSegments = 256;
    double rayLength = 1.0e6;
};

struct Stroke
{
    AcDbEntity common;
    std::vector<glm::dvec3> points;
    bool closed = false;
    double lineWeight = 0.0;
    // The stored endpoint is only a tessellation proxy. Renderers and pickers
    // must treat the geometry as starting at points[0] and continuing forever
    // along points[1] - points[0].
    bool semiInfinite = false;
    AcDbObjectId layerId;
    AcDbObjectId linetypeId;
    Adesk::LongPtr selectionMarker = -1;
    short colorIndex = 0;
    bool hasColorIndex = false;
};

struct Triangle
{
    AcDbEntity common;
    glm::dvec3 a;
    glm::dvec3 b;
    glm::dvec3 c;
    bool is3DFace = false;
    std::array<glm::vec4, 3> vertexColors{};
    std::array<AcGeVector3d, 3> vertexNormals{};
    bool hasVertexColors = false;
    bool hasVertexNormals = false;
    AcDbObjectId layerId;
    AcDbObjectId materialId;
    Adesk::LongPtr selectionMarker = -1;
    short colorIndex = 0;
    bool hasColorIndex = false;
    std::array<AcGePoint3d, 3> allMappingCoords{};
    std::array<AcGePoint3d, 3> mapMappingCoords{};
    std::array<glm::vec2, 3> textureCoords{};
    bool hasAllMappingCoords = false;
    bool hasMapMappingCoords = false;
    bool hasTextureCoords = false;
    std::uint8_t mapperUTiling = 0;
    std::uint8_t mapperVTiling = 0;
    std::uint32_t imageIndex = 0;
    bool imageFallback = false;
};

struct RasterImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
    std::uint64_t contentHash = 0;
};

struct TessellatedPoint
{
    glm::dvec3 location{0.0};
    AcDbEntity common;
    double pointSize = 7.0;
    acge::PointDisplayStyle displayStyle{acge::PointDisplayStyle::Circle};
};

struct TessellatedEntity
{
    std::vector<Stroke> strokes;
    std::vector<Triangle> fills;
    std::vector<TessellatedPoint> points;
    std::vector<RasterImage> images;
};

[[nodiscard]] inline glm::dvec3 normalizedOr(
    const glm::dvec3 &value, const glm::dvec3 &fallback)
{
    const double lengthSquared = glm::dot(value, value);
    if (!(lengthSquared > 1.0e-18) || !std::isfinite(lengthSquared))
        return fallback;
    return value / std::sqrt(lengthSquared);
}

[[nodiscard]] inline glm::dvec3 planeBasisU(const glm::dvec3 &normal)
{
    // AutoCAD arbitrary axis algorithm: the OCS x-axis for DWG planar
    // entities, so arc/ellipse angle parameters match libredwg data.
    const glm::dvec3 n =
        normalizedOr(normal, glm::dvec3(0.0, 0.0, 1.0));
    if (std::abs(n.x) < 1.0 / 64.0 && std::abs(n.y) < 1.0 / 64.0)
        return normalizedOr(glm::cross(glm::dvec3(0.0, 1.0, 0.0), n),
                            glm::dvec3(1.0, 0.0, 0.0));
    return normalizedOr(glm::cross(glm::dvec3(0.0, 0.0, 1.0), n),
                        glm::dvec3(1.0, 0.0, 0.0));
}

[[nodiscard]] inline glm::dvec3 planeBasisV(const glm::dvec3 &normal)
{
    const glm::dvec3 n =
        normalizedOr(normal, glm::dvec3(0.0, 0.0, 1.0));
    return normalizedOr(glm::cross(n, planeBasisU(normal)),
                        glm::dvec3(0.0, 1.0, 0.0));
}

int curveSegmentCount(double sweep, const TesselationOptions &options);

Stroke &addStroke(TessellatedEntity &result, bool closed = false);

void appendSegment(TessellatedEntity &result,
                          const glm::dvec3 &start,
                          const glm::dvec3 &end);

void tessellate(const AcDbLine &line, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbArc &arc, TessellatedEntity &result,
                       const TesselationOptions &options = {});

void tessellate(const AcDbCircle &circle, TessellatedEntity &result,
                       const TesselationOptions &options = {});

void tessellate(const AcDbEllipse &ellipse, TessellatedEntity &result,
                       const TesselationOptions &options = {});

[[nodiscard]] inline glm::dvec3 bulgeArcPoint(const glm::dvec3 &start,
                                              const glm::dvec3 &end,
                                              double bulge, double t,
                                              const glm::dvec3 &normal)
{
    const glm::dvec3 chord = end - start;
    const double chordLength = glm::length(chord);
    if (chordLength < 1.0e-12 || std::abs(bulge) < 1.0e-12)
        return start + chord * t;

    const double b2 = bulge * bulge;
    const double radius = chordLength * (1.0 + b2) / (4.0 * std::abs(bulge));
    const double centerDistance = radius * (1.0 - b2) / (1.0 + b2);
    const glm::dvec3 safeNormal =
        normalizedOr(normal, glm::dvec3(0.0, 0.0, 1.0));
    const glm::dvec3 perp = normalizedOr(
        glm::cross(safeNormal, chord), planeBasisV(safeNormal));
    const glm::dvec3 center = (start + end) * 0.5 +
        glm::sign(bulge) * centerDistance * perp;
    const double startAngle = std::atan2(glm::dot(start - center, planeBasisV(normal)),
                                         glm::dot(start - center, planeBasisU(normal)));
    const double endAngle = std::atan2(glm::dot(end - center, planeBasisV(normal)),
                                       glm::dot(end - center, planeBasisU(normal)));
    double sweep = endAngle - startAngle;
    if (bulge > 0.0 && sweep <= 0.0)
        sweep += glm::two_pi<double>();
    if (bulge < 0.0 && sweep >= 0.0)
        sweep -= glm::two_pi<double>();
    const double angle = startAngle + sweep * t;
    return center + planeBasisU(normal) * (radius * std::cos(angle)) +
           planeBasisV(normal) * (radius * std::sin(angle));
}

// The tessellated outline (bulge arcs subdivided) shared by the direct
// tessellation path and the AcGiWorldDraw callback path.
template <typename PolylineType>
inline std::vector<AcGePoint3d> polylineOutline(
    const PolylineType &polyline, const TesselationOptions &options)
{
    std::vector<AcGePoint3d> outline;
    const size_t vertexCount = polyline.vertices.size();
    if (vertexCount < 2)
        return outline;
    const glm::dvec3 normal = normalizedOr(
        glm::dvec3(polyline.normal), glm::dvec3(0.0, 0.0, 1.0));
    outline.push_back(pointAt(polyline, 0));
    const size_t segmentCount = polyline.closed ? vertexCount : vertexCount - 1;
    for (size_t i = 0; i < segmentCount; ++i)
    {
        const size_t next = (i + 1) % vertexCount;
        const glm::dvec3 start = pointAt(polyline, i);
        const glm::dvec3 end = pointAt(polyline, next);
        const double bulge = i < polyline.bulges.size()
                                 ? polyline.bulges[i]
                                 : 0.0;
        if (std::abs(bulge) < 1.0e-12)
        {
            if (i != 0)
                outline.push_back(start);
            outline.push_back(end);
            continue;
        }

        const double included = 4.0 * std::atan(std::abs(bulge));
        const int segments = curveSegmentCount(included, options);
        for (int s = 1; s <= segments; ++s)
        {
            const double t = static_cast<double>(s) / segments;
            outline.push_back(
                bulgeArcPoint(start, end, bulge, t, normal));
        }
    }
    return outline;
}

// Wall quads for non-zero polyline thickness, expressed as triangles so the
// AcGi callback path can emit them via worldTriangle.
template <typename PolylineType>
inline std::vector<Triangle> polylineWallTriangles(
    const PolylineType &polyline, const std::vector<AcGePoint3d> &outline)
{
    std::vector<Triangle> walls;
    if (std::abs(polyline.thickness) <= 1.0e-12 || outline.size() < 2)
        return walls;
    const glm::dvec3 normal = normalizedOr(
        glm::dvec3(polyline.normal), glm::dvec3(0.0, 0.0, 1.0));
    const glm::dvec3 offset = normal * polyline.thickness;
    const size_t wallCount =
        polyline.closed ? outline.size() : outline.size() - 1;
    walls.reserve(wallCount * 2);
    for (size_t i = 0; i < wallCount; ++i)
    {
        const glm::dvec3 &a = outline[i];
        const glm::dvec3 &b = outline[(i + 1) % outline.size()];
        walls.push_back(Triangle{{}, a, b, b + offset});
        walls.push_back(Triangle{{}, a, b + offset, a + offset});
    }
    return walls;
}

template <typename PolylineType>
inline void tessellatePolyline(const PolylineType &polyline,
                               TessellatedEntity &result,
                               const TesselationOptions &options)
{
    const std::vector<AcGePoint3d> outline = polylineOutline(polyline, options);
    if (outline.size() < 2)
        return;
    Stroke &stroke = addStroke(result, polyline.closed);
    for (const AcGePoint3d &point : outline)
        stroke.points.push_back(point);

    // Non-zero thickness extrudes the outline into wall quads along the
    // polyline normal, so the entity reads as a 3D ribbon instead of a
    // flat curve.  Bulge arcs are subdivided first, so curved segments get
    // per-span walls.
    for (const Triangle &wall : polylineWallTriangles(polyline, outline))
        result.fills.push_back(wall);
}

glm::dvec3 pointAt(const AcDb3dPolyline &polyline, size_t index);

glm::dvec3 pointAt(const AcDbPolyline &polyline, size_t index);

void tessellate(const AcDb3dPolyline &polyline, TessellatedEntity &result,
                       const TesselationOptions &options = {});

void tessellate(const AcDbPolyline &polyline, TessellatedEntity &result,
                       const TesselationOptions &options = {});

[[nodiscard]] inline glm::dvec3 bsplinePoint(const AcDbSpline &spline, double u)
{
    const int degree = std::clamp(spline.degree, 1, 3);
    const size_t controlCount = spline.controlPoints.size();
    if (controlCount < static_cast<size_t>(degree) + 1)
        return glm::dvec3(0.0);
    if (u < 0.0) u = 0.0;
    if (u > 1.0) u = 1.0;
    size_t span = static_cast<size_t>(degree);
    // A clamped B-spline uses the last control-point span at u == 1.  Using
    // the knot count here advances one span too far for four-point cubics.
    const size_t lastSpan = controlCount - 1;
    while (span < lastSpan && u >= spline.knots[span + 1])
        ++span;
    std::vector<glm::dvec3> d(static_cast<size_t>(degree) + 1);
    for (int j = 0; j <= degree; ++j)
        d[j] = spline.controlPoints[span - static_cast<size_t>(degree) + j];
    for (int r = 1; r <= degree; ++r)
    {
        for (int j = degree; j >= r; --j)
        {
            const size_t knotIndex = span - static_cast<size_t>(degree) +
                                     static_cast<size_t>(j);
            const double denominator = spline.knots[knotIndex + static_cast<size_t>(degree) -
                                                    static_cast<size_t>(r) + 1] -
                                       spline.knots[knotIndex];
            const double alpha = denominator > 1.0e-12
                                     ? (u - spline.knots[knotIndex]) / denominator
                                     : 0.0;
            d[j] = (1.0 - alpha) * d[j - 1] + alpha * d[j];
        }
    }
    return d[degree];
}

// Build (or evaluate) the fit-point spline: chord-length parametrized
// natural cubic interpolation, periodic for closed splines.
AcGeFitSpline3d fitPointSpline(const AcDbSpline &spline);

std::vector<AcGePoint3d> sampleFitPointSpline(
    const AcDbSpline &spline, const TesselationOptions &options);

void tessellate(const AcDbSpline &spline, TessellatedEntity &result,
                       const TesselationOptions &options = {});

void tessellate(const AcDbRay &ray, TessellatedEntity &result,
                       const TesselationOptions &options = {});

// An AcDbXline is infinite in both directions.  It tessellates as two opposite
// semi-infinite strokes sharing the base point, so every consumer that
// already understands Stroke::semiInfinite (rendering, picking, slab depth)
// handles it without protocol changes.
void tessellate(const AcDbXline &xline, TessellatedEntity &result,
                       const TesselationOptions &options = {});

// AcDbLine pattern scanlines in the pattern frame (patternAngle rotates it,
// patternScale widens the spacing), clipped to the loops with the even-odd
// rule so inner loops punch real holes in the pattern.  Shared by the
// tessellation path and the AcGiWorldDraw callback path.
std::vector<std::pair<AcGePoint3d, AcGePoint3d>> hatchPatternSegments(
    const AcDbHatch &hatch);

void tessellate(const AcDbHatch &hatch, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbSolid &solid, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbPolyFaceMesh &mesh, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbPoint &point, TessellatedEntity &result,
                       const TesselationOptions & = {});

// An AcDbBlockReference carries no geometry of its own: its members are
// rendered through the nested-instance walk (walkInsertInstances), and
// the reference itself contributes an empty fragment.
void tessellate(const AcDbBlockReference &, TessellatedEntity &,
                       const TesselationOptions &);

void tessellate(const AcDbMline &mline, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbText &text, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbMText &text, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDb3dSolid &solid, TessellatedEntity &result,
                       const TesselationOptions & = {});

void tessellate(const AcDbLight &light, TessellatedEntity &result,
                       const TesselationOptions & = {});

} // namespace acdb
