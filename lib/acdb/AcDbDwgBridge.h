#pragma once

#include "AcDbPort.h"

// Conversion bridge between libredwg's data model (include/dwg.h) and the
// acdb layer.  Read direction (this header): one Dwg_Data -> AcDbDatabase
// fill shared by every file entry point — DWG binary (dwg_read_file) and
// DXF text/binary (dxf_read_file) decode into the same Dwg_Data, so
// addDwgEntities() serves AcDbDatabase::readDwgFile and dxfIn alike.
// The write direction lives in AcDbDwgWriteBridge.h.
//
// Each toEntity() overload maps one DWG entity struct onto its acdb::
// counterpart; field coverage tracks the DWG structures, so gaps surface
// as compile errors when libredwg adds fields we do not carry.  The
// orchestration mirrors a failsafe reader: unsupported types are skipped
// and counted, never fatal.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "libredwg/include/dwg.h"

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbEntities.h"

// libredwg lookup (dwg_api.h surface, declared here to avoid the full
// API header in this bridge).
extern "C" BITCODE_H dwg_find_tablehandle_silent(Dwg_Data *dwg,
                                                 const char *name,
                                                 const char *table);

namespace acdb
{

// ---- ACI palette (DXF 62 / DWG CMC) ----
//
// AutoCAD's base 9 plus the 250..255 grayscale ramp; 0 = ByBlock,
// 256 = ByLayer.  ACIs in 10..249 are the hue/shade ramp the demo does
// not model — they import as white (the write side would re-derive them
// only for entities that kept an explicit index).

glm::vec4 aciToColor(int index);

AcCmEntityColor::ColorMethod dwgColorMethod(std::uint8_t method);

AcCmEntityColor dwgColorToEntityColor(const Dwg_Color &color);

glm::vec4 dwgColorToColor(const Dwg_Color &color);

double lineWeightValueToPixels(int hundredthsOfMillimeter);

double dwgLineWeightToPixels(BITCODE_RC lineWeight);

// Write-side companion: nearest palette entry for a display color whose
// ACI was never recorded (demo-authored entities carry rgba only).
int nearestAciIndex(const glm::vec4 &color);

// File index for the entity's color method. Truecolor keeps its sentinel
// index; its RGB bytes are written separately.
int fileColorIndex(const AcDbEntity &common);

// ---- symbol-table reference resolution (null-safe) ----
//
// Table records all carry the COMMON_TABLE_FIELDS `name`; the Dwg_Object
// union member matches the fixed type token.

const char *symbolTableName(const Dwg_Object *object,
                                   Dwg_Object_Type type);

// Per-import lookup context.  A freshly decoded Dwg_Data leaves part of
// its references unresolved (ref->obj null, dirty_refs), so every
// resolution falls back to a one-shot handle index built here.
struct DwgImportContext
{
    const Dwg_Data *dwg = nullptr;
    std::unordered_map<std::uint64_t, const Dwg_Object *> byHandle;

    explicit DwgImportContext(const Dwg_Data &data) : dwg(&data)
    {
        for (BITCODE_BL i = 0; i < dwg->num_objects; ++i)
            byHandle.emplace(dwg->object[i].handle.value, &dwg->object[i]);
    }

