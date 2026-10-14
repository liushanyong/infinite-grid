#pragma once

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

inline glm::vec4 aciToColor(int index)
{
    switch (index)
    {
    case 1: return {1.0f, 0.0f, 0.0f, 1.0f};
    case 2: return {1.0f, 1.0f, 0.0f, 1.0f};
    case 3: return {0.0f, 1.0f, 0.0f, 1.0f};
    case 4: return {0.0f, 1.0f, 1.0f, 1.0f};
    case 5: return {0.0f, 0.0f, 1.0f, 1.0f};
    case 6: return {1.0f, 0.0f, 1.0f, 1.0f};
    case 7: return {1.0f, 1.0f, 1.0f, 1.0f};
    case 8: return {0.5f, 0.5f, 0.5f, 1.0f};
    case 9: return {0.75f, 0.75f, 0.75f, 1.0f};
    case 250: return {0.2f, 0.2f, 0.2f, 1.0f};
    case 251: return {0.314f, 0.314f, 0.314f, 1.0f};
    case 252: return {0.412f, 0.412f, 0.412f, 1.0f};
    case 253: return {0.51f, 0.51f, 0.51f, 1.0f};
    case 254: return {0.745f, 0.745f, 0.745f, 1.0f};
    case 255: return {1.0f, 1.0f, 1.0f, 1.0f};
    default: break;
    }

    // ACI 10..249 is a 24-hue wheel with five value levels, each paired
    // with a half-saturation tint. Keeping the full wheel avoids turning
    // most imported layer and entity colors white.
    if (index >= 10 && index <= 249)
    {
        static constexpr float values[] = {
            1.0f, 1.0f, 0.647f, 0.647f, 0.498f,
            0.498f, 0.298f, 0.298f, 0.149f, 0.149f};
        const int hueIndex = (index - 10) / 10;
        const int shade = (index - 10) % 10;
        const float hue = static_cast<float>(hueIndex) * 15.0f;
        const float value = values[shade];
        const float saturation = (shade % 2 == 0) ? 1.0f : 0.5f;
        const float chroma = value * saturation;
        const float x = chroma *
            (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        if (hue < 60.0f)
            red = chroma, green = x;
        else if (hue < 120.0f)
            red = x, green = chroma;
        else if (hue < 180.0f)
            green = chroma, blue = x;
        else if (hue < 240.0f)
            green = x, blue = chroma;
        else if (hue < 300.0f)
            red = x, blue = chroma;
        else
            red = chroma, blue = x;
        const float offset = value - chroma;
        return {red + offset, green + offset, blue + offset, 1.0f};
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

inline AcCmEntityColor::ColorMethod dwgColorMethod(std::uint8_t method)
{
    switch (method)
    {
    case 0xc0: return AcCmEntityColor::ColorMethod::kByLayer;
    case 0xc1: return AcCmEntityColor::ColorMethod::kByBlock;
    case 0xc3: return AcCmEntityColor::ColorMethod::kByColor;
    case 0xc4: return AcCmEntityColor::ColorMethod::kByPen;
    case 0xc5: return AcCmEntityColor::ColorMethod::kForeground;
    case 0xc6: return AcCmEntityColor::ColorMethod::kLayerOff;
    case 0xc7: return AcCmEntityColor::ColorMethod::kLayerFrozen;
    case 0xc8: return AcCmEntityColor::ColorMethod::kNone;
    default: return AcCmEntityColor::ColorMethod::kByACI;
    }
}

inline AcCmEntityColor dwgColorToEntityColor(const Dwg_Color &color)
{
    AcCmEntityColor result;
    const int index = color.index < 0 ? -color.index : color.index;
    AcCmEntityColor::ColorMethod method = dwgColorMethod(color.method);
    if (method == AcCmEntityColor::ColorMethod::kByACI && index == 256)
        method = AcCmEntityColor::ColorMethod::kByLayer;
    else if (method == AcCmEntityColor::ColorMethod::kByACI && index == 0)
        method = AcCmEntityColor::ColorMethod::kByBlock;
    const glm::vec4 rgba = [&] {
        if (color.method == 0xc3)
        {
            const std::uint32_t rgb = color.rgb & 0x00ffffffu;
            return glm::vec4(float((rgb >> 16) & 0xffu) / 255.0f,
                             float((rgb >> 8) & 0xffu) / 255.0f,
                             float(rgb & 0xffu) / 255.0f, 1.0f);
        }
        return aciToColor(index);
    }();
    result.setFileColor(method, color.index, glm::vec3(rgba));
    result.setAlpha(rgba.a);
    return result;
}

inline glm::vec4 dwgColorToColor(const Dwg_Color &color)
{
    return dwgColorToEntityColor(color).renderColor();
}

inline double lineWeightValueToPixels(int hundredthsOfMillimeter)
{
    return hundredthsOfMillimeter >= 0
               ? (double(hundredthsOfMillimeter) / 100.0) * (96.0 / 25.4)
               : 0.0;
}

inline double dwgLineWeightToPixels(BITCODE_RC lineWeight)
{
    const int hundredthsOfMillimeter =
        dxf_cvt_lweight(static_cast<BITCODE_BSd>(lineWeight));
    return lineWeightValueToPixels(hundredthsOfMillimeter);
}

// Write-side companion: nearest palette entry for a display color whose
// ACI was never recorded (demo-authored entities carry rgba only).
inline int nearestAciIndex(const glm::vec4 &color)
{
    static constexpr std::pair<int, glm::vec3> kPalette[] = {
        {1, {1, 0, 0}},   {2, {1, 1, 0}},   {3, {0, 1, 0}},
        {4, {0, 1, 1}},   {5, {0, 0, 1}},   {6, {1, 0, 1}},
        {7, {1, 1, 1}},   {8, {0.5, 0.5, 0.5}}, {9, {0.75, 0.75, 0.75}},
        {250, {0.2, 0.2, 0.2}},       {251, {0.357, 0.357, 0.357}},
        {252, {0.517, 0.517, 0.517}}, {253, {0.678, 0.678, 0.678}},
        {254, {0.839, 0.839, 0.839}}, {255, {1, 1, 1}},
    };
    int best = 7;
    double bestDistance = 1.0e30;
    for (const auto &[index, rgb] : kPalette)
    {
        const glm::dvec3 delta = glm::dvec3(color) - glm::dvec3(rgb);
        const double distance = glm::dot(delta, delta);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

// File index for the entity's color method. Truecolor keeps its sentinel
// index; its RGB bytes are written separately.
inline int fileColorIndex(const AcDbEntity &common)
{
    switch (common.color.colorMethod())
    {
    case AcCmEntityColor::ColorMethod::kByLayer: return 256;
    case AcCmEntityColor::ColorMethod::kByBlock: return 0;
    case AcCmEntityColor::ColorMethod::kByACI:
        return common.color.colorIndex();
    case AcCmEntityColor::ColorMethod::kByColor:
        return common.color.colorIndex();
    case AcCmEntityColor::ColorMethod::kByPen:
    case AcCmEntityColor::ColorMethod::kLayerOff:
    case AcCmEntityColor::ColorMethod::kLayerFrozen:
        return common.color.colorIndex();
    case AcCmEntityColor::ColorMethod::kForeground: return 7;
    case AcCmEntityColor::ColorMethod::kNone: return 0;
    }
    return 256;
}

// ---- symbol-table reference resolution (null-safe) ----
//
// Table records all carry the COMMON_TABLE_FIELDS `name`; the Dwg_Object
// union member matches the fixed type token.

inline const char *symbolTableName(const Dwg_Object *object,
                                   Dwg_Object_Type type)
{
    if (object == nullptr || object->supertype != DWG_SUPERTYPE_OBJECT ||
        object->fixedtype != type || object->tio.object == nullptr)
        return nullptr;
    switch (type)
    {
    case DWG_TYPE_LAYER: return object->tio.object->tio.LAYER->name;
    case DWG_TYPE_LTYPE: return object->tio.object->tio.LTYPE->name;
    case DWG_TYPE_STYLE: return object->tio.object->tio.STYLE->name;
    case DWG_TYPE_DIMSTYLE:
        return object->tio.object->tio.DIMSTYLE->name;
    case DWG_TYPE_BLOCK_HEADER:
        return object->tio.object->tio.BLOCK_HEADER->name;
    default: return nullptr;
    }
}

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

inline const char *layerNameOf(const DwgImportContext &ctx,
                               const Dwg_Object_Entity &ent)
{
    return symbolTableName(ctx.resolve(ent.layer), DWG_TYPE_LAYER);
}

inline const char *lineTypeNameOf(const DwgImportContext &ctx,
                                  const Dwg_Object_Entity &ent)
{
    return symbolTableName(ctx.resolve(ent.ltype), DWG_TYPE_LTYPE);
}

inline const char *blockHeaderNameOf(const DwgImportContext &ctx,
                                     const Dwg_Object_Ref *ref)
{
    return symbolTableName(ctx.resolve(ref), DWG_TYPE_BLOCK_HEADER);
}

// Model/paper space block records never become user-visible block
// definitions (libredwg spells them "*MODEL_SPACE"/"*PAPER_SPACE"; DWG
// files use the mixed-case "*Model_Space" form).
inline bool isModelOrPaperSpaceName(const std::string &name)
{
    std::string upper;
    upper.reserve(name.size());
    for (const char c : name)
        upper.push_back(c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c);
    return upper.rfind("*MODEL_SPACE", 0) == 0 ||
           upper.rfind("*PAPER_SPACE", 0) == 0;
}

// ---- entity field mapping (common block filled by applyCommon) ----

inline AcDbLine toEntity(const Dwg_Entity_LINE &dwg)
{
    AcDbLine line;
    line.start = AcGePoint3d(dwg.start.x, dwg.start.y, dwg.start.z);
    line.end = AcGePoint3d(dwg.end.x, dwg.end.y, dwg.end.z);
    line.thickness = dwg.thickness;
    line.extrusion = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                  dwg.extrusion.z);
    return line;
}

inline AcDbArc toEntity(const Dwg_Entity_ARC &dwg)
{
    AcDbArc arc;
    arc.center = AcGePoint3d(dwg.center.x, dwg.center.y, dwg.center.z);
    arc.radius = dwg.radius;
    arc.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                              dwg.extrusion.z);
    arc.thickness = dwg.thickness;
    arc.startAngle = dwg.start_angle;
    arc.endAngle = dwg.end_angle;
    return arc;
}

inline AcDbCircle toEntity(const Dwg_Entity_CIRCLE &dwg)
{
    AcDbCircle circle;
    circle.center = AcGePoint3d(dwg.center.x, dwg.center.y, dwg.center.z);
    circle.radius = dwg.radius;
    circle.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                 dwg.extrusion.z);
    circle.thickness = dwg.thickness;
    return circle;
}

inline AcDbEllipse toEntity(const Dwg_Entity_ELLIPSE &dwg)
{
    AcDbEllipse ellipse;
    ellipse.center = AcGePoint3d(dwg.center.x, dwg.center.y, dwg.center.z);
    ellipse.majorAxis = AcGeVector3d(dwg.sm_axis.x, dwg.sm_axis.y,
                                     dwg.sm_axis.z);
    ellipse.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                  dwg.extrusion.z);
    ellipse.radiusRatio = dwg.axis_ratio;
    ellipse.startParameter = dwg.start_angle;
    ellipse.endParameter = dwg.end_angle;
    return ellipse;
}

inline AcDbPoint toEntity(const Dwg_Entity_POINT &dwg)
{
    AcDbPoint point;
    point.location = AcGePoint3d(dwg.x, dwg.y, dwg.z);
    return point;
}

// DWG stores XLINE as the identical Dwg_Entity_RAY struct, so one overload
// serves both; callers re-interpret the result as acdb::AcDbXline.
inline AcDbRay toEntity(const Dwg_Entity_RAY &dwg)
{
    AcDbRay ray;
    ray.start = AcGePoint3d(dwg.point.x, dwg.point.y, dwg.point.z);
    ray.direction = AcGeVector3d(dwg.vector.x, dwg.vector.y, dwg.vector.z);
    return ray;
}

// 2D fill quad (SOLID); corners are OCS 2D + a shared elevation.
inline AcDbSolid toEntity(const Dwg_Entity_SOLID &dwg)
{
    AcDbSolid solid;
    const double z = dwg.elevation;
    solid.firstCorner = AcGePoint3d(dwg.corner1.x, dwg.corner1.y, z);
    solid.secondCorner = AcGePoint3d(dwg.corner2.x, dwg.corner2.y, z);
    solid.thirdCorner = AcGePoint3d(dwg.corner3.x, dwg.corner3.y, z);
    solid.fourthCorner = AcGePoint3d(dwg.corner4.x, dwg.corner4.y, z);
    return solid;
}

// 3DFACE keeps per-corner depth, so it lands in the same quad payload.
inline AcDbSolid toEntity(const Dwg_Entity__3DFACE &dwg)
{
    AcDbSolid solid;
    solid.firstCorner =
        AcGePoint3d(dwg.corner1.x, dwg.corner1.y, dwg.corner1.z);
    solid.secondCorner =
        AcGePoint3d(dwg.corner2.x, dwg.corner2.y, dwg.corner2.z);
    solid.thirdCorner =
        AcGePoint3d(dwg.corner3.x, dwg.corner3.y, dwg.corner3.z);
    solid.fourthCorner =
        AcGePoint3d(dwg.corner4.x, dwg.corner4.y, dwg.corner4.z);
    return solid;
}

// LWPOLYLINE: optimized 2D polyline; bulges align with the leading
// vertices (num_bulges <= num_points per the DWG coding).
inline AcDbPolyline toEntity(const Dwg_Entity_LWPOLYLINE &dwg)
{
    AcDbPolyline polyline;
    polyline.elevation = dwg.elevation;
    polyline.thickness = dwg.thickness;
    polyline.closed = (dwg.flag & 512) != 0;
    polyline.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                   dwg.extrusion.z);
    polyline.vertices.reserve(dwg.num_points);
    for (BITCODE_BL i = 0; i < dwg.num_points; ++i)
        polyline.vertices.push_back(
            AcGePoint2d(dwg.points[i].x, dwg.points[i].y));
    polyline.bulges.assign(dwg.num_points, 0.0);
    const BITCODE_BL bulgeCount =
        std::min<BITCODE_BL>(dwg.num_bulges, dwg.num_points);
    for (BITCODE_BL i = 0; i < bulgeCount; ++i)
        polyline.bulges[i] = dwg.bulges[i];
    return polyline;
}

