#pragma once

// Conversion bridge from libredwg's DWG data model (include/dwg.h) to the
// entities layer.  Each toEntity() overload maps one DWG entity struct onto
// its entities:: counterpart; field coverage tracks the DWG structures, so
// gaps surface as compile errors when libredwg adds fields we do not carry.

#include "libredwg/include/dwg.h"

#include "tessellate.h"

namespace entities
{

inline Line toEntity(const Dwg_Entity_LINE &dwg)
{
    Line line;
    line.start = AcGePoint3d(dwg.start.x, dwg.start.y, dwg.start.z);
    line.end = AcGePoint3d(dwg.end.x, dwg.end.y, dwg.end.z);
    line.thickness = dwg.thickness;
    line.extrusion = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                  dwg.extrusion.z);
    return line;
}

inline Arc toEntity(const Dwg_Entity_ARC &dwg)
{
    Arc arc;
    arc.center = AcGePoint3d(dwg.center.x, dwg.center.y, dwg.center.z);
    arc.radius = dwg.radius;
    arc.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                              dwg.extrusion.z);
    arc.thickness = dwg.thickness;
    arc.startAngle = dwg.start_angle;
    arc.endAngle = dwg.end_angle;
    return arc;
}

inline Circle toEntity(const Dwg_Entity_CIRCLE &dwg)
{
    Circle circle;
    circle.center = AcGePoint3d(dwg.center.x, dwg.center.y, dwg.center.z);
    circle.radius = dwg.radius;
    circle.normal = AcGeVector3d(dwg.extrusion.x, dwg.extrusion.y,
                                 dwg.extrusion.z);
    circle.thickness = dwg.thickness;
    return circle;
}

inline Ellipse toEntity(const Dwg_Entity_ELLIPSE &dwg)
{
    Ellipse ellipse;
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

inline Point toEntity(const Dwg_Entity_POINT &dwg)
{
    Point point;
    point.location = AcGePoint3d(dwg.x, dwg.y, dwg.z);
    return point;
}

// DWG stores XLINE as the identical Dwg_Entity_RAY struct, so one overload
// serves both; callers re-interpret the result as entities::XLine.
inline Ray toEntity(const Dwg_Entity_RAY &dwg)
{
    Ray ray;
    ray.start = AcGePoint3d(dwg.point.x, dwg.point.y, dwg.point.z);
    ray.direction = AcGeVector3d(dwg.vector.x, dwg.vector.y, dwg.vector.z);
    return ray;
}

} // namespace entities
