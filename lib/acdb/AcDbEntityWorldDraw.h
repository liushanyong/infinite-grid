#pragma once

#include <glm/glm.hpp>

#include "acgs/model/DrawContext.h"
#include "../acgi/AcGiWorldDraw.h"
#include "acdb/AcDbTessellate.h"

namespace acdb
{

// Translate the established value-type tessellation overload set into the
// AcGi-lite draw protocol.  The entity-local fragment keeps generated geometry
// isolated until GeometrySink applies the current traits.
template <typename EntityType>
inline void worldDraw(const EntityType &entity, acgs::WorldDraw &draw,
                      const TesselationOptions &options, bool fillIs3DFace = false)
{
    TessellatedEntity fragment;
    tessellate(entity, fragment, options);
    draw.sink().append(fragment, draw.subEntityTraits().traits(), fillIs3DFace);
}

template <typename EntityType>
inline void worldDraw(const EntityType &entity, acgs::WorldDraw &draw,
                      bool fillIs3DFace = false)
{
    worldDraw(entity, draw, draw.options(), fillIs3DFace);
}

// Viewport-aware curves choose their chord angle from ViewportDraw::deviation.
inline void worldDraw(const AcDbArc &arc, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(arc.center, arc.normal, arc.radius,
                                       arc.startAngle, arc.endAngle));
}

inline void worldDraw(const AcDbCircle &circle, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(circle.center, circle.normal,
                                       circle.radius));
}

// Protocol-pattern overloads: these entities draw themselves through the
// AcGiWorldDraw callback interface instead of tessellating up front, which
// is how ObjectARX custom entities emit geometry.

inline void worldDraw(const AcDbEllipse &ellipse, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldEllipse(AcGeEllip3d(ellipse.center, ellipse.majorAxis,
                                      ellipse.normal, ellipse.radiusRatio,
                                      ellipse.startParameter,
                                      ellipse.endParameter));
}

inline void worldDraw(const AcDbLine &line, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldLine(line.start, line.end);
}

// Fit-point splines draw through the same chord-length natural cubic
// interpolation as the tessellation path.
inline void worldDraw(const AcDbSpline &spline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    const std::vector<AcGePoint3d> points =
        acdb::sampleFitPointSpline(spline, draw.options());
    if (points.size() < 2)
        return;
    AcGiWorldDraw graphics(draw);
    graphics.worldPolyline(points.data(), static_cast<int>(points.size()),
                           spline.closed);
}

inline void worldDraw(const AcDbRay &ray, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldInfiniteLine(ray.start, ray.direction);
}

// AcDbXline draws as two opposite semi-infinite strokes sharing the point.
inline void worldDraw(const AcDbXline &xline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldInfiniteLine(xline.point, xline.direction);
    graphics.worldInfiniteLine(xline.point, -xline.direction);
}

inline void worldDraw(const AcDbSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
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

// AcDbHatch: the solid variant fans its loop into triangles; line patterns emit
// one worldLine per clipped scanline segment.
inline void worldDraw(const AcDbHatch &hatch, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
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

// Polylines emit their subdivided outline (bulge arcs included) as one
// worldPolyline callback, then their thickness walls as worldTriangle
// callbacks — the same geometry the tessellation path produces.
template <typename PolylineType>
inline void drawPolylineCallbacks(AcGiWorldDraw &graphics,
                                  const PolylineType &polyline,
                                  const TesselationOptions &options)
{
    const std::vector<AcGePoint3d> outline = polylineOutline(polyline, options);
    if (outline.size() < 2)
        return;
    graphics.worldPolyline(outline.data(),
                           static_cast<int>(outline.size()),
                           polyline.closed);
    for (const acdb::Triangle &wall :
         acdb::polylineWallTriangles(polyline, outline))
        graphics.worldTriangle(wall.a, wall.b, wall.c, false);
}

inline void worldDraw(const AcDb3dPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    drawPolylineCallbacks(graphics, polyline, draw.options());
}

inline void worldDraw(const AcDbPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    drawPolylineCallbacks(graphics, polyline, draw.options());
}

// AcDbText entities draw through the worldText callback; the style carries the
// entity height so the layout frame matches the tessellation path.
inline void worldDraw(const AcDbText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    AcGiTextStyle style;
    style.setTextSize(text.height);
    const AcGeVector3d direction(std::cos(text.rotation),
                                 std::sin(text.rotation), 0.0);
    graphics.text(text.insertion, AcGeVector3d(0.0, 0.0, 1.0), direction,
                  text.text.c_str(), style);
}

inline void worldDraw(const AcDbMText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    AcGiTextStyle style;
    style.setTextSize(text.height);
    graphics.text(text.insertion, AcGeVector3d(0.0, 0.0, 1.0),
                  text.direction, text.text.c_str(), style);
}

// A AcDb3dSolid renders its fill with the entity color and its feature-edge
// border strokes with the inverted color, so the border reads against the
// fill no matter which entity color is authored.
inline void worldDraw(const AcDb3dSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false)
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

} // namespace acdb
