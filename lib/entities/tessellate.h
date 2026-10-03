#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <vector>

#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/norm.hpp>

#include "arc.h"
#include "circle.h"
#include "ellipse.h"
#include "entity_common.h"
#include "hatch.h"
#include "light.h"
#include "line.h"
#include "lwpolyline.h"
#include "mtext.h"
#include "mesh.h"
#include "mline.h"
#include "point.h"
#include "polyline.h"
#include "ray.h"
#include "solid.h"
#include "solid3d.h"
#include "spline.h"
#include "text.h"
#include "xline.h"

namespace entities
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
    EntityCommon common;
    std::vector<glm::dvec3> points;
    bool closed = false;
    double lineWeight = 0.0;
    // The stored endpoint is only a tessellation proxy. Renderers and pickers
    // must treat the geometry as starting at points[0] and continuing forever
    // along points[1] - points[0].
    bool semiInfinite = false;
};

struct Triangle
{
    EntityCommon common;
    glm::dvec3 a;
    glm::dvec3 b;
    glm::dvec3 c;
    bool is3DFace = false;
};

struct TessellatedPoint
{
    glm::dvec3 location{0.0};
    EntityCommon common;
    double pointSize = 7.0;
};

struct TessellatedEntity
{
    std::vector<Stroke> strokes;
    std::vector<Triangle> fills;
    std::vector<TessellatedPoint> points;
};

[[nodiscard]] inline glm::dvec3 planeBasisU(const glm::dvec3 &normal)
{
    // AutoCAD arbitrary axis algorithm: the OCS x-axis for DWG planar
    // entities, so arc/ellipse angle parameters match libredwg data.
    const glm::dvec3 n = glm::normalize(normal);
    if (std::abs(n.x) < 1.0 / 64.0 && std::abs(n.y) < 1.0 / 64.0)
        return glm::normalize(glm::cross(glm::dvec3(0.0, 1.0, 0.0), n));
    return glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), n));
}

[[nodiscard]] inline glm::dvec3 planeBasisV(const glm::dvec3 &normal)
{
    return glm::normalize(glm::cross(glm::normalize(normal), planeBasisU(normal)));
}

inline int curveSegmentCount(double sweep, const TesselationOptions &options)
{
    sweep = std::abs(sweep);
    const int byAngle = static_cast<int>(std::ceil(sweep / options.angleStep));
    return std::clamp(std::max(byAngle, options.minSegments),
                      options.minSegments, options.maxSegments);
}

inline Stroke &addStroke(TessellatedEntity &result, bool closed = false)
{
    result.strokes.push_back(Stroke{});
    result.strokes.back().closed = closed;
    return result.strokes.back();
}

inline void appendSegment(TessellatedEntity &result,
                          const glm::dvec3 &start,
                          const glm::dvec3 &end)
{
    if (glm::distance2(start, end) > 1.0e-18)
        addStroke(result).points = {start, end};
}

inline void tessellate(const Line &line, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    // DWG lines are authored in the OCS of their extrusion normal; the
    // default (0,0,1) extrusion is the identity mapping.
    const AcGeVector3d identityNormal(0.0, 0.0, 1.0);
    const bool identityOcs = line.extrusion == identityNormal;
    const glm::dvec3 start =
        identityOcs ? glm::dvec3(line.start) : glm::dvec3(transformBy(line.start, setToPlaneToWorld(line.extrusion)));
    const glm::dvec3 end =
        identityOcs ? glm::dvec3(line.end) : glm::dvec3(transformBy(line.end, setToPlaneToWorld(line.extrusion)));
    if (std::abs(line.thickness) <= 1.0e-12)
    {
        appendSegment(result, start, end);
        return;
    }
    const glm::dvec3 offset =
        identityOcs ? glm::dvec3(0.0, 0.0, line.thickness)
                    : glm::dvec3(line.extrusion.normal()) * line.thickness;
    result.fills.push_back(Triangle{{}, start, end, end + offset});
    result.fills.push_back(Triangle{{}, start, end + offset, start + offset});
}

