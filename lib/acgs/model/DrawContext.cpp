#include "acgs/model/DrawContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace acgs
{

acdb::AcDbEntity DrawTraits::toEntityCommon() const
{
    // AcDbEntity carries a base class (AcDbObject), so aggregate
    // initialization no longer applies: assign field by field.
    acdb::AcDbEntity entity;
    entity.handle = handle;
    entity.name = name;
    entity.layer = layer;
    entity.color = color;
    entity.lineType = lineType;
    entity.lineWeight = lineWeight;
    entity.visible = visible;
    return entity;
}

DrawTraits DrawTraits::from(const acdb::AcDbEntity &common)
{
    DrawTraits traits;
    traits.handle = static_cast<std::uint32_t>(common.handle.value);
    traits.name = common.name;
    traits.layer = common.layer;
    traits.color = common.color.renderColor();
    traits.lineType = common.lineType;
    traits.lineWeight = common.lineWeight;
    traits.visible = common.visible;
    return traits;
}

GeometrySink::EntityRanges GeometrySink::append(
    const acdb::TessellatedEntity &fragment, const DrawTraits &traits,
    bool fillIs3DFace)
{
    EntityRanges ranges;
    ranges.strokeBegin = geometry_.strokes.size();
    ranges.fillBegin = geometry_.fills.size();
    ranges.pointBegin = geometry_.points.size();

    geometry_.strokes.insert(geometry_.strokes.end(), fragment.strokes.begin(),
                             fragment.strokes.end());
    geometry_.fills.insert(geometry_.fills.end(), fragment.fills.begin(),
                           fragment.fills.end());
    geometry_.points.insert(geometry_.points.end(), fragment.points.begin(),
                            fragment.points.end());

    for (size_t i = ranges.strokeBegin; i < geometry_.strokes.size(); ++i)
    {
        acdb::Stroke &stroke = geometry_.strokes[i];
        stroke.common = traits.toEntityCommon();
        stroke.lineWeight = traits.lineWeight;
    }
    for (size_t i = ranges.fillBegin; i < geometry_.fills.size(); ++i)
    {
        acdb::Triangle &fill = geometry_.fills[i];
        fill.common = traits.toEntityCommon();
        fill.is3DFace = fillIs3DFace;
    }
    for (size_t i = ranges.pointBegin; i < geometry_.points.size(); ++i)
    {
        acdb::TessellatedPoint &point = geometry_.points[i];
        point.common = traits.toEntityCommon();
        point.pointSize = traits.lineWeight > 0.0 ? traits.lineWeight : 7.0;
    }

    ranges.strokeCount = geometry_.strokes.size() - ranges.strokeBegin;
    ranges.fillCount = geometry_.fills.size() - ranges.fillBegin;
    ranges.pointCount = geometry_.points.size() - ranges.pointBegin;
    return ranges;
}

double ViewportDraw::deviation(double curveRadius) const
{
    if (!(pixelsPerUnit_ > 0.0) || !(curveRadius > 0.0) ||
        !std::isfinite(curveRadius))
    {
        return 0.0;
    }

    const double worldPerPixel = 1.0 / pixelsPerUnit_;
    const double desired =
        std::max(std::numeric_limits<double>::min(),
                 worldPerPixel * chordToleranceFactor_);
    return std::clamp(desired, curveRadius * 1.0e-5, curveRadius * 0.15);
}

acdb::TesselationOptions ViewportDraw::optionsFor(double curveRadius) const
{
    acdb::TesselationOptions options = WorldDraw::options();
    const double tolerance = deviation(curveRadius);
    if (!(tolerance > 0.0) || !(curveRadius > tolerance))
        return options;

    const double cosine =
        std::clamp(1.0 - tolerance / curveRadius, -1.0, 1.0);
    options.angleStep =
        std::clamp(2.0 * std::acos(cosine), glm::pi<double>() / 180.0,
                   glm::pi<double>() / 12.0);
    return options;
}

} // namespace acgs
