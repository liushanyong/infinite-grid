#pragma once

// AcGi-compatible graphics interface: the draw-callback protocol entities
// use to emit primitives (see ObjectARX docs, "AcGi Overview").  Signatures
// follow AcGiWorldDraw / AcGiSubEntityTraits; the implementation records the
// callbacks into the scene geometry sink with the current sub-entity traits
// applied per call, exactly like the traits-state semantics of AcGi.

#include <cmath>
#include <vector>

#include "../ge/ge.h"
#include "../scene/DrawContext.h"

class AcGiWorldDraw
{
public:
    explicit AcGiWorldDraw(scene::ViewportDraw &draw) : draw_(draw) {}

    scene::SubEntityTraits &subEntityTraits() { return draw_.subEntityTraits(); }

    bool worldLine(const AcGePoint3d &from, const AcGePoint3d &to)
    {
        entities::Stroke &stroke = sink().addStroke();
        stroke.points = {apply(from), apply(to)};
        applyTraits(stroke);
        return true;
    }

    bool worldPolyline(const AcGePoint3d *points, int count, bool closed)
    {
        if (!points || count < 2)
            return false;
        entities::Stroke &stroke = sink().addStroke(closed);
        stroke.points.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            stroke.points.push_back(apply(points[i]));
        applyTraits(stroke);
        return true;
    }

    bool worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                       const AcGePoint3d &c, bool fillIs3DFace = false)
    {
        entities::Triangle triangle{{}, apply(a), apply(b), apply(c)};
        applyTraits(triangle, fillIs3DFace);
        sink().appendTriangle(triangle);
        return true;
    }

    // A full circle when the arc spans two pi, an open arc otherwise.
    bool worldCircle(const AcGeCircArc3d &arc)
    {
        const double sweep = arc.endAngle - arc.startAngle;
        if (!(arc.radius > 0.0) || !(sweep > 0.0))
            return false;
        const entities::TesselationOptions options =
            draw_.optionsFor(arc.radius);
        const int segments = std::max(
            8, static_cast<int>(std::ceil(sweep / options.angleStep)));
        if (arc.isFullCircle())
        {
            entities::Stroke &stroke = sink().addStroke(true);
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
            entities::Stroke &stroke = sink().addStroke(false);
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

    // ObjectARX draws ellipses as worldPolyline; this convenience keeps the
    // same tessellation policy in one place.
    bool worldEllipse(const AcGeEllip3d &ellipse)
    {
        double sweep = ellipse.endAngle - ellipse.startAngle;
        if (!(sweep > 0.0))
            return false;
        const entities::TesselationOptions options =
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

    scene::GeometrySink &sink() { return draw_.sink(); }

    void applyTraits(entities::Stroke &stroke) const
    {
        const scene::DrawTraits &traits = draw_.subEntityTraits().traits();
        stroke.common = traits.toEntityCommon();
        stroke.lineWeight = traits.lineWeight;
    }

    void applyTraits(entities::Triangle &triangle, bool fillIs3DFace) const
    {
        const scene::DrawTraits &traits = draw_.subEntityTraits().traits();
        triangle.common = traits.toEntityCommon();
        triangle.is3DFace = fillIs3DFace;
    }

    scene::ViewportDraw &draw_;
    std::vector<AcGeMatrix3d> transformStack_;
};
