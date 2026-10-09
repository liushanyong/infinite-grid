#pragma once

// acgs::DocumentSceneBridge — the document↔memory binding behind the
// identity split "SQLite 管理 AcDbHandle 管文件，ECS 管理
// AcDbObjectId 管内存": an acdb::AcDbDatabaseReactor that keeps one
// acgs::SceneStore mirroring one acdb::AcDbDatabase for the life of
// the session.  The document stays the sole owner of residents (the
// SQLite/AcDbStore side persists them handle-keyed); the bridge only
// maintains the in-memory ECS mirror:
//
//   open()            bulk mirror of every current resident (call
//                     after construction and after loadDatabase)
//   objectAppended    resident added   -> ECS entity ensured
//   objectErased      flagged erased   -> Dirty (visual drops out)
//   objectUnerased    flag cleared     -> Dirty (visual returns)
//   objectRemoved     record dropped   -> ECS entity forgotten
//
// Undo replays through the same notifications (applyUndoDelta fires
// appended/unerased/removed), so a resurrected resident re-enters the
// mirror automatically.  Non-graphical NOD mutations do not notify
// yet — open() re-mirror covers loaded documents; live NOD growth is
// a follow-up.

#include <unordered_set>
#include <vector>

#include "acdb/AcDbDatabase.h"
#include "acgs/EcsScene.h"

namespace acgs
{

class DocumentSceneBridge : public acdb::AcDbDatabaseReactor
{
public:
    DocumentSceneBridge(acdb::AcDbDatabase &document, SceneStore &store)
        : document_(document), store_(store)
    {
        document_.addReactor(this);
    }

    ~DocumentSceneBridge() override { document_.removeReactor(this); }

    DocumentSceneBridge(const DocumentSceneBridge &) = delete;
    DocumentSceneBridge &operator=(const DocumentSceneBridge &) = delete;

    // Mirrors current document residents, refreshes drawable attributes,
    // marks resident entities dirty, and forgets handles absent after load.
    void open()
    {
        std::unordered_set<std::uint64_t> residents;
        document_.forEachEntity([&](acdb::AcDbHandle handle,
                                    const acdb::AcDbEntityVariant &) {
            residents.insert(handle.value);
            store_.ensure(acdb::AcDbObjectId{handle});
            attachAttributes(handle);
            store_.markDirty(acdb::AcDbObjectId{handle});
        });
        document_.forEachNonGraphicalObject(
            [&](acdb::AcDbHandle handle,
                const acdb::AcDbNonGraphicalObject &) {
                residents.insert(handle.value);
                store_.ensure(acdb::AcDbObjectId{handle});
            });
        std::vector<acdb::AcDbObjectId> stale;
        store_.forEachObjectId([&](acdb::AcDbObjectId objectId) {
            if (!residents.contains(objectId.persistentHandle().value))
                stale.push_back(objectId);
        });
        for (const acdb::AcDbObjectId objectId : stale)
            store_.forget(objectId);
    }

    SceneStore &store() { return store_; }
    const SceneStore &store() const { return store_; }

    // ---- AcDbDatabaseReactor ----

    void objectAppended(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override
    {
        store_.ensure(acdb::AcDbObjectId{handle});
        attachAttributes(handle);
    }

    void objectErased(const acdb::AcDbDatabase &,
                      acdb::AcDbHandle handle) override
    {
        store_.markDirty(acdb::AcDbObjectId{handle});
    }

    void objectUnerased(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override
    {
        attachAttributes(handle);
        store_.markDirty(acdb::AcDbObjectId{handle});
    }

    void objectRemoved(const acdb::AcDbDatabase &,
                       acdb::AcDbHandle handle) override
    {
        store_.forget(acdb::AcDbObjectId{handle});
    }

    void objectModified(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override
    {
        attachAttributes(handle);
        store_.markDirty(acdb::AcDbObjectId{handle});
    }

    void databaseReplaced(const acdb::AcDbDatabase &) override { open(); }

private:
    // Populates the attribute components of one mirrored entity from its
    // document payload (v1: LayerRef on every entity, InstanceXform on
    // block references — the resolved placement matrix).  Payload edits
    // re-attach through markDirty + the batch-sync follow-up; today a
    // mutated payload refreshes only when its entity is re-ensured.
    void attachAttributes(acdb::AcDbHandle handle)
    {
        const entt::entity entity = store_.find(acdb::AcDbObjectId{handle});
        if (entity == entt::null)
            return;
        const acdb::AcDbEntityVariant *payload =
            document_.getEntity(handle);
        if (payload == nullptr)
            return;
        entt::registry &registry = store_.registry();
        const auto assignOrReplace = [&](auto &&component) {
            using Component =
                std::decay_t<decltype(component)>;
            if (registry.has<Component>(entity))
                registry.replace<Component>(
                    entity, std::forward<decltype(component)>(component));
            else
                registry.assign<Component>(
                    entity, std::forward<decltype(component)>(component));
        };
        assignOrReplace(LayerRef{acdb::common(*payload).getLayerName()});
        if (const auto *reference =
                std::get_if<acdb::AcDbBlockReference>(payload))
        {
            assignOrReplace(
                InstanceXform{document_.referenceTransform(*reference)});
        }
        else if (registry.has<InstanceXform>(entity))
        {
            registry.remove<InstanceXform>(entity);
        }
    }

    acdb::AcDbDatabase &document_;
    SceneStore &store_;
};

} // namespace acgs