inline void tessellate(const Arc &arc, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    if (!(arc.radius > 0.0) || !std::isfinite(arc.radius))
        return;
    const glm::dvec3 u = planeBasisU(arc.normal);
    const glm::dvec3 v = planeBasisV(arc.normal);
    double sweep = std::fmod(arc.endAngle - arc.startAngle,
                             glm::two_pi<double>());
    if (sweep <= 0.0)
        sweep += glm::two_pi<double>();
    const int segments = curveSegmentCount(sweep, options);
    Stroke &stroke = addStroke(result);
    stroke.points.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i)
    {
        const double t = static_cast<double>(i) / segments;
        const double angle = arc.startAngle + sweep * t;
        stroke.points.push_back(arc.center +
            u * (arc.radius * std::cos(angle)) +
            v * (arc.radius * std::sin(angle)));
    }

    // DWG arcs carry an extrusion thickness: the curve extrudes into wall
    // quads along the normal, one per span so curved segments stay smooth.
    if (std::abs(arc.thickness) > 1.0e-12)
    {
        const glm::dvec3 offset = glm::dvec3(arc.normal.normal()) * arc.thickness;
        for (size_t i = 0; i + 1 < stroke.points.size(); ++i)
        {
            const glm::dvec3 &a = stroke.points[i];
            const glm::dvec3 &b = stroke.points[i + 1];
            result.fills.push_back(Triangle{{}, a, b, b + offset});
            result.fills.push_back(Triangle{{}, a, b + offset, a + offset});
        }
    }
}

inline void tessellate(const Circle &circle, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    if (!(circle.radius > 0.0) || !std::isfinite(circle.radius))
        return;

    Arc arc;
    arc.center = circle.center;
    arc.radius = circle.radius;
    arc.startAngle = 0.0;
    arc.endAngle = glm::two_pi<double>();
    arc.normal = circle.normal;
    arc.thickness = circle.thickness;
    const size_t strokeBefore = result.strokes.size();
    tessellate(arc, result, options);
    if (result.strokes.size() > strokeBefore)
        result.strokes.back().closed = true;
}

inline void tessellate(const Ellipse &ellipse, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    const glm::dvec3 major = ellipse.majorAxis;
    const double majorLength = glm::length(major);
    if (!(majorLength > 0.0) || !std::isfinite(majorLength) ||
        !(ellipse.radiusRatio > 0.0) ||
        !std::isfinite(ellipse.radiusRatio))
        return;
    const glm::dvec3 n = glm::normalize(glm::dvec3(ellipse.normal));
    const glm::dvec3 u = major / majorLength;
    const glm::dvec3 v = glm::normalize(glm::cross(n, u));
    double sweep = std::fmod(ellipse.endParameter - ellipse.startParameter,
                             glm::two_pi<double>());
    if (sweep <= 0.0)
        sweep += glm::two_pi<double>();
    const int segments = curveSegmentCount(sweep, options);
    Stroke &stroke = addStroke(result);
    stroke.points.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i)
    {
        const double t = static_cast<double>(i) / segments;
        const double angle = ellipse.startParameter + sweep * t;
        stroke.points.push_back(ellipse.center +
            u * (majorLength * std::cos(angle)) +
            v * (majorLength * ellipse.radiusRatio * std::sin(angle)));
    }
    if (std::abs(sweep - glm::two_pi<double>()) < 1.0e-12)
        stroke.closed = true;
}

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
    const glm::dvec3 perp = glm::normalize(glm::cross(normal, chord));
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