inline AcDbText toEntity(const Dwg_Entity_TEXT &dwg)
{
    AcDbText text;
    text.insertion = AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.elevation);
    text.height = dwg.height;
    text.rotation = dwg.rotation;
    text.text = arxcompat::fromUtf8(dwg.text_value ? dwg.text_value : "");
    return text;
}

inline AcDbMText toEntity(const Dwg_Entity_MTEXT &dwg)
{
    AcDbMText mtext;
    mtext.insertion =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.ins_pt.z);
    mtext.direction = AcGeVector3d(dwg.x_axis_dir.x, dwg.x_axis_dir.y,
                                   dwg.x_axis_dir.z);
    mtext.width = dwg.rect_width;
    mtext.height = dwg.text_height;
    mtext.text = arxcompat::fromUtf8(dwg.text ? dwg.text : "");
    return mtext;
}

inline AcDbSpline toEntity(const Dwg_Entity_SPLINE &dwg)
{
    AcDbSpline spline;
    spline.degree = dwg.degree > 0 ? dwg.degree : 3;
    spline.closed = dwg.closed_b;
    spline.controlPoints.reserve(dwg.num_ctrl_pts);
    for (BITCODE_BL i = 0; i < dwg.num_ctrl_pts; ++i)
        spline.controlPoints.push_back(
            AcGePoint3d(dwg.ctrl_pts[i].x, dwg.ctrl_pts[i].y,
                        dwg.ctrl_pts[i].z));
    spline.knots.reserve(dwg.num_knots);
    for (BITCODE_BL i = 0; i < dwg.num_knots; ++i)
        spline.knots.push_back(dwg.knots[i]);
    spline.fitPoints.reserve(dwg.num_fit_pts);
    for (BITCODE_BL i = 0; i < dwg.num_fit_pts; ++i)
        spline.fitPoints.push_back(AcGePoint3d(dwg.fit_pts[i].x,
                                               dwg.fit_pts[i].y,
                                               dwg.fit_pts[i].z));
    return spline;
}

