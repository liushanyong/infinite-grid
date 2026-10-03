#pragma once

#include <glm/glm.hpp>

#include "scene/DrawContext.h"
#include "../acgi/AcGiWorldDraw.h"
#include "tessellate.h"

namespace entities
{

// Translate the established value-type tessellation overload set into the
// AcGi-lite draw protocol.  The entity-local fragment keeps generated geometry
// isolated until GeometrySink applies the current traits.
template <typename EntityType>
inline void worldDraw(const EntityType &entity, scene::WorldDraw &draw,
                      const TesselationOptions &options, bool fillIs3DFace = false)
{
    TessellatedEntity fragment;
    tessellate(entity, fragment, options);
    draw.sink().append(fragment, draw.subEntityTraits().traits(), fillIs3DFace);
}

template <typename EntityType>
inline void worldDraw(const EntityType &entity, scene::WorldDraw &draw,
                      bool fillIs3DFace = false)
{
    worldDraw(entity, draw, draw.options(), fillIs3DFace);
}

// Viewport-aware curves choose their chord angle from ViewportDraw::deviation.
inline void worldDraw(const Arc &arc, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(arc.center, arc.normal, arc.radius,
                                       arc.startAngle, arc.endAngle));
}

inline void worldDraw(const Circle &circle, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldCircle(AcGeCircArc3d(circle.center, circle.normal,
                                       circle.radius));
}

// Protocol-pattern overloads: these entities draw themselves through the
// AcGiWorldDraw callback interface instead of tessellating up front, which
// is how ObjectARX custom entities emit geometry.

inline void worldDraw(const Ellipse &ellipse, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldEllipse(AcGeEllip3d(ellipse.center, ellipse.majorAxis,
                                      ellipse.normal, ellipse.radiusRatio,
                                      ellipse.startParameter,
                                      ellipse.endParameter));
}

inline void worldDraw(const Line &line, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    AcGiWorldDraw graphics(draw);
    graphics.worldLine(line.start, line.end);
}

// A Solid3d renders its fill with the entity color and its feature-edge
// border strokes with the inverted color, so the border reads against the
// fill no matter which entity color is authored.
inline void worldDraw(const Solid3d &solid, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    entities::TessellatedEntity fragment;
    entities::tessellate(solid, fragment, draw.options());

    entities::TessellatedEntity fillsOnly;
    fillsOnly.fills = std::move(fragment.fills);
    draw.sink().append(fillsOnly, draw.subEntityTraits().traits(),
                       fillIs3DFace);

    const glm::vec4 &color = draw.subEntityTraits().traits().color;
    scene::DrawTraits edgeTraits = draw.subEntityTraits().traits();
    edgeTraits.color = glm::vec4(1.0f - color.r, 1.0f - color.g,
                                 1.0f - color.b, color.a);
    entities::TessellatedEntity strokesOnly;
    strokesOnly.strokes = std::move(fragment.strokes);
    draw.sink().append(strokesOnly, edgeTraits, fillIs3DFace);
}

} // namespace entities
