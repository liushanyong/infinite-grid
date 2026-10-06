#pragma once

// AcDb store self-test (hidden runtime mode): GRID_SELFTEST=store runs
// the SQLite persistence round-trip and the ECS scene-store checks in
// the trusted WINDOW host, reporting through store_selftest.log (GUI
// processes have no valid stderr handle).

#include <cstdio>
#include <map>
#include <string>

#include <entt/entt.hpp>

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbEntities.h"
#include "acdb/AcDbStore.h"
#include "acgs/EcsScene.h"

namespace acdb
{

inline int runStoreSelfTest()
{
    FILE *report = std::fopen("store_selftest.log", "w");
    if (report == nullptr)
        return 99;
    int fails = 0;
    auto check = [&](bool condition, const char *what) {
        if (!condition)
        {
            ++fails;
            std::fprintf(report, "FAIL %s\n", what);
            std::printf("FAIL %s\n", what);
        }
    };

    // ---------------- ECS scene store ----------------
    {
        acgs::SceneStore store;
        AcDbHandle a{10}, b{11}, c{12};
        entt::entity ea = store.ensure(a);
        store.ensure(b);
        check(store.contains(a) && store.contains(b) && !store.contains(c),
              "scene store ensures/contains by handle");
        check(store.find(a) == ea, "find returns the bound entity");
        store.registry().assign<acgs::Drawable>(ea,
                                                acgs::Drawable{10});
        store.markDirty(a);
        store.markDirty(b);
        store.markDirty(a); // idempotent tag
        check(store.dirtyCount() == 2, "dirty tag is idempotent");
        std::map<std::uint64_t, int> dirtyVisited;
        store.forEachDirty([&](AcDbHandle handle) {
            ++dirtyVisited[handle.value];
        });
        check(dirtyVisited.size() == 2 && store.dirtyCount() == 0,
              "forEachDirty visits and clears");
        store.forget(b);
        check(!store.contains(b) && store.count() == 1,
              "forget drops the ECS entity only");
    }

    // ---------------- persistence round-trip ----------------
    AcDbDatabase document;
    AcDbHandle lineHandle{}, blockRefHandle{};
    {
        AcDbLayerTableRecord &walls = document.layerTable().add(
            "Walls", document.allocateHandle());
        walls.color = glm::vec4(0.8f, 0.2f, 0.1f, 1.0f);
        walls.isFrozen = true;

        AcDbLine line;
        line.common.setLayerName("Walls");
        line.start = {1.0, 2.0, 3.0};
        line.end = {4.0, 5.0, 6.0};
        line.thickness = 0.5;
        lineHandle = document.addEntity(std::move(line));

        AcDbArc arc;
        arc.center = {7.0, 8.0, 9.0};
        arc.radius = 2.5;
        arc.startAngle = 0.25;
        arc.endAngle = 1.75;
        document.addEntity(std::move(arc));

        AcDbText text;
        text.insertion = {0.0, 0.0, 0.0};
        text.height = 3.5;
        text.text = "起居室\n面积 42m2";
        text.styleName = "Standard";
        document.addEntity(std::move(text));

        AcDbSpline spline;
        spline.degree = 3;
        spline.controlPoints = {{0.0, 0.0, 0.0}, {1.0, 2.0, 0.0},
                                {3.0, 2.0, 0.0}, {4.0, 0.0, 0.0}};
        spline.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
        document.addEntity(std::move(spline));

        // Block definition over two members + one reference.
        AcDbPoint memberA;
        memberA.location = {0.0, 0.0, 0.0};
        const AcDbHandle memberAHandle =
            document.addEntity(std::move(memberA));
        AcDbCircle memberB;
        memberB.center = {0.0, 0.0, 0.0};
        memberB.radius = 1.0;
        const AcDbHandle memberBHandle =
            document.addEntity(std::move(memberB));
        document.createBlockDefinition("Bolt", {0.0, 0.0, 0.0},
                                       {memberAHandle, memberBHandle});
        blockRefHandle = document.addBlockReference(
            "Bolt", {5.0, 5.0, 0.0}, 0.5, {2.0, 2.0, 2.0});

        document.setActiveLayerName("Walls");
        document.setActiveColor(glm::vec4(0.9f, 0.5f, 0.1f, 1.0f));
    }
    check(document.entityCount() == 7, "document holds 7 entities");
    check(blockRefHandle.isValid(), "block reference inserted");

    const StoreResult saved = saveDatabase(document,
                                           "store_selftest.db");
    check(saved.ok, "saveDatabase succeeds");
    check(saved.entities == 7, "save wrote 7 entity rows");
    if (!saved.ok)
        std::fprintf(report, "  save error: %s\n",
                     saved.error.c_str());

    AcDbDatabase loaded;
    const StoreResult load = loadDatabase("store_selftest.db", loaded);
    check(load.ok, "loadDatabase succeeds");
    if (!load.ok)
        std::fprintf(report, "  load error: %s\n",
                     load.error.c_str());
    check(load.entities == 7, "load read 7 entity rows");
    check(loaded.entityCount() == document.entityCount(),
          "loaded entity count matches");

    // Handles and payloads survive verbatim.
    {
        const AcDbEntityVariant *line =
            loaded.getEntity(lineHandle);
        check(line != nullptr &&
                  std::holds_alternative<AcDbLine>(*line),
              "line survives as AcDbLine");
        if (line && std::holds_alternative<AcDbLine>(*line))
        {
            const AcDbLine &payload = std::get<AcDbLine>(*line);
            check(payload.start.distanceTo({1.0, 2.0, 3.0}) < 1.0e-12 &&
                      payload.end.distanceTo({4.0, 5.0, 6.0}) < 1.0e-12,
                  "line geometry round-trips");
            check(payload.common.getLayerName() == "Walls",
                  "line layer round-trips");
            check(std::abs(payload.thickness - 0.5) < 1.0e-12,
                  "line thickness round-trips");
        }
        const AcDbEntityVariant *text = loaded.getEntity(
            loaded.modelSpace().entityHandles()[2]);
        check(text != nullptr &&
                  std::holds_alternative<AcDbText>(*text) &&
                  std::get<AcDbText>(*text).text == "起居室\n面积 42m2",
              "text payload (UTF-8) round-trips");
    }

    // Block membership and reference placement.
    {
        const AcDbBlockTableRecord *bolt =
            loaded.blockTable().get("Bolt");
        check(bolt != nullptr && bolt->entityHandles().size() == 2,
              "block definition members round-trip");
        const AcDbEntityVariant *reference =
            loaded.getEntity(blockRefHandle);
        check(reference != nullptr &&
                  std::holds_alternative<AcDbBlockReference>(
                      *reference),
              "block reference survives");
        if (reference &&
            std::holds_alternative<AcDbBlockReference>(*reference))
        {
            const AcDbBlockReference &ref =
                std::get<AcDbBlockReference>(*reference);
            check(ref.position.x == 5.0 && ref.rotation == 0.5 &&
                      ref.scale.x == 2.0,
                  "block reference placement round-trips");
        }
    }

    // Layer record and active settings.
    {
        const AcDbLayerTableRecord *walls =
            loaded.layerTable().get("Walls");
        check(walls != nullptr &&
                  walls->color.r == 0.8f &&
                  walls->color.g == 0.2f && walls->isFrozen,
              "layer record round-trips");
        check(loaded.activeLayerName() == "Walls" &&
                  std::abs(loaded.activeColor().r - 0.9f) < 1.0e-6,
              "active settings round-trip");
    }

    // Handle counter: allocations after load never collide.
    {
        const AcDbHandle fresh = loaded.allocateHandle();
        check(loaded.getEntity(fresh) == nullptr &&
                  fresh.value > blockRefHandle.value,
              "handle counter restored above persisted handles");
    }

    // Re-save the loaded document: still a clean round-trip.
    {
        const StoreResult resaved = saveDatabase(loaded,
                                                 "store_selftest2.db");
        check(resaved.ok && resaved.entities == 7,
              "loaded document re-saves cleanly");
    }

    std::remove("store_selftest.db");
    std::remove("store_selftest2.db");

    std::fprintf(report, fails == 0 ? "STORE SELFTEST PASSED\n"
                                    : "STORE SELFTEST FAILED\n");
    std::fflush(report);
    std::fclose(report);
    std::printf(fails == 0 ? "STORE SELFTEST PASSED\n"
                           : "STORE SELFTEST FAILED\n");
    return fails;
}

} // namespace acdb
