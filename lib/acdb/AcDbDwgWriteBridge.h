#pragma once

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

struct AcDbDwgWriteReport
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

inline dwg_point_3d toDwgPoint(const AcGePoint3d &point)
{
    return {point.x, point.y, point.z};
}

inline dwg_point_2d toDwgPoint2(const AcGePoint3d &point)
{
    return {point.x, point.y};
}

inline dwg_point_3d toDwgVector(const AcGeVector3d &vector)
{
    return {vector.x, vector.y, vector.z};
}

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

inline void traceMark(DwgWriteContext &ctx, const char *what)
{
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "%s\n", what);
        std::fflush(ctx.trace);
    }
}

inline bool isDefaultNormal(const AcGeVector3d &normal)
{
    return std::abs(normal.x) < 1.0e-12 && std::abs(normal.y) < 1.0e-12 &&
           std::abs(normal.z - 1.0) < 1.0e-12;
}

inline void trackHandle(DwgWriteContext &ctx, void *generic)
{
    int error = 0;
    if (const Dwg_Object *object = dwg_obj_generic_to_object(generic, &error))
        ctx.maxHandle = std::max<std::uint64_t>(ctx.maxHandle,
                                                object->handle.value);
}

// The bootstrap document already created layer "0"; seed the cache from
// existing records because dwg_add_LAYER does not deduplicate by name.
inline void seedExistingLayers(DwgWriteContext &ctx)
{
    for (BITCODE_BL i = 0; i < ctx.dwg.num_objects; ++i)
    {
        const Dwg_Object &object = ctx.dwg.object[i];
        if (object.fixedtype != DWG_TYPE_LAYER || object.tio.object == nullptr)
            continue;
        const Dwg_Object_LAYER &layer = *object.tio.object->tio.LAYER;
        if (layer.name != nullptr)
            ctx.layers.emplace(layer.name, object.tio.object->tio.LAYER);
    }
}

inline Dwg_Object_LAYER *ensureLayer(DwgWriteContext &ctx,
                                     const std::string &name)
{
    const std::string &safe = name.empty() ? std::string("0") : name;
    const auto found = ctx.layers.find(safe);
    if (found != ctx.layers.end())
        return found->second;
    Dwg_Object_LAYER *layer = dwg_add_LAYER(&ctx.dwg, safe.c_str());
    if (layer != nullptr)
        ctx.layers.emplace(safe, layer);
    return layer;
}

inline Dwg_Object_Ref *layerRef(DwgWriteContext &ctx, const std::string &name)
{
    const Dwg_Object_LAYER *layer = ensureLayer(ctx, name);
    if (layer == nullptr)
        return nullptr;
    return dwg_add_handleref(&ctx.dwg, 5,
                             dwg_obj_generic_handlevalue(
                                 const_cast<Dwg_Object_LAYER *>(layer)),
                             NULL);
}

inline Dwg_Object_Ref *lineTypeRef(DwgWriteContext &ctx,
                                   const std::string &name)
{
    const auto found = ctx.lineTypes.find(name);
    if (found != ctx.lineTypes.end() && found->second != nullptr)
        return dwg_add_handleref(&ctx.dwg, 5,
                                 dwg_obj_generic_handlevalue(found->second),
                                 NULL);
    Dwg_Object_LTYPE *ltype = dwg_add_LTYPE(&ctx.dwg, name.c_str());
    if (ltype == nullptr)
        return nullptr;
    ctx.lineTypes.emplace(name, ltype);
    return dwg_add_handleref(&ctx.dwg, 5,
                             dwg_obj_generic_handlevalue(ltype), NULL);
}

inline Dwg_Object_Ref *textStyleRef(DwgWriteContext &ctx,
                                    const std::string &name)
{
    const std::string &safe = name.empty() ? std::string("Standard") : name;
    const auto found = ctx.textStyles.find(safe);
    if (found != ctx.textStyles.end() && found->second != nullptr)
        return dwg_add_handleref(&ctx.dwg, 5,
                                 dwg_obj_generic_handlevalue(found->second),
                                 NULL);
    Dwg_Object_STYLE *style = dwg_add_STYLE(&ctx.dwg, safe.c_str());
    if (style == nullptr)
        return nullptr;
    ctx.textStyles.emplace(safe, style);
    return dwg_add_handleref(&ctx.dwg, 5,
                             dwg_obj_generic_handlevalue(style), NULL);
}

inline Dwg_Object_Ref *dimStyleRef(DwgWriteContext &ctx,
                                   const std::string &name)
{
    if (name.empty())
        return nullptr;
    const auto found = ctx.dimStyles.find(name);
    if (found != ctx.dimStyles.end() && found->second != nullptr)
        return dwg_add_handleref(&ctx.dwg, 5,
                                 dwg_obj_generic_handlevalue(found->second),
                                 NULL);
    Dwg_Object_DIMSTYLE *style = dwg_add_DIMSTYLE(&ctx.dwg, name.c_str());
    if (style == nullptr)
        return nullptr;
    ctx.dimStyles.emplace(name, style);
    return dwg_add_handleref(&ctx.dwg, 5,
                             dwg_obj_generic_handlevalue(style), NULL);
}

// Common properties: ACI color, layer/linetype as record references,
// lineweight (100ths of a mm in DWG), and visibility.
inline void applyCommon(DwgWriteContext &ctx, Dwg_Object_Entity *ent,
                        const AcDbEntity &common)
{
    ent->color.index = fileColorIndex(common);
    if (Dwg_Object_Ref *ref = layerRef(ctx, common.layer))
        ent->layer = ref;
    if (!common.lineType.empty() && common.lineType != "ByLayer" &&
        common.lineType != "ByBlock")
    {
        if (Dwg_Object_Ref *ref = lineTypeRef(ctx, common.lineType))
            ent->ltype = ref;
    }
    if (common.lineWeight > 0.0)
    {
        const long weight = std::lround(common.lineWeight);
        ent->linewt =
            static_cast<BITCODE_RC>(std::clamp(weight, 0L, 211L));
    }
    if (!common.visible)
        ent->invisible = 1;
}

// ---- per-entity payload writers ----

// libredwg owns every string it stores; the adders strdup their inputs,
// so post-hoc assignments hand over a heap copy the same way.
inline char *dupString(const std::string &text)
{
#if defined(_WIN32)
    return _strdup(text.c_str());
#else
    return strdup(text.c_str());
#endif
}

