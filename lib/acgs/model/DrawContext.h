#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include "acdb/AcDbCore.h"
#include "acdb/AcDbTessellate.h"
#include "acgs/AcGsPort.h"

namespace acdb
{
class AcDbDatabase;
}

namespace acgs
{

// The renderer-visible subset of AcDbEntity.  Keeping this separate from the
// authoring struct makes the draw-list contract explicit and gives layer/color
// table changes a cache invalidation version without changing entity values.
struct ACGS_PORT DrawTraits
{
    std::uint32_t handle = 0;
    std::string name;
    std::string layer = "0";
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "ByLayer";
    double lineWeight = 0.0;
    bool visible = true;
    bool fillEnabled = true;
    acdb::AcDbObjectId materialId;

    [[nodiscard]] acdb::AcDbEntity toEntityCommon() const;
    static DrawTraits from(const acdb::AcDbEntity &common);
};

// Records renderer-ready geometry.  Trait application belongs here so generated
// primitives cannot enter a draw list with stale or partially copied traits.
class ACGS_PORT GeometrySink
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
                        const DrawTraits &traits,
                        bool fillIs3DFace = false);

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
    void setFrom(const DrawTraits &traits) { traits_ = traits; }
    void setColor(const glm::vec4 &color) { traits_.color = color; }
    void setLayer(std::string layer) { traits_.layer = std::move(layer); }
    void setLineType(std::string lineType) { traits_.lineType = std::move(lineType); }
    void setLineWeight(double lineWeight) { traits_.lineWeight = lineWeight; }
    void setVisibility(bool visible) { traits_.visible = visible; }
    void setFillEnabled(bool enabled) { traits_.fillEnabled = enabled; }
    void setMaterial(const acdb::AcDbObjectId &id) { traits_.materialId = id; }

    [[nodiscard]] const acdb::AcDbObjectId &materialId() const
    {
        return traits_.materialId;
    }

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

    // ---- AcGiContext support: owning document + persistent id minting ----
    void setDatabase(acdb::AcDbDatabase *database) { database_ = database; }
    [[nodiscard]] acdb::AcDbDatabase *database() const { return database_; }

    // Mints context-scoped AcGi persistent ids (AcGiViewportDraw::isValidId
    // accepts exactly the ids minted here).
    Adesk::ULongPtr mintAcgiId() { return ++acgiIdCounter_; }
    [[nodiscard]] Adesk::ULongPtr mintedAcgiIdCount() const
    {
        return acgiIdCounter_;
    }

private:
    GeometrySink sink_;
    acdb::TesselationOptions options_;
    SubEntityTraits traits_;
    acdb::AcDbDatabase *database_ = nullptr;
    Adesk::ULongPtr acgiIdCounter_ = 0;
};

// Recorded owner-draw request (ObjectARX AcGiOwnerDraw / OwnerDraw3d).  The
// renderer keeps these as pending display-time commands; native GDI upload
// stays host-side.
struct OwnerDrawCommand
{
    enum class Kind
    {
        Dc,
        World3d
    };

    Kind kind = Kind::Dc;
    Adesk::Int32 viewportNumber = 0;
    Adesk::Int32 left = 0;
    Adesk::Int32 top = 0;
    Adesk::Int32 right = 0;
    Adesk::Int32 bottom = 0;
    AcGePoint3d minimum{0.0, 0.0, 0.0};
    AcGePoint3d maximum{0.0, 0.0, 0.0};
};

// The per-viewport counterpart of AcGiViewportDraw.  This pass carries screen
// density as world-space chord tolerance so curve tessellation can adapt without
// mutating authored entities.

// Camera/viewport frame used by the AcGi eye- and DC-space callbacks.  The
// matrices follow the value-layer convention:
//   worldToEye maps world points to eye space,
//   eyeToDc maps eye points to normalized device coordinates,
// and the inverses are precomputed for AcGiViewportDraw.
struct ViewportFrame
{
    Adesk::Int32 viewportNumber = 0;
    double left = 0.0;
    double top = 0.0;
    double right = 1.0;
    double bottom = 1.0;
    AcGeMatrix3d worldToEye;
    AcGeMatrix3d eyeToDc;
    AcGeMatrix3d eyeToWorld;
    AcGeMatrix3d dcToEye;

    // Camera metadata for AcGiViewport.  eyeToDc already carries the
    // projection; these duplicate the camera parameters so AcGi callers do
    // not have to reverse-engineer them from matrices.
    bool perspective = false;
    AcGePoint3d cameraLocation{0.0, 0.0, 0.0};
    AcGePoint3d cameraTarget{0.0, 0.0, -1.0};
    AcGeVector3d cameraUp{0.0, 1.0, 0.0};
    Adesk::Int32 acadWindowId = 0;

