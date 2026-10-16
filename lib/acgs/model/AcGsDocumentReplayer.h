#pragma once

// AcGsDocumentReplayer (namespace acgs) — the Gs replay of a resident
// AcDbDatabase into renderer-ready draw-list content.  ObjectARX shape:
// every model-space entity IS an AcGiDrawable (AcDbEntity derives it), so
// the graphics system hands each entity to the sealed protocol
// (setAttributes -> worldDraw) and records the emitted primitives into its
// metafile cache.  Block references expand through compound drawables.
//
// The output metafile type is a template parameter: any type exposing
// `geometry` (acdb::TessellatedEntity) and stroke/fill/point range
// vectors of AcGsEntityRange works.

#include <glm/glm.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbTessellate.h"
#include "acgi/AcGiEntityDrawable.h"
#include "acgs/model/DrawContext.h"
#include "acgs/model/AcGsModel.h"

namespace acgs
{

// ---- pick shape of a recorded entity range ----

enum class AcGsPickShape
{
  // Use the tessellated primitives themselves.
  Primitives,
  // Two same-length offset strokes define an area (for example MLine).
  // Picking only the two boundary ribbons leaves the visible entity's
  // semantic body zoom-dependent and misses clicks between the edges.
  PairedStrokeBand
};

struct AcGsEntityRange
{
  std::string name;
  std::size_t begin = 0;
  std::size_t count = 0;
  AcGsPickShape pickShape = AcGsPickShape::Primitives;
  // Construction/diagnostic geometry: renders inside the depth slab (the
  // depth shader discards outside it) but never contributes bounds to the
  // slab fit — a huge-extent reference line must not re-frame the view.
  bool excludeFromFit = false;
};

// The metafile a replay records into: tessellated geometry plus the
// per-entity ranges and GPU curve commands.  Mesh-instance records are
// application-side and ride outside the metafile.
struct AcGsMetafile
{
    acdb::TessellatedEntity geometry;
    glm::dvec3 anchor{0.0};
    std::vector<AcGsEntityRange> strokeRanges;
    std::vector<AcGsEntityRange> fillRanges;
    std::vector<AcGsEntityRange> pointRanges;
    std::vector<CurveBatchCommand> curves;
};

// Per-entity replay hints, authored alongside the entity (the metafile
// records one range per entity label; the hints carry the flags that are
// replay decisions rather than entity data).
struct AcGsReplayHints
{
  AcGsPickShape pickShape = AcGsPickShape::Primitives;
  bool excludeFromFit = false;
};

inline std::map<std::string, AcGsReplayHints> &replayHints()
{
  static std::map<std::string, AcGsReplayHints> hints;
  return hints;
}

class AcGsDocumentReplayer
{
public:
  // Entities the replay must skip: they render through another channel
  // (the glTF static-mesh pipeline), fed from these same document
  // entities by the host.
  std::vector<acdb::AcDbHandle> skipHandles;

  // Replays model space of |document| into |target|.
  template <typename MetafileT>
  void record(const acdb::AcDbDatabase &document,
              const acdb::TesselationOptions &options, MetafileT &target)
  {
    for (const acdb::AcDbHandle handle :
         document.modelSpace().entityHandles())
    {
      recordEntity(document, handle, options, target);
    }
  }

  // Replays a subset of model-space residents. The host uses this for
  // fit-to-drawing after an append-style DWG import without including the
  // application's pre-existing scene.
  template <typename MetafileT>
  void recordHandles(const acdb::AcDbDatabase &document,
                     const std::vector<acdb::AcDbHandle> &handles,
                     const acdb::TesselationOptions &options,
                     MetafileT &target)
  {
    for (const acdb::AcDbHandle handle : handles)
      recordEntity(document, handle, options, target);
  }

