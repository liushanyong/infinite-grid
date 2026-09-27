#pragma once

#include <glm/glm.hpp>

#include "scene/DrawContext.h"
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
    worldDraw(arc, draw, draw.optionsFor(arc.radius), fillIs3DFace);
}

inline void worldDraw(const Circle &circle, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    worldDraw(circle, draw, draw.optionsFor(circle.radius), fillIs3DFace);
}

inline void worldDraw(const Ellipse &ellipse, scene::ViewportDraw &draw,
                      bool fillIs3DFace = false)
{
    const double majorLength = glm::length(ellipse.majorAxis);
    const double minorLength = majorLength * ellipse.radiusRatio;
    const double radius = std::max(majorLength, minorLength);
    worldDraw(ellipse, draw, draw.optionsFor(radius), fillIs3DFace);
}

} // namespace entities