template <typename PolylineType>
inline void tessellatePolyline(const PolylineType &polyline,
                               TessellatedEntity &result,
                               const TesselationOptions &options)
{
    const size_t vertexCount = polyline.vertices.size();
    if (vertexCount < 2)
        return;
    const glm::dvec3 normal = glm::dvec3(polyline.normal.normal());
    Stroke &stroke = addStroke(result, polyline.closed);
    stroke.points.push_back(pointAt(polyline, 0));
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
                stroke.points.push_back(start);
            stroke.points.push_back(end);
            continue;
        }

        const double chordLength = glm::length(end - start);
        const double b2 = bulge * bulge;
        const double included = 4.0 * std::atan(std::abs(bulge));
        const int segments = curveSegmentCount(included, options);
        for (int s = 1; s <= segments; ++s)
        {
            const double t = static_cast<double>(s) / segments;
            stroke.points.push_back(
                bulgeArcPoint(start, end, bulge, t, normal));
        }
        (void)chordLength;
        (void)b2;
    }

    // Non-zero thickness extrudes the outline into wall quads along the
    // polyline normal, so the entity reads as a 3D ribbon instead of a
    // flat curve.  Bulge arcs are subdivided first, so curved segments get
    // per-span walls.
    if (std::abs(polyline.thickness) > 1.0e-12 && stroke.points.size() >= 2)
    {
        const glm::dvec3 offset = normal * polyline.thickness;
        const size_t wallCount = polyline.closed
                                     ? stroke.points.size()
                                     : stroke.points.size() - 1;
        for (size_t i = 0; i < wallCount; ++i)
        {
            const glm::dvec3 &a = stroke.points[i];
            const glm::dvec3 &b =
                stroke.points[(i + 1) % stroke.points.size()];
            result.fills.push_back(Triangle{{}, a, b, b + offset});
            result.fills.push_back(Triangle{{}, a, b + offset, a + offset});
        }
    }
}

inline glm::dvec3 pointAt(const Polyline &polyline, size_t index)
{
    return polyline.vertices.at(index);
}

inline glm::dvec3 pointAt(const LwPolyline &polyline, size_t index)
{
    const glm::dvec2 value = polyline.vertices.at(index);
    return glm::dvec3(value, polyline.elevation);
}

inline void tessellate(const Polyline &polyline, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    tessellatePolyline(polyline, result, options);
}

inline void tessellate(const LwPolyline &polyline, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    tessellatePolyline(polyline, result, options);
}

[[nodiscard]] inline glm::dvec3 bsplinePoint(const Spline &spline, double u)
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