inline AcDbMline toEntity(const Dwg_Entity_MLINE &dwg)
{
    AcDbMline mline;
    mline.scale = AcGeVector3d(dwg.scale, dwg.scale, dwg.scale);
    mline.closed = (dwg.flags & 1) != 0;
    mline.vertices.reserve(dwg.num_verts);
    for (BITCODE_BS i = 0; i < dwg.num_verts; ++i)
        mline.vertices.push_back(
            AcGePoint3d(dwg.verts[i].vertex.x, dwg.verts[i].vertex.y,
                        dwg.verts[i].vertex.z));
    return mline;
}

// INSERT; the referenced record name resolves through the DWG block
// header handle, so the database-owning caller fills blockTableRecordName.
inline AcDbBlockReference toEntity(const Dwg_Entity_INSERT &dwg)
{
    AcDbBlockReference reference;
    reference.position =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.ins_pt.z);
    reference.scale = AcGeVector3d(dwg.scale.x, dwg.scale.y, dwg.scale.z);
    reference.rotation = dwg.rotation;
    reference.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                    dwg.extrusion.z);
    return reference;
}

// MINSERT shares the INSERT placement fields plus the row/column grid.
inline AcDbMInsertBlock toEntity(const Dwg_Entity_MINSERT &dwg)
{
    AcDbMInsertBlock insert;
    insert.position =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.ins_pt.z);
    insert.scale = AcGeVector3d(dwg.scale.x, dwg.scale.y, dwg.scale.z);
    insert.rotation = dwg.rotation;
    insert.normal =
        AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y, dwg.extrusion.z);
    insert.rowCount = dwg.num_rows;
    insert.columnCount = dwg.num_cols;
    insert.rowSpacing = dwg.row_spacing;
    insert.columnSpacing = dwg.col_spacing;
    return insert;
}

// ATTDEF: the block-resident attribute template.  Text alignment modes
// (horiz/vert alignment) beyond left-baseline are out of scope; the
// insertion point serves as the layout origin.
inline AcDbAttributeDefinition toEntity(const Dwg_Entity_ATTDEF &dwg)
{
    AcDbAttributeDefinition definition;
    definition.insertion =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.elevation);
    definition.height = dwg.height;
    definition.rotation = dwg.rotation;
    definition.widthFactor = dwg.width_factor;
    definition.obliqueAngle = dwg.oblique_angle;
    definition.tag = dwg.tag ? dwg.tag : "";
    definition.prompt = dwg.prompt ? dwg.prompt : "";
    definition.textValue = dwg.default_value ? dwg.default_value : "";
    definition.invisible = (dwg.flags & 1) != 0;
    definition.constant = (dwg.flags & 2) != 0;
    definition.verify = (dwg.flags & 4) != 0;
    definition.preset = (dwg.flags & 8) != 0;
    return definition;
}

// ATTRIB: the stamped value attached to an INSERT.
inline AcDbAttribute toEntity(const Dwg_Entity_ATTRIB &dwg)
{
    AcDbAttribute attribute;
    attribute.insertion =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.elevation);
    attribute.height = dwg.height;
    attribute.rotation = dwg.rotation;
    attribute.widthFactor = dwg.width_factor;
    attribute.tag = dwg.tag ? dwg.tag : "";
    attribute.textValue = dwg.text_value ? dwg.text_value : "";
    attribute.invisible = (dwg.flags & 1) != 0;
    return attribute;
}

// LEADER: vertex path plus the arrow/hook switches.
inline AcDbLeader toEntity(const Dwg_Entity_LEADER &dwg)
{
    AcDbLeader leader;
    leader.points.reserve(dwg.num_points);
    for (BITCODE_BL i = 0; i < dwg.num_points && dwg.points != nullptr; ++i)
        leader.points.push_back(
            AcGePoint3d(dwg.points[i].x, dwg.points[i].y, dwg.points[i].z));
    leader.splinePath = dwg.path_type == LEADER_PATHTYPE_SPLINE;
    leader.hasArrowhead = dwg.arrowhead_on != 0;
    leader.hasHookLine = dwg.hookline_on != 0;
    return leader;
}

inline std::vector<AcDbEntityVariant> toEntities(
    const Dwg_Entity_MULTILEADER &dwg)
{
    std::vector<AcDbEntityVariant> entities;
    const Dwg_MLEADER_AnnotContext &context = dwg.ctx;
    for (BITCODE_BL i = 0; i < context.num_leaders && context.leaders != nullptr;
         ++i)
    {
        const Dwg_LEADER_Node &node = context.leaders[i];
        for (BITCODE_BL j = 0; j < node.num_lines && node.lines != nullptr; ++j)
        {
            const Dwg_LEADER_Line &line = node.lines[j];
            if (line.points == nullptr || line.num_points < 2)
                continue;
            AcDbLeader leader;
            leader.points.reserve(line.num_points);
            for (BITCODE_BL k = 0; k < line.num_points; ++k)
                leader.points.emplace_back(line.points[k].x, line.points[k].y,
                                           line.points[k].z);
            leader.splinePath = line.type == 2;
            leader.hasArrowhead = context.arrow_size > 0.0;
            entities.emplace_back(std::move(leader));
        }
    }

    if (context.has_content_txt && context.content.txt.default_text != nullptr &&
        *context.content.txt.default_text != '\0')
    {
        const Dwg_MLEADER_Content_MText &content = context.content.txt;
        AcDbMText text;
        text.insertion = AcGePoint3d(content.location.x, content.location.y,
                                     content.location.z);
        text.direction = AcGeVector3d(content.direction.x, content.direction.y,
                                      content.direction.z);
        text.width = content.width;
        text.height = context.text_height > 0.0
                          ? context.text_height
                          : content.height;
        text.text = arxcompat::fromUtf8(content.default_text);
        entities.emplace_back(std::move(text));
    }
    return entities;
}

