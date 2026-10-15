#pragma once

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

class AcGiWorldDraw
{
public:
    explicit AcGiWorldDraw(acgs::ViewportDraw &draw) : draw_(draw) {}

    acgs::SubEntityTraits &subEntityTraits() { return draw_.subEntityTraits(); }

    bool worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
    {
        acdb::Stroke &stroke = sink().addStroke();
        stroke.points = {apply(from), apply(to)};
        applyTraits(stroke);
        return true;
    }

    bool worldPolyline(const AcGePoint3d *points, int count, bool closed)
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

    bool worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                       const AcGePoint3d &c, bool fillIs3DFace = false)
    {
        acdb::Triangle triangle{{}, apply(a), apply(b), apply(c)};
        applyTraits(triangle, fillIs3DFace);
        sink().appendTriangle(triangle);
        return true;
    }

    // A full circle when the arc spans two pi, an open arc otherwise.
    bool worldCircle(const AcGeCircArc3d &arc);

    bool worldPoint(
        const AcGePoint3d &position,
        acge::PointDisplayStyle displayStyle =
            acge::PointDisplayStyle::Circle)
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

    // Semi-infinite stroke: the stored endpoint is only a proxy; renderers
    // and pickers treat the geometry as extending forever along the
    // direction.  This is an extension beyond classic AcGi, needed because
    // Ray/XLine overlay semantics live in the sink payload.
    bool worldInfiniteLine(const AcGePoint3d &start,
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

    void applyTraits(acdb::Stroke &stroke) const
    {
        const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
        stroke.common = traits.toEntityCommon();
        stroke.lineWeight = traits.lineWeight;
    }

    void applyTraits(acdb::Triangle &triangle, bool fillIs3DFace) const
    {
        const acgs::DrawTraits &traits = draw_.subEntityTraits().traits();
        triangle.common = traits.toEntityCommon();
        triangle.is3DFace = fillIs3DFace;
    }

    acgs::ViewportDraw &draw_;
    std::vector<AcGeMatrix3d> transformStack_;
};
