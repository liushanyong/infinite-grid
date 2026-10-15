#pragma once

#include "AcDbPort.h"

// AcDb file-IO self-test (hidden runtime mode): GRID_SELFTEST=fileio
// drives the write/read roundtrips of the four ObjectARX entry points —
// sample document -> writeDwgFile -> readDwgFile, sample document ->
// dxfOut -> dxfIn, plus a second-generation DXF stability pass —
// reporting through fileio_selftest.log (GUI processes have no valid
// stderr handle).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <type_traits>

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbDwgBridge.h"
#include "acdb/AcDbEntities.h"
#include "acdb/AcDbFileIo.h"

#ifndef GRID_SOURCE_DIR
#define GRID_SOURCE_DIR "."
#endif

namespace acdb
{

namespace fileio_selftest
{

constexpr double kTolerance = 1.0e-6;

inline bool closeEnough(double a, double b)
{
    return std::abs(a - b) <= kTolerance * std::max(1.0, std::abs(a));
}

inline bool pointClose(const AcGePoint3d &a, const AcGePoint3d &b)
{
    return closeEnough(a.x, b.x) && closeEnough(a.y, b.y) &&
           closeEnough(a.z, b.z);
}

// One of every supported entity, a styled layer, and a block definition
// with a transformed reference.
inline AcDbDatabase buildSampleDocument()
{
    AcDbDatabase database;
    AcDbLayerTableRecord &walls =
        database.layerTable().add("Walls", database.allocateHandle());
    walls.color = {1.0, 0.0, 0.0, 1.0};

    // Default ACI for the sample entities: explicit green (ByLayer 256
    // stays untouched so per-entity overrides survive).
    auto add = [&database](auto payload) {
        if (payload.common.color.colorIndex() == 256)
        {
            payload.common.color.setColorIndex(3);
            payload.common.color = {0.0, 1.0, 0.0, 1.0};
            payload.common.color.setColorIndex(3);
        }
        return database.addEntity(std::move(payload));
    };

    AcDbLine line;
    line.start = AcGePoint3d(0.0, 0.0, 0.0);
    line.end = AcGePoint3d(100.0, 0.0, 0.0);
    line.common.layer = "Walls";
    line.common.color.setColorIndex(1);
    line.common.color = {1.0, 0.0, 0.0, 1.0};
    line.common.color.setColorIndex(1);
    add(line);

    AcDbCircle circle;
    circle.center = AcGePoint3d(200.0, 0.0, 0.0);
    circle.radius = 50.0;
    add(circle);

    AcDbArc arc;
    arc.center = AcGePoint3d(300.0, 0.0, 0.0);
    arc.radius = 40.0;
    arc.startAngle = 0.0;
    arc.endAngle = 1.5707963267948966;
    add(arc);

    AcDbEllipse ellipse;
    ellipse.center = AcGePoint3d(400.0, 0.0, 0.0);
    ellipse.majorAxis = AcGeVector3d(80.0, 0.0, 0.0);
    ellipse.radiusRatio = 0.5;
    add(ellipse);

    AcDbPoint point;
    point.location = AcGePoint3d(0.0, 100.0, 0.0);
    add(point);

    AcDbRay ray;
    ray.start = AcGePoint3d(50.0, 100.0, 0.0);
    ray.direction = AcGeVector3d(1.0, 0.5, 0.0);
    add(ray);

    AcDbXline xline;
    xline.point = AcGePoint3d(100.0, 100.0, 0.0);
    xline.direction = AcGeVector3d(0.0, 1.0, 0.0);
    add(xline);

    AcDbSolid quad;
    quad.firstCorner = AcGePoint3d(0.0, 200.0, 0.0);
    quad.secondCorner = AcGePoint3d(100.0, 200.0, 0.0);
    quad.thirdCorner = AcGePoint3d(0.0, 260.0, 0.0);
    quad.fourthCorner = AcGePoint3d(100.0, 260.0, 0.0);
    add(quad);

    AcDbPolyline lwpolyline;
    lwpolyline.vertices = {AcGePoint2d(120.0, 200.0),
                           AcGePoint2d(220.0, 200.0),
                           AcGePoint2d(220.0, 260.0),
                           AcGePoint2d(120.0, 260.0)};
    lwpolyline.bulges = {0.0, 0.5, 0.0, 0.0};
    lwpolyline.closed = true;
    add(lwpolyline);

    AcDb3dPolyline polyline3d;
    polyline3d.vertices = {AcGePoint3d(0.0, 300.0, 10.0),
                           AcGePoint3d(60.0, 320.0, 20.0),
                           AcGePoint3d(120.0, 300.0, 30.0)};
    add(polyline3d);

    AcDbSpline spline;
    spline.degree = 3;
    spline.controlPoints = {AcGePoint3d(0.0, 400.0, 0.0),
                            AcGePoint3d(40.0, 440.0, 0.0),
                            AcGePoint3d(80.0, 360.0, 0.0),
                            AcGePoint3d(120.0, 400.0, 0.0)};
    spline.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    add(spline);

    AcDbText text;
    text.insertion = AcGePoint3d(0.0, 500.0, 0.0);
    text.height = 10.0;
    text.rotation = 0.2;
    text.text = L"Hello DWG";
    add(text);

    AcDbMText mtext;
    mtext.insertion = AcGePoint3d(200.0, 500.0, 0.0);
    mtext.width = 80.0;
    mtext.height = 8.0;
    mtext.direction = AcGeVector3d(1.0, 0.0, 0.0);
    mtext.text = L"MTEXT body";
    add(mtext);

    AcDbHatch hatch;
    hatch.outerLoop = {AcGePoint3d(300.0, 450.0, 0.0),
                       AcGePoint3d(400.0, 450.0, 0.0),
                       AcGePoint3d(400.0, 520.0, 0.0),
                       AcGePoint3d(300.0, 520.0, 0.0)};
    hatch.solidFill = true;
    hatch.patternName = "SOLID";
    add(hatch);

    AcDbMline mline;
    mline.vertices = {AcGePoint3d(0.0, 600.0, 0.0),
                      AcGePoint3d(80.0, 620.0, 0.0),
                      AcGePoint3d(160.0, 600.0, 0.0)};
    add(mline);

    // ---- annotation family ----
    AcDbRotatedDimension rotatedDim;
    rotatedDim.dimension.textHeight = 8.0;
    rotatedDim.xLine1Point = AcGePoint3d(0.0, -100.0, 0.0);
    rotatedDim.xLine2Point = AcGePoint3d(100.0, -100.0, 0.0);
    rotatedDim.dimensionLinePoint = AcGePoint3d(50.0, -140.0, 0.0);
    rotatedDim.dimensionLineAngle = 0.0;
    add(rotatedDim);

    AcDbAlignedDimension alignedDim;
    alignedDim.dimension.textHeight = 8.0;
    alignedDim.xLine1Point = AcGePoint3d(140.0, -100.0, 0.0);
    alignedDim.xLine2Point = AcGePoint3d(200.0, -140.0, 0.0);
    alignedDim.dimensionLinePoint = AcGePoint3d(170.0, -120.0, 0.0);
    add(alignedDim);

    AcDbRadialDimension radialDim;
    radialDim.dimension.textHeight = 8.0;
    radialDim.centerPoint = AcGePoint3d(300.0, -100.0, 0.0);
    radialDim.radiusPoint = AcGePoint3d(340.0, -100.0, 0.0);
    radialDim.leaderLength = 10.0;
    add(radialDim);

    AcDbDiametricDimension diametricDim;
    diametricDim.dimension.textHeight = 8.0;
    diametricDim.chordPoint = AcGePoint3d(400.0, -140.0, 0.0);
    diametricDim.farChordPoint = AcGePoint3d(480.0, -60.0, 0.0);
    diametricDim.leaderLength = 10.0;
    add(diametricDim);

    AcDbOrdinateDimension ordinateDim;
    ordinateDim.dimension.textHeight = 8.0;
    ordinateDim.featureLocationPoint = AcGePoint3d(0.0, -200.0, 0.0);
    ordinateDim.leaderEndPoint = AcGePoint3d(30.0, -230.0, 0.0);
    ordinateDim.useXAxis = true;
    add(ordinateDim);

    AcDb3PointAngularDimension angularDim;
    angularDim.dimension.textHeight = 8.0;
    angularDim.centerPoint = AcGePoint3d(600.0, -100.0, 0.0);
    angularDim.xLine1Point = AcGePoint3d(660.0, -100.0, 0.0);
    angularDim.xLine2Point = AcGePoint3d(600.0, -40.0, 0.0);
    angularDim.dimension.textPosition = AcGePoint3d(635.0, -135.0, 0.0);
    add(angularDim);

    AcDbLeader leader;
    leader.points = {AcGePoint3d(700.0, -100.0, 0.0),
                     AcGePoint3d(740.0, -140.0, 0.0),
                     AcGePoint3d(780.0, -140.0, 0.0)};
    leader.hasArrowhead = true;
    add(leader);

    AcDbFcf fcf;
    fcf.insertion = AcGePoint3d(0.0, -300.0, 0.0);
    fcf.text = "{\fArial|b0;i}0.05{A}";
    fcf.height = 8.0;
    add(fcf);

    AcDbShape shape;
    shape.insertion = AcGePoint3d(200.0, -300.0, 0.0);
    shape.scale = 10.0;
    shape.rotation = 0.3;
    shape.shapeName = "Standard";
    add(shape);

    // Block with an attribute definition, placed with one stamped ATTRIB.
    AcDbLine frameLine;
    frameLine.start = AcGePoint3d(500.0, -300.0, 0.0);
    frameLine.end = AcGePoint3d(560.0, -300.0, 0.0);
    const AcDbHandle frameHandle = add(frameLine);
    AcDbAttributeDefinition tagDef;
    tagDef.insertion = AcGePoint3d(510.0, -290.0, 0.0);
    tagDef.height = 8.0;
    tagDef.tag = "PART_NO";
    tagDef.prompt = "Enter part number";
    tagDef.textValue = "A-000";
    const AcDbHandle tagDefHandle = add(tagDef);
    database.createBlockDefinition(
        "TitleBlock", AcGePoint3d(500.0, -300.0, 0.0),
        {frameHandle, tagDefHandle});
    const AcDbHandle titleRef =
        database.addBlockReference("TitleBlock",
                                   AcGePoint3d(0.0, -400.0, 0.0));
    AcDbAttribute stamped;
    stamped.insertion = AcGePoint3d(510.0, -390.0, 0.0);
    stamped.height = 8.0;
    stamped.tag = "PART_NO";
    stamped.textValue = "A-123";
    stamped.common.handle = database.allocateHandle();
    stamped.common.ownerHandle = titleRef;
    const AcDbHandle stampedHandle = stamped.common.handle;
    database.insertLoadedEntity(AcDbEntityVariant(std::move(stamped)));
    if (AcDbEntityVariant *reference = database.getEntityMutable(titleRef))
        if (auto *typed = std::get_if<AcDbBlockReference>(reference))
            typed->attributeHandles.push_back(stampedHandle);

    AcDbPolyFaceMesh pface;
    pface.style = MeshStyle::Cad;
    pface.geometry.positions = {AcGePoint3d(600.0, 300.0, 0.0),
                                AcGePoint3d(660.0, 300.0, 0.0),
                                AcGePoint3d(660.0, 360.0, 0.0),
                                AcGePoint3d(600.0, 360.0, 0.0)};
    pface.geometry.indices = {0, 1, 2, 0, 2, 3};
    add(pface);

    AcDbMInsertBlock minsert;
    minsert.blockTableRecordName = "Bolt";
    minsert.position = AcGePoint3d(500.0, -400.0, 0.0);
    minsert.rowCount = 2;
    minsert.columnCount = 2;
    minsert.rowSpacing = 40.0;
    minsert.columnSpacing = 60.0;
    add(minsert);

    // Block definition from two model-space members + a transformed
    // reference.
    AcDbLine boltA;
    boltA.start = AcGePoint3d(500.0, 0.0, 0.0);
    boltA.end = AcGePoint3d(510.0, 0.0, 0.0);
    const AcDbHandle boltAHandle = add(boltA);
    AcDbCircle boltB;
    boltB.center = AcGePoint3d(505.0, 0.0, 0.0);
    boltB.radius = 5.0;
    const AcDbHandle boltBHandle = add(boltB);
    database.createBlockDefinition("Bolt", AcGePoint3d(505.0, 0.0, 0.0),
                                   {boltAHandle, boltBHandle});
    database.addBlockReference("Bolt", AcGePoint3d(500.0, 100.0, 0.0), 0.7,
                               AcGeVector3d(1.5, 1.5, 1.5));
    return database;
}

template <typename Payload>
inline void comparePayload(const Payload &a, const Payload &b, FILE *log,
                           int &fails, const char *label)
{
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::fprintf(log, "FAIL %s: %s\n", label, what);
            std::fflush(log);
        }
    };
    check(a.common.color.colorMethod() == b.common.color.colorMethod() &&
              a.common.color.colorIndex() == b.common.color.colorIndex(),
          "entity color method/index");
    if constexpr (std::is_same_v<Payload, AcDbLine>)
    {
        check(pointClose(a.start, b.start) && pointClose(a.end, b.end),
              "line endpoints");
    }
    else if constexpr (std::is_same_v<Payload, AcDbArc>)
    {
        check(pointClose(a.center, b.center) &&
                  closeEnough(a.radius, b.radius) &&
                  closeEnough(a.startAngle, b.startAngle) &&
                  closeEnough(a.endAngle, b.endAngle),
              "arc center/radius/angles");
    }
    else if constexpr (std::is_same_v<Payload, AcDbCircle>)
    {
        check(pointClose(a.center, b.center) &&
                  closeEnough(a.radius, b.radius),
              "circle center/radius");
    }
    else if constexpr (std::is_same_v<Payload, AcDbEllipse>)
    {
        check(pointClose(a.center, b.center) &&
                  closeEnough(a.radiusRatio, b.radiusRatio) &&
                  pointClose(AcGePoint3d(a.majorAxis.x, a.majorAxis.y,
                                         a.majorAxis.z),
                             AcGePoint3d(b.majorAxis.x, b.majorAxis.y,
                                         b.majorAxis.z)),
              "ellipse center/ratio/majorAxis");
    }
    else if constexpr (std::is_same_v<Payload, AcDbPoint>)
    {
        check(pointClose(a.location, b.location), "point location");
    }
    else if constexpr (std::is_same_v<Payload, AcDbRay>)
    {
        check(pointClose(a.start, b.start), "ray start");
    }
    else if constexpr (std::is_same_v<Payload, AcDbXline>)
    {
        check(pointClose(a.point, b.point), "xline point");
    }
    else if constexpr (std::is_same_v<Payload, AcDbSolid>)
    {
        check(pointClose(a.firstCorner, b.firstCorner) &&
                  pointClose(a.secondCorner, b.secondCorner) &&
                  pointClose(a.thirdCorner, b.thirdCorner) &&
                  pointClose(a.fourthCorner, b.fourthCorner),
              "solid corners");
    }
    else if constexpr (std::is_same_v<Payload, AcDbPolyline>)
    {
        check(a.vertices.size() == b.vertices.size() &&
                  a.closed == b.closed,
              "lwpolyline vertex count/closed");
        bool ok = a.vertices.size() == b.vertices.size();
        for (std::size_t i = 0; ok && i < a.vertices.size(); ++i)
            ok = closeEnough(a.vertices[i].x, b.vertices[i].x) &&
                 closeEnough(a.vertices[i].y, b.vertices[i].y);
        check(ok, "lwpolyline vertices");
    }
    else if constexpr (std::is_same_v<Payload, AcDb3dPolyline>)
    {
        check(a.vertices.size() == b.vertices.size(),
              "polyline3d vertex count");
        bool ok = a.vertices.size() == b.vertices.size();
        for (std::size_t i = 0; ok && i < a.vertices.size(); ++i)
            ok = pointClose(a.vertices[i], b.vertices[i]);
        if (!ok)
            for (std::size_t i = 0;
                 i < std::min(a.vertices.size(), b.vertices.size()); ++i)
                std::fprintf(log,
                             "  poly3d[%zu] a=(%g,%g,%g) b=(%g,%g,%g)\n", i,
                             a.vertices[i].x, a.vertices[i].y,
                             a.vertices[i].z, b.vertices[i].x,
                             b.vertices[i].y, b.vertices[i].z);
        check(ok, "polyline3d vertices");
    }
    else if constexpr (std::is_same_v<Payload, AcDbSpline>)
    {
        check(a.degree == b.degree &&
                  a.controlPoints.size() == b.controlPoints.size(),
              "spline degree/control point count");
        bool ok = a.controlPoints.size() == b.controlPoints.size();
        for (std::size_t i = 0; ok && i < a.controlPoints.size(); ++i)
            ok = pointClose(a.controlPoints[i], b.controlPoints[i]);
        check(ok, "spline control points");
    }
    else if constexpr (std::is_same_v<Payload, AcDbText>)
    {
        check(a.text == b.text && closeEnough(a.height, b.height) &&
                  closeEnough(a.rotation, b.rotation) &&
                  pointClose(a.insertion, b.insertion),
              "text content/height/rotation/insertion");
    }
    else if constexpr (std::is_same_v<Payload, AcDbMText>)
    {
        check(a.text == b.text && closeEnough(a.width, b.width) &&
                  closeEnough(a.height, b.height) &&
                  pointClose(a.insertion, b.insertion),
              "mtext content/width/height/insertion");
    }
    else if constexpr (std::is_same_v<Payload, AcDbHatch>)
    {
        check(a.outerLoop.size() == b.outerLoop.size() &&
                  a.solidFill == b.solidFill,
              "hatch loop count/fill");
        bool ok = a.outerLoop.size() == b.outerLoop.size();
        for (std::size_t i = 0; ok && i < a.outerLoop.size(); ++i)
            ok = pointClose(a.outerLoop[i], b.outerLoop[i]);
        check(ok, "hatch outer loop");
    }
    else if constexpr (std::is_same_v<Payload, AcDbPolyFaceMesh>)
    {
        check(a.geometry.positions.size() == b.geometry.positions.size() &&
                  a.geometry.indices.size() == b.geometry.indices.size(),
              "polyface mesh counts");
    }
    else if constexpr (std::is_same_v<Payload, AcDbMline>)
    {
        check(a.vertices.size() == b.vertices.size(), "mline vertex count");
    }
    else if constexpr (std::is_same_v<Payload, AcDbBlockReference>)
    {
        check(a.blockTableRecordName == b.blockTableRecordName &&
                  pointClose(a.position, b.position) &&
                  closeEnough(a.rotation, b.rotation) &&
                  closeEnough(a.scale.x, b.scale.x),
              "insert name/position/rotation/scale");
    }
    else if constexpr (std::is_same_v<Payload, AcDbRotatedDimension>)
    {
        check(pointClose(a.xLine1Point, b.xLine1Point) &&
                  pointClose(a.xLine2Point, b.xLine2Point) &&
                  pointClose(a.dimensionLinePoint,
                             b.dimensionLinePoint) &&
                  closeEnough(a.dimensionLineAngle,
                              b.dimensionLineAngle),
              "rotated dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDbAlignedDimension>)
    {
        check(pointClose(a.xLine1Point, b.xLine1Point) &&
                  pointClose(a.xLine2Point, b.xLine2Point) &&
                  pointClose(a.dimensionLinePoint,
                             b.dimensionLinePoint),
              "aligned dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDb2LineAngularDimension>)
    {
        check(pointClose(a.xLine1StartPoint, b.xLine1StartPoint) &&
                  pointClose(a.xLine1EndPoint, b.xLine1EndPoint) &&
                  pointClose(a.xLine2StartPoint, b.xLine2StartPoint) &&
                  pointClose(a.xLine2EndPoint, b.xLine2EndPoint) &&
                  pointClose(a.arcPoint, b.arcPoint),
              "angular 2-line dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDb3PointAngularDimension>)
    {
        check(pointClose(a.centerPoint, b.centerPoint) &&
                  pointClose(a.xLine1Point, b.xLine1Point) &&
                  pointClose(a.xLine2Point, b.xLine2Point),
              "angular 3-point dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDbRadialDimension>)
    {
        check(pointClose(a.centerPoint, b.centerPoint) &&
                  pointClose(a.radiusPoint, b.radiusPoint),
              "radial dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDbDiametricDimension>)
    {
        check(pointClose(a.chordPoint, b.chordPoint) &&
                  pointClose(a.farChordPoint, b.farChordPoint),
              "diametric dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDbOrdinateDimension>)
    {
        check(pointClose(a.featureLocationPoint,
                         b.featureLocationPoint) &&
                  pointClose(a.leaderEndPoint, b.leaderEndPoint) &&
                  a.useXAxis == b.useXAxis,
              "ordinate dimension geometry");
    }
    else if constexpr (std::is_same_v<Payload, AcDbLeader>)
    {
        check(a.points.size() == b.points.size() && a.points.size() >= 2 &&
                  pointClose(a.points.front(), b.points.front()) &&
                  pointClose(a.points.back(), b.points.back()),
              "leader vertex path");
    }
    else if constexpr (std::is_same_v<Payload, AcDbFcf>)
    {
        check(a.text == b.text && pointClose(a.insertion, b.insertion),
              "fcf text/insertion");
    }
    else if constexpr (std::is_same_v<Payload, AcDbShape>)
    {
        check(pointClose(a.insertion, b.insertion) &&
                  closeEnough(a.scale, b.scale) &&
                  closeEnough(a.rotation, b.rotation),
              "shape placement");
    }
    else if constexpr (std::is_same_v<Payload, AcDbAttributeDefinition>)
    {
        check(a.tag == b.tag && a.prompt == b.prompt &&
                  a.textValue == b.textValue &&
                  pointClose(a.insertion, b.insertion),
              "attdef tag/prompt/default/insertion");
    }
    else if constexpr (std::is_same_v<Payload, AcDbAttribute>)
    {
        check(a.tag == b.tag && a.textValue == b.textValue &&
                  pointClose(a.insertion, b.insertion),
              "attribute tag/value/insertion");
    }
    else if constexpr (std::is_same_v<Payload, AcDbMInsertBlock>)
    {
        check(a.blockTableRecordName == b.blockTableRecordName &&
                  a.rowCount == b.rowCount &&
                  a.columnCount == b.columnCount &&
                  closeEnough(a.rowSpacing, b.rowSpacing) &&
                  closeEnough(a.columnSpacing, b.columnSpacing),
              "minsert name/grid");
    }
}

// INSERTs with stamped attributes: the ATTRIB payloads are database
// residents outside model space, so their roundtrip is verified through
// the owning reference's handle list.
inline void compareInsertedAttributes(const AcDbDatabase &baseline,
                                      const AcDbDatabase &loaded,
                                      FILE *log, int &fails,
                                      const char *label)
{
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::fprintf(log, "FAIL %s: %s\n", label, what);
            std::fflush(log);
        }
    };
    const std::vector<AcDbHandle> &before =
        baseline.modelSpace().entityHandles();
    const std::vector<AcDbHandle> &after =
        loaded.modelSpace().entityHandles();
    const std::size_t count = std::min(before.size(), after.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcDbEntityVariant *a = baseline.getEntity(before[i]);
        const AcDbEntityVariant *b = loaded.getEntity(after[i]);
        if (a == nullptr || b == nullptr || a->index() != b->index())
            continue;
        const auto *referenceA = std::get_if<AcDbBlockReference>(a);
        const auto *referenceB = std::get_if<AcDbBlockReference>(b);
        if (referenceA == nullptr || referenceB == nullptr)
            continue;
        check(referenceA->attributeHandles.size() ==
                  referenceB->attributeHandles.size(),
              "insert attribute count");
        const std::size_t attributes = std::min(
            referenceA->attributeHandles.size(),
            referenceB->attributeHandles.size());
        for (std::size_t k = 0; k < attributes; ++k)
        {
            const AcDbEntityVariant *payloadA = baseline.getEntity(
                referenceA->attributeHandles[k]);
            const AcDbEntityVariant *payloadB = loaded.getEntity(
                referenceB->attributeHandles[k]);
            const AcDbAttribute *attributeA =
                payloadA ? std::get_if<AcDbAttribute>(payloadA) : nullptr;
            const AcDbAttribute *attributeB =
                payloadB ? std::get_if<AcDbAttribute>(payloadB) : nullptr;
            check(attributeA != nullptr && attributeB != nullptr &&
                      attributeA->tag == attributeB->tag &&
                      attributeA->textValue == attributeB->textValue,
                  "attribute tag/value roundtrip");
        }
    }
}

// Sequential model-space compare: same length, same per-slot type, and
// per-type geometry within tolerance.
inline void compareModelSpace(const AcDbDatabase &baseline,
                              const AcDbDatabase &loaded, FILE *log,
                              int &fails, const char *label)
{
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::fprintf(log, "FAIL %s: %s\n", label, what);
            std::fflush(log);
        }
    };

    auto typeMix = [](const AcDbDatabase &database) {
        std::map<std::size_t, int> counts;
        for (const AcDbHandle handle : database.modelSpace().entityHandles())
            if (const AcDbEntityVariant *payload =
                    database.getEntity(handle))
                counts[payload->index()] += 1;
        return counts;
    };
    check(typeMix(baseline) == typeMix(loaded), "model-space entity mix");

    const std::vector<AcDbHandle> &before =
        baseline.modelSpace().entityHandles();
    const std::vector<AcDbHandle> &after =
        loaded.modelSpace().entityHandles();
    check(before.size() == after.size(), "model-space size");
    const std::size_t count = std::min(before.size(), after.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const AcDbEntityVariant *a = baseline.getEntity(before[i]);
        const AcDbEntityVariant *b = loaded.getEntity(after[i]);
        if (a == nullptr || b == nullptr)
        {
            check(false, "entity resolved");
            continue;
        }
        check(a->index() == b->index(), "entity type order");
        if (a->index() != b->index())
            continue;
        std::visit(
            [&](const auto &typedA) {
                using Payload = std::decay_t<decltype(typedA)>;
                const Payload *typedB = std::get_if<Payload>(b);
                if (typedB != nullptr)
                    comparePayload(typedA, *typedB, log, fails, label);
            },
            *a);
    }

    // Block definitions survive with their members.
    baseline.blockTable().forEach(
        [&](const std::string &name, const AcDbBlockTableRecord &record) {
            if (name == kModelSpaceName || name == kPaperSpaceName)
                return;
            const AcDbBlockTableRecord *reloaded =
                loaded.blockTable().get(name);
            check(reloaded != nullptr, "block record imported");
            if (reloaded == nullptr)
                return;
            check(reloaded->entityHandles().size() ==
                      record.entityHandles().size(),
                  "block member count");
        });
}

// Structural dump for forensics: per-slot model-space types with key
// geometry, plus the block table.
inline void dumpStructure(const AcDbDatabase &database, FILE *log,
                          const char *label)
{
    std::fprintf(log, "[%s] model space:\n", label);
    int slot = 0;
    for (const AcDbHandle handle : database.modelSpace().entityHandles())
    {
        const AcDbEntityVariant *payload = database.getEntity(handle);
        if (payload == nullptr)
        {
            std::fprintf(log, "  %d: <null>\n", slot++);
            continue;
        }
        std::visit(
            [&](const auto &typed) {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, AcDbLine>)
                    std::fprintf(log,
                                 "  %d: Line (%g,%g,%g)-(%.3g,%.3g,%.3g)\n",
                                 slot++, typed.start.x, typed.start.y,
                                 typed.start.z, typed.end.x, typed.end.y,
                                 typed.end.z);
                else if constexpr (std::is_same_v<T, AcDbCircle>)
                    std::fprintf(log, "  %d: Circle c(%g,%g) r%g\n", slot++,
                                 typed.center.x, typed.center.y, typed.radius);
                else if constexpr (std::is_same_v<T, AcDb3dPolyline>)
                    std::fprintf(log, "  %d: Polyline3d %zu verts\n", slot++,
                                 typed.vertices.size());
                else if constexpr (std::is_same_v<T, AcDbBlockReference>)
                    std::fprintf(log,
                                 "  %d: Insert '%s' at (%g,%g)\n", slot++,
                                 typed.blockTableRecordName.c_str(),
                                 typed.position.x, typed.position.y);
                else if constexpr (std::is_same_v<T, AcDbOrdinateDimension>)
                    std::fprintf(log,
                                 "  %d: Ordinate useX=%d feature (%g,%g)\n",
                                 slot++, int(typed.useXAxis),
                                 typed.featureLocationPoint.x,
                                 typed.featureLocationPoint.y);
                else if constexpr (std::is_same_v<T, AcDbDiametricDimension>)
                    std::fprintf(log,
                                 "  %d: Diameter chord (%g,%g) far (%g,%g)\n",
                                 slot++, typed.chordPoint.x,
                                 typed.chordPoint.y,
                                 typed.farChordPoint.x,
                                 typed.farChordPoint.y);
                else
                    std::fprintf(log, "  %d: variant#%zu\n", slot++,
                                 payload->index());
            },
            *payload);
    }
    std::fprintf(log, "[%s] blocks:\n", label);
    database.blockTable().forEach(
        [&](const std::string &name, const AcDbBlockTableRecord &record) {
            std::fprintf(log, "  %s: %zu members\n", name.c_str(),
                         record.entityHandles().size());
        });
    std::fflush(log);
}

inline void verifyDwgImportHelpers(FILE *log, int &fails)
{
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::fprintf(log, "FAIL import helper: %s\n", what);
        }
    };

    const glm::vec4 aci20 = aciToColor(20);
    check(closeEnough(aci20.r, 1.0) &&
              closeEnough(aci20.g, 0.25) &&
              closeEnough(aci20.b, 0.0),
          "ACI hue palette");
    Dwg_Color trueColor{};
    trueColor.method = 0xc3;
    trueColor.rgb = 0xc3123456u;
    const glm::vec4 rgb = dwgColorToColor(trueColor);
    const AcCmEntityColor importedTrueColor =
        dwgColorToEntityColor(trueColor);
    check(closeEnough(rgb.r, 0x12 / 255.0) &&
              closeEnough(rgb.g, 0x34 / 255.0) &&
              closeEnough(rgb.b, 0x56 / 255.0),
          "DWG truecolor");
    check(importedTrueColor.colorMethod() ==
                  AcCmEntityColor::ColorMethod::kByColor &&
              importedTrueColor.colorIndex() == trueColor.index,
          "DWG truecolor method/index");
    Dwg_Color byLayer{};
    byLayer.method = 0xc0;
    byLayer.index = 256;
    const AcCmEntityColor importedByLayer =
        dwgColorToEntityColor(byLayer);
    check(importedByLayer.colorMethod() ==
                  AcCmEntityColor::ColorMethod::kByLayer &&
              importedByLayer.colorIndex() == 256,
          "DWG ByLayer method/index");
    check(closeEnough(lineWeightValueToPixels(25),
                      0.25 * 96.0 / 25.4),
          "lineweight physical-to-pixel conversion");
}

inline void verifyArchitecturalFixture(FILE *log, int &fails)
{
    AcDbDatabase database;
    AcDbFileIoReport report;
    const std::string fixture =
        std::string(GRID_SOURCE_DIR) +
        "/resources/architectural_-_annotation_scaling_and_multileaders.dwg";
    if (!database.readDwgFile(fixture.c_str(), &report))
    {
        ++fails;
        std::fprintf(log, "FAIL architectural fixture '%s': read error %u\n",
                     fixture.c_str(), report.errorCode);
        return;
    }

    std::size_t multileaderPaths = 0;
    std::size_t multileaderTexts = 0;
    for (const AcDbHandle handle : database.modelSpace().entityHandles())
    {
        const AcDbEntityVariant *payload = database.getEntity(handle);
        if (payload == nullptr ||
            common(*payload).name != "DWG_MULTILEADER")
            continue;
        if (const AcDbLeader *leader = std::get_if<AcDbLeader>(payload);
            leader != nullptr && leader->points.size() >= 2)
            ++multileaderPaths;
        if (const AcDbMText *text = std::get_if<AcDbMText>(payload);
            text != nullptr && !text->text.empty())
            ++multileaderTexts;
    }
    if (multileaderPaths == 0 || multileaderTexts == 0)
    {
        ++fails;
        std::fprintf(log,
                     "FAIL architectural fixture: multileader paths=%zu text=%zu\n",
                     multileaderPaths, multileaderTexts);
    }
    else
    {
        std::fprintf(log,
                     "architectural fixture: %zu multileader paths, %zu text records\n",
                     multileaderPaths, multileaderTexts);
    }
}

} // namespace fileio_selftest

inline int runFileIoSelfTest()
{
    using namespace fileio_selftest;
    FILE *report = std::fopen("fileio_selftest.log", "w");
    if (report == nullptr)
        return 99;
    int fails = 0;
    auto stage = [&](const char *what) {
        std::fprintf(report, "-- %s\n", what);
        std::fflush(report);
    };

    stage("building sample document");
    const AcDbDatabase sample = buildSampleDocument();
    verifyDwgImportHelpers(report, fails);

    // ---- DWG out -> in ----
    stage("writeDwgFile");
    if (sample.writeDwgFile("fileio_selftest.dwg"))
    {
        stage("readDwgFile");
        AcDbDatabase fromDwg;
        AcDbFileIoReport ioReport;
        if (fromDwg.readDwgFile("fileio_selftest.dwg", &ioReport))
        {
            std::fprintf(report, "dwg roundtrip: %zu added, %zu skipped\n",
                         ioReport.entitiesAdded, ioReport.unsupportedSkipped);
            dumpStructure(sample, report, "sample");
            dumpStructure(fromDwg, report, "fromDwg");
            compareModelSpace(sample, fromDwg, report, fails, "dwg");
            compareInsertedAttributes(sample, fromDwg, report, fails,
                                      "dwg");
        }
        else
        {
            ++fails;
            std::fprintf(report, "FAIL dwg: readDwgFile error %u\n",
                         ioReport.errorCode);
        }
    }
    else
    {
        ++fails;
        std::fprintf(report, "FAIL dwg: writeDwgFile failed\n");
    }

    // ---- DXF out -> in ----
    stage("dxfOut");
    std::fprintf(report,
                 "before dxfOut: sample=%p mspace members=%zu entities=%zu\n",
                 (const void *)&sample,
                 sample.modelSpace().entityHandles().size(),
                 sample.entityCount());
    std::fflush(report);
    if (sample.dxfOut("fileio_selftest.dxf"))
    {
        stage("dxfIn");
        AcDbDatabase fromDxf;
        AcDbFileIoReport ioReport;
        if (fromDxf.dxfIn("fileio_selftest.dxf", &ioReport))
        {
            std::fprintf(report, "dxf roundtrip: %zu added, %zu skipped\n",
                         ioReport.entitiesAdded, ioReport.unsupportedSkipped);
            dumpStructure(fromDxf, report, "fromDxf");
            compareModelSpace(sample, fromDxf, report, fails, "dxf");
            compareInsertedAttributes(sample, fromDxf, report, fails,
                                      "dxf");

            // ---- second-generation DXF stability: rewrite what was read,
            // read it again, compare generation to generation ----
            if (fromDxf.dxfOut("fileio_selftest_gen2.dxf"))
            {
                AcDbDatabase gen2;
                if (gen2.dxfIn("fileio_selftest_gen2.dxf", &ioReport))
                {
                    dumpStructure(fromDxf, report, "fromDxf-gen2");
                    dumpStructure(gen2, report, "gen2");
                    compareModelSpace(fromDxf, gen2, report, fails,
                                      "dxf-gen2");
                }
                else
                {
                    ++fails;
                    std::fprintf(report,
                                 "FAIL dxf-gen2: dxfIn error %u\n",
                                 ioReport.errorCode);
                }
            }
            else
            {
                ++fails;
                std::fprintf(report, "FAIL dxf-gen2: dxfOut failed\n");
            }
        }
        else
        {
            ++fails;
            std::fprintf(report, "FAIL dxf: dxfIn error %u\n",
                         ioReport.errorCode);
        }
    }
    else
    {
        ++fails;
        std::fprintf(report, "FAIL dxf: dxfOut failed\n");
    }

    stage("architectural fixture import");
    verifyArchitecturalFixture(report, fails);

    std::fprintf(report, fails == 0 ? "ALL PASS\n" : "%d FAILURES\n", fails);
    std::fclose(report);
    return fails;
}

} // namespace acdb