    const Dwg_Object *resolve(const Dwg_Object_Ref *ref) const
    {
        if (ref == nullptr)
            return nullptr;
        if (ref->obj != nullptr)
            return ref->obj;
        const auto found = byHandle.find(ref->absolute_ref);
        return found != byHandle.end() ? found->second : nullptr;
    }
};

const char *layerNameOf(const DwgImportContext &ctx,
                               const Dwg_Object_Entity &ent);

const char *lineTypeNameOf(const DwgImportContext &ctx,
                                  const Dwg_Object_Entity &ent);

const char *blockHeaderNameOf(const DwgImportContext &ctx,
                                     const Dwg_Object_Ref *ref);

// Model/paper space block records never become user-visible block
// definitions (libredwg spells them "*MODEL_SPACE"/"*PAPER_SPACE"; DWG
// files use the mixed-case "*Model_Space" form).
bool isModelOrPaperSpaceName(const std::string &name);

// ---- entity field mapping (common block filled by applyCommon) ----

AcDbLine toEntity(const Dwg_Entity_LINE &dwg);

AcDbArc toEntity(const Dwg_Entity_ARC &dwg);

AcDbCircle toEntity(const Dwg_Entity_CIRCLE &dwg);

AcDbEllipse toEntity(const Dwg_Entity_ELLIPSE &dwg);

AcDbPoint toEntity(const Dwg_Entity_POINT &dwg);

// DWG stores XLINE as the identical Dwg_Entity_RAY struct, so one overload
// serves both; callers re-interpret the result as acdb::AcDbXline.
AcDbRay toEntity(const Dwg_Entity_RAY &dwg);

// 2D fill quad (SOLID); corners are OCS 2D + a shared elevation.
AcDbSolid toEntity(const Dwg_Entity_SOLID &dwg);

// 3DFACE keeps per-corner depth, so it lands in the same quad payload.
AcDbSolid toEntity(const Dwg_Entity__3DFACE &dwg);

// LWPOLYLINE: optimized 2D polyline; bulges align with the leading
// vertices (num_bulges <= num_points per the DWG coding).
AcDbPolyline toEntity(const Dwg_Entity_LWPOLYLINE &dwg);

AcDbText toEntity(const Dwg_Entity_TEXT &dwg);

AcDbMText toEntity(const Dwg_Entity_MTEXT &dwg);

AcDbSpline toEntity(const Dwg_Entity_SPLINE &dwg);

AcDbMline toEntity(const Dwg_Entity_MLINE &dwg);

// INSERT; the referenced record name resolves through the DWG block
// header handle, so the database-owning caller fills blockTableRecordName.
AcDbBlockReference toEntity(const Dwg_Entity_INSERT &dwg);

// MINSERT shares the INSERT placement fields plus the row/column grid.
AcDbMInsertBlock toEntity(const Dwg_Entity_MINSERT &dwg);

// ATTDEF: the block-resident attribute template.  Text alignment modes
// (horiz/vert alignment) beyond left-baseline are out of scope; the
// insertion point serves as the layout origin.
AcDbAttributeDefinition toEntity(const Dwg_Entity_ATTDEF &dwg);

// ATTRIB: the stamped value attached to an INSERT.
AcDbAttribute toEntity(const Dwg_Entity_ATTRIB &dwg);

// LEADER: vertex path plus the arrow/hook switches.
AcDbLeader toEntity(const Dwg_Entity_LEADER &dwg);

std::vector<AcDbEntityVariant> toEntities(
    const Dwg_Entity_MULTILEADER &dwg);

// TOLERANCE: the feature control frame.
AcDbFcf toEntity(const Dwg_Entity_TOLERANCE &dwg);

// SHAPE: placement + the STYLE record that carries the shape file.
AcDbShape toEntity(const Dwg_Entity_SHAPE &dwg);

// Every Dwg_Entity_DIMENSION_* expands DIMENSION_COMMON flat, so the
// shared annotation fields map through one template.
template <typename DwgDimension>
inline void fillDimensionCommon(AcDbDimension &dimension,
                                const DwgDimension &dwg)
{
    dimension.textPosition =
        AcGePoint3d(dwg.text_midpt.x, dwg.text_midpt.y, dwg.elevation);
    dimension.dimensionText = dwg.user_text ? dwg.user_text : "";
    dimension.textRotation = dwg.text_rotation;
    dimension.actualMeasurement = dwg.act_measurement;
}

template <typename DwgDimension>
inline void applyDimensionStyle(const DwgImportContext &ctx,
                                AcDbDimension &dimension,
                                const DwgDimension &dwg)
{
    if (const char *style =
            symbolTableName(ctx.resolve(dwg.dimstyle), DWG_TYPE_DIMSTYLE))
        dimension.dimensionStyleName = style;
}

AcDbRotatedDimension toEntity(const Dwg_Entity_DIMENSION_LINEAR &dwg);

AcDbAlignedDimension toEntity(const Dwg_Entity_DIMENSION_ALIGNED &dwg);

AcDb2LineAngularDimension toEntity(
    const Dwg_Entity_DIMENSION_ANG2LN &dwg);

AcDb3PointAngularDimension toEntity(
    const Dwg_Entity_DIMENSION_ANG3PT &dwg);

AcDbRadialDimension toEntity(const Dwg_Entity_DIMENSION_RADIUS &dwg);

AcDbDiametricDimension toEntity(
    const Dwg_Entity_DIMENSION_DIAMETER &dwg);

AcDbOrdinateDimension toEntity(
    const Dwg_Entity_DIMENSION_ORDINATE &dwg);

// HATCH: only polyline boundary paths map onto the simplified acdb::
// payload; edge-defined (spline/arc) loops are counted unsupported.
// Returns nullopt when no boundary survived.
std::optional<AcDbHatch> tryToEntity(const Dwg_Entity_HATCH &dwg);

// ---- POLYLINE children (VERTEX_* owned-handle chain) ----
//
// POLYLINE_2D/3D/PFACE/MESH share the COMMON_ENTITY_POLYLINE prologue
// (num_owned + vertex[]), but the union member is typed per entity, so
// the accessor switches on the fixed type.

const Dwg_Object_Ref *polylineVertexRef(const Dwg_Object &polyline,
                                               BITCODE_BL index);

template <typename VertexFn>
inline void forEachPolylineVertex(const DwgImportContext &ctx,
                                  const Dwg_Object &polyline, VertexFn &&fn)
{
    for (BITCODE_BL i = 0;; ++i)
    {
        const Dwg_Object *vertex = ctx.resolve(polylineVertexRef(polyline, i));
        if (vertex == nullptr || vertex->supertype != DWG_SUPERTYPE_ENTITY)
            break;
        fn(*vertex);
    }
}

// Fallback for decodes that leave num_owned/vertex[] empty while the
// children themselves survive: scan for VERTEX_* entities owned by the
// polyline (file order preserves the vertex sequence).
template <typename Fn>
inline void forEachVertexByOwner(const DwgImportContext &ctx,
                                 const Dwg_Object &polyline, Fn &&fn)
{
    if (ctx.dwg == nullptr)
        return;
    for (BITCODE_BL i = 0; i < ctx.dwg->num_objects; ++i)
    {
        const Dwg_Object &object = ctx.dwg->object[i];
        if (object.supertype != DWG_SUPERTYPE_ENTITY ||
            object.tio.entity == nullptr)
            continue;
        const Dwg_Object_Entity &ent = *object.tio.entity;
        if (ent.ownerhandle == nullptr ||
            ent.ownerhandle->absolute_ref != polyline.handle.value)
            continue;
        fn(object);
    }
}

// Runs |walk| (the owned-handle chain); if it yields no children, falls
// back to the ownerhandle scan.
template <typename Fn>
inline void forEachPolylineChild(const DwgImportContext &ctx,
                                 const Dwg_Object &polyline, Fn &&fn)
{
    bool any = false;
    forEachPolylineVertex(ctx, polyline, [&](const Dwg_Object &vertex) {
        any = true;
        fn(vertex);
    });
    if (!any)
        forEachVertexByOwner(ctx, polyline, fn);
}

// 2D POLYLINE: heavy polyline with per-vertex bulges and widths.
AcDbPolyline polyline2dFromChildren(const DwgImportContext &ctx,
                                           const Dwg_Object &polyline);

// 3D POLYLINE: straight 3D segments; DXF flag 70 bit 32 = closed.
AcDb3dPolyline polyline3dFromChildren(const DwgImportContext &ctx,
                                             const Dwg_Object &polyline);

// Polyface mesh: numverts VERTEX_PFACE positions followed by numfaces
// VERTEX_PFACE_FACE records (1-based vertind, 0 = absent corner,
// negative = invisible edge).  Faces triangulate as (a,b,c)+(a,c,d).
std::optional<AcDbPolyFaceMesh> pfaceFromChildren(
    const DwgImportContext &ctx, const Dwg_Object &polyline);

// ---- common entity properties ----

void ensureLayer(AcDbDatabase &database, const std::string &name);

void ensureLineType(AcDbDatabase &database, const std::string &name);

void ensureTextStyle(AcDbDatabase &database, const std::string &name);

// Layer records import first so entity colors can resolve ByLayer through
// the real record and layers keep their ACI.
void importLayerRecords(AcDbDatabase &database, const Dwg_Data &dwg);

// Fills the common-properties block from the DWG entity common data:
// layer, semantic entity color, resolved display color,
// linetype, visibility, and the display name.
template <typename Payload>
void applyCommon(Payload &payload, const DwgImportContext &ctx,
                 const Dwg_Object &object, AcDbDatabase &database)
{
    const Dwg_Object_Entity &ent = *object.tio.entity;
    AcDbEntity &common = payload;

    if (const char *layer = layerNameOf(ctx, ent))
        common.layer = layer;
    ensureLayer(database, common.layer);

    common.color = dwgColorToEntityColor(ent.color);
    if (common.color.colorMethod() == AcCmEntityColor::ColorMethod::kByLayer)
    {
        const AcDbLayerTableRecord *record =
            database.layerTable().get(common.layer);
        const glm::vec4 displayColor =
            record != nullptr ? record->color : aciToColor(7);
        common.color.setFileColor(common.color.colorMethod(),
                                  common.color.colorIndex(),
                                  glm::vec3(displayColor), displayColor.a);
    }
    else if (common.color.colorMethod() ==
             AcCmEntityColor::ColorMethod::kByBlock)
    {
        common.color.setFileColor(common.color.colorMethod(),
                                  common.color.colorIndex(),
                                  glm::vec3(aciToColor(7)));
    }
    const int lineWeight = dxf_cvt_lweight(
        static_cast<BITCODE_BSd>(ent.linewt));
    if (lineWeight == -1)
    {
        const AcDbLayerTableRecord *record =
            database.layerTable().get(common.layer);
        common.lineWeight = record != nullptr ? record->lineWeight : 0.0;
    }
    else
    {
        common.lineWeight = lineWeightValueToPixels(lineWeight);
    }

    if (const char *ltype = lineTypeNameOf(ctx, ent))
        if (ltype != std::string("ByLayer"))
        {
            common.lineType = ltype;
            ensureLineType(database, common.lineType);
        }

    common.visible = ent.invisible == 0;
    common.name =
        std::string("DWG_") + (object.dxfname ? object.dxfname : "ENTITY");
}

// ---- post-decode block-reference repair ----
//
// in_dxf registers every BLOCK_RECORD under the last record's handle
// (the BLOCK_CONTROL entries all end up on one absref), which breaks
// its own deferred group-2 resolution: INSERTs whose block_header name
// failed to resolve decode with a NULL block_header and the name is
// gone.  Rebuild the control entries from the actual BLOCK_HEADER
// objects, then re-link the dangling INSERTs through the BLOCK_RECORD
// BLKREFS lists (header->inserts[]), which decoded fine.
void repairBlockReferences(const Dwg_Data &dwg);

// ---- direct fill: DWG/DXF file -> AcDbDatabase (ARX readDwgFile shape) ----

struct ACDB_PORT AcDbDwgImportReport
{
    std::size_t added = 0;
    std::size_t unsupported = 0;
};

// Walks every DWG object, converts the supported entity set through
// toEntity(), routes model/paper-space entities into the database and
// rebuilds user block records from their owned members.  Unsupported
// entity types are skipped and counted (failsafe-reader semantics).
AcDbDwgImportReport addDwgEntities(AcDbDatabase &database,
                                          const Dwg_Data &dwg);

} // namespace acdb
