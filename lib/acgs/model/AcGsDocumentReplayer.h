#pragma once

// AcGsDocumentReplayer (namespace acgs) — the Gs replay of a resident
// AcDbDatabase into renderer-ready draw-list content.  ObjectARX shape:
// the graphics system walks the model-space entities of the database and
// records each entity's worldDraw output into its metafile cache; block
// references expand through nested instance walks, text with an "SHX"
// style diverts to the stroke-font engine.
//
// The output metafile type is a template parameter: any type exposing
// `geometry` (acdb::TessellatedEntity) and stroke/fill/point range
// vectors of AcGsEntityRange works.

#include <glm/glm.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbEntityWorldDraw.h"
#include "acdb/AcDbTransform.h"
#include "acgi/AcGiTextEngine.h"
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
  bool fillIs3DFace = false;
  AcGsPickShape pickShape = AcGsPickShape::Primitives;
};

inline std::map<std::string, AcGsReplayHints> &replayHints()
{
  static std::map<std::string, AcGsReplayHints> hints;
  return hints;
}

class AcGsDocumentReplayer
{
public:
  // The stroke-font engine is only consulted when the demo loaded its
  // SHX fonts.
  bool shxFontReady = false;

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

  // Records one entity: worldDraw into the metafile fragment, then
  // register the per-channel ranges under the entity label.
  template <typename EntityType, typename MetafileT>
  static void appendEntity(const EntityType &entity, const char *name,
                           const acdb::TesselationOptions &options,
                           MetafileT &target, bool fillIs3DFace = false,
                           AcGsPickShape pickShape =
                               AcGsPickShape::Primitives)
  {
    auto addRange = [name, pickShape](
                        std::vector<AcGsEntityRange> &ranges, std::size_t begin,
                        std::size_t end) {
      if (end != begin)
        ranges.push_back({name, begin, end - begin, pickShape});
    };
    const std::size_t strokeBegin = target.geometry.strokes.size();
    const std::size_t fillBegin = target.geometry.fills.size();
    const std::size_t pointBegin = target.geometry.points.size();
    ViewportDraw draw(target.geometry, options);
    draw.subEntityTraits().setFrom(entity.common);
    acdb::worldDraw(entity, draw, fillIs3DFace);
    addRange(target.strokeRanges, strokeBegin,
             target.geometry.strokes.size());
    addRange(target.fillRanges, fillBegin, target.geometry.fills.size());
    addRange(target.pointRanges, pointBegin, target.geometry.points.size());
  }

  // SHX-styled text renders through the stroke-font engine: each glyph
  // stroke becomes an ordinary CAD stroke, so picking/outlines/styles
  // treat text like any other entity.
  template <typename MetafileT>
  static void appendShxText(const acdb::AcDbText &text, MetafileT &target)
  {
    const glm::dvec3 textOrigin(text.insertion);
    const double textHeight = text.height;
    const glm::dvec3 textRight(1.0, 0.0, 0.0);
    const glm::dvec3 textUp(0.0, 0.0, 1.0);
    const std::size_t strokeBegin = target.geometry.strokes.size();
    const AcUtf8String utf8Text = arxcompat::toUtf8(text.text);
    for (const rendering::ShxGlyphStroke &glyphStroke :
         acgi::textEngine().shxStrokes(utf8Text))
    {
      acdb::Stroke segment;
      segment.common.color = text.common.color;
      segment.points = {
          textOrigin + textRight * (glyphStroke.fromX * textHeight) +
              textUp * (glyphStroke.fromY * textHeight),
          textOrigin + textRight * (glyphStroke.toX * textHeight) +
              textUp * (glyphStroke.toY * textHeight)};
      target.geometry.strokes.push_back(std::move(segment));
    }
    if (target.geometry.strokes.size() > strokeBegin)
    {
      target.strokeRanges.push_back(
          {text.common.name.c_str(), strokeBegin,
           target.geometry.strokes.size() - strokeBegin,
           AcGsPickShape::Primitives});
    }
  }

private:
  // Attribute values stamped on an INSERT/MINSERT draw under the
  // instance: a copy translated to the instance origin, exactly the
  // placement policy the block members themselves get (rotation/scale of
  // member geometry degrade through the sampled forms and stay lossy by
  // design).
  template <typename MetafileT>
  static void appendInstanceAttributes(
      const acdb::AcDbDatabase &document,
      const std::vector<acdb::AcDbHandle> &attributeHandles,
      const std::string &instanceName, const AcGePoint3d &originWorld,
      const acdb::TesselationOptions &options, MetafileT &target)
  {
    for (const acdb::AcDbHandle handle : attributeHandles)
    {
      const acdb::AcDbEntityVariant *payload = document.getEntity(handle);
      if (payload == nullptr || document.isErased(handle))
        continue;
      const auto *attribute = std::get_if<acdb::AcDbAttribute>(payload);
      if (attribute == nullptr)
        continue;
      acdb::AcDbAttribute instance = *attribute;
      instance.insertion += originWorld.asVector();
      instance.common.name =
          instanceName + "/" +
          (attribute->tag.empty() ? "ATT" : attribute->tag);
      appendEntity(instance, instance.common.name.c_str(), options,
                   target);
    }
  }

