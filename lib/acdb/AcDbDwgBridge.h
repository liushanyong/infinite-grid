#pragma once

// Conversion bridge from libredwg's DWG data model (include/dwg.h) to the
// entities layer.  Each toEntity() overload maps one DWG entity struct onto
// its acdb:: counterpart; field coverage tracks the DWG structures, so
// gaps surface as compile errors when libredwg adds fields we do not carry.

#include "libredwg/include/dwg.h"

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbTessellate.h"

namespace acdb
{

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


// ACI color index -> RGBA (AutoCAD's base 9 + grayscale ramp), aligned
// with the demo's aciColor table.
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
    default: return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}


// ---- direct fill: DWG file -> AcDbDatabase (ObjectARX readDwg shape) ----

// Registers a layer record for |name| when missing (OpenCADStudio's
// ensure_layer: a table record without a real handle cannot survive a
// handle-based file format).
inline void ensureLayer(AcDbDatabase &database, const std::string &name)
{
    if (name.empty() || database.layerTable().contains(name))
        return;
    database.layerTable().add(name, database.allocateHandle());
}

// Walks every DWG entity, converts the supported set through toEntity(),
// and inserts it into the database with its ACI color.  Unsupported
// entity types are skipped and counted, mirroring a failsafe reader.
// Returns the number of entities added.
inline std::size_t addDwgEntities(AcDbDatabase &database,
                                  const Dwg_Data &dwg)
{
    std::size_t added = 0;
    for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
    {
        const Dwg_Object &object = dwg.object[i];
        if (object.supertype != DWG_SUPERTYPE_ENTITY || !object.tio.entity)
            continue;
        const glm::vec4 entityColor =
            aciToColor(static_cast<int>(object.tio.entity->color.index));

        switch (object.type)
        {
        case DWG_TYPE_LINE:
        {
            AcDbLine line = toEntity(*object.tio.entity->tio.LINE);
            line.common.name = "DWG_LINE";
            line.common.color = entityColor;
            ensureLayer(database, line.common.layer);
            database.addEntity(std::move(line));
            ++added;
            break;
        }
        case DWG_TYPE_ARC:
        {
            AcDbArc arc = toEntity(*object.tio.entity->tio.ARC);
            arc.common.name = "DWG_ARC";
            arc.common.color = entityColor;
            ensureLayer(database, arc.common.layer);
            database.addEntity(std::move(arc));
            ++added;
            break;
        }
        case DWG_TYPE_CIRCLE:
        {
            AcDbCircle circle = toEntity(*object.tio.entity->tio.CIRCLE);
            circle.common.name = "DWG_CIRCLE";
            circle.common.color = entityColor;
            ensureLayer(database, circle.common.layer);
            database.addEntity(std::move(circle));
            ++added;
            break;
        }
        case DWG_TYPE_ELLIPSE:
        {
            AcDbEllipse ellipse = toEntity(*object.tio.entity->tio.ELLIPSE);
            ellipse.common.name = "DWG_ELLIPSE";
            ellipse.common.color = entityColor;
            ensureLayer(database, ellipse.common.layer);
            database.addEntity(std::move(ellipse));
            ++added;
            break;
        }
        case DWG_TYPE_POINT:
        {
            AcDbPoint point = toEntity(*object.tio.entity->tio.POINT);
            point.common.name = "DWG_POINT";
            point.common.color = entityColor;
            ensureLayer(database, point.common.layer);
            database.addEntity(std::move(point));
            ++added;
            break;
        }
        case DWG_TYPE_RAY:
        {
            AcDbRay ray = toEntity(*object.tio.entity->tio.RAY);
            ray.common.name = "DWG_RAY";
            ray.common.color = entityColor;
            ensureLayer(database, ray.common.layer);
            database.addEntity(std::move(ray));
            ++added;
            break;
        }
        case DWG_TYPE_XLINE:
        {
            const AcDbRay ray = toEntity(*object.tio.entity->tio.RAY);
            AcDbXline xline;
            xline.common.name = "DWG_XLINE";
            xline.point = ray.start;
            xline.direction = ray.direction;
            xline.common.color = entityColor;
            ensureLayer(database, xline.common.layer);
            database.addEntity(std::move(xline));
            ++added;
            break;
        }
        default:
            break;
        }
    }
    return added;
}

} // namespace acdb
