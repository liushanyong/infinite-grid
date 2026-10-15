#pragma once

#include "AcDbPort.h"

// Write-side bridge: AcDbDatabase -> libredwg's Dwg_Data, built with the
// dwg_api "adder" surface (dwg_add_Document + dwg_add_<ENTITY>).  The
// adders own every file-format invariant — handle allocation, entmode,
// owner/first/last entity chains, BLOCK/ENDBLK markers, control-object
// wiring — so this bridge only maps fields and resolves common
// properties into real table records (files reference records by
// handle, never by name).
//
// One filled Dwg_Data feeds all three exporters: dwg_write_file (binary
// DWG, R13..R2000), dwg_write_dxf / dwg_write_dxfb (text/binary DXF).
// The read direction lives in AcDbDwgBridge.h.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "libredwg/include/dwg.h"
#include "libredwg/include/dwg_api.h"

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbDwgBridge.h" // fileColorIndex / isModelOrPaperSpaceName
#include "acdb/AcDbEntities.h"

namespace acdb
{

struct ACDB_PORT AcDbDwgWriteReport
{
    bool ok = false;
    std::size_t added = 0;
    std::size_t unsupported = 0;
};

// Per-export state: the growing Dwg_Data plus the table-record caches
// (name -> object) so entity common properties resolve to the same
// record every time.
struct DwgWriteContext
{
    Dwg_Data dwg{};
    // The exported database (set by fillDwgFromDatabase): attached
    // ATTRIB entities resolve through it by handle.
    const AcDbDatabase *database = nullptr;
    std::unordered_map<std::string, Dwg_Object_LAYER *> layers;
    std::unordered_map<std::string, Dwg_Object_LTYPE *> lineTypes;
    std::unordered_map<std::string, Dwg_Object_STYLE *> textStyles;
    std::unordered_map<std::string, Dwg_Object_DIMSTYLE *> dimStyles;
    std::uint64_t maxHandle = 0;
    // Crash-position tracer (GRID_DWGWRITE_TRACE=<path>): the GUI
    // subsystem has no valid stderr, so forensics go to a real file.
    std::FILE *trace = nullptr;
};

namespace detail
{

dwg_point_3d toDwgPoint(const AcGePoint3d &point);

dwg_point_2d toDwgPoint2(const AcGePoint3d &point);

dwg_point_3d toDwgVector(const AcGeVector3d &vector);

// The libredwg point fields come in several layout-identical but distinct
// struct types (BITCODE_3RD / BITCODE_BE = Dwg_Bitcode_3BD / BITCODE_3DPOINT)
// with no implicit conversion from dwg_point_3d; copy field by field.
template <typename DstPoint>
inline void assign3rd(DstPoint &destination, const dwg_point_3d &source)
{
    destination.x = source.x;
    destination.y = source.y;
    destination.z = source.z;
}

void traceMark(DwgWriteContext &ctx, const char *what);

bool isDefaultNormal(const AcGeVector3d &normal);

void trackHandle(DwgWriteContext &ctx, void *generic);

// The bootstrap document already created layer "0"; seed the cache from
// existing records because dwg_add_LAYER does not deduplicate by name.
void seedExistingLayers(DwgWriteContext &ctx);

Dwg_Object_LAYER *ensureLayer(DwgWriteContext &ctx,
                                     const std::string &name);

Dwg_Object_Ref *layerRef(DwgWriteContext &ctx, const std::string &name);

Dwg_Object_Ref *lineTypeRef(DwgWriteContext &ctx,
                                   const std::string &name);

Dwg_Object_Ref *textStyleRef(DwgWriteContext &ctx,
                                    const std::string &name);

Dwg_Object_Ref *dimStyleRef(DwgWriteContext &ctx,
                                   const std::string &name);

// Common properties: AcCmEntityColor method/index/RGB, layer/linetype as
// record references, lineweight (100ths of a mm in DWG), and visibility.
void applyCommon(DwgWriteContext &ctx, Dwg_Object_Entity *ent,
                        const AcDbEntity &common);

// ---- per-entity payload writers ----

// libredwg owns every string it stores; the adders strdup their inputs,
// so post-hoc assignments hand over a heap copy the same way.
char *dupString(const std::string &text);

// dwg_find_tablehandle resolves through the BLOCK_CONTROL entries, whose
// bookkeeping the adder macros do not keep reliably (the same quirk that
// leaves the dwg->block_control snapshot stale); scan the object list
// directly instead.
Dwg_Object *findBlockHeaderByName(Dwg_Data &dwg,
                                         const std::string &name);

// dwg_add_INSERT maintains blkhdr->inserts[] only when its own table
// lookup succeeds; with the explicit resolution above the list must be
// maintained here so the DXF BLOCK_RECORD BLKREFS lists survive.
template <typename DwgInsert>
inline void registerInsertReference(DwgWriteContext &ctx,
                                    Dwg_Object *headerObject,
                                    DwgInsert *insert)
{
    Dwg_Object_BLOCK_HEADER *header =
        headerObject->tio.object->tio.BLOCK_HEADER;
    if (header == nullptr)
        return;
    int error = 0;
    const Dwg_Object *insertObject =
        dwg_obj_generic_to_object(insert, &error);
    if (insertObject == nullptr || error != 0)
        return;
    // dwg_add_INSERT's internal name lookup runs through the unreliable
    // BLOCK_CONTROL iteration and can append this reference to the WRONG
    // header; scrub it from every other header before registering the
    // resolved one.
    for (BITCODE_BL i = 0; i < ctx.dwg.num_objects; ++i)
    {
        Dwg_Object &object = ctx.dwg.object[i];
        if (object.fixedtype != DWG_TYPE_BLOCK_HEADER ||
            object.tio.object == nullptr ||
            object.tio.object->tio.BLOCK_HEADER == nullptr ||
            &object == headerObject)
            continue;
        Dwg_Object_BLOCK_HEADER &other =
            *object.tio.object->tio.BLOCK_HEADER;
        BITCODE_BL kept = 0;
        for (BITCODE_BL j = 0; j < other.num_inserts; ++j)
        {
            const BITCODE_H entry =
                other.inserts != nullptr ? other.inserts[j] : nullptr;
            if (entry != nullptr &&
                entry->absolute_ref == insertObject->handle.value)
                continue; // drop the misfiled reference
            if (kept != j && other.inserts != nullptr)
                other.inserts[kept] = entry;
            ++kept;
        }
        other.num_inserts = kept;
    }
    // Skip duplicates when the adder's lookup landed correctly.
    for (BITCODE_BL j = 0; j < header->num_inserts; ++j)
        if (header->inserts != nullptr &&
            header->inserts[j] != nullptr &&
            header->inserts[j]->absolute_ref ==
                insertObject->handle.value)
            return;
    header->num_inserts++;
    header->inserts = (BITCODE_H *)realloc(
        header->inserts, header->num_inserts * sizeof(BITCODE_H));
    if (header->inserts != nullptr)
        header->inserts[header->num_inserts - 1] =
            dwg_add_handleref(&ctx.dwg, 4, insertObject->handle.value,
                              NULL);
}

// Stamps the INSERT's owned ATTRIB payloads after the INSERT itself.
void writeAttachedAttributes(
    DwgWriteContext &ctx, Dwg_Entity_INSERT *insert,
    const std::vector<AcDbHandle> &attributeHandles);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbLine &line);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbArc &arc);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbCircle &circle);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbEllipse &ellipse);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbPoint &point);