  template <typename MetafileT>
  static void appendBlockReference(
      const acdb::AcDbDatabase &document, acdb::AcDbHandle insertHandle,
      const acdb::AcDbBlockReference &reference,
      const acdb::TesselationOptions &options, MetafileT &target)
  {
    const glm::dmat4 topWorld =
        glm::dmat4(document.referenceTransform(reference));
    const std::string instanceName =
        acdb::common(reference).name + "@" +
        std::to_string(insertHandle.value);
    document.walkInsertInstances(
        reference.blockTableRecordName, topWorld,
        [&](const glm::dmat4 &world, acdb::AcDbHandle memberHandle,
            const acdb::AcDbEntityVariant &payload) {
          (void)memberHandle;
          const glm::dvec3 origin =
              glm::dvec3(world * glm::dvec4(0.0, 0.0, 0.0, 1.0));
          acdb::AcDbEntityVariant instance = payload;
          acdb::transformBy(instance, origin);
          acdb::common(instance).name =
              instanceName + "/" + acdb::common(payload).name;
          std::visit(
              [&](const auto &entity) {
                appendEntity(entity, acdb::common(instance).name.c_str(),
                             options, target);
              },
              instance);
        });
    appendInstanceAttributes(
        document, reference.attributeHandles, instanceName,
        reference.position, options, target);
  }

  // MINSERT: the same walk per grid cell; the cell offset rotates with
  // the reference (spacings are authored in drawing units along the
  // insert's own axes).
  template <typename MetafileT>
  static void appendMInsertBlock(
      const acdb::AcDbDatabase &document, acdb::AcDbHandle insertHandle,
      const acdb::AcDbMInsertBlock &insert,
      const acdb::TesselationOptions &options, MetafileT &target)
  {
    const std::string instanceName =
        acdb::common(insert).name + "@" + std::to_string(insertHandle.value);
    const AcGeVector3d axis =
        insert.normal.lengthSq() > 1.0e-12
            ? insert.normal.normal()
            : AcGeVector3d(0.0, 0.0, 1.0);
    const AcGeMatrix3d rotation =
        insert.rotation != 0.0
            ? AcGeMatrix3d::setToRotation(insert.rotation, axis,
                                          AcGePoint3d(0.0, 0.0, 0.0))
            : AcGeMatrix3d();
    const int rows = std::clamp(insert.rowCount, 1, 32768);
    const int columns = std::clamp(insert.columnCount, 1, 32768);
    for (int row = 0; row < rows; ++row)
    {
      for (int column = 0; column < columns; ++column)
      {
        const AcGeVector3d offset = transformBy(
            AcGeVector3d(double(column) * insert.columnSpacing,
                         double(row) * insert.rowSpacing, 0.0),
            rotation);
        acdb::AcDbBlockReference cell;
        cell.blockTableRecordName = insert.blockTableRecordName;
        cell.position = insert.position + offset;
        cell.rotation = insert.rotation;
        cell.scale = insert.scale;
        cell.normal = insert.normal;
        const glm::dmat4 cellWorld =
            glm::dmat4(document.referenceTransform(cell));
        document.walkInsertInstances(
            insert.blockTableRecordName, cellWorld,
            [&](const glm::dmat4 &world, acdb::AcDbHandle memberHandle,
                const acdb::AcDbEntityVariant &payload) {
              (void)memberHandle;
              const glm::dvec3 origin =
                  glm::dvec3(world * glm::dvec4(0.0, 0.0, 0.0, 1.0));
              acdb::AcDbEntityVariant instance = payload;
              acdb::transformBy(instance, origin);
              acdb::common(instance).name =
                  instanceName + "/" + acdb::common(payload).name;
              std::visit(
                  [&](const auto &entity) {
                    appendEntity(entity,
                                 acdb::common(instance).name.c_str(),
                                 options, target);
                  },
                  instance);
            });
        appendInstanceAttributes(document, insert.attributeHandles,
                                 instanceName, cell.position, options,
                                 target);
      }
    }
  }

  template <typename MetafileT>
  void recordEntity(const acdb::AcDbDatabase &document,
                    acdb::AcDbHandle handle,
                    const acdb::TesselationOptions &options,
                    MetafileT &target)
  {
    const acdb::AcDbEntityVariant *payload = document.getEntity(handle);
    if (payload == nullptr || document.isErased(handle))
      return;
    if (const auto *reference =
            std::get_if<acdb::AcDbBlockReference>(payload))
    {
      appendBlockReference(document, handle, *reference, options, target);
      return;
    }
    if (const auto *gridInsert =
            std::get_if<acdb::AcDbMInsertBlock>(payload))
    {
      appendMInsertBlock(document, handle, *gridInsert, options, target);
      return;
    }
    if (const auto *text = std::get_if<acdb::AcDbText>(payload);
        text != nullptr && shxFontReady && text->styleName == "SHX")
    {
      appendShxText(*text, target);
      return;
    }
    const std::string &name = acdb::common(*payload).name;
    const AcGsReplayHints hints = replayHints().count(name)
                                      ? replayHints()[name]
                                      : AcGsReplayHints{};
    std::visit(
        [&](const auto &entity) {
          appendEntity(entity, name.c_str(), options, target,
                       hints.fillIs3DFace, hints.pickShape);
        },
        *payload);
  }
};

} // namespace acgs
