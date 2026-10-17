#include "acgi/AcGiWorldDraw.h"

#include "acgi/AcGiTextEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <limits>
#include <map>
#include <type_traits>
#include <utility>

// Return convention (ObjectARX contract): the primitive emitters return
// kFalse (false) when the primitive was stored, kTrue (true) when the
// operation was terminated — see the AcGiGeometry header note.  Internal
// helpers in this file use plain bool (true = ok) and adapt at the boundary.

namespace
{
constexpr double kTwoPi = 6.2831853071795864769;

long double signedDistance(const AcGePoint2d &edgeStart,
                           const AcGePoint2d &edgeEnd,
                           const AcGePoint2d &point, double winding)
{
    if (!std::isfinite(edgeStart.x) || !std::isfinite(edgeStart.y) ||
        !std::isfinite(edgeEnd.x) || !std::isfinite(edgeEnd.y) ||
        !std::isfinite(point.x) || !std::isfinite(point.y))
        return -std::numeric_limits<long double>::infinity();
    const long double dx =
        static_cast<long double>(edgeEnd.x) - edgeStart.x;
    const long double dy =
        static_cast<long double>(edgeEnd.y) - edgeStart.y;
    const long double length = std::hypot(dx, dy);
    if (!(length > 0.0L) || !std::isfinite(length))
        return -std::numeric_limits<long double>::infinity();
    const long double px =
        static_cast<long double>(point.x) - edgeStart.x;
    const long double py =
        static_cast<long double>(point.y) - edgeStart.y;
    return ((dx * py - dy * px) / length) * winding;
}

std::vector<std::array<size_t, 3>>
triangulatePolygon(const std::vector<AcGePoint3d> &vertices)
{
    if (vertices.size() < 3)
        return {};

    AcGeVector3d normal;
    for (size_t index = 0; index < vertices.size(); ++index)
    {
        const AcGePoint3d &current = vertices[index];
        const AcGePoint3d &next = vertices[(index + 1) % vertices.size()];
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    const double normalLengthSq = normal.lengthSq();
    if (!(normalLengthSq > 1.0e-24) || !std::isfinite(normalLengthSq))
        return {};
    normal = normal.normal();
    const AcGeVector3d u =
        AcGeCircArc3d(vertices.front(), normal, 1.0).referenceAxis();
    const AcGeVector3d v = normal.crossProduct(u);
    std::vector<AcGePoint2d> projected;
    projected.reserve(vertices.size());
    for (const AcGePoint3d &point : vertices)
    {
        const AcGeVector3d relative = point - vertices.front();
        const AcGePoint2d planar(relative.dotProduct(u), relative.dotProduct(v));
        if (!std::isfinite(planar.x) || !std::isfinite(planar.y))
            return {};
        projected.push_back(planar);
    }

    long double areaTwice = 0.0L;
    for (size_t index = 0; index < projected.size(); ++index)
    {
        const AcGePoint2d &current = projected[index];
        const AcGePoint2d &next = projected[(index + 1) % projected.size()];
        areaTwice += static_cast<long double>(current.x) * next.y -
                     static_cast<long double>(current.y) * next.x;
    }
    if (!std::isfinite(areaTwice) || areaTwice == 0.0L)
        return {};
    const long double winding = areaTwice > 0.0L ? 1.0L : -1.0L;
    std::vector<size_t> remaining(vertices.size());
    for (size_t index = 0; index < remaining.size(); ++index)
        remaining[index] = index;

    const auto cross = [&](size_t a, size_t b, size_t c) {
        const long double abx =
            static_cast<long double>(projected[b].x) - projected[a].x;
        const long double aby =
            static_cast<long double>(projected[b].y) - projected[a].y;
        const long double acx =
            static_cast<long double>(projected[c].x) - projected[a].x;
        const long double acy =
            static_cast<long double>(projected[c].y) - projected[a].y;
        return (abx * acy - aby * acx) * winding;
    };
    const auto insideTriangle = [&](size_t point, size_t a, size_t b,
                                    size_t c) {
        return cross(a, b, point) >= -1.0e-12L &&
               cross(b, c, point) >= -1.0e-12L &&
               cross(c, a, point) >= -1.0e-12L;
    };

    std::vector<std::array<size_t, 3>> triangles;
    triangles.reserve(vertices.size() - 2);
    while (remaining.size() > 3)
    {
        bool clippedEar = false;
        for (size_t index = 0; index < remaining.size(); ++index)
        {
            const size_t previous =
                remaining[(index + remaining.size() - 1) % remaining.size()];
            const size_t current = remaining[index];
            const size_t next = remaining[(index + 1) % remaining.size()];
            if (cross(previous, current, next) <= 1.0e-12L)
                continue;
            bool containsVertex = false;
            for (const size_t candidate : remaining)
            {
                if (candidate != previous && candidate != current &&
                    candidate != next &&
                    insideTriangle(candidate, previous, current, next))
                {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex)
                continue;
            triangles.push_back({previous, current, next});
            remaining.erase(remaining.begin() +
                            static_cast<std::ptrdiff_t>(index));
            clippedEar = true;
            break;
        }
        if (!clippedEar)
            return {};
    }
    triangles.push_back({remaining[0], remaining[1], remaining[2]});
    return triangles;
}

double ccwNormalized(double angle)
{
    const double wrapped = std::fmod(angle, kTwoPi);
    return wrapped < 0.0 ? wrapped + kTwoPi : wrapped;
}

// The circumarc through three points, swept CCW from |first| through
// |second| to |third|.  Returns false when the points are collinear.
bool circumArcThrough(const AcGePoint3d &first, const AcGePoint3d &second,
                      const AcGePoint3d &third, AcGeCircArc3d &arc)
{
    const AcGeVector3d a = second - first;
    const AcGeVector3d b = third - first;
    const AcGeVector3d cross = a.crossProduct(b);
    const double crossLengthSq = cross.lengthSq();
    if (!(crossLengthSq > 1.0e-30))
        return false;
    const AcGeVector3d toCenter =
        (b.crossProduct(cross) * a.lengthSq() +
         cross.crossProduct(a) * b.lengthSq()) *
        (0.5 / crossLengthSq);
    const AcGePoint3d center = first + toCenter;
    const double radius = toCenter.length();
    AcGeVector3d normal = cross.normal();

    // CCW sweep first -> third must pass through second; when it does not,
    // flip the plane normal (which mirrors the CCW sense) and re-measure.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const AcGeVector3d u =
            AcGeCircArc3d(center, normal, radius).referenceAxis();
        const AcGeVector3d v = normal.crossProduct(u);
        const auto angleOf = [&](const AcGePoint3d &point) {
            const AcGeVector3d radial = point - center;
            return std::atan2(radial.dotProduct(v), radial.dotProduct(u));
        };
        const double startAngle = angleOf(first);
        const double toSecond = ccwNormalized(angleOf(second) - startAngle);
        const double toThird = ccwNormalized(angleOf(third) - startAngle);
        if (toSecond <= toThird || attempt == 1)
        {
            arc.set(center, normal, radius, startAngle, startAngle + toThird);
            return true;
        }
        normal = -normal;
    }
    return false;
}

std::vector<AcGePoint3d> sampleArcPoints(const acgs::ViewportDraw &context,
                                         const AcGeCircArc3d &arc)
{
    const acdb::TesselationOptions options = context.optionsFor(arc.radius);
    const double sweep = arc.endAngle - arc.startAngle;
    const int segments =
        std::max(8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    std::vector<AcGePoint3d> samples;
    samples.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double t = sweep * static_cast<double>(index) / segments;
        samples.push_back(arc.pointAt(arc.startAngle + t));
    }
    return samples;
}

// Sector/chord closure strokes and the filled fan (ARX fills the closure
// areas per AcGiSubEntityTraits::fillType; the port treats fillType as its
// default kFill).
Adesk::Boolean emitArcClosure(AcGiWorldDraw &graphics,
                              const AcGePoint3d &fanOrigin,
                              const AcGePoint3d &start,
                              const AcGePoint3d &end,
                              const std::vector<AcGePoint3d> &samples,
                              AcGiArcType arcType)
{
    if (arcType == AcGiArcType::kAcGiArcSimple)
        return false;
    if (arcType == AcGiArcType::kAcGiArcSector)
    {
        graphics.worldLine(fanOrigin, start);
        graphics.worldLine(fanOrigin, end);
    }
    else // kAcGiArcChord
    {
        graphics.worldLine(start, end);
    }
    for (size_t index = 1; index < samples.size(); ++index)
        graphics.worldTriangle(fanOrigin, samples[index - 1], samples[index]);
    return false;
}

void applyVertexData(acdb::Triangle &triangle,
                     const std::array<AcGePoint3d, 3> &sourcePoints,
                     const std::array<size_t, 3> &sourceIndices,
                     const AcGiVertexData *vertexData)
{
    if (vertexData == nullptr)
        return;
    const AcGeVector3d edge0 = sourcePoints[1] - sourcePoints[0];
    const AcGeVector3d edge1 = sourcePoints[2] - sourcePoints[0];
    const double d00 = edge0.dotProduct(edge0);
    const double d01 = edge0.dotProduct(edge1);
    const double d11 = edge1.dotProduct(edge1);
    const double denominator = d00 * d11 - d01 * d01;
    if (!(std::abs(denominator) > 1.0e-30) ||
        !std::isfinite(denominator))
        return;

    const std::array<AcGePoint3d, 3> outputPoints{
        triangle.a, triangle.b, triangle.c};
    for (size_t outputIndex = 0; outputIndex < outputPoints.size();
         ++outputIndex)
    {
        const AcGeVector3d relative = outputPoints[outputIndex] - sourcePoints[0];
        const double d20 = relative.dotProduct(edge0);
        const double d21 = relative.dotProduct(edge1);
        const double weight1 = (d11 * d20 - d01 * d21) / denominator;
        const double weight2 = (d00 * d21 - d01 * d20) / denominator;
        const double weight0 = 1.0 - weight1 - weight2;
        if (vertexData->trueColors.size() > sourceIndices[0] &&
            vertexData->trueColors.size() > sourceIndices[1] &&
            vertexData->trueColors.size() > sourceIndices[2])
        {
            triangle.vertexColors[outputIndex] =
                glm::vec4(vertexData->trueColors[sourceIndices[0]].renderColor()) *
                    static_cast<float>(weight0) +
                glm::vec4(vertexData->trueColors[sourceIndices[1]].renderColor()) *
                    static_cast<float>(weight1) +
                glm::vec4(vertexData->trueColors[sourceIndices[2]].renderColor()) *
                    static_cast<float>(weight2);
            triangle.hasVertexColors = true;
        }
        if (vertexData->normals.size() > sourceIndices[0] &&
            vertexData->normals.size() > sourceIndices[1] &&
            vertexData->normals.size() > sourceIndices[2])
        {
            AcGeVector3d normal =
                vertexData->normals[sourceIndices[0]] * weight0 +
                vertexData->normals[sourceIndices[1]] * weight1 +
                vertexData->normals[sourceIndices[2]] * weight2;
            const double lengthSq = normal.lengthSq();
            if (lengthSq > 1.0e-24 && std::isfinite(lengthSq))
            {
                triangle.vertexNormals[outputIndex] = normal.normal();
                triangle.hasVertexNormals = true;
            }
        }
    }
}

void applyTriangleNormal(acdb::Triangle &triangle,
                         const AcGeVector3d *normal = nullptr)
{
    AcGeVector3d value;
    if (normal != nullptr)
        value = *normal;
    else
        value = glm::cross(triangle.b - triangle.a, triangle.c - triangle.a);
    const double lengthSq = value.lengthSq();
    if (!(lengthSq > 1.0e-24) || !std::isfinite(lengthSq))
        return;
    value = value.normal();
    triangle.vertexNormals = {value, value, value};
    triangle.hasVertexNormals = true;
}

bool validNormals(const std::vector<AcGeVector3d> &normals)
{
    return std::all_of(normals.begin(), normals.end(),
                       [](const AcGeVector3d &normal) {
                           const double lengthSq = normal.lengthSq();
                           return std::isfinite(normal.x) &&
                                  std::isfinite(normal.y) &&
                                  std::isfinite(normal.z) &&
                                  lengthSq > 1.0e-24 &&
                                  std::isfinite(lengthSq);
                       });
}
} // namespace

bool AcGiWorldDraw::worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
{
    AcGePoint3d start = apply(from);
    AcGePoint3d end = apply(to);
    if (!clipSegment(start, end))
        return false;
    acdb::Stroke &stroke = sink().addStroke();
    stroke.points = {start, end};
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::worldPolyline(const AcGePoint3d *points, int count,
                                  bool closed)
{
    if (!points || count < 2)
        return true;
    if (!clipStack_.empty())
    {
        const int segmentCount = closed ? count : count - 1;
        for (int index = 0; index < segmentCount; ++index)
            worldLine(points[index], points[(index + 1) % count]);
        return false;
    }
    acdb::Stroke &stroke = sink().addStroke(closed);
    stroke.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        stroke.points.push_back(apply(points[i]));
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::polyPolygon(
    const Adesk::UInt32 numPolygonIndices,
    const Adesk::UInt32 *numPolygonPositions,
    const AcGePoint3d *polygonPositions,
    const Adesk::UInt32 *numPolygonPoints,
    const AcGePoint3d *polygonPoints,
    const AcCmEntityColor *outlineColors,
    const AcGiLineType *outlineTypes,
    const AcCmEntityColor *fillColors,
    const AcCmTransparency *fillOpacities)
{
    if (numPolygonIndices == 0 || numPolygonPositions == nullptr ||
        polygonPositions == nullptr || numPolygonPoints == nullptr ||
        polygonPoints == nullptr)
        return true;

    size_t totalPositions = 0;
    size_t totalPoints = 0;
    for (Adesk::UInt32 type = 0; type < numPolygonIndices; ++type)
    {
        const size_t instanceCount = numPolygonPositions[type];
        const size_t vertexCount = numPolygonPoints[type];
        if ((instanceCount != 0 && vertexCount < 3) ||
            vertexCount > static_cast<size_t>(INT_MAX) ||
            instanceCount >
                std::numeric_limits<size_t>::max() - totalPositions ||
            vertexCount >
                std::numeric_limits<size_t>::max() - totalPoints)
            return true;
        totalPositions += instanceCount;
        totalPoints += vertexCount;
    }
    (void)totalPositions;
    (void)totalPoints;

    const acdb::AcDbEntity originalTraits =
        draw_.subEntityTraits().traits().toEntityCommon();
    size_t positionIndex = 0;
    size_t pointIndex = 0;
    for (Adesk::UInt32 type = 0; type < numPolygonIndices; ++type)
    {
        const Adesk::UInt32 instanceCount = numPolygonPositions[type];
        const Adesk::UInt32 vertexCount = numPolygonPoints[type];
        glm::vec4 fillColor = originalTraits.color.renderColor();
        if (fillColors != nullptr)
            fillColor = fillColors[type].renderColor();
        if (fillOpacities != nullptr &&
            fillOpacities[type].isByAlpha())
            fillColor.a *=
                static_cast<float>(fillOpacities[type].alpha()) / 255.0f;

        for (Adesk::UInt32 instance = 0; instance < instanceCount; ++instance)
        {
            std::vector<AcGePoint3d> vertices;
            vertices.reserve(vertexCount);
            const AcGeVector3d insertion = polygonPositions[positionIndex++].asVector();
            for (Adesk::UInt32 vertex = 0; vertex < vertexCount; ++vertex)
                vertices.push_back(polygonPoints[pointIndex + vertex] + insertion);

            if (fillColor.a > 0.0f)
            {
                draw_.subEntityTraits().setColor(fillColor);
                for (const auto &indices : triangulatePolygon(vertices))
                    worldTriangle(vertices[indices[0]], vertices[indices[1]],
                                  vertices[indices[2]]);
            }

            glm::vec4 outlineColor = originalTraits.color.renderColor();
            if (outlineColors != nullptr)
                outlineColor = outlineColors[type].renderColor();
            draw_.subEntityTraits().setColor(outlineColor);
            if (outlineTypes != nullptr && outlineTypes[type].name != nullptr)
                draw_.subEntityTraits().setLineType(outlineTypes[type].name);
            worldPolyline(vertices.data(), static_cast<int>(vertices.size()), true);
        }
        pointIndex += vertexCount;
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    draw_.subEntityTraits().setFrom(originalTraits);
    return false;
}

bool AcGiWorldDraw::image(const AcGiImageBGRA32 &imageSource,
                          const AcGePoint3d &position,
                          const AcGeVector3d &u, const AcGeVector3d &v,
                          TransparencyMode transparencyMode)
{
    constexpr size_t kMaximumRasterPixels = 65536;
    const size_t width = imageSource.width();
    const size_t height = imageSource.height();
    const AcGiPixelBGRA32 *pixels = imageSource.image();
    if (width == 0 || height == 0 || pixels == nullptr ||
        width > kMaximumRasterPixels / height)
        return true;

    const acdb::AcDbEntity originalTraits =
        draw_.subEntityTraits().traits().toEntityCommon();
    for (size_t row = 0; row < height; ++row)
    {
        for (size_t column = 0; column < width; ++column)
        {
            const AcGiPixelBGRA32 &pixel = pixels[row * width + column];
            float alpha = 1.0f;
            if (transparencyMode == kTransparency1Bit)
                alpha = pixel.a == 0 ? 0.0f : 1.0f;
            else if (transparencyMode == kTransparency8Bit)
                alpha = static_cast<float>(pixel.a) / 255.0f;
            if (alpha == 0.0f)
                continue;

            draw_.subEntityTraits().setColor(
                glm::vec4(static_cast<float>(pixel.r) / 255.0f,
                          static_cast<float>(pixel.g) / 255.0f,
                          static_cast<float>(pixel.b) / 255.0f, alpha));
            const double x0 = static_cast<double>(column) / width;
            const double x1 = static_cast<double>(column + 1) / width;
            const double y0 = static_cast<double>(row) / height;
            const double y1 = static_cast<double>(row + 1) / height;
            const AcGePoint3d p00 = position + u * x0 + v * y0;
            const AcGePoint3d p10 = position + u * x1 + v * y0;
            const AcGePoint3d p11 = position + u * x1 + v * y1;
            const AcGePoint3d p01 = position + u * x0 + v * y1;
            worldTriangle(p00, p10, p11);
            worldTriangle(p00, p11, p01);
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);
    return false;
}

bool AcGiWorldDraw::edge(const std::vector<AcGiEdge2d> &edges)
{
    if (edges.empty())
        return true;

    for (const AcGiEdge2d &edge : edges)
    {
        std::vector<AcGePoint3d> points;
        std::visit(
            [&](const auto &curve) {
                using Curve = std::decay_t<decltype(curve)>;
                if constexpr (std::is_same_v<Curve, AcGeLineSeg2d>)
                {
                    points.emplace_back(curve.startPoint().x,
                                        curve.startPoint().y, 0.0);
                    points.emplace_back(curve.endPoint().x,
                                        curve.endPoint().y, 0.0);
                }
                else
                {
                    double sweep = 0.0;
                    double radius = 0.0;
                    if constexpr (std::is_same_v<Curve, AcGeCircArc2d>)
                    {
                        sweep = curve.sweep() *
                                (curve.isClockWise() ? -1.0 : 1.0);
                        radius = curve.radius();
                    }
                    else
                    {
                        sweep = curve.endAng() - curve.startAng();
                        radius = curve.majorRadius();
                    }
                    const acdb::TesselationOptions options =
                        context().optionsFor(radius);
                    if (!(options.angleStep > 0.0) ||
                        !std::isfinite(options.angleStep) ||
                        !std::isfinite(sweep) || sweep == 0.0)
                        return;
                    const int maxSegments =
                        std::clamp(options.maxSegments, 1, 65536);
                    const int minSegments =
                        std::clamp(options.minSegments, 1, maxSegments);
                    const double desiredSegments =
                        std::ceil(std::abs(sweep) / options.angleStep);
                    const int segments =
                        desiredSegments >= maxSegments
                            ? maxSegments
                            : std::max(minSegments,
                                       static_cast<int>(desiredSegments));
                    points.reserve(static_cast<size_t>(segments) + 1);
                    for (int index = 0; index <= segments; ++index)
                    {
                        const double t =
                            static_cast<double>(index) / segments;
                        AcGePoint2d point;
                        if constexpr (std::is_same_v<Curve, AcGeCircArc2d>)
                        {
                            const double angle =
                                curve.startAng() + sweep * t;
                            point = curve.pointAtAngle(angle);
                        }
                        else
                        {
                            const double angle =
                                curve.startAng() + sweep * t;
                            point = curve.pointAt(angle);
                        }
                        points.emplace_back(point.x, point.y, 0.0);
                    }
                }
            },
            edge.curve);
        if (points.size() < 2)
            return true;
        if (worldPolyline(points.data(), static_cast<int>(points.size()), false))
            return true;
    }
    return false;
}

bool AcGiWorldDraw::worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                                  const AcGePoint3d &c, bool fillIs3DFace)
{
    std::vector<AcGePoint3d> clipped = clipPolygon({apply(a), apply(b), apply(c)});
    for (size_t index = 2; index < clipped.size(); ++index)
    {
        acdb::Triangle triangle{{}, clipped[0], clipped[index - 1],
                                clipped[index]};
        applyTraits(triangle, fillIs3DFace);
        sink().appendTriangle(triangle);
    }
    return false;
}

bool AcGiWorldDraw::worldPoint(const AcGePoint3d &position,
                               acge::PointDisplayStyle displayStyle)
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    AcGePoint3d worldPosition = apply(position);
    if (!clipPoint(worldPosition))
        return false;
    acdb::TessellatedPoint point;
    point.location = worldPosition;
    point.common = traits.toEntityCommon();
    point.pointSize =
        traits.lineWeight > 0.0 ? traits.lineWeight : 7.0;
    point.displayStyle = displayStyle;
    sink().appendPoint(point);
    return false;
}

bool AcGiWorldDraw::worldInfiniteLine(const AcGePoint3d &start,
                                      const AcGeVector3d &direction)
{
    const AcGeVector3d dir = direction.normal();
    if (dir.lengthSq() <= 0.0)
        return true;
    constexpr double kProxyLength = 1.0e6;
    if (!clipStack_.empty())
        return worldLine(start, start + dir * kProxyLength);
    acdb::Stroke &stroke = sink().addStroke(false);
    stroke.points = {apply(start),
                     apply(start + dir * kProxyLength)};
    stroke.semiInfinite = true;
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::pushClipBoundaryImpl(const AcGiClipBoundary *boundary)
{
    if (boundary == nullptr)
        return false;
    const std::vector<AcGePoint2d> &inputPoints =
        boundary->m_aptPoints.empty() ? boundary->points : boundary->m_aptPoints;
    if (inputPoints.size() < 2)
        return false;

    AcGeVector3d normal = boundary->m_vNormal;
    const double normalLengthSq = normal.lengthSq();
    if (!(normalLengthSq > 1.0e-24) || !std::isfinite(normalLengthSq))
        return false;

    AcGeClipBoundary2d validated;
    try
    {
        std::vector<AcGePoint2d> points = inputPoints;
        if (points.size() == 2)
        {
            const double minX = std::min(points[0].x, points[1].x);
            const double minY = std::min(points[0].y, points[1].y);
            const double maxX = std::max(points[0].x, points[1].x);
            const double maxY = std::max(points[0].y, points[1].y);
            points = {{minX, minY}, {maxX, minY},
                      {maxX, maxY}, {minX, maxY}};
        }
        validated.set(points);
    }
    catch (const std::exception &)
    {
        return false;
    }

    ClipFrame frame;
    frame.boundary = validated.vertices();
    frame.toClipSpace =
        boundary->m_xToClipSpace * boundary->m_xInverseBlockRefXForm;
    frame.clippingFront = boundary->m_bClippingFront;
    frame.clippingBack = boundary->m_bClippingBack;
    frame.frontClipZ = boundary->m_dFrontClipZ;
    frame.backClipZ = boundary->m_dBackClipZ;
    if ((frame.clippingFront && !std::isfinite(frame.frontClipZ)) ||
        (frame.clippingBack && !std::isfinite(frame.backClipZ)))
        return false;
    long double areaTwice = 0.0L;
    for (size_t index = 0; index < frame.boundary.size(); ++index)
    {
        const AcGePoint2d &current = frame.boundary[index];
        const AcGePoint2d &next =
            frame.boundary[(index + 1) % frame.boundary.size()];
        areaTwice += static_cast<long double>(current.x) * next.y -
                     static_cast<long double>(current.y) * next.x;
    }
    if (!std::isfinite(areaTwice) || areaTwice == 0.0L)
        return false;
    frame.winding = areaTwice > 0.0L ? 1.0 : -1.0;
    clipStack_.push_back(std::move(frame));
    return true;
}

void AcGiWorldDraw::popClipBoundaryImpl()
{
    if (!clipStack_.empty())
        clipStack_.pop_back();
}

bool AcGiWorldDraw::clipPoint(AcGePoint3d &point) const
{
    for (const ClipFrame &frame : clipStack_)
    {
        AcGePoint3d clipPoint;
        if (!frame.coordinates(point, clipPoint))
            return false;
        if ((frame.clippingFront &&
             clipPoint.z > frame.frontClipZ + 1.0e-10) ||
            (frame.clippingBack &&
             clipPoint.z < frame.backClipZ - 1.0e-10))
            return false;
        const AcGePoint2d projected(clipPoint.x, clipPoint.y);
        for (size_t index = 0; index < frame.boundary.size(); ++index)
        {
            const AcGePoint2d &a = frame.boundary[index];
            const AcGePoint2d &b =
                frame.boundary[(index + 1) % frame.boundary.size()];
            if (signedDistance(a, b, projected, frame.winding) < -1.0e-10L)
                return false;
        }
    }
    return true;
}

bool AcGiWorldDraw::clipSegment(AcGePoint3d &start, AcGePoint3d &end) const
{
    for (const ClipFrame &frame : clipStack_)
    {
        AcGePoint3d clipStart;
        AcGePoint3d clipEnd;
        if (!frame.coordinates(start, clipStart) ||
            !frame.coordinates(end, clipEnd))
            return false;
        const AcGePoint2d a0(clipStart.x, clipStart.y);
        const AcGePoint2d a1(clipEnd.x, clipEnd.y);
        double first = 0.0;
        double last = 1.0;
        for (size_t index = 0; index < frame.boundary.size(); ++index)
        {
            const AcGePoint2d &edgeStart = frame.boundary[index];
            const AcGePoint2d &edgeEnd =
                frame.boundary[(index + 1) % frame.boundary.size()];
            const long double d0 =
                signedDistance(edgeStart, edgeEnd, a0, frame.winding);
            const long double d1 =
                signedDistance(edgeStart, edgeEnd, a1, frame.winding);
            if (!std::isfinite(d0) || !std::isfinite(d1))
                return false;
            if (d0 < -1.0e-10L && d1 < -1.0e-10L)
                return false;
            if ((d0 < -1.0e-10L) != (d1 < -1.0e-10L))
            {
                const double t = static_cast<double>(
                    std::clamp(d0 / (d0 - d1), 0.0L, 1.0L));
                if (d0 < -1.0e-10L)
                    first = std::max(first, t);
                else
                    last = std::min(last, t);
            }
        }
        const auto clipDepth = [&](double planeZ, bool keepBelow) {
            const double z0 = keepBelow ? clipStart.z : -clipStart.z;
            const double z1 = keepBelow ? clipEnd.z : -clipEnd.z;
            const double boundaryZ = keepBelow ? planeZ : -planeZ;
            const double d0 = boundaryZ - z0;
            const double d1 = boundaryZ - z1;
            if (d0 < -1.0e-10 && d1 < -1.0e-10)
                return false;
            if ((d0 < -1.0e-10) != (d1 < -1.0e-10))
            {
                const double t = std::clamp(d0 / (d0 - d1), 0.0, 1.0);
                if (d0 < -1.0e-10)
                    first = std::max(first, t);
                else
                    last = std::min(last, t);
            }
            return first <= last;
        };
        if ((frame.clippingFront &&
             !clipDepth(frame.frontClipZ, true)) ||
            (frame.clippingBack && !clipDepth(frame.backClipZ, false)))
            return false;
        if (first > last)
            return false;
        const AcGePoint3d originalStart = start;
        const AcGeVector3d direction = end - start;
        start = originalStart + direction * first;
        end = originalStart + direction * last;
    }
    return true;
}

std::vector<AcGePoint3d>
AcGiWorldDraw::clipPolygon(std::vector<AcGePoint3d> polygon) const
{
    for (const ClipFrame &frame : clipStack_)
    {
        for (size_t edgeIndex = 0;
             edgeIndex < frame.boundary.size() && !polygon.empty();
             ++edgeIndex)
        {
            const AcGePoint2d &edgeStart = frame.boundary[edgeIndex];
            const AcGePoint2d &edgeEnd =
                frame.boundary[(edgeIndex + 1) % frame.boundary.size()];
            const auto side = [&](const AcGePoint3d &point) {
                AcGePoint3d clipPoint;
                if (!frame.coordinates(point, clipPoint))
                    return std::numeric_limits<long double>::quiet_NaN();
                const AcGePoint2d projected(clipPoint.x, clipPoint.y);
                return signedDistance(edgeStart, edgeEnd, projected,
                                      frame.winding);
            };
            std::vector<AcGePoint3d> output;
            output.reserve(polygon.size() + 1);
            AcGePoint3d previous = polygon.back();
            long double previousSide = side(previous);
            if (!std::isfinite(previousSide))
                return {};
            for (const AcGePoint3d &current : polygon)
            {
                const long double currentSide = side(current);
                if (!std::isfinite(currentSide))
                    return {};
                const bool previousInside = previousSide >= -1.0e-10L;
                const bool currentInside = currentSide >= -1.0e-10L;
                if (previousInside != currentInside)
                {
                    const double t = static_cast<double>(std::clamp(
                        previousSide / (previousSide - currentSide),
                        0.0L, 1.0L));
                    output.push_back(previous + (current - previous) * t);
                }
                if (currentInside)
                    output.push_back(current);
                previous = current;
                previousSide = currentSide;
            }
            polygon = std::move(output);
        }
        const auto clipDepth = [&](double planeZ, bool keepBelow) {
            if (polygon.empty())
                return;
            std::vector<AcGePoint3d> output;
            output.reserve(polygon.size() + 1);
            AcGePoint3d previous = polygon.back();
            AcGePoint3d previousClip;
            if (!frame.coordinates(previous, previousClip))
            {
                polygon.clear();
                return;
            }
            long double previousSide =
                keepBelow ? planeZ - previousClip.z
                          : previousClip.z - planeZ;
            for (const AcGePoint3d &current : polygon)
            {
                AcGePoint3d currentClip;
                if (!frame.coordinates(current, currentClip))
                {
                    polygon.clear();
                    return;
                }
                const long double currentSide =
                    keepBelow ? planeZ - currentClip.z
                              : currentClip.z - planeZ;
                const bool previousInside = previousSide >= -1.0e-10L;
                const bool currentInside = currentSide >= -1.0e-10L;
                if (previousInside != currentInside)
                {
                    const double t = static_cast<double>(std::clamp(
                        previousSide / (previousSide - currentSide),
                        0.0L, 1.0L));
                    output.push_back(previous + (current - previous) * t);
                }
                if (currentInside)
                    output.push_back(current);
                previous = current;
                previousSide = currentSide;
            }
            polygon = std::move(output);
        };
        if (frame.clippingFront)
            clipDepth(frame.frontClipZ, true);
        if (frame.clippingBack)
            clipDepth(frame.backClipZ, false);
    }
    return polygon;
}

void AcGiWorldDraw::applyTraits(acdb::Stroke &stroke) const
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    stroke.common = traits.toEntityCommon();
    stroke.lineWeight = traits.lineWeight;
}

void AcGiWorldDraw::applyTraits(acdb::Triangle &triangle,
                                bool fillIs3DFace) const
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    triangle.common = traits.toEntityCommon();
    triangle.is3DFace = fillIs3DFace;
}

bool AcGiWorldDraw::worldCircle(const AcGeCircArc3d &arc)
{
    const double sweep = arc.endAngle - arc.startAngle;
    if (!(arc.radius > 0.0) || !(sweep > 0.0))
        return true;
    const acdb::TesselationOptions options = draw_.optionsFor(arc.radius);
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    const bool closed = arc.isFullCircle();
    const int pointCount = closed ? segments : segments + 1;
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(pointCount));
    for (int i = 0; i < pointCount; ++i)
    {
        const double t = sweep * static_cast<double>(i) / segments;
        points.push_back(arc.pointAt(arc.startAngle + t));
    }
    return worldPolyline(points.data(), pointCount, closed);
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const char *message,
                         const AcGiTextStyle &style)
{
    if (!message || !*message)
        return true;
    const double height = style.textSize;
    if (!(height > 0.0) || !std::isfinite(height))
        return true;

    // SHX vector glyphs: when the style names a text style whose SHX fonts
    // the acdb system loaded, each glyph stroke becomes an ordinary CAD
    // stroke (cap line at the anchor, baseline one em down) — same output
    // the stroke pipeline serves for any other SHX geometry.
    if (style.fileName != nullptr && *style.fileName != '\0' &&
        acgi::textEngine().shxStyleFontBound(style.fileName))
    {
        const AcUtf8String utf8Glyphs(message);
        const AcGeVector3d right = direction.normal();
        const AcGeVector3d up = normal.normal().crossProduct(right);
        for (const rendering::ShxGlyphStroke &glyphStroke :
             acgi::textEngine().shxStrokes(utf8Glyphs))
        {
            worldLine(position +
                          AcGeVector3d(right * (glyphStroke.fromX * height) +
                                       up * (glyphStroke.fromY * height)),
                      position +
                          AcGeVector3d(right * (glyphStroke.toX * height) +
                                       up * (glyphStroke.toY * height)));
        }
        return false;
    }

    acgi::TextRequest request;
    request.position = glm::dvec3(apply(position));
    request.normal = glm::dvec3(normal.normal());
    request.direction = glm::dvec3(direction.normal());
    request.message = message;
    request.height = height;
    request.xScale = style.xScale;
    request.color = draw_.subEntityTraits().traits().color;
    acgi::textRequests().push_back(std::move(request));

    if (!acgi::textFrameFallback())
        return false;

    size_t lineBreaks = 0;
    size_t longest = 0;
    size_t current = 0;
    for (const char *character = message; *character; ++character)
    {
        if (*character == 10)
        {
            ++lineBreaks;
            longest = std::max(longest, current);
            current = 0;
        }
        else
        {
            ++current;
        }
    }
    longest = std::max(longest, current);
    const size_t lineCount = lineBreaks + 1;
    const double width = std::max(double(longest) * height * 0.6 *
                                      style.xScale,
                                  height);
    const glm::dvec3 right = glm::dvec3(direction.normal());
    const glm::dvec3 up =
        glm::normalize(glm::cross(glm::dvec3(normal.normal()), right));
    const glm::dvec3 frameHeight = up * (height * double(lineCount));

    const AcGePoint3d framePoints[] = {
        position,
        position + AcGeVector3d(right * width),
        position + AcGeVector3d(right * width + frameHeight),
        position + AcGeVector3d(frameHeight),
        position};
    worldPolyline(framePoints, 5, true);
    worldPoint(position);
    return false;
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const ACHAR *message,
                         const AcGiTextStyle &style)
{
    if (!message)
        return true;
    const AcUtf8String utf8Message =
        arxcompat::toUtf8(AcStringView(message));
    return text(position, normal, direction, utf8Message.c_str(), style);
}

bool AcGiWorldDraw::worldEllipse(const AcGeEllip3d &ellipse)
{
    const double sweep = ellipse.endAngle - ellipse.startAngle;
    if (!(sweep > 0.0))
        return true;
    const acdb::TesselationOptions options =
        draw_.optionsFor(ellipse.majorRadius());
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    const bool closed = sweep >= kTwoPi - 1.0e-12;
    const int emitted = closed ? segments : segments + 1;
    std::vector<AcGePoint3d> points;
    points.reserve(static_cast<size_t>(emitted));
    for (int i = 0; i < emitted; ++i)
    {
        const double t = sweep * static_cast<double>(i) / segments;
        points.push_back(apply(ellipse.pointAt(ellipse.startAngle + t)));
    }
    return worldPolyline(points.data(), emitted, closed);
}

bool AcGiWorldDraw::circle(const AcGePoint3d &center, const double radius,
                           const AcGeVector3d &normal)
{
    if (!(radius > 0.0))
        return true;
    // Full circle: the default angle span of the center/normal/radius form.
    return worldCircle(AcGeCircArc3d(center, normal.normal(), radius));
}

bool AcGiWorldDraw::circle(const AcGePoint3d &first, const AcGePoint3d &second,
                           const AcGePoint3d &third)
{
    AcGeCircArc3d through;
    if (!circumArcThrough(first, second, third, through))
        return true;
    return worldCircle(
        AcGeCircArc3d(through.center, through.normal, through.radius));
}

bool AcGiWorldDraw::circularArc(const AcGePoint3d &center, const double radius,
                                const AcGeVector3d &normal,
                                const AcGeVector3d &startVector,
                                const double sweepAngle,
                                const AcGiArcType arcType)
{
    if (!(radius > 0.0) || sweepAngle == 0.0)
        return true;
    AcGeCircArc3d arc(center, normal.normal(), radius, 0.0, sweepAngle);
    const AcGeVector3d u = arc.referenceAxis();
    const AcGeVector3d v = arc.normal.crossProduct(u);
    const double startAngle = std::atan2(startVector.dotProduct(v),
                                         startVector.dotProduct(u));
    arc.setAngles(startAngle, startAngle + sweepAngle);
    if (worldCircle(arc))
        return true;
    return emitArcClosure(*this, center, arc.startPoint(), arc.endPoint(),
                          sampleArcPoints(context(), arc), arcType);
}

bool AcGiWorldDraw::circularArc(const AcGePoint3d &start,
                                const AcGePoint3d &point,
                                const AcGePoint3d &end,
                                const AcGiArcType arcType)
{
    AcGeCircArc3d arc;
    if (!circumArcThrough(start, point, end, arc))
        return true;
    if (worldCircle(arc))
        return true;
    return emitArcClosure(*this, arc.center, arc.startPoint(), arc.endPoint(),
                          sampleArcPoints(context(), arc), arcType);
}

bool AcGiWorldDraw::ellipticalArc(const AcGePoint3d &center,
                                  const AcGeVector3d &normal,
                                  double majorAxisLength,
                                  double minorAxisLength,
                                  double startAngleInRad,
                                  double endAngleInRad, double tiltAngleInRad,
                                  AcGiArcType arcType)
{
    if (!(majorAxisLength > 0.0) || !(minorAxisLength > 0.0) ||
        endAngleInRad == startAngleInRad)
        return true;
    const AcGeVector3d unitNormal = normal.normal();
    const AcGeVector3d reference =
        AcGeCircArc3d(center, unitNormal, majorAxisLength).referenceAxis();
    const AcGeVector3d planeSide = unitNormal.crossProduct(reference);
    const AcGeVector3d majorAxis =
        (reference * std::cos(tiltAngleInRad) +
         planeSide * std::sin(tiltAngleInRad)) *
        majorAxisLength;
    const double ratio =
        std::clamp(minorAxisLength / majorAxisLength, 0.0, 1.0);
    const AcGeEllip3d ellipse(center, majorAxis, unitNormal, ratio,
                              startAngleInRad, endAngleInRad);
    if (worldEllipse(ellipse))
        return true;
    if (arcType == AcGiArcType::kAcGiArcSimple)
        return false;

    // Closure areas: spokes from the center (sector) or the chord line
    // (chord), plus the fan fill over the same segmentation worldEllipse
    // used for the boundary.
    const double sweep = endAngleInRad - startAngleInRad;
    const acdb::TesselationOptions options =
        context().optionsFor(ellipse.majorRadius());
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    std::vector<AcGePoint3d> samples;
    samples.reserve(static_cast<size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double t = sweep * static_cast<double>(index) / segments;
        samples.push_back(ellipse.pointAt(startAngleInRad + t));
    }
    const AcGePoint3d start = ellipse.startPoint();
    const AcGePoint3d end = ellipse.endPoint();
    const AcGePoint3d fanOrigin =
        arcType == AcGiArcType::kAcGiArcSector
            ? center
            : start + (end - start) * 0.5;
    return emitArcClosure(*this, fanOrigin, start, end, samples, arcType);
}

bool AcGiWorldDraw::mesh(const Adesk::UInt32 rows, const Adesk::UInt32 columns,
                         const AcGePoint3d *vertexList,
                         const AcGiEdgeData *edgeData,
                         const AcGiFaceData *faceData,
                         const AcGiVertexData *vertexData,
                         const bool autoGenerateNormals)
{
    if (rows < 2 || columns < 2 || vertexList == nullptr)
        return true;
    const uint64_t vertexCount = uint64_t(rows) * columns;
    const uint64_t faceCount = uint64_t(rows - 1) * (columns - 1);
    const uint64_t horizontalEdgeCount = uint64_t(rows) * (columns - 1);
    const uint64_t verticalEdgeCount = uint64_t(rows - 1) * columns;
    if (horizontalEdgeCount >
        std::numeric_limits<uint64_t>::max() - verticalEdgeCount)
        return true;
    const uint64_t edgeCount = horizontalEdgeCount + verticalEdgeCount;
    if (vertexCount > std::numeric_limits<size_t>::max() ||
        faceCount > std::numeric_limits<size_t>::max() ||
        edgeCount > std::numeric_limits<size_t>::max())
        return true;
    const size_t vertexCountSize = static_cast<size_t>(vertexCount);
    const size_t faceCountSize = static_cast<size_t>(faceCount);
    const size_t edgeCountSize = static_cast<size_t>(edgeCount);
    if ((faceData != nullptr &&
         ((!faceData->trueColors.empty() &&
           faceData->trueColors.size() != faceCountSize) ||
          (!faceData->visibility.empty() &&
           faceData->visibility.size() != faceCountSize) ||
          (!faceData->normals.empty() &&
           faceData->normals.size() != faceCountSize) ||
          (!faceData->transparency.empty() &&
           faceData->transparency.size() != faceCountSize) ||
          !validNormals(faceData->normals))) ||
        (vertexData != nullptr &&
         ((!vertexData->trueColors.empty() &&
           vertexData->trueColors.size() != vertexCountSize) ||
          (!vertexData->normals.empty() &&
           vertexData->normals.size() != vertexCountSize) ||
          !validNormals(vertexData->normals))) ||
        (edgeData != nullptr &&
         ((!edgeData->trueColors.empty() &&
           edgeData->trueColors.size() != edgeCountSize) ||
          (!edgeData->visibility.empty() &&
           edgeData->visibility.size() != edgeCountSize))))
        return true;

    const acdb::AcDbEntity originalTraits =
        draw_.subEntityTraits().traits().toEntityCommon();
    size_t faceIndex = 0;
    auto emitTriangle = [&](const std::array<AcGePoint3d, 3> &points,
                            const std::array<size_t, 3> &indices) {
        const size_t firstFill = sink().geometry().fills.size();
        const std::array<AcGePoint3d, 3> worldPoints{
            apply(points[0]), apply(points[1]), apply(points[2])};
        worldTriangle(points[0], points[1], points[2]);
        for (size_t fillIndex = firstFill;
             fillIndex < sink().geometry().fills.size(); ++fillIndex)
        {
            acdb::Triangle &triangle = sink().geometry().fills[fillIndex];
            applyVertexData(triangle, worldPoints, indices, vertexData);
            if (vertexData == nullptr || vertexData->normals.empty())
            {
                if (faceData != nullptr && !faceData->normals.empty())
                    applyTriangleNormal(triangle, &faceData->normals[faceIndex]);
                else if (autoGenerateNormals)
                    applyTriangleNormal(triangle);
            }
        }
    };

    for (Adesk::UInt32 row = 0; row + 1 < rows; ++row)
    {
        for (Adesk::UInt32 column = 0; column + 1 < columns; ++column)
        {
            const size_t i00 = size_t(row) * columns + column;
            const size_t i01 = i00 + 1;
            const size_t i10 = size_t(row + 1) * columns + column;
            const size_t i11 = i10 + 1;
            if (faceData != nullptr &&
                !faceData->visibility.empty() &&
                faceData->visibility[faceIndex] == 0)
            {
                ++faceIndex;
                continue;
            }
            if (faceData != nullptr && !faceData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    faceData->trueColors[faceIndex].renderColor());
            const AcGePoint3d points[] = {vertexList[i00], vertexList[i01],
                                          vertexList[i10], vertexList[i11]};
            emitTriangle({points[0], points[1], points[3]},
                         {i00, i01, i11});
            emitTriangle({points[0], points[3], points[2]},
                         {i00, i11, i10});
            ++faceIndex;
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);

    const bool emitEdges =
        edgeData != nullptr &&
        (!edgeData->trueColors.empty() || !edgeData->visibility.empty());
    if (emitEdges)
    {
        auto emitEdge = [&](size_t edgeIndex, const AcGePoint3d &a,
                            const AcGePoint3d &b) {
            if (!edgeData->visibility.empty() &&
                edgeData->visibility[edgeIndex] == 0)
                return;
            if (!edgeData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    edgeData->trueColors[edgeIndex].renderColor());
            worldLine(a, b);
        };
        size_t edgeIndex = 0;
        for (Adesk::UInt32 row = 0; row < rows; ++row)
            for (Adesk::UInt32 column = 0; column + 1 < columns; ++column)
                emitEdge(edgeIndex++, vertexList[size_t(row) * columns + column],
                         vertexList[size_t(row) * columns + column + 1]);
        for (Adesk::UInt32 column = 0; column < columns; ++column)
            for (Adesk::UInt32 row = 0; row + 1 < rows; ++row)
                emitEdge(edgeIndex++,
                         vertexList[size_t(row) * columns + column],
                         vertexList[size_t(row + 1) * columns + column]);
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    (void)autoGenerateNormals;
    return false;
}

bool AcGiWorldDraw::shell(const Adesk::UInt32 nbVertex,
                          const AcGePoint3d *vertexList,
                          const Adesk::UInt32 faceListSize,
                          const Adesk::Int32 *faceList,
                          const AcGiEdgeData *edgeData,
                          const AcGiFaceData *faceData,
                          const AcGiVertexData *vertexData)
{
    if (nbVertex == 0 || vertexList == nullptr ||
        (faceListSize != 0 && faceList == nullptr))
        return true;

    struct Face
    {
        std::vector<size_t> indices;
        bool edgesVisible = true;
    };
    std::vector<Face> faces;
    std::vector<std::pair<size_t, size_t>> edges;
    std::map<std::pair<size_t, size_t>, size_t> edgeIndices;
    Adesk::UInt32 position = 0;
    while (position < faceListSize)
    {
        const Adesk::Int32 count = faceList[position++];
        if (count == 0)
            break;
        const uint64_t faceSize =
            count < 0 ? uint64_t(-int64_t(count)) : uint64_t(count);
        if (faceSize < 3 || faceSize > faceListSize - position)
            return true; // malformed face list
        Face face;
        face.edgesVisible = count > 0;
        face.indices.reserve(static_cast<size_t>(faceSize));
        for (uint64_t index = 0; index < faceSize; ++index)
        {
            const Adesk::Int32 vertexIndex = faceList[position++];
            if (vertexIndex < 0 ||
                static_cast<Adesk::UInt32>(vertexIndex) >= nbVertex)
                return true;
            face.indices.push_back(static_cast<size_t>(vertexIndex));
        }
        for (size_t index = 0; index < face.indices.size(); ++index)
        {
            const size_t a = face.indices[index];
            const size_t b = face.indices[(index + 1) % face.indices.size()];
            const std::pair<size_t, size_t> key{std::min(a, b), std::max(a, b)};
            auto [edge, inserted] = edgeIndices.emplace(key, edges.size());
            if (inserted)
                edges.push_back(key);
            (void)edge;
        }
        faces.push_back(std::move(face));
    }

    if ((faceData != nullptr &&
         ((!faceData->trueColors.empty() &&
           faceData->trueColors.size() != faces.size()) ||
          (!faceData->visibility.empty() &&
           faceData->visibility.size() != faces.size()) ||
          (!faceData->normals.empty() &&
           faceData->normals.size() != faces.size()) ||
          (!faceData->transparency.empty() &&
           faceData->transparency.size() != faces.size()) ||
          !validNormals(faceData->normals))) ||
        (vertexData != nullptr &&
         ((!vertexData->trueColors.empty() &&
           vertexData->trueColors.size() != nbVertex) ||
          (!vertexData->normals.empty() &&
           vertexData->normals.size() != nbVertex) ||
          !validNormals(vertexData->normals))) ||
        (edgeData != nullptr &&
         ((!edgeData->trueColors.empty() &&
           edgeData->trueColors.size() != edges.size()) ||
          (!edgeData->visibility.empty() &&
           edgeData->visibility.size() != edges.size()))))
        return true;

    const acdb::AcDbEntity originalTraits =
        draw_.subEntityTraits().traits().toEntityCommon();
    for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        const Face &face = faces[faceIndex];
        if (faceData != nullptr &&
            !faceData->visibility.empty() &&
            faceData->visibility[faceIndex] == 0)
            continue;
        if (faceData != nullptr && !faceData->trueColors.empty())
            draw_.subEntityTraits().setColor(
                faceData->trueColors[faceIndex].renderColor());
        std::vector<AcGePoint3d> points;
        points.reserve(face.indices.size());
        for (const size_t index : face.indices)
            points.push_back(vertexList[index]);
        for (const auto &triangleIndices : triangulatePolygon(points))
        {
            const std::array<size_t, 3> indices{
                face.indices[triangleIndices[0]],
                face.indices[triangleIndices[1]],
                face.indices[triangleIndices[2]]};
            const std::array<AcGePoint3d, 3> trianglePoints{
                points[triangleIndices[0]], points[triangleIndices[1]],
                points[triangleIndices[2]]};
            const size_t firstFill = sink().geometry().fills.size();
            const std::array<AcGePoint3d, 3> worldPoints{
                apply(trianglePoints[0]), apply(trianglePoints[1]),
                apply(trianglePoints[2])};
            worldTriangle(trianglePoints[0], trianglePoints[1],
                          trianglePoints[2]);
            for (size_t fillIndex = firstFill;
                 fillIndex < sink().geometry().fills.size(); ++fillIndex)
            {
                acdb::Triangle &triangle = sink().geometry().fills[fillIndex];
                applyVertexData(triangle, worldPoints, indices, vertexData);
                if (vertexData == nullptr || vertexData->normals.empty())
                    if (faceData != nullptr && !faceData->normals.empty())
                        applyTriangleNormal(triangle,
                                            &faceData->normals[faceIndex]);
            }
        }
    }
    draw_.subEntityTraits().setFrom(originalTraits);

    const bool emitEdges =
        edgeData != nullptr &&
        (!edgeData->trueColors.empty() || !edgeData->visibility.empty());
    if (emitEdges)
    {
        std::vector<bool> edgeVisible(edges.size(), false);
        for (const Face &face : faces)
        {
            if (!face.edgesVisible)
                continue;
            for (size_t index = 0; index < face.indices.size(); ++index)
            {
                const size_t a = face.indices[index];
                const size_t b = face.indices[(index + 1) % face.indices.size()];
                const size_t edgeIndex =
                    edgeIndices.at({std::min(a, b), std::max(a, b)});
                edgeVisible[edgeIndex] = true;
            }
        }
        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
        {
            if (!edgeVisible[edgeIndex] ||
                (!edgeData->visibility.empty() &&
                 edgeData->visibility[edgeIndex] == 0))
                continue;
            if (!edgeData->trueColors.empty())
                draw_.subEntityTraits().setColor(
                    edgeData->trueColors[edgeIndex].renderColor());
            const auto [a, b] = edges[edgeIndex];
            worldLine(vertexList[a], vertexList[b]);
        }
        draw_.subEntityTraits().setFrom(originalTraits);
    }
    return false;
}