// RAY and XLINE share the acdb::AcDbRay field shape; the caller
// re-interprets for xline.
bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbRay &ray);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbXline &xline);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbSolid &solid);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbPolyline &polyline);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb3dPolyline &polyline);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbSpline &spline);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbText &text);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbMText &mtext);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbHatch &hatch);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbPolyFaceMesh &mesh);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbMline &mline);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbBlockReference &reference);

// Every Dwg_Entity_DIMENSION_* expands DIMENSION_COMMON flat; the shared
// annotation fields map through one template.
template <typename DwgDimension>
inline void fillDwgDimension(DwgWriteContext &ctx, DwgDimension *obj,
                             const AcDbDimension &dimension)
{
    // text_midpt is a 2RD pair (z rides the shared elevation).
    obj->text_midpt.x = dimension.textPosition.x;
    obj->text_midpt.y = dimension.textPosition.y;
    if (!dimension.dimensionText.empty())
        obj->user_text = dupString(dimension.dimensionText);
    obj->text_rotation = dimension.textRotation;
    obj->act_measurement = dimension.actualMeasurement;
    if (Dwg_Object_Ref *ref = dimStyleRef(ctx, dimension.dimensionStyleName))
        obj->dimstyle = ref;
}

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbRotatedDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbAlignedDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb2LineAngularDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb3PointAngularDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbRadialDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbDiametricDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbOrdinateDimension &dimension);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbLeader &leader);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbFcf &fcf);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbShape &shape);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbAttributeDefinition &definition);

bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbMInsertBlock &insert);

// Payloads without a DWG counterpart in this bridge (AcDb3dSolid,
// AcDbLight).
template <typename Payload>
inline bool writePayload(DwgWriteContext &, Dwg_Object_BLOCK_HEADER *,
                         const Payload &)
{
    return false;
}

} // namespace detail

// Writes one database entity into |hdr|; dispatches through the variant.
bool writeEntity(DwgWriteContext &ctx, Dwg_Object_BLOCK_HEADER *hdr,
                        const AcDbEntityVariant &payload);

// ---- document orchestration: AcDbDatabase -> Dwg_Data ----

AcDbDwgWriteReport fillDwgFromDatabase(DwgWriteContext &ctx,
                                              const AcDbDatabase &database);

} // namespace acdb
