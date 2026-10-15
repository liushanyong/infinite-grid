#include "acdb/AcDbEntityWorldDraw.h"


namespace acdb
{
void worldDraw(const AcDbArc &arc, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(arc.center, arc.normal, arc.radius,
                                       arc.startAngle, arc.endAngle));
}

void worldDraw(const AcDbCircle &circle, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(circle.center, circle.normal,
                                       circle.radius));
}

void worldDraw(const AcDbEllipse &ellipse, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldEllipse(AcGeEllip3d(ellipse.center, ellipse.majorAxis,
                                      ellipse.normal, ellipse.radiusRatio,
                                      ellipse.startParameter,
                                      ellipse.endParameter));
}

void worldDraw(const AcDbLine &line, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldLine(line.start, line.end);
}

void worldDraw(const AcDbSpline &spline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    const std::vector<AcGePoint3d> points =
        acdb::sampleFitPointSpline(spline, draw.options());
    if (points.size() < 2)
        return;
    AcGiWorldDraw graphics(draw);
    graphics.worldPolyline(points.data(), static_cast<int>(points.size()),
                           spline.closed);
}

void worldDraw(const AcDbRay &ray, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldInfiniteLine(ray.start, ray.direction);
}

void worldDraw(const AcDbXline &xline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldInfiniteLine(xline.point, xline.direction);
    graphics.worldInfiniteLine(xline.point, -xline.direction);
}

void worldDraw(const AcDbSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    // DXF SOLID semantics: the corners are authored in zigzag order and
    // the quad is 1-2-4-3, split along the 2-3 diagonal — matching
    // tessellate(AcDbSolid) exactly, winding included.
    graphics.worldTriangle(solid.firstCorner, solid.secondCorner,
                           solid.thirdCorner, fillIs3DFace);
    graphics.worldTriangle(solid.secondCorner, solid.fourthCorner,
                           solid.thirdCorner, fillIs3DFace);
}

void worldDraw(const AcDbHatch &hatch, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    if (hatch.solidFill || hatch.patternName == "SOLID")
    {
        for (size_t i = 1; i + 1 < hatch.outerLoop.size(); ++i)
            graphics.worldTriangle(hatch.outerLoop[0], hatch.outerLoop[i],
                                   hatch.outerLoop[i + 1], fillIs3DFace);
        return;
    }
    for (const auto &segment : hatchPatternSegments(hatch))
        graphics.worldLine(segment.first, segment.second);
}

void worldDraw(const AcDb3dPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    drawPolylineCallbacks(graphics, polyline, draw.options());
}

void worldDraw(const AcDbPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    drawPolylineCallbacks(graphics, polyline, draw.options());
}

void worldDraw(const AcDbText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    AcGiTextStyle style;
    style.setTextSize(text.height);
    const AcGeVector3d direction(std::cos(text.rotation),
                                 std::sin(text.rotation), 0.0);
    graphics.text(text.insertion, AcGeVector3d(0.0, 0.0, 1.0), direction,
                  text.text.c_str(), style);
}

void worldDraw(const AcDbMText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    AcGiTextStyle style;
    style.setTextSize(text.height);
    graphics.text(text.insertion, AcGeVector3d(0.0, 0.0, 1.0),
                  text.direction, text.text.c_str(), style);
}

void worldDraw(const AcDb3dSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    acdb::TessellatedEntity fragment;
    acdb::tessellate(solid, fragment, draw.options());

    acdb::TessellatedEntity fillsOnly;
    fillsOnly.fills = std::move(fragment.fills);
    draw.sink().append(fillsOnly, draw.subEntityTraits().traits(),
                       fillIs3DFace);

    const glm::vec4 &color = draw.subEntityTraits().traits().color;
    acgs::DrawTraits edgeTraits = draw.subEntityTraits().traits();
    edgeTraits.color = glm::vec4(1.0f - color.r, 1.0f - color.g,
                                 1.0f - color.b, color.a);
    acdb::TessellatedEntity strokesOnly;
    strokesOnly.strokes = std::move(fragment.strokes);
    draw.sink().append(strokesOnly, edgeTraits, fillIs3DFace);
}

