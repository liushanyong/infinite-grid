#pragma once

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
#include "acdb/AcDbEntities.h"
#include "acdb/AcDbFileIo.h"

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
        if (payload.common.colorIndex == 256)
        {
            payload.common.colorIndex = 3;
            payload.common.color = {0.0, 1.0, 0.0, 1.0};
        }
        return database.addEntity(std::move(payload));
    };

    AcDbLine line;
    line.start = AcGePoint3d(0.0, 0.0, 0.0);
    line.end = AcGePoint3d(100.0, 0.0, 0.0);
    line.common.layer = "Walls";
    line.common.colorIndex = 1;
    line.common.color = {1.0, 0.0, 0.0, 1.0};
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
    text.text = "Hello DWG";
    add(text);

    AcDbMText mtext;
    mtext.insertion = AcGePoint3d(200.0, 500.0, 0.0);
    mtext.width = 80.0;
    mtext.height = 8.0;
    mtext.direction = AcGeVector3d(1.0, 0.0, 0.0);
    mtext.text = "MTEXT body";
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

    std::fprintf(report, fails == 0 ? "ALL PASS\n" : "%d FAILURES\n", fails);
    std::fclose(report);
    return fails;
}

} // namespace acdb
