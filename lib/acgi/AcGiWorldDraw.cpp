#include "acgi/AcGiWorldDraw.h"

#include <algorithm>
#include <cmath>
#include <utility>

// Return convention (ObjectARX contract): the primitive emitters return
// kFalse (false) when the primitive was stored, kTrue (true) when the
// operation was terminated — see the AcGiGeometry header note.  Internal
// helpers in this file use plain bool (true = ok) and adapt at the boundary.

namespace
{
constexpr double kTwoPi = 6.2831853071795864769;

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
} // namespace

bool AcGiWorldDraw::worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
{
    acdb::Stroke &stroke = sink().addStroke();
    stroke.points = {apply(from), apply(to)};
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::worldPolyline(const AcGePoint3d *points, int count,
                                  bool closed)
{
    if (!points || count < 2)
        return true;
    acdb::Stroke &stroke = sink().addStroke(closed);
    stroke.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        stroke.points.push_back(apply(points[i]));
    applyTraits(stroke);
    return false;
}

bool AcGiWorldDraw::worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                                  const AcGePoint3d &c, bool fillIs3DFace)
{
    acdb::Triangle triangle{{}, apply(a), apply(b), apply(c)};
    applyTraits(triangle, fillIs3DFace);
    sink().appendTriangle(triangle);
    return false;
}

bool AcGiWorldDraw::worldPoint(const AcGePoint3d &position,
                               acge::PointDisplayStyle displayStyle)
{
    const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
    acdb::TessellatedPoint point;
    point.location = apply(position);
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
    acdb::Stroke &stroke = sink().addStroke(false);
    stroke.points = {apply(start),
                     apply(start + dir * kProxyLength)};
    stroke.semiInfinite = true;
    applyTraits(stroke);
    return false;
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
    if (arc.isFullCircle())
    {
        acdb::Stroke &stroke = sink().addStroke(true);
        stroke.points.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i)
        {
            const double t = sweep * static_cast<double>(i) / segments;
            stroke.points.push_back(apply(arc.pointAt(arc.startAngle + t)));
        }
        applyTraits(stroke);
    }
    else
    {
        acdb::Stroke &stroke = sink().addStroke(false);
        stroke.points.reserve(static_cast<size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i)
        {
            const double t = sweep * static_cast<double>(i) / segments;
            stroke.points.push_back(apply(arc.pointAt(arc.startAngle + t)));
        }
        applyTraits(stroke);
    }
    return false;
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

    acdb::Stroke &frame = sink().addStroke(true);
    frame.points.reserve(5);
    frame.points.push_back(apply(position));
    frame.points.push_back(apply(position + AcGeVector3d(right * width)));
    frame.points.push_back(
        apply(position + AcGeVector3d(right * width + frameHeight)));
    frame.points.push_back(apply(position + AcGeVector3d(frameHeight)));
    frame.points.push_back(apply(position));
    applyTraits(frame);
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
    (void)edgeData;
    (void)faceData;
    (void)vertexData;
    (void)autoGenerateNormals;
    if (rows < 2 || columns < 2 || vertexList == nullptr)
        return true;
    for (Adesk::UInt32 row = 0; row + 1 < rows; ++row)
    {
        for (Adesk::UInt32 column = 0; column + 1 < columns; ++column)
        {
            const AcGePoint3d &v00 =
                vertexList[row * columns + column];
            const AcGePoint3d &v01 =
                vertexList[row * columns + column + 1];
            const AcGePoint3d &v10 =
                vertexList[(row + 1) * columns + column];
            const AcGePoint3d &v11 =
                vertexList[(row + 1) * columns + column + 1];
            worldTriangle(v00, v01, v11);
            worldTriangle(v00, v11, v10);
        }
    }
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
    (void)edgeData;
    (void)faceData;
    (void)vertexData;
    if (nbVertex == 0 || vertexList == nullptr ||
        (faceListSize != 0 && faceList == nullptr))
        return true;
    Adesk::UInt32 position = 0;
    while (position < faceListSize)
    {
        const Adesk::Int32 count = faceList[position++];
        if (count == 0)
            break;
        const Adesk::UInt32 faceSize =
            count < 0 ? static_cast<Adesk::UInt32>(-count)
                      : static_cast<Adesk::UInt32>(count);
        if (faceSize < 3 || position + faceSize > faceListSize)
            return true; // malformed face list
        const Adesk::Int32 *face = faceList + position;
        position += faceSize;
        for (Adesk::UInt32 index = 2; index < faceSize; ++index)
        {
            const Adesk::Int32 i0 = face[0];
            const Adesk::Int32 i1 = face[index - 1];
            const Adesk::Int32 i2 = face[index];
            if (i0 < 0 || i1 < 0 || i2 < 0 ||
                static_cast<Adesk::UInt32>(i0) >= nbVertex ||
                static_cast<Adesk::UInt32>(i1) >= nbVertex ||
                static_cast<Adesk::UInt32>(i2) >= nbVertex)
                return true;
            worldTriangle(vertexList[i0], vertexList[i1], vertexList[i2]);
        }
    }
    return false;
}