inline void tessellate(const Spline &spline, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    Stroke &stroke = addStroke(result, spline.closed);
    if (!spline.controlPoints.empty() &&
        spline.knots.size() >= spline.controlPoints.size() + spline.degree + 1)
    {
        constexpr int kSegments = 64;
        for (int i = 0; i <= kSegments; ++i)
        {
            const double t = static_cast<double>(i) / kSegments;
            glm::dvec3 point = bsplinePoint(spline, t);
            if (!std::isfinite(point.x + point.y + point.z))
                point = spline.controlPoints.front();
            stroke.points.push_back(point);
        }
        return;
    }

    // Fit-point splines without explicit knots use a stable C1 fallback until
    // the full NURBS interpolation bridge is introduced.
    const size_t fitCount = spline.fitPoints.size();
    if (fitCount < 2)
    {
        result.strokes.pop_back();
        return;
    }
    auto fitPoint = [&](long long index) {
        if (spline.closed)
        {
            index %= static_cast<long long>(fitCount);
            if (index < 0)
                index += static_cast<long long>(fitCount);
        }
        else
        {
            index = std::clamp<long long>(index, 0, static_cast<long long>(fitCount) - 1);
        }
        return spline.fitPoints[static_cast<size_t>(index)];
    };
    const int segmentsPerSpan = curveSegmentCount(
        glm::pi<double>() / static_cast<double>(std::max<size_t>(fitCount, 1)),
        options);
    const long long spans = spline.closed ? static_cast<long long>(fitCount)
                                          : static_cast<long long>(fitCount) - 1;
    for (long long span = 0; span < spans; ++span)
    {
        const glm::dvec3 p0 = fitPoint(span - 1);
        const glm::dvec3 p1 = fitPoint(span);
        const glm::dvec3 p2 = fitPoint(span + 1);
        const glm::dvec3 p3 = fitPoint(span + 2);
        for (int s = span == 0 ? 0 : 1; s <= segmentsPerSpan; ++s)
        {
            const double t = static_cast<double>(s) / segmentsPerSpan;
            const double t2 = t * t;
            const double t3 = t2 * t;
            stroke.points.push_back(0.5 * ((2.0 * p1) +
                (-p0 + p2) * t +
                (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3));
        }
    }
}

inline void tessellate(const Ray &ray, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    const double directionLength = ray.direction.length();
    if (directionLength <= 1.0e-18)
        return;

    Stroke &stroke = addStroke(result);
    stroke.points = {ray.start,
                     ray.start + ray.direction / directionLength *
                                      options.rayLength};
    stroke.semiInfinite = true;
}

// An XLine is infinite in both directions.  It tessellates as two opposite
// semi-infinite strokes sharing the base point, so every consumer that
// already understands Stroke::semiInfinite (rendering, picking, slab depth)
// handles it without protocol changes.
inline void tessellate(const XLine &xline, TessellatedEntity &result,
                       const TesselationOptions &options = {})
{
    const double directionLength = xline.direction.length();
    if (directionLength <= 1.0e-18)
        return;

    const AcGeVector3d offset =
        xline.direction / directionLength * options.rayLength;
    Stroke &forward = addStroke(result);
    forward.points = {xline.point, xline.point + offset};
    forward.semiInfinite = true;
    Stroke &backward = addStroke(result);
    backward.points = {xline.point, xline.point - offset};
    backward.semiInfinite = true;
}

inline void tessellate(const Hatch &hatch, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    if (hatch.outerLoop.size() < 3)
        return;
    if (hatch.solidFill || hatch.patternName == "SOLID")
    {
        for (size_t i = 1; i + 1 < hatch.outerLoop.size(); ++i)
        {
            result.fills.push_back(Triangle{{}, hatch.outerLoop[0],
                                             hatch.outerLoop[i],
                                             hatch.outerLoop[i + 1]});
        }
        return;
    }

    // Line patterns are generated as parallel scanlines in the pattern
    // frame (patternAngle rotates it, patternScale widens the spacing) and
    // clipped to the loops with the even-odd rule, so inner loops punch
    // real holes in the pattern instead of being ignored.
    const glm::dvec3 origin = hatch.outerLoop.front();
    const glm::dvec3 u(std::cos(hatch.patternAngle),
                       std::sin(hatch.patternAngle), 0.0);
    const glm::dvec3 v(-std::sin(hatch.patternAngle),
                       std::cos(hatch.patternAngle), 0.0);

    std::vector<std::vector<glm::dvec2>> loops;
    glm::dvec2 low(0.0, 0.0);
    glm::dvec2 high(0.0, 0.0);
    auto addLoop = [&](const std::vector<AcGePoint3d> &loop) {
        if (loop.size() < 3)
            return;
        std::vector<glm::dvec2> flat;
        flat.reserve(loop.size());
        for (const glm::dvec3 &point : loop)
        {
            const AcGeVector3d delta = point - origin;
            flat.emplace_back(delta.dotProduct(u), delta.dotProduct(v));
            low = glm::min(low, flat.back());
            high = glm::max(high, flat.back());
        }
        loops.push_back(std::move(flat));
    };
    addLoop(hatch.outerLoop);
    for (const std::vector<AcGePoint3d> &loop : hatch.innerLoops)
        addLoop(loop);
    if (loops.empty())
        return;

    const double spacing = std::max(1.0e-3, hatch.patternScale * 16.0);
    for (double y = low.y + spacing * 0.5; y < high.y; y += spacing)
    {
        std::vector<double> crossings;
        for (const std::vector<glm::dvec2> &loop : loops)
        {
            for (size_t i = 0; i < loop.size(); ++i)
            {
                const glm::dvec2 &a = loop[i];
                const glm::dvec2 &b = loop[(i + 1) % loop.size()];
                if ((a.y > y) == (b.y > y))
                    continue;
                crossings.push_back(
                    a.x + (y - a.y) / (b.y - a.y) * (b.x - a.x));
            }
        }
        std::sort(crossings.begin(), crossings.end());
        for (size_t i = 0; i + 1 < crossings.size(); i += 2)
        {
            if (crossings[i + 1] - crossings[i] <= 1.0e-9)
                continue;
            Stroke &stroke = addStroke(result);
            stroke.points = {
                origin + u * crossings[i] + v * y,
                origin + u * crossings[i + 1] + v * y};
        }
    }
}

inline void tessellate(const Solid &solid, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    result.fills.push_back(
        Triangle{{}, solid.firstCorner, solid.secondCorner, solid.thirdCorner});
    result.fills.push_back(
        Triangle{{}, solid.secondCorner, solid.fourthCorner, solid.thirdCorner});
}

inline void tessellate(const Mesh &mesh, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    const auto &positions = mesh.geometry.positions;
    const auto &indices = mesh.geometry.indices;
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const uint32_t a = indices[i];
        const uint32_t b = indices[i + 1];
        const uint32_t c = indices[i + 2];
        if (a >= positions.size() || b >= positions.size() ||
            c >= positions.size())
        {
            continue;
        }
        result.fills.push_back(Triangle{{}, positions[a], positions[b], positions[c]});
    }
}

