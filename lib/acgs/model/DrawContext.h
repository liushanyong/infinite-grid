#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include "acdb/AcDbCore.h"
#include "acdb/AcDbTessellate.h"

namespace acgs
{

// The renderer-visible subset of AcDbEntity.  Keeping this separate from the
// authoring struct makes the draw-list contract explicit and gives layer/color
// table changes a cache invalidation version without changing entity values.
struct DrawTraits
{
    std::uint32_t handle = 0;
    std::string name;
    std::string layer = "0";
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "ByLayer";
    double lineWeight = 0.0;
    bool visible = true;

    [[nodiscard]] acdb::AcDbEntity toEntityCommon() const
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

    static DrawTraits from(const acdb::AcDbEntity &common)
    {
        DrawTraits traits;
        traits.handle = static_cast<std::uint32_t>(common.handle.value);
        traits.name = common.name;
        traits.layer = common.layer;
        traits.color = common.color;
        traits.lineType = common.lineType;
        traits.lineWeight = common.lineWeight;
        traits.visible = common.visible;
        return traits;
    }
};

// Records renderer-ready geometry.  Trait application belongs here so generated
// primitives cannot enter a draw list with stale or partially copied traits.
class GeometrySink
{
public:
    explicit GeometrySink(acdb::TessellatedEntity &geometry)
        : geometry_(geometry)
    {
    }

    [[nodiscard]] acdb::TessellatedEntity &geometry() { return geometry_; }
    [[nodiscard]] const acdb::TessellatedEntity &geometry() const
    {
        return geometry_;
    }

    // Low-level primitive emitters for the AcGiWorldDraw callback layer,
    // which applies traits itself per callback.
    acdb::Stroke &addStroke(bool closed = false)
    {
        geometry_.strokes.push_back(acdb::Stroke{});
        geometry_.strokes.back().closed = closed;
        return geometry_.strokes.back();
    }

    void appendTriangle(const acdb::Triangle &triangle)
    {
        geometry_.fills.push_back(triangle);
    }

    void appendPoint(const acdb::TessellatedPoint &point)
    {
        geometry_.points.push_back(point);
    }

    struct EntityRanges
    {
        size_t strokeBegin = 0;
        size_t strokeCount = 0;
        size_t fillBegin = 0;
        size_t fillCount = 0;
        size_t pointBegin = 0;
        size_t pointCount = 0;
    };

    EntityRanges append(const acdb::TessellatedEntity &fragment,
                        const DrawTraits &traits, bool fillIs3DFace = false)
    {
        EntityRanges ranges;
        ranges.strokeBegin = geometry_.strokes.size();
        ranges.fillBegin = geometry_.fills.size();
        ranges.pointBegin = geometry_.points.size();

        geometry_.strokes.insert(geometry_.strokes.end(),
                                 fragment.strokes.begin(),
                                 fragment.strokes.end());
        geometry_.fills.insert(geometry_.fills.end(),
                               fragment.fills.begin(), fragment.fills.end());
        geometry_.points.insert(geometry_.points.end(),
                                fragment.points.begin(), fragment.points.end());

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

private:
    acdb::TessellatedEntity &geometry_;
};

// Modeled on AcGiSubEntityTraits: state set by an entity while its worldDraw
// executes and applied by the geometry sink to subsequently emitted primitives.
class SubEntityTraits
{
public:
    void setFrom(const acdb::AcDbEntity &common)
    {
        traits_ = DrawTraits::from(common);
    }
    void setColor(const glm::vec4 &color) { traits_.color = color; }
    void setLayer(std::string layer) { traits_.layer = std::move(layer); }
    void setLineType(std::string lineType) { traits_.lineType = std::move(lineType); }
    void setLineWeight(double lineWeight) { traits_.lineWeight = lineWeight; }
    void setVisibility(bool visible) { traits_.visible = visible; }

    [[nodiscard]] const DrawTraits &traits() const { return traits_; }

private:
    DrawTraits traits_;
};

// The shared-world counterpart of AcGiWorldDraw.  Geometry emitted here is safe
// to cache across viewports; viewport-specific policy lives in ViewportDraw.
class WorldDraw
{
public:
    WorldDraw(acdb::TessellatedEntity &geometry,
              const acdb::TesselationOptions &options = {})
        : sink_(geometry), options_(options)
    {
    }