// TOLERANCE: the feature control frame.
inline AcDbFcf toEntity(const Dwg_Entity_TOLERANCE &dwg)
{
    AcDbFcf fcf;
    fcf.insertion = AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.ins_pt.z);
    fcf.direction = AcGeVector3d(dwg.x_direction.x, dwg.x_direction.y,
                                 dwg.x_direction.z);
    fcf.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                              dwg.extrusion.z);
    fcf.text = dwg.text_value ? dwg.text_value : "";
    fcf.height = dwg.height > 0.0 ? dwg.height : 1.0;
    return fcf;
}

// SHAPE: placement + the STYLE record that carries the shape file.
inline AcDbShape toEntity(const Dwg_Entity_SHAPE &dwg)
{
    AcDbShape shape;
    shape.insertion =
        AcGePoint3d(dwg.ins_pt.x, dwg.ins_pt.y, dwg.ins_pt.z);
    shape.scale = dwg.scale;
    shape.rotation = dwg.rotation;
    shape.widthFactor = dwg.width_factor;
    shape.obliqueAngle = dwg.oblique_angle;
    shape.thickness = dwg.thickness;
    return shape;
}

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

inline AcDbRotatedDimension toEntity(const Dwg_Entity_DIMENSION_LINEAR &dwg)
{
    AcDbRotatedDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.xLine1Point =
        AcGePoint3d(dwg.xline1_pt.x, dwg.xline1_pt.y, dwg.xline1_pt.z);
    dimension.xLine2Point =
        AcGePoint3d(dwg.xline2_pt.x, dwg.xline2_pt.y, dwg.xline2_pt.z);
    dimension.dimensionLinePoint =
        AcGePoint3d(dwg.def_pt.x, dwg.def_pt.y, dwg.def_pt.z);
    dimension.dimensionLineAngle = dwg.dim_rotation;
    dimension.obliqueAngle = dwg.oblique_angle;
    return dimension;
}

inline AcDbAlignedDimension toEntity(const Dwg_Entity_DIMENSION_ALIGNED &dwg)
{
    AcDbAlignedDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.xLine1Point =
        AcGePoint3d(dwg.xline1_pt.x, dwg.xline1_pt.y, dwg.xline1_pt.z);
    dimension.xLine2Point =
        AcGePoint3d(dwg.xline2_pt.x, dwg.xline2_pt.y, dwg.xline2_pt.z);
    dimension.dimensionLinePoint =
        AcGePoint3d(dwg.def_pt.x, dwg.def_pt.y, dwg.def_pt.z);
    dimension.obliqueAngle = dwg.oblique_angle;
    return dimension;
}

inline AcDb2LineAngularDimension toEntity(
    const Dwg_Entity_DIMENSION_ANG2LN &dwg)
{
    AcDb2LineAngularDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.xLine1StartPoint = AcGePoint3d(dwg.xline1start_pt.x,
                                             dwg.xline1start_pt.y,
                                             dwg.xline1start_pt.z);
    dimension.xLine1EndPoint = AcGePoint3d(dwg.xline1end_pt.x,
                                           dwg.xline1end_pt.y,
                                           dwg.xline1end_pt.z);
    dimension.xLine2StartPoint = AcGePoint3d(dwg.xline2start_pt.x,
                                             dwg.xline2start_pt.y,
                                             dwg.xline2start_pt.z);
    dimension.xLine2EndPoint = AcGePoint3d(dwg.xline2end_pt.x,
                                           dwg.xline2end_pt.y,
                                           dwg.xline2end_pt.z);
    dimension.arcPoint =
        AcGePoint3d(dwg.def_pt.x, dwg.def_pt.y, dwg.def_pt.z);
    return dimension;
}

inline AcDb3PointAngularDimension toEntity(
    const Dwg_Entity_DIMENSION_ANG3PT &dwg)
{
    AcDb3PointAngularDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.centerPoint =
        AcGePoint3d(dwg.center_pt.x, dwg.center_pt.y, dwg.center_pt.z);
    dimension.xLine1Point =
        AcGePoint3d(dwg.xline1_pt.x, dwg.xline1_pt.y, dwg.xline1_pt.z);
    dimension.xLine2Point =
        AcGePoint3d(dwg.xline2_pt.x, dwg.xline2_pt.y, dwg.xline2_pt.z);
    return dimension;
}

inline AcDbRadialDimension toEntity(const Dwg_Entity_DIMENSION_RADIUS &dwg)
{
    AcDbRadialDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.centerPoint =
        AcGePoint3d(dwg.def_pt.x, dwg.def_pt.y, dwg.def_pt.z);
    dimension.radiusPoint =
        AcGePoint3d(dwg.first_arc_pt.x, dwg.first_arc_pt.y,
                    dwg.first_arc_pt.z);
    dimension.leaderLength = dwg.leader_len;
    return dimension;
}

inline AcDbDiametricDimension toEntity(
    const Dwg_Entity_DIMENSION_DIAMETER &dwg)
{
    AcDbDiametricDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    // libredwg's data model (and adder) stores the far chord point in
    // def_pt and the near one in first_arc_pt; mirror that so a written
    // file reads back identically.
    dimension.chordPoint =
        AcGePoint3d(dwg.first_arc_pt.x, dwg.first_arc_pt.y,
                    dwg.first_arc_pt.z);
    dimension.farChordPoint =
        AcGePoint3d(dwg.def_pt.x, dwg.def_pt.y, dwg.def_pt.z);
    dimension.leaderLength = dwg.leader_len;
    return dimension;
}

inline AcDbOrdinateDimension toEntity(
    const Dwg_Entity_DIMENSION_ORDINATE &dwg)
{
    AcDbOrdinateDimension dimension;
    fillDimensionCommon(dimension.dimension, dwg);
    dimension.featureLocationPoint = AcGePoint3d(dwg.feature_location_pt.x,
                                                 dwg.feature_location_pt.y,
                                                 dwg.feature_location_pt.z);
    dimension.leaderEndPoint =
        AcGePoint3d(dwg.leader_endpt.x, dwg.leader_endpt.y,
                    dwg.leader_endpt.z);
    // DWG decode carries the axis bit in flag2; the DXF path leaves
    // flag2 zero and records bit 0x80 in group 70's flag (libredwg's
    // own decoder convention; bit 64 is the DXF spec spelling).
    dimension.useXAxis = (dwg.flag2 & 1) != 0 ||
                         (dwg.flag & 0x80) != 0 || (dwg.flag & 64) != 0;
    return dimension;
}

// HATCH: only polyline boundary paths map onto the simplified acdb::
// payload; edge-defined (spline/arc) loops are counted unsupported.
// Returns nullopt when no boundary survived.
inline std::optional<AcDbHatch> tryToEntity(const Dwg_Entity_HATCH &dwg)
{
    AcDbHatch hatch;
    hatch.solidFill = dwg.is_solid_fill;
    hatch.patternName = dwg.name ? dwg.name : "SOLID";
    hatch.patternScale = dwg.scale_spacing;
    hatch.patternAngle = dwg.angle;
    bool outerTaken = false;
    for (BITCODE_BL i = 0; i < dwg.num_paths; ++i)
    {
        const Dwg_HATCH_Path &path = dwg.paths[i];
        if ((path.flag & 2) == 0)
            continue; // edge-defined path (arc/spline/ellipse loops)
        std::vector<AcGePoint3d> loop;
        loop.reserve(path.num_segs_or_paths);
        for (BITCODE_BL j = 0; j < path.num_segs_or_paths; ++j)
            loop.push_back(
                AcGePoint3d(path.polyline_paths[j].point.x,
                            path.polyline_paths[j].point.y,
                            dwg.elevation));
        if (loop.size() < 2)
            continue;
        if (!outerTaken)
        {
            hatch.outerLoop = std::move(loop);
            outerTaken = true;
        }
        else
        {
            hatch.innerLoops.push_back(std::move(loop));
        }
    }
    if (!outerTaken)
        return std::nullopt;
    return hatch;
}