// dwg_find_tablehandle resolves through the BLOCK_CONTROL entries, whose
// bookkeeping the adder macros do not keep reliably (the same quirk that
// leaves the dwg->block_control snapshot stale); scan the object list
// directly instead.
inline Dwg_Object *findBlockHeaderByName(Dwg_Data &dwg,
                                         const std::string &name)
{
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        Dwg_Object &object = dwg.object[i];
        if (object.fixedtype != DWG_TYPE_BLOCK_HEADER ||
            object.tio.object == nullptr)
            continue;
        const Dwg_Object_BLOCK_HEADER *header =
            object.tio.object->tio.BLOCK_HEADER;
        if (header != nullptr && header->name != nullptr &&
            name == header->name)
            return &object;
    }
    return nullptr;
}

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
inline void writeAttachedAttributes(
    DwgWriteContext &ctx, Dwg_Entity_INSERT *insert,
    const std::vector<AcDbHandle> &attributeHandles)
{
    std::vector<std::uint64_t> attachedAttribHandles;
    // dwg_add_ATTRIB overwrites insert->block_header with the OWNING
    // block (model space) - capture the definition reference and restore
    // it afterwards.
    Dwg_Object_Ref *definition = insert->block_header;
    for (const AcDbHandle handle : attributeHandles)
    {
        if (ctx.database == nullptr)
            return;
        const AcDbEntityVariant *payload =
            ctx.database->getEntity(handle);
        if (payload == nullptr || ctx.database->isErased(handle))
            continue;
        if (const AcDbAttribute *attribute =
                std::get_if<AcDbAttribute>(payload))
        {
            const dwg_point_3d insertion = toDwgPoint(attribute->insertion);
            const int flags = attribute->invisible ? 1 : 0;
            Dwg_Entity_ATTRIB *attrib = dwg_add_ATTRIB(
                insert, attribute->height, flags, &insertion,
                attribute->tag.c_str(), attribute->textValue.c_str());
            if (attrib == nullptr)
                continue;
            attrib->rotation = attribute->rotation;
            if (attribute->widthFactor > 0.0)
                attrib->width_factor = attribute->widthFactor;
            if (!attribute->styleName.empty() &&
                attribute->styleName != "Standard")
                attrib->style = textStyleRef(ctx, attribute->styleName);
            // add_attrib_links leaves the ATTRIB owned implicitly by
            // the model-space header (entmode 2); force the explicit
            // ATTRIB -> INSERT owner link instead, so both the DWG
            // object stream and (when present) the DXF ENTITIES section
            // carry the ownership.
            int ownerError = 0;
            if (Dwg_Object *attribObject =
                    dwg_obj_generic_to_object(attrib, &ownerError))
            {
                Dwg_Object *insertObject =
                    dwg_obj_generic_to_object(insert, &ownerError);
                if (insertObject != nullptr)
                {
                    attribObject->tio.entity->entmode = 0;
                    attribObject->tio.entity->ownerhandle =
                        dwg_add_handleref(&ctx.dwg, 3,
                                          insertObject->handle.value,
                                          NULL);
                }
            }
            applyCommon(ctx, attrib->parent, attribute->common);
            trackHandle(ctx, attrib);
            if (Dwg_Object *attribObject =
                    dwg_obj_generic_to_object(attrib, &ownerError))
                attachedAttribHandles.push_back(
                    attribObject->handle.value);
        }
    }
    // add_attrib_links captures the ATTRIB/SEQEND handles before the
    // adder assigns them, so insert->first_attrib lands on the poisoned
    // (4,0) ref and the encoder's chain rebuild writes abs 0 — the DXF
    // INSERT writer then refuses to emit the followers.  Rebuild every
    // link from the real handle values.
    if (!attachedAttribHandles.empty())
    {
        Dwg_Object *seqendObject = nullptr;
        for (BITCODE_BL i = ctx.dwg.num_objects; i > 0; --i)
        {
            Dwg_Object &object = ctx.dwg.object[i - 1];
            if (object.fixedtype != DWG_TYPE_SEQEND ||
                object.supertype != DWG_SUPERTYPE_ENTITY ||
                object.tio.entity == nullptr)
                continue;
            if (object.tio.entity->ownerhandle != nullptr &&
                object.tio.entity->ownerhandle->absolute_ref ==
                    dwg_obj_generic_handlevalue(insert))
            {
                seqendObject = &object;
                break;
            }
        }
        const std::size_t count = attachedAttribHandles.size();
        insert->has_attribs = 1;
        insert->num_owned = BITCODE_BL(count);
        insert->attribs = (BITCODE_H *)calloc(count, sizeof(BITCODE_H));
        if (insert->attribs != nullptr)
        {
            for (std::size_t i = 0; i < count; ++i)
                insert->attribs[i] = dwg_add_handleref(
                    &ctx.dwg, 4, attachedAttribHandles[i], NULL);
            insert->first_attrib = insert->attribs[0];
            insert->last_attrib = insert->attribs[count - 1];
        }
        if (seqendObject != nullptr)
            insert->seqend = dwg_add_handleref(
                &ctx.dwg, 3, seqendObject->handle.value, NULL);
    }
    insert->block_header = definition;
    // has_attribs stays set: out_dxf's INSERT writer emits the
    // ATTRIB ... SEQEND followers from it, and the binary DWG stream
    // carries the ATTRIB objects themselves.  The read bridge
    // re-attaches through the ATTRIB's ownerhandle either way.
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbLine &line)
{
    const dwg_point_3d start = toDwgPoint(line.start);
    const dwg_point_3d end = toDwgPoint(line.end);
    Dwg_Entity_LINE *obj = dwg_add_LINE(hdr, &start, &end);
    if (obj == nullptr)
        return false;
    obj->thickness = line.thickness;
    if (!isDefaultNormal(line.extrusion))
        assign3rd(obj->extrusion, toDwgVector(line.extrusion));
    applyCommon(ctx, obj->parent, line.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbArc &arc)
{
    const dwg_point_3d center = toDwgPoint(arc.center);
    Dwg_Entity_ARC *obj =
        dwg_add_ARC(hdr, &center, arc.radius, arc.startAngle, arc.endAngle);
    if (obj == nullptr)
        return false;
    obj->thickness = arc.thickness;
    if (!isDefaultNormal(arc.normal))
        assign3rd(obj->extrusion, toDwgVector(arc.normal));
    applyCommon(ctx, obj->parent, arc.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbCircle &circle)
{
    const dwg_point_3d center = toDwgPoint(circle.center);
    Dwg_Entity_CIRCLE *obj = dwg_add_CIRCLE(hdr, &center, circle.radius);
    if (obj == nullptr)
        return false;
    obj->thickness = circle.thickness;
    if (!isDefaultNormal(circle.normal))
        assign3rd(obj->extrusion, toDwgVector(circle.normal));
    applyCommon(ctx, obj->parent, circle.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbEllipse &ellipse)
{
    const dwg_point_3d center = toDwgPoint(ellipse.center);
    const double majorAxis = ellipse.majorAxis.length();
    Dwg_Entity_ELLIPSE *obj =
        dwg_add_ELLIPSE(hdr, &center, majorAxis, ellipse.radiusRatio);
    if (obj == nullptr)
        return false;
    assign3rd(obj->sm_axis, toDwgVector(ellipse.majorAxis));
    obj->start_angle = ellipse.startParameter;
    obj->end_angle = ellipse.endParameter;
    if (!isDefaultNormal(ellipse.normal))
        assign3rd(obj->extrusion, toDwgVector(ellipse.normal));
    applyCommon(ctx, obj->parent, ellipse.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbPoint &point)
{
    const dwg_point_3d location = toDwgPoint(point.location);
    Dwg_Entity_POINT *obj = dwg_add_POINT(hdr, &location);
    if (obj == nullptr)
        return false;
    applyCommon(ctx, obj->parent, point.common);
    trackHandle(ctx, obj);
    return true;
}

// RAY and XLINE share the acdb::AcDbRay field shape; the caller
// re-interprets for xline.
inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbRay &ray)
{
    const dwg_point_3d start = toDwgPoint(ray.start);
    const dwg_point_3d direction = toDwgVector(ray.direction);
    Dwg_Entity_RAY *obj = dwg_add_RAY(hdr, &start, &direction);
    if (obj == nullptr)
        return false;
    applyCommon(ctx, obj->parent, ray.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbXline &xline)
{
    const dwg_point_3d point = toDwgPoint(xline.point);
    const dwg_point_3d direction = toDwgVector(xline.direction);
    Dwg_Entity_XLINE *obj = dwg_add_XLINE(hdr, &point, &direction);
    if (obj == nullptr)
        return false;
    applyCommon(ctx, obj->parent, xline.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbSolid &solid)
{
    const dwg_point_3d first = toDwgPoint(solid.firstCorner);
    const dwg_point_2d second = toDwgPoint2(solid.secondCorner);
    const dwg_point_2d third = toDwgPoint2(solid.thirdCorner);
    const dwg_point_2d fourth = toDwgPoint2(solid.fourthCorner);
    Dwg_Entity_SOLID *obj =
        dwg_add_SOLID(hdr, &first, &second, &third, &fourth);
    if (obj == nullptr)
        return false;
    obj->elevation = solid.firstCorner.z;
    applyCommon(ctx, obj->parent, solid.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbPolyline &polyline)
{
    std::vector<dwg_point_2d> points;
    points.reserve(polyline.vertices.size());
    for (const AcGePoint2d &vertex : polyline.vertices)
        points.push_back({vertex.x, vertex.y});
    Dwg_Entity_LWPOLYLINE *obj =
        dwg_add_LWPOLYLINE(hdr, int(points.size()), points.data());
    if (obj == nullptr)
        return false;

    BITCODE_BS flag = polyline.closed ? 512 : 0;
    // bulges[] maps onto the leading vertices; trim the array at the last
    // non-zero entry so positions stay aligned.
    std::size_t bulgeCount = 0;
    for (std::size_t i = polyline.bulges.size(); i > 0; --i)
        if (polyline.bulges[i - 1] != 0.0)
        {
            bulgeCount = i;
            break;
        }
    if (bulgeCount > 0)
    {
        obj->num_bulges = BITCODE_BL(bulgeCount);
        obj->bulges = (BITCODE_BD *)calloc(bulgeCount, sizeof(BITCODE_BD));
        if (obj->bulges != nullptr)
            std::copy(polyline.bulges.begin(),
                      polyline.bulges.begin() + std::ptrdiff_t(bulgeCount),
                      obj->bulges);
        flag |= 16;
    }
    if (polyline.elevation != 0.0)
    {
        obj->elevation = polyline.elevation;
        flag |= 8;
    }
    if (polyline.thickness != 0.0)
    {
        obj->thickness = polyline.thickness;
        flag |= 2;
    }
    if (!isDefaultNormal(polyline.normal))
    {
        assign3rd(obj->extrusion, toDwgVector(polyline.normal));
        flag |= 1;
    }
    obj->flag = flag; // presence bitmask the encoder keys off
    applyCommon(ctx, obj->parent, polyline.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb3dPolyline &polyline)
{
    std::vector<dwg_point_3d> points;
    points.reserve(polyline.vertices.size());
    for (const AcGePoint3d &vertex : polyline.vertices)
        points.push_back(toDwgPoint(vertex));
    Dwg_Entity_POLYLINE_3D *obj =
        dwg_add_POLYLINE_3D(hdr, int(points.size()), points.data());
    if (obj == nullptr)
        return false;
    // The adder pre-set FLAG_POLYLINE_3D (bit 8); OR in the closed bit
    // instead of clobbering the type marker.
    if (polyline.closed)
        obj->flag |= FLAG_POLYLINE_CLOSED;
    applyCommon(ctx, obj->parent, polyline.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbSpline &spline)
{
    // The adder dereferences the tangent vectors and memcpy's the fit
    // points unconditionally, so it needs valid (possibly dummy) arrays.
    const dwg_point_3d zeroVector = {0.0, 0.0, 0.0};
    Dwg_Entity_SPLINE *obj =
        dwg_add_SPLINE(hdr, 0, &zeroVector, &zeroVector, &zeroVector);
    if (obj == nullptr)
        return false;
    obj->scenario = 1; // SPLINE_SCENARIO_SPLINE
    obj->degree = spline.degree == 2 ? 2 : 3;
    obj->closed_b = spline.closed;
    if (!spline.controlPoints.empty())
    {
        const std::size_t count = spline.controlPoints.size();
        obj->num_ctrl_pts = BITCODE_BL(count);
        obj->ctrl_pts = (Dwg_SPLINE_control_point *)calloc(
            count, sizeof(Dwg_SPLINE_control_point));
        if (obj->ctrl_pts != nullptr)
            for (std::size_t i = 0; i < count; ++i)
            {
                obj->ctrl_pts[i].x = spline.controlPoints[i].x;
                obj->ctrl_pts[i].y = spline.controlPoints[i].y;
                obj->ctrl_pts[i].z = spline.controlPoints[i].z;
                obj->ctrl_pts[i].w = 1.0;
            }
    }
    if (!spline.knots.empty())
    {
        const std::size_t count = spline.knots.size();
        obj->num_knots = BITCODE_BL(count);
        obj->knots = (BITCODE_BD *)calloc(count, sizeof(BITCODE_BD));
        if (obj->knots != nullptr)
            std::copy(spline.knots.begin(), spline.knots.end(), obj->knots);
    }
    if (!spline.fitPoints.empty())
    {
        const std::size_t count = spline.fitPoints.size();
        obj->num_fit_pts = BITCODE_BS(count);
        obj->fit_pts = (BITCODE_3DPOINT *)calloc(count,
                                                 sizeof(BITCODE_3DPOINT));
        if (obj->fit_pts != nullptr)
            for (std::size_t i = 0; i < count; ++i)
            {
                obj->fit_pts[i].x = spline.fitPoints[i].x;
                obj->fit_pts[i].y = spline.fitPoints[i].y;
                obj->fit_pts[i].z = spline.fitPoints[i].z;
            }
    }
    applyCommon(ctx, obj->parent, spline.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbText &text)
{
    const dwg_point_3d insertion3 = toDwgPoint(text.insertion);
    Dwg_Entity_TEXT *obj =
        dwg_add_TEXT(hdr, text.text.c_str(), &insertion3, text.height);
    if (obj == nullptr)
        return false;
    obj->rotation = text.rotation;
    if (insertion3.z != 0.0)
    {
        obj->dataflags |= 1; // elevation present
        obj->elevation = insertion3.z;
    }
    if (text.styleName != "Standard")
        obj->style = textStyleRef(ctx, text.styleName);
    applyCommon(ctx, obj->parent, text.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbMText &mtext)
{
    const dwg_point_3d insertion = toDwgPoint(mtext.insertion);
    Dwg_Entity_MTEXT *obj = dwg_add_MTEXT(hdr, &insertion, mtext.width,
                                          mtext.text.c_str());
    if (obj == nullptr)
        return false;
    obj->text_height = mtext.height;
    assign3rd(obj->x_axis_dir, toDwgVector(mtext.direction));
    applyCommon(ctx, obj->parent, mtext.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbHatch &hatch)
{
    Dwg_Entity_HATCH *obj =
        dwg_add_HATCH(hdr, 1 /* predefined pattern */, hatch.patternName.c_str(),
                      false, 0, NULL);
    if (obj == nullptr)
        return false;
    obj->is_solid_fill = hatch.solidFill;
    obj->angle = hatch.patternAngle;
    obj->scale_spacing = hatch.patternScale;
    const double elevation =
        hatch.outerLoop.empty() ? 0.0 : hatch.outerLoop.front().z;

    std::vector<const std::vector<AcGePoint3d> *> loops;
    loops.reserve(hatch.innerLoops.size() + 1);
    if (!hatch.outerLoop.empty())
        loops.push_back(&hatch.outerLoop);
    for (const auto &inner : hatch.innerLoops)
        if (inner.size() >= 2)
            loops.push_back(&inner);
    if (loops.empty())
        return false;

    obj->num_paths = BITCODE_BL(loops.size());
    obj->paths = (Dwg_HATCH_Path *)calloc(loops.size(),
                                          sizeof(Dwg_HATCH_Path));
    if (obj->paths == nullptr)
        return false;
    for (std::size_t i = 0; i < loops.size(); ++i)
    {
        const auto &loop = *loops[i];
        Dwg_HATCH_Path &path = obj->paths[i];
        path.flag = 2; // is_polyline
        if (!hatch.solidFill)
            path.flag |= 0x20; // is_open (pattern fills usually closed)
        path.num_segs_or_paths = BITCODE_BL(loop.size());
        path.polyline_paths = (Dwg_HATCH_PolylinePath *)calloc(
            loop.size(), sizeof(Dwg_HATCH_PolylinePath));
        if (path.polyline_paths == nullptr)
            return false;
        for (std::size_t j = 0; j < loop.size(); ++j)
        {
            path.polyline_paths[j].point.x = loop[j].x;
            path.polyline_paths[j].point.y = loop[j].y;
        }
    }
    obj->elevation = elevation;
    applyCommon(ctx, obj->parent, hatch.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbPolyFaceMesh &mesh)
{
    const std::size_t triangleCount = mesh.geometry.indices.size() / 3;
    if (mesh.geometry.positions.empty() || triangleCount == 0)
        return false;
    std::vector<dwg_point_3d> vertices;
    vertices.reserve(mesh.geometry.positions.size());
    for (const AcGePoint3d &position : mesh.geometry.positions)
        vertices.push_back(toDwgPoint(position));
    // The tri soup becomes 1-based pface faces; a zero 4th corner marks
    // the triangle degenerate.  dwg_face is BITCODE_BSd[4] (a C array
    // type std::vector cannot hold), so the face table is one flat block
    // viewed as const dwg_face*.
    std::vector<BITCODE_BSd> faces(size_t(triangleCount) * 4, 0);
    for (std::size_t i = 0; i < triangleCount; ++i)
    {
        faces[i * 4] = BITCODE_BSd(mesh.geometry.indices[i * 3] + 1);
        faces[i * 4 + 1] = BITCODE_BSd(mesh.geometry.indices[i * 3 + 1] + 1);
        faces[i * 4 + 2] = BITCODE_BSd(mesh.geometry.indices[i * 3 + 2] + 1);
    }
    const dwg_face *faceTable =
        reinterpret_cast<const dwg_face *>(faces.data());
    Dwg_Entity_POLYLINE_PFACE *obj = dwg_add_POLYLINE_PFACE(
        hdr, unsigned(vertices.size()), unsigned(triangleCount),
        vertices.data(), faceTable);
    if (obj == nullptr)
        return false;
    applyCommon(ctx, obj->parent, mesh.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbMline &mline)
{
    std::vector<dwg_point_3d> vertices;
    vertices.reserve(mline.vertices.size());
    for (const AcGePoint3d &vertex : mline.vertices)
        vertices.push_back(toDwgPoint(vertex));
    Dwg_Entity_MLINE *obj =
        dwg_add_MLINE(hdr, unsigned(vertices.size()), vertices.data());
    if (obj == nullptr)
        return false;
    obj->scale = mline.scale.x;
    applyCommon(ctx, obj->parent, mline.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbBlockReference &reference)
{
    if (reference.blockTableRecordName.empty())
        return false;
    const dwg_point_3d position = toDwgPoint(reference.position);
    Dwg_Entity_INSERT *obj = dwg_add_INSERT(
        hdr, &position, reference.blockTableRecordName.c_str(),
        reference.scale.x, reference.scale.y, reference.scale.z,
        reference.rotation);
    if (obj == nullptr)
        return false;
    if (Dwg_Object *headerObject = findBlockHeaderByName(
            ctx.dwg, reference.blockTableRecordName))
    {
        obj->block_header =
            dwg_add_handleref(&ctx.dwg, 5, headerObject->handle.value,
                              NULL);
        registerInsertReference(ctx, headerObject, obj);
    }
    if (!isDefaultNormal(reference.normal))
        assign3rd(obj->extrusion, toDwgVector(reference.normal));
    applyCommon(ctx, obj->parent, reference.common);
    trackHandle(ctx, obj);
    writeAttachedAttributes(ctx, obj, reference.attributeHandles);
    return true;
}

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

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbRotatedDimension &dimension)
{
    const dwg_point_3d xline1 = toDwgPoint(dimension.xLine1Point);
    const dwg_point_3d xline2 = toDwgPoint(dimension.xLine2Point);
    const dwg_point_3d linePoint = toDwgPoint(dimension.dimensionLinePoint);
    Dwg_Entity_DIMENSION_LINEAR *obj = dwg_add_DIMENSION_LINEAR(
        hdr, &xline1, &xline2, &linePoint, dimension.dimensionLineAngle);
    if (obj == nullptr)
        return false;
    obj->oblique_angle = dimension.obliqueAngle;
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbAlignedDimension &dimension)
{
    const dwg_point_3d xline1 = toDwgPoint(dimension.xLine1Point);
    const dwg_point_3d xline2 = toDwgPoint(dimension.xLine2Point);
    const dwg_point_3d textPt = toDwgPoint(dimension.dimension.textPosition);
    Dwg_Entity_DIMENSION_ALIGNED *obj =
        dwg_add_DIMENSION_ALIGNED(hdr, &xline1, &xline2, &textPt);
    if (obj == nullptr)
        return false;
    obj->oblique_angle = dimension.obliqueAngle;
    assign3rd(obj->def_pt, toDwgPoint(dimension.dimensionLinePoint));
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb2LineAngularDimension &dimension)
{
    const dwg_point_3d center = toDwgPoint(dimension.arcPoint);
    const dwg_point_3d xline1end = toDwgPoint(dimension.xLine1EndPoint);
    const dwg_point_3d xline2end = toDwgPoint(dimension.xLine2EndPoint);
    const dwg_point_3d textPt = toDwgPoint(dimension.dimension.textPosition);
    Dwg_Entity_DIMENSION_ANG2LN *obj = dwg_add_DIMENSION_ANG2LN(
        hdr, &center, &xline1end, &xline2end, &textPt);
    if (obj == nullptr)
        return false;
    assign3rd(obj->xline1start_pt, toDwgPoint(dimension.xLine1StartPoint));
    assign3rd(obj->xline2start_pt, toDwgPoint(dimension.xLine2StartPoint));
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDb3PointAngularDimension &dimension)
{
    const dwg_point_3d center = toDwgPoint(dimension.centerPoint);
    const dwg_point_3d xline1 = toDwgPoint(dimension.xLine1Point);
    const dwg_point_3d xline2 = toDwgPoint(dimension.xLine2Point);
    const dwg_point_3d textPt = toDwgPoint(dimension.dimension.textPosition);
    Dwg_Entity_DIMENSION_ANG3PT *obj = dwg_add_DIMENSION_ANG3PT(
        hdr, &center, &xline1, &xline2, &textPt);
    if (obj == nullptr)
        return false;
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbRadialDimension &dimension)
{
    const dwg_point_3d center = toDwgPoint(dimension.centerPoint);
    const dwg_point_3d chord = toDwgPoint(dimension.radiusPoint);
    Dwg_Entity_DIMENSION_RADIUS *obj = dwg_add_DIMENSION_RADIUS(
        hdr, &center, &chord, dimension.leaderLength);
    if (obj == nullptr)
        return false;
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbDiametricDimension &dimension)
{
    const dwg_point_3d chord = toDwgPoint(dimension.chordPoint);
    const dwg_point_3d farChord = toDwgPoint(dimension.farChordPoint);
    Dwg_Entity_DIMENSION_DIAMETER *obj = dwg_add_DIMENSION_DIAMETER(
        hdr, &chord, &farChord, dimension.leaderLength);
    if (obj == nullptr)
        return false;
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbOrdinateDimension &dimension)
{
    const dwg_point_3d feature = toDwgPoint(dimension.featureLocationPoint);
    const dwg_point_3d leaderEnd = toDwgPoint(dimension.leaderEndPoint);
    Dwg_Entity_DIMENSION_ORDINATE *obj = dwg_add_DIMENSION_ORDINATE(
        hdr, &feature, &leaderEnd, dimension.useXAxis);
    if (obj == nullptr)
        return false;
    // DXF group 70 carries the type bits (6 = ordinate, 32 = block
    // reference); libredwg's own decoder records the X-axis choice as
    // bit 0x80 in this flag, so mirror that for the DXF roundtrip.
    obj->flag = BITCODE_RC(6 | 32 | (dimension.useXAxis ? 0x80 : 0));
    fillDwgDimension(ctx, obj, dimension.dimension);
    applyCommon(ctx, obj->parent, dimension.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbLeader &leader)
{
    if (leader.points.size() < 2)
        return false;
    std::vector<dwg_point_3d> points;
    points.reserve(leader.points.size());
    for (const AcGePoint3d &point : leader.points)
        points.push_back(toDwgPoint(point));
    Dwg_Entity_LEADER *obj =
        dwg_add_LEADER(hdr, unsigned(points.size()), points.data(), NULL,
                       LEADER_ANNOTTYPE_NO_ANNOT);
    if (obj == nullptr)
        return false;
    obj->path_type = leader.splinePath ? LEADER_PATHTYPE_SPLINE
                                       : LEADER_PATHTYPE_STRAIGHT;
    obj->arrowhead_on = leader.hasArrowhead ? 1 : 0;
    obj->hookline_on = leader.hasHookLine ? 1 : 0;
    if (Dwg_Object_Ref *ref = dimStyleRef(ctx, leader.dimensionStyleName))
        obj->dimstyle = ref;
    applyCommon(ctx, obj->parent, leader.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbFcf &fcf)
{
    const dwg_point_3d insertion = toDwgPoint(fcf.insertion);
    const dwg_point_3d direction = toDwgVector(fcf.direction);
    Dwg_Entity_TOLERANCE *obj = dwg_add_TOLERANCE(
        hdr, fcf.text.c_str(), &insertion, &direction);
    if (obj == nullptr)
        return false;
    if (fcf.height > 0.0)
        obj->height = fcf.height;
    if (!isDefaultNormal(fcf.normal))
        assign3rd(obj->extrusion, toDwgVector(fcf.normal));
    applyCommon(ctx, obj->parent, fcf.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr, const AcDbShape &shape)
{
    const dwg_point_3d insertion = toDwgPoint(shape.insertion);
    Dwg_Entity_SHAPE *obj = dwg_add_SHAPE(hdr, shape.shapeName.c_str(),
                                          &insertion, shape.scale,
                                          shape.obliqueAngle);
    if (obj == nullptr)
        return false;
    obj->rotation = shape.rotation;
    if (shape.widthFactor > 0.0)
        obj->width_factor = shape.widthFactor;
    obj->thickness = shape.thickness;
    if (!shape.shapeName.empty())
        obj->style = textStyleRef(ctx, shape.shapeName);
    applyCommon(ctx, obj->parent, shape.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbAttributeDefinition &definition)
{
    const dwg_point_3d insertion = toDwgPoint(definition.insertion);
    const int mode = (definition.invisible ? 1 : 0) |
                     (definition.constant ? 2 : 0) |
                     (definition.verify ? 4 : 0) |
                     (definition.preset ? 8 : 0);
    Dwg_Entity_ATTDEF *obj = dwg_add_ATTDEF(
        hdr, definition.height, mode, definition.prompt.c_str(), &insertion,
        definition.tag.c_str(), definition.textValue.c_str());
    if (obj == nullptr)
        return false;
    obj->rotation = definition.rotation;
    if (definition.widthFactor > 0.0)
        obj->width_factor = definition.widthFactor;
    if (definition.obliqueAngle != 0.0)
        obj->oblique_angle = definition.obliqueAngle;
    if (insertion.z != 0.0)
    {
        obj->dataflags |= 1; // elevation present
        obj->elevation = insertion.z;
    }
    if (!definition.styleName.empty() && definition.styleName != "Standard")
        obj->style = textStyleRef(ctx, definition.styleName);
    applyCommon(ctx, obj->parent, definition.common);
    trackHandle(ctx, obj);
    return true;
}

inline bool writePayload(DwgWriteContext &ctx,
                         Dwg_Object_BLOCK_HEADER *hdr,
                         const AcDbMInsertBlock &insert)
{
    if (insert.blockTableRecordName.empty())
        return false;
    const dwg_point_3d position = toDwgPoint(insert.position);
    Dwg_Entity_MINSERT *obj = dwg_add_MINSERT(
        hdr, &position, insert.blockTableRecordName.c_str(),
        insert.scale.x, insert.scale.y, insert.scale.z, insert.rotation,
        insert.rowCount, insert.columnCount, insert.rowSpacing,
        insert.columnSpacing);
    if (obj == nullptr)
        return false;
    if (Dwg_Object *headerObject =
            findBlockHeaderByName(ctx.dwg, insert.blockTableRecordName))
    {
        obj->block_header =
            dwg_add_handleref(&ctx.dwg, 5, headerObject->handle.value,
                              NULL);
        registerInsertReference(ctx, headerObject, obj);
    }
    if (!isDefaultNormal(insert.normal))
        assign3rd(obj->extrusion, toDwgVector(insert.normal));
    applyCommon(ctx, obj->parent, insert.common);
    trackHandle(ctx, obj);
    // Attached ATTRIBs are not written back: dwg_add_ATTRIB only accepts
    // a Dwg_Entity_INSERT*, and the two structs share no safe alias.
    // MINSERT-with-attributes is rare; the read path handles it fully.
    return true;
}

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
inline bool writeEntity(DwgWriteContext &ctx, Dwg_Object_BLOCK_HEADER *hdr,
                        const AcDbEntityVariant &payload)
{
    return std::visit(
        [&](const auto &typed) {
            return detail::writePayload(ctx, hdr, typed);
        },
        payload);
}

// ---- document orchestration: AcDbDatabase -> Dwg_Data ----

inline AcDbDwgWriteReport fillDwgFromDatabase(DwgWriteContext &ctx,
                                              const AcDbDatabase &database)
{
    using detail::traceMark;
    AcDbDwgWriteReport report;
    memset(&ctx.dwg, 0, sizeof(Dwg_Data));
    ctx.database = &database;
    // R_2000 (AC1015) is the newest binary write path LibreDWG fully
    // implements; every adder branches on this, so it must be set before
    // dwg_add_Document builds the standard tables.
    traceMark(ctx, "fill: versions set");
    ctx.dwg.header.version = R_2000;
    ctx.dwg.header.from_version = R_2000;
    // Debug hook: libredwg's own LOG spew (0=quiet..9=trace) on stderr.
    if (const char *logLevel = std::getenv("GRID_LIBREDWG_LOG");
        logLevel != nullptr && *logLevel != '\0')
        ctx.dwg.opts = std::atoi(logLevel);
    traceMark(ctx, "fill: dwg_add_Document enter");
    if (dwg_add_Document(&ctx.dwg, 0) != 0)
        return report;
    traceMark(ctx, "fill: dwg_add_Document done");
    // Pre-reserve the object array.  Several dwg_api adders (e.g.
    // dwg_add_POLYLINE_3D / dwg_add_POLYLINE_PFACE creating VERTEX and
    // SEQEND children) hold Dwg_Object* across further adds; a realloc in
    // between leaves those pointers dangling and corrupts the file or
    // crashes.  Estimate the child expansion per payload (polyline
    // vertices, pface positions/faces, seqends) and size the array so
    // dwg_add_object never reallocs.
    {
        auto budgetFor = [](const AcDbEntityVariant &payload) -> BITCODE_BL {
            return std::visit(
                [](const auto &typed) -> BITCODE_BL {
                    using T = std::decay_t<decltype(typed)>;
                    if constexpr (std::is_same_v<T, AcDbPolyFaceMesh>)
                        return BITCODE_BL(typed.geometry.positions.size() +
                                          typed.geometry.indices.size() / 3 +
                                          2);
                    else if constexpr (std::is_same_v<T, AcDbPolyline> ||
                                       std::is_same_v<T, AcDb3dPolyline>)
                        return BITCODE_BL(typed.vertices.size() + 2);
                    else if constexpr (std::is_same_v<T, AcDbBlockReference> ||
                                       std::is_same_v<T, AcDbMInsertBlock>)
                        return BITCODE_BL(typed.attributeHandles.size() + 2);
                    else
                        return 2; // entity + slack for markers
                },
                payload);
        };
        BITCODE_BL budget = ctx.dwg.num_objects + 64;
        for (const AcDbHandle handle : database.modelSpace().entityHandles())
            if (const AcDbEntityVariant *payload =
                    database.getEntity(handle))
                budget += budgetFor(*payload);
        database.blockTable().forEach(
            [&](const std::string &, const AcDbBlockTableRecord &record) {
                for (const AcDbHandle handle : record.entityHandles())
                    if (const AcDbEntityVariant *payload =
                            database.getEntity(handle))
                        budget += budgetFor(*payload);
            });
        const BITCODE_BL needed = budget;
        if (needed > ctx.dwg.num_alloced_objects)
        {
            ctx.dwg.object = (Dwg_Object *)realloc(
                ctx.dwg.object, needed * sizeof(Dwg_Object));
            ctx.dwg.num_alloced_objects = needed;
        }
    }
    detail::seedExistingLayers(ctx);
    traceMark(ctx, "fill: layers seeded");

    Dwg_Object *modelSpaceObject = dwg_model_space_object(&ctx.dwg);
    if (modelSpaceObject == nullptr ||
        modelSpaceObject->tio.object == nullptr)
        return report;
    Dwg_Object_BLOCK_HEADER *modelSpace =
        modelSpaceObject->tio.object->tio.BLOCK_HEADER;
    traceMark(ctx, "fill: mspace resolved");
    if (ctx.trace != nullptr)
        std::fprintf(ctx.trace,
                     "  db this=%p mspace members=%zu entities=%zu\n",
                     (const void *)&database,
                     database.modelSpace().entityHandles().size(),
                     database.entityCount());

    // Layer records with their ACI colors (negative index = layer off).
    database.layerTable().forEach(
        [&](const std::string &name, const AcDbLayerTableRecord &record) {
            Dwg_Object_LAYER *layer = detail::ensureLayer(ctx, name);
            if (layer == nullptr)
                return;
            const int aci = nearestAciIndex(record.color);
            layer->color.index = record.isOff ? -aci : aci;
            layer->frozen = record.isFrozen;
            traceMark(ctx, ("fill: layer " + name).c_str());
            layer->plotflag = record.isPlottable;
        });

    // User block definitions precede model space so INSERTs resolve their
    // block header by name (dwg_add_INSERT -> dwg_find_tablehandle).
    database.blockTable().forEach(
        [&](const std::string &name, const AcDbBlockTableRecord &record) {
            if (isModelOrPaperSpaceName(name))
                return;
            Dwg_Object_BLOCK_HEADER *header =
                dwg_add_BLOCK_HEADER(&ctx.dwg, name.c_str());
            if (header == nullptr)
            {
                report.unsupported += record.entityHandles().size();
                return;
            }
            detail::assign3rd(header->base_pt, detail::toDwgPoint(record.basePoint()));
            dwg_add_BLOCK(header, name.c_str());
            traceMark(ctx, "fill: user block marker");
            for (const AcDbHandle handle : record.entityHandles())
            {
                const AcDbEntityVariant *payload = database.getEntity(handle);
                if (payload == nullptr || database.isErased(handle))
                    continue;
                traceMark(ctx, "fill: user block member enter");
                if (writeEntity(ctx, header, *payload))
                    ++report.added;
                else
                    ++report.unsupported;
                traceMark(ctx, "fill: user block member done");
            }
            dwg_add_ENDBLK(header);
            traceMark(ctx, "fill: user block endblk");
        });

    // Model space in membership order.
    for (const AcDbHandle handle : database.modelSpace().entityHandles())
    {
        const AcDbEntityVariant *payload = database.getEntity(handle);
        if (payload == nullptr || database.isErased(handle))
            continue;
        if (ctx.trace != nullptr)
        {
            std::fprintf(ctx.trace, "fill: mspace entity enter variant=%zu\n",
                         payload->index());
            std::fflush(ctx.trace);
        }
        if (writeEntity(ctx, modelSpace, *payload))
            ++report.added;
        else
            ++report.unsupported;
        traceMark(ctx, "fill: mspace entity done");
    }

    // Extents from a coarse per-type bbox so viewers open with a sensible
    // zoom; rays/xlines are unbounded and skipped.
    AcGePoint3d extMin(1.0e20, 1.0e20, 1.0e20);
    AcGePoint3d extMax(-1.0e20, -1.0e20, -1.0e20);
    auto accumulate = [&extMin, &extMax](const AcGePoint3d &point) {
        extMin = AcGePoint3d(std::min(extMin.x, point.x),
                             std::min(extMin.y, point.y),
                             std::min(extMin.z, point.z));
        extMax = AcGePoint3d(std::max(extMax.x, point.x),
                             std::max(extMax.y, point.y),
                             std::max(extMax.z, point.z));
    };
    for (const AcDbHandle handle : database.modelSpace().entityHandles())
    {
        const AcDbEntityVariant *payload = database.getEntity(handle);
        if (payload == nullptr || database.isErased(handle))
            continue;
        std::visit(
            [&](const auto &typed) {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, AcDbLine>)
                {
                    accumulate(typed.start);
                    accumulate(typed.end);
                }
                else if constexpr (std::is_same_v<T, AcDbArc> ||
                                   std::is_same_v<T, AcDbCircle>)
                {
                    const double radius = typed.radius;
                    accumulate(typed.center + AcGeVector3d(radius, radius, 0));
                    accumulate(typed.center - AcGeVector3d(radius, radius, 0));
                }
                else if constexpr (std::is_same_v<T, AcDbSolid>)
                {
                    accumulate(typed.firstCorner);
                    accumulate(typed.secondCorner);
                    accumulate(typed.thirdCorner);
                    accumulate(typed.fourthCorner);
                }
                else if constexpr (std::is_same_v<T, AcDbPolyline>)
                {
                    for (const AcGePoint2d &v : typed.vertices)
                        accumulate(AcGePoint3d(v.x, v.y, typed.elevation));
                }
                else if constexpr (std::is_same_v<T, AcDb3dPolyline>)
                {
                    for (const AcGePoint3d &v : typed.vertices)
                        accumulate(v);
                }
                else if constexpr (std::is_same_v<T, AcDbSpline>)
                {
                    for (const AcGePoint3d &v : typed.controlPoints)
                        accumulate(v);
                }
                else if constexpr (std::is_same_v<T, AcDbText>)
                {
                    accumulate(typed.insertion);
                }
                else if constexpr (std::is_same_v<T, AcDbMText>)
                {
                    accumulate(typed.insertion);
                }
                else if constexpr (std::is_same_v<T, AcDbHatch>)
                {
                    for (const AcGePoint3d &v : typed.outerLoop)
                        accumulate(v);
                }
                else if constexpr (std::is_same_v<T, AcDbPolyFaceMesh>)
                {
                    for (const AcGePoint3d &v : typed.geometry.positions)
                        accumulate(v);
                }
                else if constexpr (std::is_same_v<T, AcDbPoint>)
                {
                    accumulate(typed.location);
                }
                else if constexpr (std::is_same_v<T, AcDbBlockReference>)
                {
                    accumulate(typed.position);
                }
                else if constexpr (std::is_same_v<T, AcDbMline>)
                {
                    for (const AcGePoint3d &v : typed.vertices)
                        accumulate(v);
                }
            },
            *payload);
    }
    if (extMin.x <= extMax.x && extMin.y <= extMax.y)
    {
        detail::assign3rd(ctx.dwg.header_vars.EXTMIN, detail::toDwgPoint(extMin));
        detail::assign3rd(ctx.dwg.header_vars.EXTMAX, detail::toDwgPoint(extMax));
    }

    traceMark(ctx, "fill: extents done");
    if (ctx.trace != nullptr)
    {
        // Forensic dump: every entity's type/entmode/owner + all block
        // headers, to pin down ownership corruption at the source.
        for (BITCODE_BL i = 0; i < ctx.dwg.num_objects; ++i)
        {
            const Dwg_Object &object = ctx.dwg.object[i];
            if (object.supertype != DWG_SUPERTYPE_ENTITY ||
                object.tio.entity == nullptr)
            {
                if (object.fixedtype == DWG_TYPE_BLOCK_HEADER &&
                    object.tio.object != nullptr &&
                    object.tio.object->tio.BLOCK_HEADER != nullptr)
                    std::fprintf(
                        ctx.trace, "  hdr '%s' handle=%llu num_owned=%u\n",
                        object.tio.object->tio.BLOCK_HEADER->name != nullptr
                            ? object.tio.object->tio.BLOCK_HEADER->name
                            : "?",
                        (unsigned long long)object.handle.value,
                        (unsigned)object.tio.object->tio.BLOCK_HEADER
                            ->num_owned);
                continue;
            }
            const Dwg_Object_Entity &ent = *object.tio.entity;
            if (object.fixedtype == DWG_TYPE_INSERT ||
                object.fixedtype == DWG_TYPE_MINSERT)
                std::fprintf(
                    ctx.trace,
                    "    insert definition absref=%llu\n",
                    ent.tio.INSERT->block_header != nullptr
                        ? (unsigned long long)
                              ent.tio.INSERT->block_header->absolute_ref
                        : 0ULL);
            std::fprintf(
                ctx.trace, "  ent %-14s type=%d entmode=%d owner=%llu\n",
                object.dxfname != nullptr ? object.dxfname : "?",
                (int)object.fixedtype, (int)ent.entmode,
                ent.ownerhandle != nullptr
                    ? (unsigned long long)ent.ownerhandle->absolute_ref
                    : 0ULL);
            if (ent.entmode == 0 && object.fixedtype == DWG_TYPE_LINE)
                std::fprintf(ctx.trace,
                             "    ownerhandle ptr=%p handleref: code=%u "
                             "value=%llu absref=%llu\n",
                             (const void *)ent.ownerhandle,
                             ent.ownerhandle != nullptr
                                 ? (unsigned)ent.ownerhandle->handleref.code
                                 : 0u,
                             ent.ownerhandle != nullptr
                                 ? (unsigned long long)
                                       ent.ownerhandle->handleref.value
                                 : 0ULL,
                             ent.ownerhandle != nullptr
                                 ? (unsigned long long)
                                       ent.ownerhandle->absolute_ref
                                 : 0ULL);
        }
        std::fflush(ctx.trace);
    }
    if (ctx.trace != nullptr)
    {
        // The DXF writer iterates this snapshot for the BLOCK_RECORD
        // table; dump it to catch stale-copy desync.
        std::fprintf(ctx.trace, "  block_control copy: num_entries=%u\n",
                     (unsigned)ctx.dwg.block_control.num_entries);
        for (BITCODE_BL i = 0; i < ctx.dwg.block_control.num_entries; ++i)
        {
            const BITCODE_H entry = ctx.dwg.block_control.entries
                ? ctx.dwg.block_control.entries[i]
                : nullptr;
            std::fprintf(ctx.trace, "    entry[%u] absref=%llu\n",
                         (unsigned)i,
                         entry != nullptr
                             ? (unsigned long long)entry->absolute_ref
                             : 0ULL);
        }
        // The live control object inside dwg->object[], for comparison.
        for (BITCODE_BL i = 0; i < ctx.dwg.num_objects; ++i)
        {
            const Dwg_Object &object = ctx.dwg.object[i];
            if (object.fixedtype != DWG_TYPE_BLOCK_CONTROL ||
                object.tio.object == nullptr)
                continue;
            const Dwg_Object_BLOCK_CONTROL &live =
                *object.tio.object->tio.BLOCK_CONTROL;
            std::fprintf(ctx.trace,
                         "  live control obj[%u] handle=%llu num_entries=%u\n",
                         (unsigned)i, (unsigned long long)object.handle.value,
                         (unsigned)live.num_entries);
            for (BITCODE_BL j = 0; j < live.num_entries; ++j)
            {
                const BITCODE_H entry = live.entries ? live.entries[j] : nullptr;
                std::fprintf(ctx.trace, "    entry[%u] absref=%llu\n",
                             (unsigned)j,
                             entry != nullptr
                                 ? (unsigned long long)entry->absolute_ref
                                 : 0ULL);
            }
        }
        std::fflush(ctx.trace);
    }
    // HANDSEED must be above every allocated handle.
    ctx.dwg.header_vars.HANDSEED =
        dwg_add_handleref(&ctx.dwg, 0, ctx.maxHandle + 1, NULL);

    traceMark(ctx, "fill: handseed done");
    // Resolve every reference's ->obj pointer.  The DXF writer prints a
    // handle's absolute_ref only when the reference is resolved and
    // writes 0 otherwise, and freshly added refs (ownerhandles, table
    // entries) are born unresolved.
    for (BITCODE_BL i = 0; i < ctx.dwg.num_object_refs; ++i)
    {
        Dwg_Object_Ref *ref = ctx.dwg.object_ref[i];
        if (ref == nullptr || ref->obj != nullptr)
            continue;
        ref->obj =
            dwg_resolve_handle(&ctx.dwg, (BITCODE_RLL)ref->absolute_ref);
    }
    // libredwg quirk: dwg_add_BLOCK_HEADER cannot refresh the
    // dwg->block_control snapshot (the API_ADD_TABLE macro returns out of
    // the function, so the trailing copy is dead code).  The DXF
    // BLOCK_RECORD table writer iterates that stale snapshot and would
    // silently drop every user block — refresh it from the live control.
    for (BITCODE_BL i = 0; i < ctx.dwg.num_objects; ++i)
    {
        const Dwg_Object &object = ctx.dwg.object[i];
        if (object.fixedtype == DWG_TYPE_BLOCK_CONTROL &&
            object.tio.object != nullptr)
        {
            ctx.dwg.block_control =
                *object.tio.object->tio.BLOCK_CONTROL;
            break;
        }
    }
    report.ok = true;
    return report;
}

} // namespace acdb