    [[nodiscard]] GeometrySink &sink() { return sink_; }
    [[nodiscard]] const GeometrySink &sink() const { return sink_; }
    [[nodiscard]] SubEntityTraits &subEntityTraits() { return traits_; }
    [[nodiscard]] const SubEntityTraits &subEntityTraits() const { return traits_; }
    [[nodiscard]] const acdb::TesselationOptions &options() const
    {
        return options_;
    }

private:
    GeometrySink sink_;
    acdb::TesselationOptions options_;
    SubEntityTraits traits_;
};

// The per-viewport counterpart of AcGiViewportDraw.  This pass carries screen
// density as world-space chord tolerance so curve tessellation can adapt without
// mutating authored entities.
class ViewportDraw final : public WorldDraw
{
public:
    ViewportDraw(acdb::TessellatedEntity &geometry,
                 const acdb::TesselationOptions &options = {},
                 double pixelsPerUnit = 0.0, double chordToleranceFactor = 0.25)
        : WorldDraw(geometry, options),
          pixelsPerUnit_(pixelsPerUnit),
          chordToleranceFactor_(chordToleranceFactor)
    {
    }

    void setViewport(double pixelsPerUnit, double chordToleranceFactor = 0.25)
    {
        pixelsPerUnit_ = pixelsPerUnit;
        chordToleranceFactor_ = chordToleranceFactor;
    }

    [[nodiscard]] double pixelsPerUnit() const { return pixelsPerUnit_; }

    // Returns the maximum world-space distance from a chord to its curve.
    // Zero pixelsPerUnit means "unknown viewport" and callers retain the options
    // supplied by WorldDraw.
    [[nodiscard]] double deviation(double curveRadius) const
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

    [[nodiscard]] acdb::TesselationOptions optionsFor(
        double curveRadius) const
    {
        acdb::TesselationOptions options = WorldDraw::options();
        const double tolerance = deviation(curveRadius);
        if (!(tolerance > 0.0) || !(curveRadius > tolerance))
            return options;

        const double cosine = std::clamp(1.0 - tolerance / curveRadius, -1.0, 1.0);
        const double angleStep = std::clamp(2.0 * std::acos(cosine),
                                            glm::pi<double>() / 180.0,
                                            glm::pi<double>() / 12.0);
        options.angleStep = angleStep;
        return options;
    }

private:
    double pixelsPerUnit_ = 0.0;
    double chordToleranceFactor_ = 0.25;
};

struct DrawListKey
{
    std::uint64_t entityRevision = 1;
    std::uint64_t traitsVersion = 1;
    glm::dvec3 renderOrigin{0.0};
    std::uint32_t chordToleranceBucket = 0;

    friend bool operator<(const DrawListKey &left, const DrawListKey &right)
    {
        if (left.entityRevision != right.entityRevision)
            return left.entityRevision < right.entityRevision;
        if (left.traitsVersion != right.traitsVersion)
            return left.traitsVersion < right.traitsVersion;
        if (left.chordToleranceBucket != right.chordToleranceBucket)
            return left.chordToleranceBucket < right.chordToleranceBucket;
        if (left.renderOrigin.x != right.renderOrigin.x)
            return left.renderOrigin.x < right.renderOrigin.x;
        if (left.renderOrigin.y != right.renderOrigin.y)
            return left.renderOrigin.y < right.renderOrigin.y;
        return left.renderOrigin.z < right.renderOrigin.z;
    }
};

// A deliberately small stand-in for the AcGs model invalidate path.  Values are
// type-checked on lookup so one cache instance can hold independent draw lists.
class DrawListCache
{
    struct Entry
    {
        std::type_index type{typeid(void)};
        std::shared_ptr<void> value;
    };

public:
    template <typename Builder, typename DrawList = std::decay_t<
                   decltype(std::declval<Builder &&>()())>>
    const DrawList &get(const DrawListKey &key, Builder &&build)
    {
        Entry &entry = entries_[key];
        if (!entry.value || entry.type != typeid(DrawList))
        {
            DrawList value = std::forward<Builder>(build)();
            entry.type = typeid(DrawList);
            entry.value = std::make_shared<DrawList>(std::move(value));
        }
        return *std::static_pointer_cast<DrawList>(entry.value);
    }

    void invalidate(const DrawListKey &key) { entries_.erase(key); }
    void invalidateAll() { entries_.clear(); }
    [[nodiscard]] size_t size() const { return entries_.size(); }

private:
    std::map<DrawListKey, Entry> entries_;
};

} // namespace acgs
