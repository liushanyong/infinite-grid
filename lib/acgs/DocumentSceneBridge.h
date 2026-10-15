#pragma once

#include "AcGsPort.h"

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

class ACGS_PORT DocumentSceneBridge : public acdb::AcDbDatabaseReactor
{
public:
    DocumentSceneBridge(acdb::AcDbDatabase &document, SceneStore &store);
    ~DocumentSceneBridge() override;

    DocumentSceneBridge(const DocumentSceneBridge &) = delete;
    DocumentSceneBridge &operator=(const DocumentSceneBridge &) = delete;

    // Mirrors current document residents, refreshes drawable attributes,
    // marks resident entities dirty, and forgets handles absent after load.
    void open();

    SceneStore &store() { return store_; }
    const SceneStore &store() const { return store_; }

    // ---- AcDbDatabaseReactor ----

    void objectAppended(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override;
    void objectErased(const acdb::AcDbDatabase &,
                      acdb::AcDbHandle handle) override;
    void objectUnerased(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override;
    void objectRemoved(const acdb::AcDbDatabase &,
                       acdb::AcDbHandle handle) override;
    void objectModified(const acdb::AcDbDatabase &,
                        acdb::AcDbHandle handle) override;
    void databaseReplaced(const acdb::AcDbDatabase &) override;

private:
    // Populates the attribute components of one mirrored entity from its
    // document payload (v1: LayerRef on every entity, InstanceXform on
    // block references — the resolved placement matrix).  Payload edits
    // re-attach through markDirty + the batch-sync follow-up; today a
    // mutated payload refreshes only when its entity is re-ensured.
    void attachAttributes(acdb::AcDbHandle handle);

    acdb::AcDbDatabase &document_;
    SceneStore &store_;
};

} // namespace acgs