  // Records one drawable through the sealed protocol (setAttributes ->
  // worldDraw) into the metafile fragment, then registers the per-channel
  // ranges under |name|.
  template <typename MetafileT>
  static void appendDrawable(AcGiDrawable &drawable, const char *name,
                             const acdb::TesselationOptions &options,
                             MetafileT &target,
                             AcGsPickShape pickShape =
                                 AcGsPickShape::Primitives,
                             bool excludeFromFit = false)
  {
    auto addRange = [name, pickShape, excludeFromFit](
                        std::vector<AcGsEntityRange> &ranges, std::size_t begin,
                        std::size_t end) {
      if (end != begin)
        ranges.push_back({name, begin, end - begin, pickShape,
                          excludeFromFit});
    };
    const std::size_t strokeBegin = target.geometry.strokes.size();
    const std::size_t fillBegin = target.geometry.fills.size();
    const std::size_t pointBegin = target.geometry.points.size();
    ViewportDraw draw(target.geometry, options);
    AcGiViewportDraw graphics(draw);
    drawable.worldDraw(&graphics);
    addRange(target.strokeRanges, strokeBegin,
             target.geometry.strokes.size());
    addRange(target.fillRanges, fillBegin, target.geometry.fills.size());
    addRange(target.pointRanges, pointBegin, target.geometry.points.size());
  }

  // Compounds register one range per member (block members stay
  // individually named and pickable); leaves register themselves.
  template <typename MetafileT>
  static void appendDrawableTree(AcGiDrawable &drawable, const char *name,
                                 const acdb::TesselationOptions &options,
                                 MetafileT &target,
                                 AcGsPickShape pickShape =
                                     AcGsPickShape::Primitives,
                                 bool excludeFromFit = false)
  {
    if (const auto *compound =
            dynamic_cast<const AcGiCompoundDrawable *>(&drawable))
    {
      for (const AcGiCompoundDrawable::Member &member : compound->members())
        if (member.drawable != nullptr)
          appendDrawable(*member.drawable, member.name.c_str(), options,
                         target);
      return;
    }
    appendDrawable(drawable, name, options, target, pickShape,
                   excludeFromFit);
  }

private:
  template <typename MetafileT>
  void recordEntity(const acdb::AcDbDatabase &document,
                    acdb::AcDbHandle handle,
                    const acdb::TesselationOptions &options,
                    MetafileT &target)
  {
    const acdb::AcDbEntityVariant *payload = document.getEntity(handle);
    if (payload == nullptr || document.isErased(handle))
      return;
    if (std::find(skipHandles.begin(), skipHandles.end(), handle) !=
        skipHandles.end())
      return; // served by the host's other render channel
    const std::string &name = acdb::common(*payload).name;
    const auto hintIt = replayHints().find(name);
    const AcGsReplayHints hints =
        hintIt != replayHints().end() ? hintIt->second : AcGsReplayHints{};

    // Control-point splines drive the GPU curve channel: the document
    // entity's data (degree/weights/control points/color) converts into a
    // curve batch, and the entity's own subWorldDraw deliberately emits no
    // stroke for them (no fit points → nothing to tessellate).
    if (const auto *spline = std::get_if<acdb::AcDbSpline>(payload);
        spline != nullptr && !spline->controlPoints.empty())
    {
      acgs::CurveBatchCommand &curve = target.curves.emplace_back();
      curve.algorithm =
          !spline->weights.empty()
              ? rendering::CurveAlgorithm::NURBS
              : (spline->controlPoints.size() ==
                         static_cast<size_t>(spline->degree + 1)
                     ? rendering::CurveAlgorithm::Bezier
                     : rendering::CurveAlgorithm::BSpline);
      curve.name = name;
      curve.degree = spline->degree;
      curve.sampleCount = 192;
      curve.controlPoints.reserve(spline->controlPoints.size());
      for (const AcGePoint3d &point : spline->controlPoints)
        curve.controlPoints.push_back(glm::dvec3(point));
      curve.weights = spline->weights;
      curve.acgiMaterial.algorithm = acgs::AcGiShaderAlgorithm::Shaded;
      curve.acgiMaterial.baseColor = acdb::common(*payload).color.renderColor();
      return;
    }

    // The entity IS the drawable (AcDbEntity : AcGiDrawable); block
    // references expand through the replay's compound drawables.
    acgi::AcGiDrawableContext context;
    context.document = &document;
    context.handle = handle;
    std::unique_ptr<AcGiDrawable> owned =
        acgi::makeOwningEntityDrawable(*payload, context);
    AcGiDrawable *drawable =
        owned != nullptr ? owned.get() : acgi::entityDrawable(*payload);
    if (drawable == nullptr)
      return;

    appendDrawableTree(*drawable, name.c_str(), options, target,
                       hints.pickShape, hints.excludeFromFit);
    acgiDrawableEvent->sendModified(drawable);
  }
};

} // namespace acgs