inline void tessellate(const Point &point, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    result.points.push_back({point.location});
}

inline void tessellate(const MLine &mline, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    const size_t count = mline.vertices.size();
    if (count < 2)
        return;
    glm::dvec3 normal(0.0, 0.0, 1.0);
    for (size_t i = 0; i + 1 < count; ++i)
    {
        const AcGeVector3d tangent = mline.vertices[i + 1] - mline.vertices[i];
        if (tangent.lengthSq() > 1.0e-18)
        {
            normal = tangent.crossProduct(AcGeVector3d(0.0, 0.0, 1.0)).normal();
            break;
        }
    }
    result.strokes.reserve(result.strokes.size() + 2);
    Stroke &left = addStroke(result);
    Stroke &right = addStroke(result);
    left.points.reserve(count);
    right.points.reserve(count);
    for (const glm::dvec3 &vertex : mline.vertices)
    {
        left.points.push_back(vertex - normal * mline.scale.x);
        right.points.push_back(vertex + normal * mline.scale.x);
    }
    if (!mline.closed)
        return;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t next = (i + 1) % count;
        result.fills.push_back(Triangle{{}, left.points[i], right.points[i],
                                         right.points[next]});
        result.fills.push_back(Triangle{{}, left.points[i], right.points[next],
                                         left.points[next]});
    }
}

inline void tessellate(const Text &text, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    const double width = std::max(double(text.text.size()) *
                                      text.height * 0.6,
                                  text.height);
    const double cosine = std::cos(text.rotation);
    const double sine = std::sin(text.rotation);
    const glm::dvec3 right(cosine, sine, 0.0);
    const glm::dvec3 up(-sine, cosine, 0.0);
    Stroke &frame = addStroke(result, true);
    frame.points.reserve(5);
    frame.points.push_back(text.insertion);
    frame.points.push_back(text.insertion + right * width);
    frame.points.push_back(text.insertion + right * width + up * text.height);
    frame.points.push_back(text.insertion + up * text.height);
    frame.points.push_back(text.insertion);
    result.points.push_back({text.insertion});
}