namespace dimdraw
{

AcGeVector3d perpXY(const AcGeVector3d &direction)
{
    return AcGeVector3d(-direction.y, direction.x, 0.0);
}

AcGePoint3d projectOnLine(const AcGePoint3d &point,
                                 const AcGePoint3d &origin,
                                 const AcGeVector3d &direction)
{
    return origin + direction *
                        ((point - origin).dotProduct(direction) /
                         direction.lengthSq());
}

AcGeVector3d directionXY(double angle, double length)
{
    return AcGeVector3d(std::cos(angle) * length, std::sin(angle) * length,
                        0.0);
}

double angleOf(const AcGeVector3d &vector)
{
    return std::atan2(vector.y, vector.x);
}

double ccwSweep(double start, double end)
{
    double sweep = std::fmod(end - start, kTwoPi);
    if (sweep < 0.0)
        sweep += kTwoPi;
    return sweep;
}

double deriveTextHeight(double span)
{
    const double height = span * 0.07;
    return height > 1.0e-4 ? height : 1.0e-4;
}

double arrowSizeFor(double textHeight)
{
    return textHeight * 0.85;
}

void arrowhead(AcGiWorldDraw &graphics, const AcGePoint3d &tip,
                      const AcGeVector3d &direction, double size)
{
    const AcGeVector3d unit = direction.lengthSq() > 0.0
                                  ? direction.normal()
                                  : AcGeVector3d(1.0, 0.0, 0.0);
    const AcGeVector3d side = perpXY(unit) * (size * 0.28);
    const AcGePoint3d back = tip - unit * size;
    graphics.worldTriangle(tip, back + side, back - side, false);
}

void extensionLine(AcGiWorldDraw &graphics,
                          const AcGePoint3d &feature,
                          const AcGePoint3d &lineEnd,
                          const AcGeVector3d &overshoot,
                          double gap)
{
    const AcGeVector3d run = lineEnd - feature;
    const double length = run.length();
    if (length <= gap + 1.0e-9)
        return;
    const AcGePoint3d start = feature + run * (gap / length);
    graphics.worldLine(start, lineEnd + overshoot);
}

void dimensionText(AcGiWorldDraw &graphics, const AcGePoint3d &position,
                          const AcGeVector3d &direction,
                          const AcDbDimension &dimension,
                          const std::string &message)
{
    AcGiTextStyle style;
    style.setTextSize(dimension.textHeight);
    AcGeVector3d textDirection = direction.lengthSq() > 0.0
                                     ? direction.normal()
                                     : AcGeVector3d(1.0, 0.0, 0.0);
    if (dimension.textRotation != 0.0)
        textDirection = directionXY(dimension.textRotation);
    graphics.text(position, AcGeVector3d(0.0, 0.0, 1.0), textDirection,
                  message.c_str(), style);
}

std::string measurementMessage(const AcDbDimension &dimension,
                                      double measured,
                                      const char *prefix)
{
    if (!dimension.dimensionText.empty())
        return dimension.dimensionText;
    const double value = dimension.actualMeasurement != 0.0
                             ? dimension.actualMeasurement
                             : measured;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s%.6g", prefix, value);
    return buffer;
}

std::string stripFrameCodes(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        const char c = text[i];
        if (c == '{' || c == '}')
        {
            ++i;
            continue;
        }
        if (c == '\\' && i + 1 < text.size())
        {
            std::size_t j = i + 1;
            while (j < text.size() &&
                   ((text[j] >= 'a' && text[j] <= 'z') ||
                    (text[j] >= 'A' && text[j] <= 'Z')))
                ++j;
            if (j > i + 1)
            {
                // \f, \A, \F...; style switches — skip up to the ';'.
                while (j < text.size() && text[j] != ';')
                    ++j;
                i = j < text.size() ? j + 1 : j;
                continue;
            }
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

void drawLinearDimension(
    AcGiWorldDraw &graphics, const AcDbDimension &dimension,
    const AcGePoint3d &xLine1, const AcGePoint3d &xLine2,
    const AcGePoint3d &dimensionLinePoint, const AcGeVector3d &direction,
    double measured, const char *prefix)
{
    AcDbDimension styled = dimension;
    styled.textHeight =
        dimension.textHeight > 0.0
            ? dimension.textHeight
            : deriveTextHeight(measured > 0.0 ? measured
                                              : (xLine2 - xLine1).length());
    const double textHeight = styled.textHeight;
    const double arrowSize = arrowSizeFor(textHeight);
    const AcGeVector3d unit = direction.lengthSq() > 0.0
                                  ? direction.normal()
                                  : AcGeVector3d(1.0, 0.0, 0.0);
    const AcGeVector3d across = perpXY(unit);

    // Foot of each extension line on the dimension line through
    // |dimensionLinePoint|.
    const AcGePoint3d foot1 = projectOnLine(xLine1, dimensionLinePoint, unit);
    const AcGePoint3d foot2 = projectOnLine(xLine2, dimensionLinePoint, unit);
    if ((foot2 - foot1).lengthSq() < 1.0e-12)
        return;

    extensionLine(graphics, xLine1, foot1, across * (textHeight * 0.2),
                  textHeight * 0.15);
    extensionLine(graphics, xLine2, foot2, across * (textHeight * 0.2),
                  textHeight * 0.15);
    graphics.worldLine(foot1, foot2);
    arrowhead(graphics, foot1, unit, arrowSize);
    arrowhead(graphics, foot2, -unit, arrowSize);

    const AcGePoint3d textPosition =
        styled.textPosition != AcGePoint3d(0.0, 0.0, 0.0)
            ? styled.textPosition
            : foot1 + (foot2 - foot1) * 0.5 + across * (textHeight * 0.5);
    dimensionText(graphics, textPosition, unit, styled,
                  measurementMessage(styled, measured, prefix));
}

void drawAngularDimension(AcGiWorldDraw &graphics,
                                 const AcGePoint3d &center,
                                 const AcGePoint3d &ray1Point,
                                 const AcGePoint3d &ray2Point,
                                 const AcDbDimension &dimension)
{
    const AcGeVector3d ray1 = ray1Point - center;
    const AcGeVector3d ray2 = ray2Point - center;
    if (ray1.lengthSq() < 1.0e-18 || ray2.lengthSq() < 1.0e-18)
        return;
    double startAngle = angleOf(ray1);
    double sweep = ccwSweep(startAngle, angleOf(ray2));
    const AcGePoint3d *startRayPoint = &ray1Point;
    const AcGePoint3d *endRayPoint = &ray2Point;

    double radius = std::min(ray1.length(), ray2.length()) * 0.5;
    if (dimension.textPosition != AcGePoint3d(0.0, 0.0, 0.0))
    {
        radius = (dimension.textPosition - center).length();
        const double textAngle = angleOf(dimension.textPosition - center);
        if (ccwSweep(startAngle, textAngle) > sweep)
        {
            // The text sits on the complementary sector: draw CCW from
            // ray 2 back to ray 1 instead (worldCircle rejects negative
            // sweeps, so the flip re-anchors the arc rather than
            // reversing it).
            startAngle = angleOf(ray2);
            sweep = kTwoPi - sweep;
            std::swap(startRayPoint, endRayPoint);
        }
    }
    if (sweep < 1.0e-9 || radius < 1.0e-9)
        return;

    AcDbDimension styled = dimension;
    styled.textHeight = dimension.textHeight > 0.0
                            ? dimension.textHeight
                            : deriveTextHeight(radius * sweep);
    const double textHeight = styled.textHeight;
    const double arrowSize = arrowSizeFor(textHeight);

    // Extension lines run from the arc out to the feature points.
    const AcGeVector3d unit1(std::cos(startAngle), std::sin(startAngle), 0.0);
    const AcGeVector3d unit2(std::cos(startAngle + sweep),
                             std::sin(startAngle + sweep), 0.0);
    graphics.worldLine(center + unit1 * radius,
                       *startRayPoint + unit1 * (textHeight * 0.2));
    graphics.worldLine(center + unit2 * radius,
                       *endRayPoint + unit2 * (textHeight * 0.2));

    graphics.worldCircle(AcGeCircArc3d(center, AcGeVector3d(0.0, 0.0, 1.0),
                                       radius, startAngle,
                                       startAngle + sweep));
    // Arrows sit tangent to the arc at both ends, pointing inward.
    arrowhead(graphics, center + unit1 * radius,
              AcGeVector3d(-unit1.y, unit1.x, 0.0), arrowSize);
    arrowhead(graphics, center + unit2 * radius,
              AcGeVector3d(unit2.y, -unit2.x, 0.0), arrowSize);

    const AcGePoint3d textPosition =
        styled.textPosition != AcGePoint3d(0.0, 0.0, 0.0)
            ? styled.textPosition
            : center + AcGeVector3d(std::cos(startAngle + sweep * 0.5),
                                    std::sin(startAngle + sweep * 0.5), 0.0) *
                              (radius + textHeight * 0.5);
    dimensionText(graphics, textPosition,
                  AcGeVector3d(-std::sin(startAngle + sweep * 0.5),
                               std::cos(startAngle + sweep * 0.5), 0.0),
                  styled, measurementMessage(styled,
                                             sweep * 180.0 /
                                                 3.14159265358979323846));
}

} // namespace dimdraw

void worldDraw(const AcDbRotatedDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const AcGeVector3d unit =
        dimdraw::directionXY(dimension.dimensionLineAngle);
    const double measured = std::abs(
        (dimension.xLine2Point - dimension.xLine1Point).dotProduct(unit));
    dimdraw::drawLinearDimension(graphics, dimension.dimension,
                                 dimension.xLine1Point,
                                 dimension.xLine2Point,
                                 dimension.dimensionLinePoint, unit, measured,
                                 "");
}

void worldDraw(const AcDbAlignedDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const AcGeVector3d unit = dimension.xLine2Point - dimension.xLine1Point;
    const double measured = unit.length();
    dimdraw::drawLinearDimension(graphics, dimension.dimension,
                                 dimension.xLine1Point,
                                 dimension.xLine2Point,
                                 dimension.dimensionLinePoint, unit, measured,
                                 "");
}

void worldDraw(const AcDb2LineAngularDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const auto farther = [&dimension](const AcGePoint3d &a,
                                      const AcGePoint3d &b) {
        return (a - dimension.arcPoint).lengthSq() >
                       (b - dimension.arcPoint).lengthSq()
                   ? a
                   : b;
    };
    dimdraw::drawAngularDimension(
        graphics, dimension.arcPoint,
        farther(dimension.xLine1StartPoint, dimension.xLine1EndPoint),
        farther(dimension.xLine2StartPoint, dimension.xLine2EndPoint),
        dimension.dimension);
}

void worldDraw(const AcDb3PointAngularDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    dimdraw::drawAngularDimension(graphics, dimension.centerPoint,
                                  dimension.xLine1Point,
                                  dimension.xLine2Point,
                                  dimension.dimension);
}

void worldDraw(const AcDbRadialDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const AcGeVector3d run = dimension.radiusPoint - dimension.centerPoint;
    const double measured = run.length();
    if (measured < 1.0e-9)
        return;
    const AcGeVector3d unit = run.normal();
    AcDbDimension styled = dimension.dimension;
    styled.textHeight =
        dimension.dimension.textHeight > 0.0
            ? dimension.dimension.textHeight
            : dimdraw::deriveTextHeight(measured);
    const double textHeight = styled.textHeight;
    graphics.worldLine(dimension.centerPoint, dimension.radiusPoint);
    dimdraw::arrowhead(graphics, dimension.radiusPoint, unit,
                       dimdraw::arrowSizeFor(textHeight));
    const AcGePoint3d textPosition =
        styled.textPosition != AcGePoint3d(0.0, 0.0, 0.0)
            ? styled.textPosition
            : dimension.radiusPoint +
                  unit * std::max(dimension.leaderLength,
                                  textHeight * 1.5);
    dimdraw::dimensionText(graphics, textPosition,
                           dimdraw::perpXY(unit),
                           styled,
                           dimdraw::measurementMessage(styled, measured,
                                                       "R"));
}

void worldDraw(const AcDbDiametricDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const AcGeVector3d run =
        dimension.farChordPoint - dimension.chordPoint;
    const double measured = run.length();
    if (measured < 1.0e-9)
        return;
    const AcGeVector3d unit = run.normal();
    AcDbDimension styled = dimension.dimension;
    styled.textHeight =
        dimension.dimension.textHeight > 0.0
            ? dimension.dimension.textHeight
            : dimdraw::deriveTextHeight(measured);
    const double textHeight = styled.textHeight;
    graphics.worldLine(dimension.chordPoint, dimension.farChordPoint);
    dimdraw::arrowhead(graphics, dimension.chordPoint, -unit,
                       dimdraw::arrowSizeFor(textHeight));
    dimdraw::arrowhead(graphics, dimension.farChordPoint, unit,
                       dimdraw::arrowSizeFor(textHeight));
    const AcGePoint3d textPosition =
        styled.textPosition != AcGePoint3d(0.0, 0.0, 0.0)
            ? styled.textPosition
            : dimension.chordPoint -
                  unit * std::max(dimension.leaderLength,
                                  textHeight * 1.5);
    dimdraw::dimensionText(graphics, textPosition, dimdraw::perpXY(unit),
                           styled,
                           dimdraw::measurementMessage(styled, measured,
                                                       "\xE2\x8C\x8D"));
}

void worldDraw(const AcDbOrdinateDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const AcGePoint3d &feature = dimension.featureLocationPoint;
    const AcGePoint3d &end = dimension.leaderEndPoint;
    const AcGePoint3d corner = dimension.useXAxis
                                   ? AcGePoint3d(end.x, feature.y, feature.z)
                                   : AcGePoint3d(feature.x, end.y, feature.z);
    graphics.worldPolyline(
        std::vector<AcGePoint3d>{feature, corner, end}.data(), 3, false);
    const double value = dimension.useXAxis ? feature.x : feature.y;
    AcDbDimension styled = dimension.dimension;
    styled.textHeight =
        dimension.dimension.textHeight > 0.0
            ? dimension.dimension.textHeight
            : dimdraw::deriveTextHeight(
                  std::max((end - feature).length(), 1.0));
    dimdraw::dimensionText(graphics,
                           corner + (end - corner) * 0.5 +
                               dimdraw::perpXY(end - corner) *
                                   (styled.textHeight * 0.6),
                           end - corner, styled,
                           dimdraw::measurementMessage(styled, value));
}

void worldDraw(const AcDbLeader &leader, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    if (leader.points.size() < 2)
        return;
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    graphics.worldPolyline(leader.points.data(),
                           static_cast<int>(leader.points.size()), false);
    if (leader.hasArrowhead)
    {
        const AcGeVector3d launch = leader.points[1] - leader.points[0];
        if (launch.lengthSq() > 0.0)
        {
            const double textHeight =
                std::max(launch.length() * 0.1, 1.0e-3);
            dimdraw::arrowhead(graphics, leader.points[0], launch.normal(),
                               dimdraw::arrowSizeFor(textHeight));
        }
    }
}

void worldDraw(const AcDbFcf &fcf, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    const std::string message = dimdraw::stripFrameCodes(fcf.text);
    if (message.empty())
        return;
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    AcGiTextStyle style;
    const double height = fcf.height > 0.0 ? fcf.height : 1.0;
    style.setTextSize(height);
    const AcGeVector3d right = fcf.direction.lengthSq() > 0.0
                                   ? fcf.direction.normal()
                                   : AcGeVector3d(1.0, 0.0, 0.0);
    const AcGeVector3d up =
        fcf.normal.lengthSq() > 0.0
            ? fcf.normal.normal().crossProduct(right).normal()
            : AcGeVector3d(0.0, 1.0, 0.0);
    graphics.text(fcf.insertion, fcf.normal, right, message.c_str(), style);

    // Frame box sized like the text layout fallback: ~0.6 em per glyph.
    std::size_t longest = 0;
    std::size_t current = 0;
    std::size_t lineCount = 1;
    for (const char c : message)
    {
        if (c == 10)
        {
            longest = std::max(longest, current);
            current = 0;
            ++lineCount;
        }
        else
            ++current;
    }
    longest = std::max(longest, current);
    const double padding = height * 0.3;
    const AcGeVector3d width = right * (double(longest) * height * 0.6 +
                                        padding * 2.0);
    const AcGeVector3d lift = up * (height * double(lineCount) +
                                    padding * 2.0);
    const AcGePoint3d origin = fcf.insertion - right * padding - up * padding;
    graphics.worldLine(origin, origin + width);
    graphics.worldLine(origin + width, origin + width + lift);
    graphics.worldLine(origin + width + lift, origin + lift);
    graphics.worldLine(origin + lift, origin);
}

void worldDraw(const AcDbShape &shape, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    const double size = shape.scale > 0.0 ? shape.scale : 1.0;
    const AcGeVector3d right = dimdraw::directionXY(shape.rotation, size);
    const AcGeVector3d up = dimdraw::perpXY(right);
    graphics.worldLine(shape.insertion - right, shape.insertion + right);
    graphics.worldLine(shape.insertion - up, shape.insertion + up);
}

void worldDraw(const AcDbAttributeDefinition &definition,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    if (definition.tag.empty())
        return;
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    AcGiTextStyle style;
    style.setTextSize(definition.height);
    style.setXScale(definition.widthFactor > 0.0 ? definition.widthFactor
                                                 : 1.0);
    graphics.text(definition.insertion, AcGeVector3d(0.0, 0.0, 1.0),
                  dimdraw::directionXY(definition.rotation),
                  definition.tag.c_str(), style);
}

void worldDraw(const AcDbAttribute &attribute,
                      acgs::ViewportDraw &draw, bool fillIs3DFace)
{
    if (attribute.textValue.empty())
        return;
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    AcGiTextStyle style;
    style.setTextSize(attribute.height);
    style.setXScale(attribute.widthFactor > 0.0 ? attribute.widthFactor
                                                : 1.0);
    graphics.text(attribute.insertion, AcGeVector3d(0.0, 0.0, 1.0),
                  dimdraw::directionXY(attribute.rotation),
                  attribute.textValue.c_str(), style);
}

void worldDraw(const AcDbMInsertBlock &insert, acgs::ViewportDraw &draw,
                      bool fillIs3DFace)
{
    AcGiWorldDraw graphics(draw);
    (void)fillIs3DFace;
    graphics.worldPoint(insert.position);
}

// ---- AcGiDrawable protocol glue (ARX IS-A: entities draw themselves) ----
//
// AcDbEntity defaults + one subWorldDraw override per concrete entity,
// each funneling into the worldDraw overload set above so the drawable
// protocol and the tessellation path emit identical geometry.

Adesk::UInt32 AcDbEntity::subSetAttributes(AcGiDrawableTraits *traits)
{
    if (traits != nullptr)
        traits->setFrom(*this);
    return AcGiDrawable::kDrawableIsAnEntity;
}

Adesk::Boolean AcDbEntity::subWorldDraw(AcGiWorldDraw *wd)
{
    (void)wd;
    return true;
}

void AcDbEntity::subViewportDraw(AcGiViewportDraw *vd)
{
    subWorldDraw(vd);
}

#define ACGI_ENTITY_WORLD_DRAW(EntityType)                                     \
    Adesk::Boolean EntityType::subWorldDraw(AcGiWorldDraw *wd)                 \
    {                                                                          \
        if (wd == nullptr)                                                     \
            return false;                                                      \
        wd->subEntityTraits().setFrom(*this);                                  \
        acdb::worldDraw(*this, wd->context());                                 \
        return true;                                                           \
    }

#define ACGI_DIMENSION_WORLD_DRAW(EntityType)                                  \
    Adesk::Boolean EntityType::subWorldDraw(AcGiWorldDraw *wd)                 \
    {                                                                          \
        if (wd == nullptr)                                                     \
            return false;                                                      \
        wd->subEntityTraits().setFrom(*this);                                  \
        acdb::worldDraw(*this, wd->context());                                 \
        return true;                                                           \
    }                                                                          \
    Adesk::UInt32 EntityType::subSetAttributes(AcGiDrawableTraits *traits)     \
    {                                                                          \
        AcDbEntity::subSetAttributes(traits);                                  \
        return AcGiDrawable::kDrawableIsDimension;                             \
    }

ACGI_ENTITY_WORLD_DRAW(AcDb3dPolyline)
// AcDb3dSolid: its own fillIs3DFace data selects the surface channel.
Adesk::Boolean AcDb3dSolid::subWorldDraw(AcGiWorldDraw *wd)
{
    if (wd == nullptr)
        return false;
    wd->subEntityTraits().setFrom(*this);
    acdb::worldDraw(*this, wd->context(), fillIs3DFace);
    return true;
}
ACGI_ENTITY_WORLD_DRAW(AcDbArc)
ACGI_ENTITY_WORLD_DRAW(AcDbAttribute)
ACGI_ENTITY_WORLD_DRAW(AcDbAttributeDefinition)
ACGI_ENTITY_WORLD_DRAW(AcDbBlockReference)
ACGI_ENTITY_WORLD_DRAW(AcDbCircle)
ACGI_ENTITY_WORLD_DRAW(AcDbEllipse)
ACGI_ENTITY_WORLD_DRAW(AcDbFcf)
ACGI_ENTITY_WORLD_DRAW(AcDbHatch)
ACGI_ENTITY_WORLD_DRAW(AcDbLeader)
ACGI_ENTITY_WORLD_DRAW(AcDbLight)
ACGI_ENTITY_WORLD_DRAW(AcDbLine)
ACGI_ENTITY_WORLD_DRAW(AcDbMInsertBlock)
ACGI_ENTITY_WORLD_DRAW(AcDbMText)
ACGI_ENTITY_WORLD_DRAW(AcDbMline)
ACGI_ENTITY_WORLD_DRAW(AcDbPoint)
ACGI_ENTITY_WORLD_DRAW(AcDbPolyFaceMesh)
ACGI_ENTITY_WORLD_DRAW(AcDbPolyline)
ACGI_ENTITY_WORLD_DRAW(AcDbRay)
ACGI_ENTITY_WORLD_DRAW(AcDbShape)
ACGI_ENTITY_WORLD_DRAW(AcDbSolid)
ACGI_ENTITY_WORLD_DRAW(AcDbSpline)
ACGI_ENTITY_WORLD_DRAW(AcDbText)
ACGI_ENTITY_WORLD_DRAW(AcDbXline)
ACGI_DIMENSION_WORLD_DRAW(AcDbRotatedDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDbAlignedDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDb2LineAngularDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDb3PointAngularDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDbRadialDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDbDiametricDimension)
ACGI_DIMENSION_WORLD_DRAW(AcDbOrdinateDimension)

#undef ACGI_ENTITY_WORLD_DRAW
#undef ACGI_DIMENSION_WORLD_DRAW

} // namespace acdb
