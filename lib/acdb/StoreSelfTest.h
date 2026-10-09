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
#include "acgs/AcGsManager.h"
#include "acgs/AcGsView.h"
#include "acgs/DocumentSceneBridge.h"
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
            std::fflush(report);
            std::printf("FAIL %s\n", what);
        }
    };

    // ---------------- ECS scene store (AcDbObjectId keys) ----------------
    {
        acgs::SceneStore store;
        const AcDbObjectId a{AcDbHandle{10}};
        const AcDbObjectId b{AcDbHandle{11}};
        const AcDbObjectId c{AcDbHandle{12}};
        entt::entity ea = store.ensure(a);
        store.ensure(b);
        check(store.contains(a) && store.contains(b) && !store.contains(c),
              "scene store ensures/contains by objectId");
        check(store.find(a) == ea, "find returns the bound entity");
        check(store.objectIdOf(ea) == a, "objectIdOf reverse lookup");
        check(!store.objectIdOf(entt::null).isValid(),
              "objectIdOf rejects foreign entities");
        store.registry().assign<acgs::Drawable>(ea, acgs::Drawable{7});
        store.markDirty(a);
        store.markDirty(b);
        store.markDirty(a); // idempotent tag
        check(store.dirtyCount() == 2, "dirty tag is idempotent");
        std::map<std::uint64_t, int> dirtyVisited;
        store.forEachDirty([&](AcDbObjectId objectId) {
            ++dirtyVisited[objectId.persistentHandle().value];
        });
        check(dirtyVisited.size() == 2 && store.dirtyCount() == 0,
              "forEachDirty visits and clears");
        // Revision semantics: idempotent re-dirty stays put, a fresh tag
        // and a fresh entity each move the generation once.
        const std::uint64_t revisionBefore = store.revision();
        store.markDirty(a); // already mirrored, not dirty anymore
        check(store.revision() == revisionBefore + 1,
              "fresh dirty bumps the revision");
        store.markDirty(a); // idempotent tag
        check(store.revision() == revisionBefore + 1,
              "idempotent dirty keeps the revision");
        store.ensure(c);
        check(store.revision() == revisionBefore + 2,
              "new entity bumps the revision");
        store.forget(b);
        check(!store.contains(b) && store.find(b) == entt::null &&
                  store.contains(c) && store.count() == 2,
              "forget drops the ECS entity only");
    }

    // ---------------- document ↔ scene bridge ----------------
    {
        AcDbDatabase doc;
        acgs::SceneStore store;
        {
            acgs::DocumentSceneBridge bridge(doc, store);
            bridge.open();
            check(store.count() == doc.entityCount() +
                                       doc.nonGraphicalObjectCount(),
                  "open mirrors every current resident");

            AcDbLine line;
            line.start = {0.0, 0.0, 0.0};
            line.end = {1.0, 1.0, 0.0};
            const AcDbHandle liveHandle = doc.addEntity(std::move(line));
            const AcDbObjectId live{liveHandle};
            check(store.contains(live),
                  "addEntity notifies the bridge (ensure)");
            check(store.registry()
                          .get<acgs::LayerRef>(
                              store.find(live))
                          .name == "0",
                  "appended entity mirrors its layer (LayerRef)");

            // Block references additionally mirror their placement.
            AcDbPoint member;
            member.location = {0.0, 0.0, 0.0};
            const AcDbHandle memberHandle =
                doc.addEntity(std::move(member));
            doc.createBlockDefinition("MirrorBlock", {0.0, 0.0, 0.0},
                                      {memberHandle});
            const AcDbHandle refHandle = doc.addBlockReference(
                "MirrorBlock", {5.0, 6.0, 0.0});
            check(refHandle.isValid() &&
                      store.registry().has<acgs::InstanceXform>(
                          store.find(AcDbObjectId{refHandle})),
                  "block reference mirrors its InstanceXform");

            doc.eraseEntity(liveHandle);
            check(store.isDirty(live), "erase notifies dirty");
            store.clearDirty(live);
            doc.uneraseEntity(liveHandle);
            check(store.isDirty(live), "unerase notifies dirty");

            doc.removeEntity(liveHandle);
            check(!store.contains(live) && liveHandle.isValid(),
                  "remove notifies forget");

            // Undo/redo replays through the same notifications.
            AcDbLine undoLine;
            undoLine.start = {2.0, 0.0, 0.0};
            undoLine.end = {3.0, 0.0, 0.0};
            doc.beginTransaction();
            const AcDbHandle undoHandle =
                doc.addEntity(std::move(undoLine));
            const auto delta = doc.commitTransaction();
            check(store.contains(AcDbObjectId{undoHandle}),
                  "transactional insert mirrors");
            doc.applyUndoDelta(delta, false);
            check(!store.contains(AcDbObjectId{undoHandle}),
                  "undo of insert notifies forget");
            doc.applyUndoDelta(delta, true);
            check(store.contains(AcDbObjectId{undoHandle}),
                  "redo of insert notifies ensure");

            // Mutable entity access captures the transaction before-image;
            // commit, undo and redo all refresh render-side attributes and
            // invalidate the entity's cached draw output.
            const AcDbObjectId editedId{undoHandle};
            store.clearDirty(editedId);
            doc.beginTransaction();
            AcDbEntityVariant *editedPayload = doc.getEntityMutable(undoHandle);
            if (editedPayload != nullptr)
            {
                common(*editedPayload).setLayerName("Edited");
                std::get<AcDbLine>(*editedPayload).start = {7.0, 0.0, 0.0};
            }
            const auto editDelta = doc.commitTransaction();
            const entt::entity editedEntity = store.find(editedId);
            check(store.isDirty(editedId) &&
                      store.registry()
                              .get<acgs::LayerRef>(editedEntity)
                              .name == "Edited",
                  "committed edit refreshes attributes and dirties draw cache");
            doc.applyUndoDelta(editDelta, false);
            check(store.isDirty(editedId) &&
                      store.registry()
                              .get<acgs::LayerRef>(editedEntity)
                              .name == "0" &&
                      std::get<AcDbLine>(*doc.getEntity(undoHandle))
                              .start.x == 2.0,
                  "undo refreshes geometry and attributes");
            store.clearDirty(editedId);
            doc.applyUndoDelta(editDelta, true);
            check(store.isDirty(editedId) &&
                      store.registry()
                              .get<acgs::LayerRef>(editedEntity)
                              .name == "Edited" &&
                      std::get<AcDbLine>(*doc.getEntity(undoHandle))
                              .start.x == 7.0,
                  "redo refreshes geometry and attributes");

            // A changed block-reference placement must reach the ECS render
            // projection, not leave its previous instance transform cached.
            const AcDbObjectId referenceId{refHandle};
            store.clearDirty(referenceId);
            doc.beginTransaction();
            AcDbEntityVariant *referencePayload =
                doc.getEntityMutable(refHandle);
            if (referencePayload != nullptr)
                std::get<AcDbBlockReference>(*referencePayload).position =
                    {15.0, 6.0, 0.0};
            const auto referenceDelta = doc.commitTransaction();
            const auto &updatedTransform =
                store.registry()
                    .get<acgs::InstanceXform>(store.find(referenceId))
                    .matrix;
            check(store.isDirty(referenceId) &&
                      std::abs(transformBy(AcGePoint3d(0.0, 0.0, 0.0),
                                           updatedTransform)
                                    .x -
                                15.0) < 1e-9,
                  "block-reference edit refreshes its instance transform");
            doc.applyUndoDelta(referenceDelta, false);
            const auto &undoneTransform =
                store.registry()
                    .get<acgs::InstanceXform>(store.find(referenceId))
                    .matrix;
            check(std::abs(transformBy(AcGePoint3d(0.0, 0.0, 0.0),
                                      undoneTransform)
                               .x -
                           5.0) < 1e-9,
                  "undo restores the block-reference transform");
            doc.applyUndoDelta(referenceDelta, true);
            const auto &redoneTransform =
                store.registry()
                    .get<acgs::InstanceXform>(store.find(referenceId))
                    .matrix;
            check(std::abs(transformBy(AcGePoint3d(0.0, 0.0, 0.0),
                                      redoneTransform)
                               .x -
                           15.0) < 1e-9,
                  "redo restores the block-reference transform");

            // Replacing a loaded document reconciles the mirror instead of
            // silently dropping its reactor subscription.
            AcDbDatabase replacement;
            AcDbLine replacementLine;
            replacementLine.start = {20.0, 0.0, 0.0};
            replacementLine.end = {21.0, 0.0, 0.0};
            const AcDbHandle replacementHandle =
                replacement.addEntity(std::move(replacementLine));
            doc.replaceContents(std::move(replacement));
            check(store.contains(AcDbObjectId{replacementHandle}) &&
                      store.isDirty(AcDbObjectId{replacementHandle}) &&
                      !store.contains(editedId) &&
                      !store.contains(referenceId),
                  "database replacement refreshes and reconciles the scene");
            AcDbPoint afterReplace;
            const AcDbHandle afterReplaceHandle =
                doc.addEntity(std::move(afterReplace));
            check(store.contains(AcDbObjectId{afterReplaceHandle}),
                  "database replacement keeps its reactor attached");
        }
        // Bridge destroyed: later mutations stay unnoticed by the store.
        AcDbPoint stray;
        stray.location = {9.0, 9.0, 0.0};
        const AcDbHandle strayHandle = doc.addEntity(std::move(stray));
        check(!store.contains(AcDbObjectId{strayHandle}),
              "detached bridge no longer mirrors");
    }

    // ---------------- viewport configuration + DB↔GS sync ----------------
    {
        AcDbDatabase doc;
        // The default single viewport exists (ObjectARX VPORT "*Active").
        check(doc.viewportTable().get(kActiveViewportName) != nullptr,
              "default *Active viewport record exists");
        check(doc.cvport() == 1, "cvport defaults to 1");

        // AcGsView ↔ record parameter transfer (ObjectARX DB↔GS sync).
        AcDbViewportTableRecord top;
        top.setTarget(AcGePoint3d(1.0, 2.0, 3.0));
        top.setViewDirection(AcGeVector3d(0.0, 0.0, -1.0)); // looking down
        top.setHeight(40.0);
        top.setLensLength(50.0);
        top.setPerspectiveEnabled(false);
        {
            acgs::AcGsView view;
            view.applyViewportRecord(top);
            check(glm::distance(view.orbitCamera().Target,
                                glm::dvec3(1.0, 2.0, 3.0)) < 1e-9,
                  "apply restores the view target");
            check(view.orthoMode(),
                  "orthographic record restores ortho");
            check(std::abs(view.orbitCamera().orthoSize() - 40.0) <
                      1e-6,
                  "apply restores the view height");

            AcDbViewportTableRecord written;
            view.writeToViewportRecord(written);
            check(std::abs(written.height() - 40.0) < 1e-6,
                  "write-back keeps the height");
            check(!written.isPerspectiveEnabled(),
                  "write-back keeps ortho");
            const AcGeVector3d &direction = written.viewDirection();
            check(direction.z < -0.999 &&
                      std::abs(direction.x) < 1e-9 &&
                      std::abs(direction.y) < 1e-9,
                  "write-back keeps the gaze direction");
            check(std::abs(written.lensLength() - 50.0) < 1e-6,
                  "write-back keeps the lens length");
        }

        // Multi-viewport session (ObjectARX: N views, one device).
        {
            acgs::AcGsManager *manager = acgs::acgsGetManager();
            check(manager->viewCount() >= 1 &&
                      manager->activeView() != nullptr,
                  "manager owns a default active view");
            const std::size_t before = manager->viewCount();
            acgs::AcGsView *extra = manager->createView();
            check(manager->viewCount() == before + 1,
                  "createView appends a viewport");
            check(manager->tryClaimFrameRender(),
                  "frame render slot claims");
            check(!manager->tryClaimFrameRender(),
                  "second claim in a frame is refused");
            manager->resetFrameRender();
            check(manager->tryClaimFrameRender(),
                  "reset reopens the frame slot");
            manager->resetFrameRender();
            manager->destroyView(extra);
            check(manager->viewCount() == before &&
                      manager->activeView() != nullptr,
                  "destroyView keeps the active view valid");
        }
    }

    // ---------------- persistence round-trip ----------------
    AcDbDatabase document;
    AcDbHandle lineHandle{}, blockRefHandle{}, blockMemberAHandle{},
        blockMemberBHandle{}, blockDefinitionHandle{};
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
        blockMemberAHandle = document.addEntity(std::move(memberA));
        AcDbCircle memberB;
        memberB.center = {0.0, 0.0, 0.0};
        memberB.radius = 1.0;
        blockMemberBHandle = document.addEntity(std::move(memberB));
        blockDefinitionHandle = document.createBlockDefinition(
            "Bolt", {0.0, 0.0, 0.0},
            {blockMemberAHandle, blockMemberBHandle});
        for (const AcDbHandle memberHandle :
             {blockMemberAHandle, blockMemberBHandle})
        {
            const AcDbEntityVariant *member =
                document.getEntity(memberHandle);
            check(member != nullptr &&
                      common(*member).ownerHandle == blockDefinitionHandle,
                  "block member owner references its block definition");
        }
        blockRefHandle = document.addBlockReference(
            "Bolt", {5.0, 5.0, 0.0}, 0.5, {2.0, 2.0, 2.0});

        document.setActiveLayerName("Walls");
        document.setActiveColor(glm::vec4(0.9f, 0.5f, 0.1f, 1.0f));
    }
    check(document.entityCount() == 7, "document holds 7 entities");
    check(blockRefHandle.isValid(), "block reference inserted");

    // A configured second viewport rides the save (ObjectARX VPORT).
    {
        AcDbViewportTableRecord &top = document.viewportTable().add(
            "Top", document.allocateHandle());
        top.setTarget(AcGePoint3d(5.0, 6.0, 0.0));
        top.setViewDirection(AcGeVector3d(0.0, 0.0, -1.0));
        top.setHeight(24.0);
        top.setPerspectiveEnabled(false);
        document.setCvport(2);
    }

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
    for (const AcDbHandle memberHandle :
         {blockMemberAHandle, blockMemberBHandle})
    {
        const AcDbEntityVariant *member = loaded.getEntity(memberHandle);
        check(member != nullptr &&
                  common(*member).ownerHandle == blockDefinitionHandle,
              "loaded block member retains its owner");
    }

    // Fresh session, fresh mirror: the loaded document binds into a new
    // SceneStore through the bridge (SQLite 管句柄管文件 → ECS 管
    // objectId 管内存).
    {
        acgs::SceneStore scene;
        acgs::DocumentSceneBridge bridge(loaded, scene);
        bridge.open();
        check(scene.count() == loaded.entityCount() +
                                       loaded.nonGraphicalObjectCount(),
              "loaded document mirrors every resident");
        check(scene.contains(AcDbObjectId{lineHandle}) &&
                  scene.contains(AcDbObjectId{blockRefHandle}),
              "persisted handles resolve to objectIds");
        check(scene.objectIdOf(scene.find(AcDbObjectId{lineHandle}))
                      .persistentHandle() == lineHandle,
              "objectId round-trips back to its persistent handle");
    }

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

    // Viewport configuration rides the round-trip (VPORT rows + CVPORT).
    {
        const AcDbViewportTableRecord *top =
            loaded.viewportTable().get("Top");
        check(top != nullptr && std::abs(top->height() - 24.0) < 1e-9 &&
                  !top->isPerspectiveEnabled() &&
                  std::abs(top->target().x - 5.0) < 1e-9 &&
                  std::abs(top->target().y - 6.0) < 1e-9 &&
                  top->viewDirection().z < -0.999,
              "viewport record round-trips");
        check(loaded.viewportTable().get(kActiveViewportName) != nullptr,
              "default viewport record round-trips");
        check(loaded.cvport() == 2, "cvport round-trips");
    }

    // Named Objects Dictionary: protocol + standard roots + round-trip.
    {
        // The constructor creates the NOD root and the standard
        // ACAD_* subtrees.
        const AcDbHandle nod = document.namedObjectsDictionary();
        check(nod.isValid() &&
                  document.getNonGraphicalObject(nod) != nullptr,
              "NOD root exists");
        check(document.getDictionary(kAcadGroupDictionary) !=
                      kNullHandle &&
                  document.getDictionary(kAcadMlineStyleDictionary) !=
                      kNullHandle &&
                  document.getDictionary(kAcadLayoutDictionary) !=
                      kNullHandle,
              "standard ACAD_* subtrees exist");

        // Path access + deep creation.
        const AcDbHandle styles =
            document.getDictionary("ACAD_MLINESTYLE/Custom/Thin");
        check(styles != kNullHandle,
              "getDictionary creates deep paths");
        check(document.getDictionary("No/Such/Path", false) ==
                  kNullHandle,
              "getDictionary honors createIfNotFound=false");

        // XRecord with tagged slots of every relevant type.
        const AcDbHandle xrecordHandle =
            document.createXRecord(styles, "Parameters");
        check(xrecordHandle.isValid(), "createXRecord succeeds");
        AcDbNonGraphicalObject *object =
            document.getNonGraphicalObjectMutable(xrecordHandle);
        AcDbXrecord *xrecord =
            object ? std::get_if<AcDbXrecord>(object) : nullptr;
        check(xrecord != nullptr, "xrecord payload resolves");
        if (xrecord != nullptr)
        {
            xrecord->setValue(0, true);
            xrecord->setValue(1, 42);
            xrecord->setValue(2, 3.75);
            xrecord->setValue(3, std::string("panel"));
            xrecord->setValue(4, AcGePoint3d(1.0, 2.0, 3.0));
            xrecord->setValue(5, AcGeVector3d(0.0, 0.0, 1.0));
            xrecord->setValue(6, std::vector<double>{1.5, 2.5});
            xrecord->setValue(7, std::vector<int>{7, 8, 9});
            xrecord->setValue(8, std::vector<std::uint8_t>{0xAB, 0xCD});
            xrecord->setValue(
                9, std::vector<AcGePoint3d>{{0.0, 0.0, 0.0},
                                            {1.0, 1.0, 1.0}});
            check(xrecord->numValues() == 10,
                  "xrecord holds 10 tagged slots");
            double width = 0.0;
            check(xrecord->getValue(2, width) &&
                      std::abs(width - 3.75) < 1.0e-12,
                  "xrecord double slot reads back");
            int wrongTypeProbe = 0;
            check(!xrecord->getValue(2, wrongTypeProbe),
                  "typed reader rejects mismatched slot type");
        }

        // Dictionary entry semantics.
        AcDbNonGraphicalObject *customObject =
            document.getNonGraphicalObjectMutable(
                document.getDictionary("ACAD_MLINESTYLE/Custom"));
        AcDbDictionary *stylesDict =
            customObject ? std::get_if<AcDbDictionary>(customObject)
                         : nullptr;
        check(stylesDict != nullptr && stylesDict->has("Thin"),
              "parent dictionary gained the entry");
        check(stylesDict != nullptr &&
                  stylesDict->setName("Thin", "Thinner") &&
                  stylesDict->has("Thinner") && !stylesDict->has("Thin"),
              "setName renames in place");
        stylesDict->setName("Thinner", "Thin");

        // Round-trip: the subtree was built AFTER the first save,
        // so persist it now and reload before asserting survival.
        const StoreResult nodSaved = saveDatabase(
            document, "store_selftest.db");
        check(nodSaved.ok, "NOD re-save succeeds");
        const StoreResult nodLoaded = loadDatabase(
            "store_selftest.db", loaded);
        check(nodLoaded.ok, "NOD reload succeeds");
        if (!nodLoaded.ok)
            std::fprintf(report, "  nod load error: %s\n",
                         nodLoaded.error.c_str());

        const AcDbHandle loadedStyles =
            loaded.getDictionary("ACAD_MLINESTYLE/Custom/Thin", false);
        check(loadedStyles != kNullHandle,
              "NOD subtree survives persistence");
        const AcDbHandle loadedXrecord =
            loaded.getNonGraphicalObjectMutable(loadedStyles) != nullptr
                ? std::get<AcDbDictionary>(
                      *loaded.getNonGraphicalObjectMutable(loadedStyles))
                      .getAt("Parameters")
                : kNullHandle;
        check(loadedXrecord != kNullHandle,
              "xrecord entry survives persistence");
        const AcDbNonGraphicalObject *loadedObject =
            loaded.getNonGraphicalObject(loadedXrecord);
        const AcDbXrecord *loadedX =
            loadedObject
                ? std::get_if<AcDbXrecord>(loadedObject)
                : nullptr;
        check(loadedX != nullptr && loadedX->numValues() == 10,
              "xrecord slots survive persistence");
        if (loadedX != nullptr)
        {
            std::string name;
            AcGePoint3d point;
            std::vector<AcGePoint3d> points;
            check(loadedX->getValue(3, name) && name == "panel" &&
                      loadedX->getValue(4, point) &&
                      point.distanceTo({1.0, 2.0, 3.0}) < 1.0e-12 &&
                      loadedX->getValue(9, points) &&
                      points.size() == 2,
                  "xrecord tagged values round-trip");
            bool flag = false;
            check(loadedX->getValue(0, flag) && flag,
                  "xrecord bool slot round-trips");
        }
        const AcDbHandle loadedNod = loaded.namedObjectsDictionary();
        check(loadedNod.isValid() &&
                  loaded.namedObjectsDictionary() != kNullHandle &&
                  loadedNod == loaded.namedObjectsDictionary(),
              "NOD root handle restored");
        check(loaded.getDictionary("ACAD_GROUP", false) != kNullHandle,
              "standard group subtree restored");
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

    std::remove("store_selftest2.db");

    std::remove("store_selftest.db");
    std::fprintf(report, fails == 0 ? "STORE SELFTEST PASSED\n"
                                    : "STORE SELFTEST FAILED\n");
    std::fflush(report);
    std::fclose(report);
    std::printf(fails == 0 ? "STORE SELFTEST PASSED\n"
                           : "STORE SELFTEST FAILED\n");
    return fails;
}

} // namespace acdb
