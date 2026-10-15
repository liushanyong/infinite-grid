#include "acgi/AcGiWorldDraw.h"

#include <algorithm>
#include <cmath>
#include <utility>

bool AcGiWorldDraw::worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
{
    acdb::Stroke &stroke = sink().addStroke();
    stroke.points = {apply(from), apply(to)};
    applyTraits(stroke);
    return true;
}

bool AcGiWorldDraw::worldPolyline(const AcGePoint3d *points, int count,
                                  bool closed)
{
    if (!points || count < 2)
        return false;
    acdb::Stroke &stroke = sink().addStroke(closed);
    stroke.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        stroke.points.push_back(apply(points[i]));
    applyTraits(stroke);
    return true;
}

bool AcGiWorldDraw::worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                                  const AcGePoint3d &c, bool fillIs3DFace)
{
    acdb::Triangle triangle{{}, apply(a), apply(b), apply(c)};
    applyTraits(triangle, fillIs3DFace);
    sink().appendTriangle(triangle);
    return true;
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
    return true;
}

bool AcGiWorldDraw::worldInfiniteLine(const AcGePoint3d &start,
                                      const AcGeVector3d &direction)
{
    const AcGeVector3d dir = direction.normal();
    if (dir.lengthSq() <= 0.0)
        return false;
    constexpr double kProxyLength = 1.0e6;
    acdb::Stroke &stroke = sink().addStroke(false);
    stroke.points = {apply(start),
                     apply(start + dir * kProxyLength)};
    stroke.semiInfinite = true;
    applyTraits(stroke);
    return true;
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
        return false;
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
    return true;
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const char *message,
                         const AcGiTextStyle &style)
{
    if (!message || !*message)
        return false;
    const double height = style.textSize;
    if (!(height > 0.0) || !std::isfinite(height))
        return false;

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
        return true;

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
    return true;
}

bool AcGiWorldDraw::text(const AcGePoint3d &position,
                         const AcGeVector3d &normal,
                         const AcGeVector3d &direction, const ACHAR *message,
                         const AcGiTextStyle &style)
{
    if (!message)
        return false;
    const AcUtf8String utf8Message =
        arxcompat::toUtf8(AcStringView(message));
    return text(position, normal, direction, utf8Message.c_str(), style);
}

bool AcGiWorldDraw::worldEllipse(const AcGeEllip3d &ellipse)
{
    const double sweep = ellipse.endAngle - ellipse.startAngle;
    if (!(sweep > 0.0))
        return false;
    const acdb::TesselationOptions options =
        draw_.optionsFor(ellipse.majorRadius());
    const int segments = std::max(
        8, static_cast<int>(std::ceil(sweep / options.angleStep)));
    const bool closed = sweep >= 6.2831853071795864769 - 1.0e-12;
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
