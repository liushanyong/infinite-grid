#pragma once

#include "AcGiPort.h"

// AcGi-compatible graphics interface: the draw-callback protocol entities
// use to emit primitives (see ObjectARX docs, "AcGi Overview").  Signatures
// follow AcGiWorldDraw / AcGiSubEntityTraits; the implementation records the
// callbacks into the scene geometry sink with the current sub-entity traits
// applied per call, exactly like the traits-state semantics of AcGi.

#include <cmath>
#include <vector>

#include "arx/ArxTypes.h"
#include "../acge/AcGe.h"
#include "../acgs/model/DrawContext.h"
#include "AcGiTextStyle.h"
#include "AcGiTextQueue.h"

class ACGI_PORT AcGiWorldDraw
{
public:
    explicit AcGiWorldDraw(acgs::ViewportDraw &draw) : draw_(draw) {}

    acgs::SubEntityTraits &subEntityTraits() { return draw_.subEntityTraits(); }

    bool worldLine(const AcGePoint3d &from, const AcGePoint3d &to);
    bool worldPolyline(const AcGePoint3d *points, int count, bool closed);
    bool worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                       const AcGePoint3d &c, bool fillIs3DFace = false);

    // A full circle when the arc spans two pi, an open arc otherwise.
    bool worldCircle(const AcGeCircArc3d &arc);

    bool worldPoint(
        const AcGePoint3d &position,
        acge::PointDisplayStyle displayStyle =
            acge::PointDisplayStyle::Circle);

    // Semi-infinite stroke: the stored endpoint is only a proxy; renderers
    // and pickers treat the geometry as extending forever along the
    // direction.  This is an extension beyond classic AcGi, needed because
    // Ray/XLine overlay semantics live in the sink payload.
    bool worldInfiniteLine(const AcGePoint3d &start,
                           const AcGeVector3d &direction);

    // Text callback.  Until a glyph engine exists the text renders as its
    // layout frame plus an insertion impostor — the same output the
    // tessellation path produces — so protocol consumers are already shaped
    // like ObjectARX worldDraw code and only the callback implementation
    // changes when glyphs arrive.
    bool text(const AcGePoint3d &position, const AcGeVector3d &normal,
              const AcGeVector3d &direction, const char *message,
              const AcGiTextStyle &style);

    bool text(const AcGePoint3d &position, const AcGeVector3d &normal,
              const AcGeVector3d &direction, const ACHAR *message,
              const AcGiTextStyle &style);

    // ObjectARX draws ellipses as worldPolyline; this convenience keeps the
    // same tessellation policy in one place.
    bool worldEllipse(const AcGeEllip3d &ellipse);

    // Model transform stack: subsequent callbacks are expressed in the
    // pushed frame, like AcGiGeometry::pushModelTransform.
    void pushModelTransform(const AcGeMatrix3d &matrix)
    {
        transformStack_.push_back(currentTransform() * matrix);
    }

    void pushModelTransform(const AcGeVector3d &translation)
    {
        pushModelTransform(AcGeMatrix3d::setToTranslation(translation));
    }

    void popModelTransform()
    {
        if (!transformStack_.empty())
            transformStack_.pop_back();
    }

private:
    AcGeMatrix3d currentTransform() const
    {
        return transformStack_.empty() ? AcGeMatrix3d()
                                       : transformStack_.back();
    }

    AcGePoint3d apply(const AcGePoint3d &point) const
    {
        return transformBy(point, currentTransform());
    }

    acgs::GeometrySink &sink() { return draw_.sink(); }

    void applyTraits(acdb::Stroke &stroke) const;
    void applyTraits(acdb::Triangle &triangle, bool fillIs3DFace) const;

    acgs::ViewportDraw &draw_;
    std::vector<AcGeMatrix3d> transformStack_;
};
