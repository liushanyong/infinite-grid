#pragma once

#include "AcDbPort.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <string>

#include "acgs/model/DrawContext.h"
#include "../acgi/AcGiWorldDraw.h"
#include "acdb/AcDbAttribute.h"
#include "acdb/AcDbAttributeDefinition.h"
#include "acdb/AcDbDimension.h"
#include "acdb/AcDbFcf.h"
#include "acdb/AcDbLeader.h"
#include "acdb/AcDbMInsertBlock.h"
#include "acdb/AcDbShape.h"
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
void worldDraw(const AcDbArc &arc, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbCircle &circle, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// Protocol-pattern overloads: these entities draw themselves through the
// AcGiWorldDraw callback interface instead of tessellating up front, which
// is how ObjectARX custom entities emit geometry.

void worldDraw(const AcDbEllipse &ellipse, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbLine &line, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// Fit-point splines draw through the same chord-length natural cubic
// interpolation as the tessellation path.
void worldDraw(const AcDbSpline &spline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbRay &ray, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbXline draws as two opposite semi-infinite strokes sharing the point.
void worldDraw(const AcDbXline &xline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbHatch: the solid variant fans its loop into triangles; line patterns emit
// one worldLine per clipped scanline segment.
void worldDraw(const AcDbHatch &hatch, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

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

void worldDraw(const AcDb3dPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbPolyline &polyline, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbText entities draw through the worldText callback; the style carries the
// entity height so the layout frame matches the tessellation path.
void worldDraw(const AcDbText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

void worldDraw(const AcDbMText &text, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// A AcDb3dSolid renders its fill with the entity color and its feature-edge
// border strokes with the inverted color, so the border reads against the
// fill no matter which entity color is authored.
void worldDraw(const AcDb3dSolid &solid, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// ---- annotation family (dimension / leader / fcf / shape / attributes) ----
//
// All of these draw themselves through the AcGiWorldDraw callbacks like
// ObjectARX entities do: dimension lines and extension lines are
// worldLine callbacks, arrowheads are one worldTriangle each, and the
// dimension text rides the text callback.  Appearance quantities that a
// full implementation resolves through the referenced DIMSTYLE record
// (arrow size, text height) derive from the entity geometry when the
// per-entity fallback is unset.

namespace dimdraw
{

constexpr double kTwoPi = 6.2831853071795864769;

AcGeVector3d perpXY(const AcGeVector3d &direction);

// Projects |point| onto the line through |origin| along |direction|.
AcGePoint3d projectOnLine(const AcGePoint3d &point,
                                 const AcGePoint3d &origin,
                                 const AcGeVector3d &direction);

AcGeVector3d directionXY(double angle, double length = 1.0);

// The angle of |vector| in the XY plane.
double angleOf(const AcGeVector3d &vector);

// CCW-normalized sweep from |start| to |end|.
double ccwSweep(double start, double end);

double deriveTextHeight(double span);

double arrowSizeFor(double textHeight);

// Filled arrowhead with its tip at |tip| pointing along |direction|.
void arrowhead(AcGiWorldDraw &graphics, const AcGePoint3d &tip,
                      const AcGeVector3d &direction, double size);

// Extension line from a feature point toward its dimension line end,
// keeping the standard gap at the feature and a small overshoot.
void extensionLine(AcGiWorldDraw &graphics,
                          const AcGePoint3d &feature,
                          const AcGePoint3d &lineEnd,
                          const AcGeVector3d &overshoot,
                          double gap);

// Draws the dimension text (user override wins) at |position| aligned
// with |direction|; |message| already carries the measurement value.
void dimensionText(AcGiWorldDraw &graphics, const AcGePoint3d &position,
                          const AcGeVector3d &direction,
                          const AcDbDimension &dimension,
                          const std::string &message);

// Formats the measurement when no user text override exists.
std::string measurementMessage(const AcDbDimension &dimension,
                                      double measured,
                                      const char *prefix = "");

// Strips the AutoCAD formatting codes a TOLERANCE string carries
// ({\fArial|b0;...} font runs, \A1; alignment) leaving the glyphs.
std::string stripFrameCodes(const std::string &text);

// ---- shared linear-dimension construction (rotated/aligned) ----

void drawLinearDimension(
    AcGiWorldDraw &graphics, const AcDbDimension &dimension,
    const AcGePoint3d &xLine1, const AcGePoint3d &xLine2,
    const AcGePoint3d &dimensionLinePoint, const AcGeVector3d &direction,
    double measured, const char *prefix);

// Shared angular arc: extension lines from the two feature rays, the arc
// through |radius|, arrows at both ends, text at the arc mid-radial.
// The drawn sector is the one containing the authored text position.
void drawAngularDimension(AcGiWorldDraw &graphics,
                                 const AcGePoint3d &center,
                                 const AcGePoint3d &ray1Point,
                                 const AcGePoint3d &ray2Point,
                                 const AcDbDimension &dimension);

} // namespace dimdraw

// AcDbRotatedDimension: dimension line at |dimensionLineAngle| through
// the definition point; measures the projected point distance.
void worldDraw(const AcDbRotatedDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbAlignedDimension: dimension line parallel to the measured points.
void worldDraw(const AcDbAlignedDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDb2LineAngularDimension: the arc center is the authored intersection
// point; each line's farther endpoint anchors its extension line.
void worldDraw(const AcDb2LineAngularDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDb3PointAngularDimension: explicit center between two ray targets.
void worldDraw(const AcDb3PointAngularDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbRadialDimension: center→circle stroke with the arrowhead on the
// circle and the "R" callout on the landing side.
void worldDraw(const AcDbRadialDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbDiametricDimension: full chord, arrows on both ends, "Ø" callout.
void worldDraw(const AcDbDiametricDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbOrdinateDimension: datum coordinate with a jogged leader
// (feature → axis-aligned run → leader end).
void worldDraw(const AcDbOrdinateDimension &dimension,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbLeader: the vertex polyline (a spline-fit path draws as its
// control polygon until a fit engine lands here) plus the arrowhead at
// points[0].  The annotation it references draws as its own entity.
void worldDraw(const AcDbLeader &leader, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbFcf: boxed feature-control frame; the formatting codes come off
// and the plain glyph run rides the text callback.
void worldDraw(const AcDbFcf &fcf, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbShape: placement marker (a cross of the shape's authored size) —
// shape-file glyph lookup is not part of the stroke text engine yet.
void worldDraw(const AcDbShape &shape, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

// AcDbAttributeDefinition: an ATTDEF displays its tag (the stamped
// value shows up in the INSERT's attributes).
void worldDraw(const AcDbAttributeDefinition &definition,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbAttribute: the stamped value text; coordinates live in the owning
// INSERT's frame, and the replay applies the instance placement before
// this draws.
void worldDraw(const AcDbAttribute &attribute,
                      acgs::ViewportDraw &draw, bool fillIs3DFace = false);

// AcDbMInsertBlock: the replay expands the row/column grid per cell, so
// this only draws when a MINSERT sits nested inside a block definition
// (where expansion is out of scope) — a placement marker keeps it
// visible and pickable without claiming the full picture.
void worldDraw(const AcDbMInsertBlock &insert, acgs::ViewportDraw &draw,
                      bool fillIs3DFace = false);

} // namespace acdb