// ---- POLYLINE children (VERTEX_* owned-handle chain) ----
//
// POLYLINE_2D/3D/PFACE/MESH share the COMMON_ENTITY_POLYLINE prologue
// (num_owned + vertex[]), but the union member is typed per entity, so
// the accessor switches on the fixed type.

inline const Dwg_Object_Ref *polylineVertexRef(const Dwg_Object &polyline,
                                               BITCODE_BL index)
{
    const Dwg_Object_Entity &ent = *polyline.tio.entity;
    const Dwg_Object_Ref *ref = nullptr;
    switch (polyline.fixedtype)
    {
    case DWG_TYPE_POLYLINE_2D:
        if (index < ent.tio.POLYLINE_2D->num_owned)
            ref = ent.tio.POLYLINE_2D->vertex[index];
        break;
    case DWG_TYPE_POLYLINE_3D:
        if (index < ent.tio.POLYLINE_3D->num_owned)
            ref = ent.tio.POLYLINE_3D->vertex[index];
        break;
    case DWG_TYPE_POLYLINE_PFACE:
        if (index < ent.tio.POLYLINE_PFACE->num_owned)
            ref = ent.tio.POLYLINE_PFACE->vertex[index];
        break;
    case DWG_TYPE_POLYLINE_MESH:
        if (index < ent.tio.POLYLINE_MESH->num_owned)
            ref = ent.tio.POLYLINE_MESH->vertex[index];
        break;
    default:
        break;
    }
    return ref;
}

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
inline AcDbPolyline polyline2dFromChildren(const DwgImportContext &ctx,
                                           const Dwg_Object &polyline)
{
    AcDbPolyline result;
    const Dwg_Entity_POLYLINE_2D &poly =
        *polyline.tio.entity->tio.POLYLINE_2D;
    result.closed = (poly.flag & 1) != 0;
    result.elevation = poly.elevation;
    result.thickness = poly.thickness;
    result.normal = AcGeVector3d(poly.extrusion.x, poly.extrusion.y,
                                 poly.extrusion.z);
    forEachPolylineChild(ctx, polyline,
                          [&](const Dwg_Object &vertex) {
        if (vertex.fixedtype != DWG_TYPE_VERTEX_2D ||
            vertex.tio.entity == nullptr ||
            vertex.tio.entity->tio.VERTEX_2D == nullptr)
            return;
        const Dwg_Entity_VERTEX_2D &v =
            *vertex.tio.entity->tio.VERTEX_2D;
        result.vertices.push_back(AcGePoint2d(v.point.x, v.point.y));
        result.bulges.push_back(v.bulge);
    });
    return result;
}

// 3D POLYLINE: straight 3D segments; DXF flag 70 bit 32 = closed.
inline AcDb3dPolyline polyline3dFromChildren(const DwgImportContext &ctx,
                                             const Dwg_Object &polyline)
{
    AcDb3dPolyline result;
    const Dwg_Entity_POLYLINE_3D &poly =
        *polyline.tio.entity->tio.POLYLINE_3D;
    result.closed = (poly.flag & FLAG_POLYLINE_CLOSED) != 0; // bit 1
    forEachPolylineChild(ctx, polyline,
                          [&](const Dwg_Object &vertex) {
        if (vertex.fixedtype != DWG_TYPE_VERTEX_3D ||
            vertex.tio.entity == nullptr ||
            vertex.tio.entity->tio.VERTEX_3D == nullptr)
            return;
        const Dwg_Entity_VERTEX_3D &v =
            *vertex.tio.entity->tio.VERTEX_3D;
        result.vertices.push_back(AcGePoint3d(v.point.x, v.point.y,
                                              v.point.z));
    });
    return result;
}

// Polyface mesh: numverts VERTEX_PFACE positions followed by numfaces
// VERTEX_PFACE_FACE records (1-based vertind, 0 = absent corner,
// negative = invisible edge).  Faces triangulate as (a,b,c)+(a,c,d).
inline std::optional<AcDbPolyFaceMesh> pfaceFromChildren(
    const DwgImportContext &ctx, const Dwg_Object &polyline)
{
    AcDbPolyFaceMesh mesh;
    mesh.style = MeshStyle::Cad;
    BITCODE_BL positionCount = 0;
    forEachPolylineChild(ctx, polyline,
                          [&](const Dwg_Object &vertex) {
        if (vertex.tio.entity == nullptr)
            return;
        if (vertex.fixedtype == DWG_TYPE_VERTEX_PFACE &&
            vertex.tio.entity->tio.VERTEX_PFACE)
        {
            const Dwg_Entity_VERTEX_3D &v =
                *vertex.tio.entity->tio.VERTEX_PFACE;
            mesh.geometry.positions.push_back(
                AcGePoint3d(v.point.x, v.point.y, v.point.z));
        }
        else if (vertex.fixedtype == DWG_TYPE_VERTEX_PFACE_FACE &&
                 vertex.tio.entity->tio.VERTEX_PFACE_FACE)
        {
            // vertind is BITCODE_BS[4] (unsigned; 0 = absent corner, the
            // spec's negative invisible-edge flag maps to a signed view).
            const BITCODE_BSd *ind = reinterpret_cast<const BITCODE_BSd *>(
                vertex.tio.entity->tio.VERTEX_PFACE_FACE->vertind);
            const std::uint32_t a = ind[0] > 0 ? std::uint32_t(ind[0] - 1) : 0;
            const std::uint32_t b = ind[1] > 0 ? std::uint32_t(ind[1] - 1) : 0;
            const std::uint32_t c = ind[2] > 0 ? std::uint32_t(ind[2] - 1) : 0;
            const std::uint32_t d = ind[3] > 0 ? std::uint32_t(ind[3] - 1) : 0;
            if (ind[0] > 0 && ind[1] > 0 && ind[2] > 0)
            {
                mesh.geometry.indices.insert(
                    mesh.geometry.indices.end(), {a, b, c});
            }
            if (ind[3] > 0 && ind[0] > 0 && ind[2] > 0)
            {
                mesh.geometry.indices.insert(
                    mesh.geometry.indices.end(), {a, c, d});
            }
        }
    });
    positionCount = std::uint32_t(mesh.geometry.positions.size());
    if (positionCount == 0 || mesh.geometry.indices.empty())
        return std::nullopt;
    for (const std::uint32_t index : mesh.geometry.indices)
        if (index >= positionCount)
            return std::nullopt; // dangling face index: corrupt source
    return mesh;
}

// ---- common entity properties ----

inline void ensureLayer(AcDbDatabase &database, const std::string &name)
{
    if (name.empty() || database.layerTable().contains(name))
        return;
    database.layerTable().add(name, database.allocateHandle());
}

inline void ensureLineType(AcDbDatabase &database, const std::string &name)
{
    if (name.empty() || database.linetypeTable().contains(name))
        return;
    database.linetypeTable().add(name, database.allocateHandle());
}