inline void tessellate(const MText &text, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    if (!(text.height > 0.0) || !std::isfinite(text.height))
        return;
    const size_t lineBreaks = static_cast<size_t>(
        std::count(text.text.begin(), text.text.end(), '\n'));
    const size_t longest = [&] {
        size_t best = 0;
        size_t current = 0;
        for (char character : text.text)
        {
            if (character == '\n')
            {
                best = std::max(best, current);
                current = 0;
            }
            else
            {
                ++current;
            }
        }
        return std::max(best, current);
    }();
    const double width = text.width > 0.0
                             ? text.width
                             : std::max(double(longest) * text.height * 0.6,
                                        text.height);
    const glm::dvec3 right = text.direction.length() > 1.0e-12
                                 ? glm::dvec3(text.direction.normal())
                                 : glm::dvec3(1.0, 0.0, 0.0);
    const glm::dvec3 up = glm::normalize(glm::cross(
        glm::dvec3(0.0, 0.0, 1.0), right));
    Stroke &frame = addStroke(result, true);
    const double frameHeight = text.height *
                               double(std::max(size_t(1), lineBreaks + 1));
    frame.points.reserve(5);
    frame.points.push_back(text.insertion);
    frame.points.push_back(text.insertion + right * width);
    frame.points.push_back(text.insertion + right * width + up * frameHeight);
    frame.points.push_back(text.insertion + up * frameHeight);
    frame.points.push_back(text.insertion);
    result.points.push_back({text.insertion});
}

inline void tessellate(const Solid3d &solid, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    const auto &positions = solid.vertices;

    // Feature edges of the solid become visible border strokes, using the
    // same shared-edge analysis as the solid fill selection outline: an
    // edge used by a single triangle is a boundary, and an edge whose two
    // adjacent triangles are not coplanar is a crease.  Coplanar
    // triangulation diagonals stay hidden, so each flat face keeps one
    // clean border instead of internal mesh edges.
    constexpr double kSolid3dCreaseCosine = 0.9397; // cos(20 degrees)
    struct EdgeKey
    {
        uint32_t a;
        uint32_t b;
        bool operator<(const EdgeKey &other) const
        {
            return a != other.a ? a < other.a : b < other.b;
        }
    };
    std::map<EdgeKey, std::vector<glm::dvec3>> edgeNormals;
    for (size_t i = 0; i + 2 < solid.indices.size(); i += 3)
    {
        const uint32_t a = solid.indices[i];
        const uint32_t b = solid.indices[i + 1];
        const uint32_t c = solid.indices[i + 2];
        if (a >= positions.size() || b >= positions.size() ||
            c >= positions.size())
        {
            continue;
        }
        result.fills.push_back(
            Triangle{{}, positions[a], positions[b], positions[c]});
        const AcGeVector3d normal = (positions[b] - positions[a])
            .crossProduct(positions[c] - positions[a]);
        if (normal.lengthSq() <= 1.0e-24)
            continue; // a degenerate triangle carries no orientation
        const glm::dvec3 unit = glm::dvec3(normal.normal());
        const auto record = [&](uint32_t u, uint32_t v) {
            edgeNormals[{std::min(u, v), std::max(u, v)}].push_back(unit);
        };
        record(a, b);
        record(b, c);
        record(c, a);
    }

    for (const auto &[edge, normals] : edgeNormals)
    {
        const bool visible =
            normals.size() == 1 || // open boundary
            normals.size() > 2 ||  // non-manifold fold
            (normals.size() == 2 &&
             glm::dot(normals.front(), normals.back()) <
                 kSolid3dCreaseCosine); // crease between faces
        if (!visible)
            continue; // coplanar triangulation split stays hidden
        Stroke &stroke = addStroke(result);
        stroke.points = {positions[edge.a], positions[edge.b]};
    }
}

inline void tessellate(const Light &light, TessellatedEntity &result,
                       const TesselationOptions & = {})
{
    appendSegment(result, light.position, light.target);
    result.points.push_back({light.position});
}

} // namespace entities