    // AcDbViewportTableRecord binding for AcGiViewportDraw::viewportObjectId.
    acdb::AcDbObjectId viewportObjectId;

    // Clipping values reported by AcGiViewport.  The renderer still owns
    // actual slab clipping; the flags only mirror whether AcGi-visible clip
    // planes are considered active.
    bool clipFront = false;
    bool clipBack = false;
    double frontClipDistance = 0.0;
    double backClipDistance = 0.0;
};

class ACGS_PORT ViewportDraw final : public WorldDraw
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
    [[nodiscard]] double deviation(double curveRadius) const;
    [[nodiscard]] acdb::TesselationOptions optionsFor(
        double curveRadius) const;

    void setViewportFrame(
        Adesk::Int32 viewportNumber, double left, double top, double right,
        double bottom, const AcGeMatrix3d &worldToEye,
        const AcGeMatrix3d &eyeToDc)
    {
        ViewportFrame frame;
        frame.viewportNumber = viewportNumber;
        frame.left = left;
        frame.top = top;
        frame.right = right;
        frame.bottom = bottom;
        frame.worldToEye = worldToEye;
        frame.eyeToDc = eyeToDc;
        frame.eyeToWorld = worldToEye.inverted();
        frame.dcToEye = eyeToDc.inverted();
        frame_ = std::move(frame);
    }

    // Full-frame setter for callers that also supply camera / clip metadata.
    void setViewportFrame(const ViewportFrame &frame)
    {
        ViewportFrame resolved = frame;
        resolved.eyeToWorld = frame.worldToEye.inverted();
        resolved.dcToEye = frame.eyeToDc.inverted();
        frame_ = std::move(resolved);
    }

    void clearViewportFrame() { frame_.reset(); }
    [[nodiscard]] bool hasViewportFrame() const { return frame_.has_value(); }
    [[nodiscard]] const ViewportFrame &viewportFrame() const
    {
        return frame_.value();
    }

    [[nodiscard]] AcGePoint3d eyeToWorld(const AcGePoint3d &point) const
    {
        return frame_ ? transformBy(point, frame_->eyeToWorld) : point;
    }
    [[nodiscard]] AcGePoint3d dcToWorld(const AcGePoint3d &point) const
    {
        return frame_ ? transformBy(point, frame_->dcToEye *
                                              frame_->eyeToWorld)
                      : point;
    }
    [[nodiscard]] AcGeVector3d dcToWorldVector(
        const AcGePoint3d &start, const AcGePoint3d &end) const
    {
        return dcToWorld(end) - dcToWorld(start);
    }

    // ---- AcGiCommonDraw draw-state parity (AcGiViewportDraw reads these) ----
    void setRegenType(int value) { regenType_ = value; }
    void setRegenAbort(bool value) { regenAbort_ = value; }
    void setDragging(bool value) { dragging_ = value; }
    void setSecondaryCall(bool value) { secondaryCall_ = value; }
    void setNumberOfIsolines(Adesk::UInt32 value) { numberOfIsolines_ = value; }
    void setDeviationFactor(double value)
    {
        if (value > 0.0)
            chordToleranceFactor_ = value;
    }

    [[nodiscard]] int regenType() const { return regenType_; }
    [[nodiscard]] bool regenAbort() const { return regenAbort_; }
    [[nodiscard]] bool dragging() const { return dragging_; }
    [[nodiscard]] bool secondaryCall() const { return secondaryCall_; }
    [[nodiscard]] Adesk::UInt32 numberOfIsolines() const
    {
        return numberOfIsolines_;
    }

    // ---- recorded owner-draw requests ----
    std::vector<OwnerDrawCommand> &ownerDrawCommands()
    {
        return ownerDrawCommands_;
    }
    [[nodiscard]] const std::vector<OwnerDrawCommand> &ownerDrawCommands()
        const
    {
        return ownerDrawCommands_;
    }
    void clearOwnerDrawCommands() { ownerDrawCommands_.clear(); }

private:
    double pixelsPerUnit_ = 0.0;
    double chordToleranceFactor_ = 0.25;
    int regenType_ = 0;
    bool regenAbort_ = false;
    bool dragging_ = false;
    bool secondaryCall_ = false;
    Adesk::UInt32 numberOfIsolines_ = 4;
    std::vector<OwnerDrawCommand> ownerDrawCommands_;
    std::optional<ViewportFrame> frame_;
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