inline void ensureTextStyle(AcDbDatabase &database, const std::string &name)
{
    if (name.empty() || database.textStyleTable().contains(name))
        return;
    database.textStyleTable().add(name, database.allocateHandle());
}

// Layer records import first so entity colors can resolve ByLayer through
// the real record and layers keep their ACI.
inline void importLayerRecords(AcDbDatabase &database, const Dwg_Data &dwg)
{
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        const Dwg_Object &object = dwg.object[i];
        if (object.supertype != DWG_SUPERTYPE_OBJECT ||
            object.fixedtype != DWG_TYPE_LAYER || object.tio.object == nullptr)
            continue;
        const Dwg_Object_LAYER &layer = *object.tio.object->tio.LAYER;
        if (layer.name == nullptr)
            continue;
        const std::string name = layer.name;
        if (name.empty())
            continue;
        AcDbLayerTableRecord *record = database.layerTable().getMutable(name);
        if (record == nullptr)
            record = &database.layerTable().add(name, database.allocateHandle());
        const int aci = static_cast<int>(layer.color.index);
        record->color = layer.color.method == 0xc3
                            ? dwgColorToEntityColor(layer.color).renderColor()
                            : (aci == 256 || aci == 0)
                                  ? aciToColor(7)
                                  : dwgColorToColor(layer.color);
        record->lineWeight = dwgLineWeightToPixels(layer.linewt);
        record->isOff = aci < 0; // CMC: negative index = layer off
        record->isFrozen = layer.frozen;
        record->isPlottable = layer.plotflag != 0;
        ensureLineType(database, layer.ltype && layer.ltype->obj
                                     ? std::string(symbolTableName(
                                           layer.ltype->obj, DWG_TYPE_LTYPE))
                                     : std::string());
    }
}

// Fills the common-properties block from the DWG entity common data:
// layer, semantic entity color, resolved display color,
// linetype, visibility, and the display name.
template <typename Payload>
void applyCommon(Payload &payload, const DwgImportContext &ctx,
                 const Dwg_Object &object, AcDbDatabase &database)
{
    const Dwg_Object_Entity &ent = *object.tio.entity;
    AcDbEntity &common = payload.common;

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
inline void repairBlockReferences(const Dwg_Data &dwg)
{
    Dwg_Data &mutableDwg = const_cast<Dwg_Data &>(dwg);
    Dwg_Object_BLOCK_CONTROL *control = nullptr;
    std::vector<Dwg_Object *> headers;
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        Dwg_Object &object = mutableDwg.object[i];
        if (object.supertype != DWG_SUPERTYPE_OBJECT ||
            object.tio.object == nullptr)
            continue;
        if (object.fixedtype == DWG_TYPE_BLOCK_CONTROL)
            control = object.tio.object->tio.BLOCK_CONTROL;
        else if (object.fixedtype == DWG_TYPE_BLOCK_HEADER)
            headers.push_back(&object);
    }
    if (control == nullptr || headers.empty())
        return;

    // Rebuild the control entry table (the previous array is owned by
    // the decode and leaks here; dwg_free releases the replacement).
    control->num_entries = BITCODE_BL(headers.size());
    control->entries = (BITCODE_H *)calloc(headers.size(),
                                           sizeof(BITCODE_H));
    if (control->entries == nullptr)
        return;
    for (std::size_t i = 0; i < headers.size(); ++i)
        control->entries[i] = dwg_add_handleref(
            &mutableDwg, 5, headers[i]->handle.value, NULL);

    // Re-link dangling INSERT/MINSERT references through the headers'
    // inserts[] lists.
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        Dwg_Object &object = mutableDwg.object[i];
        if (object.supertype != DWG_SUPERTYPE_ENTITY ||
            object.tio.entity == nullptr)
            continue;
        if (object.fixedtype != DWG_TYPE_INSERT &&
            object.fixedtype != DWG_TYPE_MINSERT)
            continue;
        Dwg_Object_Ref *blockHeader =
            object.fixedtype == DWG_TYPE_INSERT
                ? object.tio.entity->tio.INSERT->block_header
                : object.tio.entity->tio.MINSERT->block_header;
        // The header whose inserts[] list carries this reference is
        // authoritative: in_dxf's deferred group-2 resolution can link
        // the wrong record when the BLOCK_CONTROL entries collapse onto
        // one handle, so it overrides a pre-linked block_header.
        Dwg_Object *referencedBy = nullptr;
        for (Dwg_Object *header : headers)
        {
            const Dwg_Object_BLOCK_HEADER &record =
                *header->tio.object->tio.BLOCK_HEADER;
            for (BITCODE_BL j = 0; j < record.num_inserts; ++j)
            {
                if (record.inserts == nullptr ||
                    record.inserts[j] == nullptr)
                    continue;
                if (record.inserts[j]->absolute_ref != object.handle.value)
                    continue;
                if (blockHeader == nullptr ||
                    blockHeader->absolute_ref != header->handle.value)
                    referencedBy = header;
                break;
            }
            if (referencedBy != nullptr)
                break;
        }
        if (referencedBy == nullptr)
            continue;
        const Dwg_Object_Ref *link = dwg_add_handleref(
            &mutableDwg, 5, referencedBy->handle.value, NULL);
        if (object.fixedtype == DWG_TYPE_INSERT)
            object.tio.entity->tio.INSERT->block_header =
                const_cast<Dwg_Object_Ref *>(link);
        else
            object.tio.entity->tio.MINSERT->block_header =
                const_cast<Dwg_Object_Ref *>(link);
    }
}

// ---- direct fill: DWG/DXF file -> AcDbDatabase (ARX readDwgFile shape) ----

struct AcDbDwgImportReport
{
    std::size_t added = 0;
    std::size_t unsupported = 0;
};

// Walks every DWG object, converts the supported entity set through
// toEntity(), routes model/paper-space entities into the database and
// rebuilds user block records from their owned members.  Unsupported
// entity types are skipped and counted (failsafe-reader semantics).
inline AcDbDwgImportReport addDwgEntities(AcDbDatabase &database,
                                          const Dwg_Data &dwg)
{
    AcDbDwgImportReport report;
    repairBlockReferences(dwg);
    const DwgImportContext ctx(dwg);
    importLayerRecords(database, dwg);

    // Deferred block members keyed by the owning BLOCK_HEADER handle;
    // records are created in a second pass so duplicate names merge and
    // base points arrive before members.  The file handle rides along so
    // INSERT/MINSERT members can join the attribute-attach map.
    struct DeferredMember
    {
        std::uint64_t fileHandle = 0;
        AcDbEntityVariant payload;
    };
    std::unordered_map<std::uint64_t, std::vector<DeferredMember>>
        blockMembers;
    // ATTRIB payloads parked by owning INSERT/MINSERT file handle.
    std::unordered_map<std::uint64_t, std::vector<AcDbAttribute>>
        attributesByOwner;
    // INSERT/MINSERT file handle -> database handle (both spaces).
    std::unordered_map<std::uint64_t, AcDbHandle> insertDbHandles;

    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        const Dwg_Object &object = dwg.object[i];
        if (object.supertype == DWG_SUPERTYPE_OBJECT)
            continue; // table records handled above / by the block pass
        if (object.supertype != DWG_SUPERTYPE_ENTITY || !object.tio.entity)
            continue;

        const Dwg_Object_Entity &ent = *object.tio.entity;

        // Structural companions of polylines and blocks are not entities
        // in the acdb model; ATTRIBs park for the attach pass instead.
        switch (object.fixedtype)
        {
        case DWG_TYPE_VERTEX_2D:
        case DWG_TYPE_VERTEX_3D:
        case DWG_TYPE_VERTEX_PFACE:
        case DWG_TYPE_VERTEX_PFACE_FACE:
        case DWG_TYPE_SEQEND:
        case DWG_TYPE_BLOCK:
        case DWG_TYPE_ENDBLK:
        case DWG_TYPE__3DLINE:
            continue;
        default:
            break;
        }

        // ATTRIB values park under their owning INSERT/MINSERT file
        // handle; the attach pass after the main loop links them into the
        // reference's attributeHandles (binary decodes carry the ATTRIB's
        // ownerhandle but not the insert's attribs[] chain).
        if (object.fixedtype == DWG_TYPE_ATTRIB)
        {
            if (ent.ownerhandle == nullptr || ent.tio.ATTRIB == nullptr)
            {
                ++report.unsupported;
                continue;
            }
            AcDbAttribute attribute = toEntity(*ent.tio.ATTRIB);
            applyCommon(attribute, ctx, object, database);
            if (const char *style =
                    symbolTableName(ctx.resolve(ent.tio.ATTRIB->style),
                                    DWG_TYPE_STYLE);
                style != nullptr && *style != '\0')
            {
                ensureTextStyle(database, style);
                attribute.styleName = style;
            }
            attributesByOwner[ent.ownerhandle->absolute_ref].push_back(
                std::move(attribute));
            continue;
        }

        // nullopt (not an empty variant — that would default-construct an
        // AcDbLine) marks "type unsupported".
        std::optional<AcDbEntityVariant> converted;
        std::vector<AcDbEntityVariant> additional;
        switch (object.type)
        {
        case DWG_TYPE_LINE:
            converted = toEntity(*ent.tio.LINE);
            break;
        case DWG_TYPE_ARC:
            converted = toEntity(*ent.tio.ARC);
            break;
        case DWG_TYPE_CIRCLE:
            converted = toEntity(*ent.tio.CIRCLE);
            break;
        case DWG_TYPE_ELLIPSE:
            converted = toEntity(*ent.tio.ELLIPSE);
            break;
        case DWG_TYPE_POINT:
            converted = toEntity(*ent.tio.POINT);
            break;
        case DWG_TYPE_RAY:
            converted = toEntity(*ent.tio.RAY);
            break;
        case DWG_TYPE_XLINE:
        {
            const AcDbRay ray = toEntity(*ent.tio.RAY);
            AcDbXline xline;
            xline.point = ray.start;
            xline.direction = ray.direction;
            converted = std::move(xline);
            break;
        }
        case DWG_TYPE_SOLID:
            converted = toEntity(*ent.tio.SOLID);
            break;
        case DWG_TYPE__3DFACE:
            converted = toEntity(*ent.tio._3DFACE);
            break;
        case DWG_TYPE_LWPOLYLINE:
            converted = toEntity(*ent.tio.LWPOLYLINE);
            break;
        case DWG_TYPE_POLYLINE_2D:
            converted = polyline2dFromChildren(ctx, object);
            break;
        case DWG_TYPE_POLYLINE_3D:
            converted = polyline3dFromChildren(ctx, object);
            break;
        case DWG_TYPE_POLYLINE_PFACE:
            converted = pfaceFromChildren(ctx, object);
            if (!converted)
                ++report.unsupported;
            break;
        case DWG_TYPE_POLYLINE_MESH:
            // MESH vertices arrive as VERTEX_* children; without a
            // decoded mesh grid the entity carries no picture.
            ++report.unsupported;
            break;
        case DWG_TYPE_SPLINE:
            converted = toEntity(*ent.tio.SPLINE);
            break;
        case DWG_TYPE_TEXT:
            converted = toEntity(*ent.tio.TEXT);
            break;
        case DWG_TYPE_MTEXT:
            converted = toEntity(*ent.tio.MTEXT);
            break;
        case DWG_TYPE_MLINE:
            converted = toEntity(*ent.tio.MLINE);
            break;
        case DWG_TYPE_HATCH:
            converted = tryToEntity(*ent.tio.HATCH);
            if (!converted)
                ++report.unsupported;
            break;
        case DWG_TYPE_INSERT:
        {
            if (const char *name =
                    blockHeaderNameOf(ctx, ent.tio.INSERT->block_header))
            {
                AcDbBlockReference reference = toEntity(*ent.tio.INSERT);
                reference.blockTableRecordName = name;
                converted = std::move(reference);
            }
            else
            {
                ++report.unsupported; // dangling block reference
            }
            break;
        }
        case DWG_TYPE_MINSERT:
        {
            if (const char *name =
                    blockHeaderNameOf(ctx, ent.tio.MINSERT->block_header))
            {
                AcDbMInsertBlock insert = toEntity(*ent.tio.MINSERT);
                insert.blockTableRecordName = name;
                converted = std::move(insert);
            }
            else
            {
                ++report.unsupported;
            }
            break;
        }
        case DWG_TYPE_ATTDEF:
            converted = toEntity(*ent.tio.ATTDEF);
            break;
        case DWG_TYPE_LEADER:
            converted = toEntity(*ent.tio.LEADER);
            break;
        case DWG_TYPE_MULTILEADER:
        {
            std::vector<AcDbEntityVariant> parts =
                toEntities(*ent.tio.MULTILEADER);
            if (!parts.empty())
            {
                converted = std::move(parts.front());
                for (std::size_t part = 1; part < parts.size(); ++part)
                    additional.push_back(std::move(parts[part]));
            }
            else
            {
                ++report.unsupported;
            }
            break;
        }
        case DWG_TYPE_TOLERANCE:
            converted = toEntity(*ent.tio.TOLERANCE);
            break;
        case DWG_TYPE_SHAPE:
        {
            AcDbShape shape = toEntity(*ent.tio.SHAPE);
            if (const char *style =
                    symbolTableName(ctx.resolve(ent.tio.SHAPE->style),
                                    DWG_TYPE_STYLE);
                style != nullptr && *style != '\0')
            {
                ensureTextStyle(database, style);
                shape.shapeName = style;
            }
            converted = std::move(shape);
            break;
        }
        case DWG_TYPE_DIMENSION_LINEAR:
        {
            AcDbRotatedDimension dimension =
                toEntity(*ent.tio.DIMENSION_LINEAR);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_LINEAR);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_ALIGNED:
        {
            AcDbAlignedDimension dimension =
                toEntity(*ent.tio.DIMENSION_ALIGNED);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_ALIGNED);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_ANG2LN:
        {
            AcDb2LineAngularDimension dimension =
                toEntity(*ent.tio.DIMENSION_ANG2LN);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_ANG2LN);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_ANG3PT:
        {
            AcDb3PointAngularDimension dimension =
                toEntity(*ent.tio.DIMENSION_ANG3PT);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_ANG3PT);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_RADIUS:
        {
            AcDbRadialDimension dimension =
                toEntity(*ent.tio.DIMENSION_RADIUS);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_RADIUS);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_DIAMETER:
        {
            AcDbDiametricDimension dimension =
                toEntity(*ent.tio.DIMENSION_DIAMETER);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_DIAMETER);
            converted = std::move(dimension);
            break;
        }
        case DWG_TYPE_DIMENSION_ORDINATE:
        {
            AcDbOrdinateDimension dimension =
                toEntity(*ent.tio.DIMENSION_ORDINATE);
            applyDimensionStyle(ctx, dimension.dimension,
                                *ent.tio.DIMENSION_ORDINATE);
            converted = std::move(dimension);
            break;
        }
        default:
            ++report.unsupported;
            break;
        }
        if (!converted)
            continue;
        AcDbEntityVariant payload = std::move(*converted);

        std::visit(
            [&](auto &typed) { applyCommon(typed, ctx, object, database); },
            payload);
        for (AcDbEntityVariant &part : additional)
            std::visit(
                [&](auto &typed) { applyCommon(typed, ctx, object, database); },
                part);

        // TEXT carries its style by record name; MTEXT's payload has no
        // style slot, so its style falls back to the database default.
        // ATTDEF resolves its text style the same way.
        const Dwg_Object_Ref *entityStyleRef = nullptr;
        switch (object.fixedtype)
        {
        case DWG_TYPE_TEXT: entityStyleRef = ent.tio.TEXT->style; break;
        case DWG_TYPE_ATTDEF: entityStyleRef = ent.tio.ATTDEF->style; break;
        default: break;
        }
        if (const char *style = entityStyleRef != nullptr
                                    ? symbolTableName(
                                          ctx.resolve(entityStyleRef),
                                          DWG_TYPE_STYLE)
                                    : nullptr;
            style != nullptr && *style != '\0')
        {
            ensureTextStyle(database, style);
            if (auto *text = std::get_if<AcDbText>(&payload))
                text->styleName = style;
            else if (auto *definition =
                         std::get_if<AcDbAttributeDefinition>(&payload))
                definition->styleName = style;
        }

        // Membership: entmode 1 = PSPACE, 2 = MSPACE (implicit owner),
        // 3 = explicit ownerhandle; a DXF-sourced decode can leave
        // entmode 0 while still setting ownerhandle, so the ownerhandle
        // decides whenever it names a user block header.
        Dwg_Object_Ref *ownerHandle = ent.ownerhandle;
        const char *ownerBlockName =
            blockHeaderNameOf(ctx, ownerHandle);
        const bool ownedByBlock =
            ownerBlockName != nullptr &&
            !isModelOrPaperSpaceName(ownerBlockName);

        // Bulk-load path (insertLoadedEntity + record append): load-time
        // inserts stay silent by design; a fresh load mirrors in bulk.
        auto addToSpace = [&](AcDbEntityVariant &&resident,
                              AcDbBlockTableRecord &space) -> AcDbHandle {
            AcDbEntity &commonRef = std::visit(
                [](auto &typed) -> AcDbEntity & { return typed.common; },
                resident);
            commonRef.handle = database.allocateHandle();
            const AcDbHandle handle = commonRef.handle;
            if (database.insertLoadedEntity(std::move(resident)))
            {
                space.appendEntityHandle(handle);
                ++report.added;
                return handle;
            }
            return kNullHandle;
        };

        const bool isInsertLike = object.fixedtype == DWG_TYPE_INSERT ||
                                  object.fixedtype == DWG_TYPE_MINSERT;
        AcDbHandle insertedHandle = kNullHandle;
        if (ent.entmode == 1)
        {
            // Paper space record (created by the database constructor).
            if (AcDbBlockTableRecord *pspace =
                    database.blockTable().getMutable(kPaperSpaceName))
                insertedHandle = addToSpace(std::move(payload), *pspace);
        }
        else if (ownedByBlock && ownerHandle != nullptr)
        {
            blockMembers[ownerHandle->absolute_ref].push_back(
                {object.handle.value, std::move(payload)});
        }
        else
        {
            insertedHandle = addToSpace(std::move(payload),
                                        database.modelSpace());
        }
        for (AcDbEntityVariant &part : additional)
        {
            if (ent.entmode == 1)
            {
                if (AcDbBlockTableRecord *pspace =
                        database.blockTable().getMutable(kPaperSpaceName))
                    addToSpace(std::move(part), *pspace);
            }
            else if (ownedByBlock && ownerHandle != nullptr)
            {
                blockMembers[ownerHandle->absolute_ref].push_back(
                    {object.handle.value, std::move(part)});
            }
            else
            {
                addToSpace(std::move(part), database.modelSpace());
            }
        }
        if (insertedHandle.isValid() && isInsertLike)
            insertDbHandles[object.handle.value] = insertedHandle;
    }

    // Rebuild user block records in file order.
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        const Dwg_Object &object = dwg.object[i];
        if (object.supertype != DWG_SUPERTYPE_OBJECT ||
            object.fixedtype != DWG_TYPE_BLOCK_HEADER ||
            object.tio.object == nullptr)
            continue;
        const Dwg_Object_BLOCK_HEADER &header =
            *object.tio.object->tio.BLOCK_HEADER;
        if (header.name == nullptr ||
            isModelOrPaperSpaceName(header.name))
            continue;
        const std::string name = header.name;

        AcDbBlockTableRecord *record =
            database.blockTable().getMutable(name);
        if (record == nullptr)
        {
            const AcDbHandle recordHandle = database.allocateHandle();
            record = &database.blockTable().add(name, recordHandle);
            record->handle = recordHandle;
            record->setRecordName(name);
            record->setBasePoint(AcGePoint3d(header.base_pt.x,
                                             header.base_pt.y,
                                             header.base_pt.z));
        }
        for (DeferredMember &member : blockMembers[object.handle.value])
        {
            AcDbEntity &commonRef = std::visit(
                [](auto &typed) -> AcDbEntity & { return typed.common; },
                member.payload);
            commonRef.handle = database.allocateHandle();
            commonRef.ownerHandle = record->handle;
            const AcDbHandle handle = commonRef.handle;
            if (database.insertLoadedEntity(std::move(member.payload)))
            {
                record->appendEntityHandle(handle);
                const bool memberIsInsertLike = std::visit(
                    [](const auto &typed) {
                        using T = std::decay_t<decltype(typed)>;
                        return std::is_same_v<T, AcDbBlockReference> ||
                               std::is_same_v<T, AcDbMInsertBlock>;
                    },
                    member.payload);
                if (memberIsInsertLike)
                    insertDbHandles[member.fileHandle] = handle;
            }
        }
    }

    // Attach pass: link parked ATTRIB payloads to their owning
    // INSERT/MINSERT payloads.  Attributes become database residents
    // without model-space membership.
    for (auto &[ownerFileHandle, parked] : attributesByOwner)
    {
        const auto found = insertDbHandles.find(ownerFileHandle);
        if (found == insertDbHandles.end())
        {
            report.unsupported += parked.size();
            continue;
        }
        AcDbEntityVariant *stored = database.getEntityMutable(found->second);
        if (stored == nullptr)
        {
            report.unsupported += parked.size();
            continue;
        }
        AcDbBlockReference *reference =
            std::get_if<AcDbBlockReference>(stored);
        AcDbMInsertBlock *gridInsert =
            reference != nullptr ? nullptr
                                 : std::get_if<AcDbMInsertBlock>(stored);
        if (reference == nullptr && gridInsert == nullptr)
        {
            report.unsupported += parked.size();
            continue;
        }
        for (AcDbAttribute &attribute : parked)
        {
            attribute.common.handle = database.allocateHandle();
            attribute.common.ownerHandle = found->second;
            const AcDbHandle attributeHandle = attribute.common.handle;
            if (database.insertLoadedEntity(
                    AcDbEntityVariant(std::move(attribute))))
            {
                if (reference != nullptr)
                    reference->attributeHandles.push_back(attributeHandle);
                else
                    gridInsert->attributeHandles.push_back(attributeHandle);
                ++report.added;
            }
        }
    }

    return report;
}

} // namespace acdb
