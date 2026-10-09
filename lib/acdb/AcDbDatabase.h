#pragma once

// AcDbDatabase (namespace acdb) — the drawing document, aligned with
// ObjectARX AcDbDatabase as mirrored by the wecad_sdk CoreDB reference:
// symbol tables (layer / linetype / text style / block), a model-space
// AcDbBlockTableRecord, a handle allocator (monotonic, never recycled),
// a flat handle-indexed entity store, and active draw settings applied to
// new entities via AcDbEntity::setDatabaseDefaults.
//
// Storage is value-semantic: payloads live in an
// unordered_map<AcDbHandle, AcDbEntityVariant>; block membership is a
// handle list on each AcDbBlockTableRecord (OpenCADStudio's CadDocument
// keeps the same double bookkeeping — flat store + block handle lists —
// so erasure never rescans block members).

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>

#include "acdb/AcDbDictionary.h"
#include "acdb/AcDbEntities.h"
#include "acdb/AcDbTransaction.h"
#include "acdb/AcDbViewportTable.h"

namespace acdb
{

// ObjectARX: ACDB_MODEL_SPACE.
inline constexpr const char *kModelSpaceName = "*Model_Space";
inline constexpr const char *kPaperSpaceName = "*Paper_Space";

class AcDbDatabase;

// ---- AcDbDatabaseReactor: mutation notifications (ObjectARX reactor) ----
//
// Minimal ARX-faithful lifecycle slice: append / erase / unerase /
// remove / modify, fired after the mutation has committed.  The in-memory
// graphics mirror (acgs::DocumentSceneBridge) subscribes so ECS
// entities stay in step with document residents.  Scope notes: load-time
// bulk inserts (insertLoadedEntity / insertNonGraphicalObject) stay
// silent — a fresh load mirrors in bulk through the bridge's open();
// undo replays fire through the same notifications; non-graphical NOD
// mutations do not notify yet.
class AcDbDatabaseReactor
{
public:
    virtual ~AcDbDatabaseReactor() = default;

    virtual void objectAppended(const AcDbDatabase &database,
                                AcDbHandle handle);
    virtual void objectErased(const AcDbDatabase &database,
                              AcDbHandle handle);
    virtual void objectUnerased(const AcDbDatabase &database,
                                AcDbHandle handle);
    virtual void objectRemoved(const AcDbDatabase &database,
                               AcDbHandle handle);
    virtual void objectModified(const AcDbDatabase &database,
                                AcDbHandle handle);
    virtual void databaseReplaced(const AcDbDatabase &database);
};

class AcDbLayerTable
{
public:
    const AcDbLayerTableRecord *get(const std::string &name) const;
    AcDbLayerTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const { return records_.count(name) != 0; }
    AcDbLayerTableRecord &add(const std::string &name, AcDbHandle handle);
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbLayerTableRecord> records_;
};

class AcDbLinetypeTable
{
public:
    const AcDbLinetypeTableRecord *get(const std::string &name) const;
    AcDbLinetypeTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const { return records_.count(name) != 0; }
    void add(const std::string &name, AcDbHandle handle,
             const std::string &description = {});
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbLinetypeTableRecord> records_;
};

class AcDbTextStyleTable
{
public:
    const AcDbTextStyleTableRecord *get(const std::string &name) const;
    AcDbTextStyleTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const { return records_.count(name) != 0; }
    AcDbTextStyleTableRecord &add(const std::string &name, AcDbHandle handle);
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbTextStyleTableRecord> records_;
};

class AcDbBlockTable
{
public:
    const AcDbBlockTableRecord *get(const std::string &name) const;
    AcDbBlockTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const { return records_.count(name) != 0; }
    AcDbBlockTableRecord &add(const std::string &name, AcDbHandle handle);
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbBlockTableRecord> records_;
};

class AcDbDatabase
{
public:
    // ObjectARX: creates the standard tables and their default records
    // (layer "0", linetypes ByLayer/ByBlock/Continuous, Standard text
    // style, model + paper space block records).
    AcDbDatabase();

    // ---- identity ----
    // Monotonic handle allocation; handles are never recycled, matching
    // the handle-based DWG/DXF formats (OpenCADStudio issue #67: a table
    // record without a real handle is dropped on save).
    AcDbHandle allocateHandle();

    // ---- reactors (ObjectARX AcDbDatabase::addReactor) ----
    void addReactor(AcDbDatabaseReactor *reactor);
    void removeReactor(AcDbDatabaseReactor *reactor);

    // Store/import introspection: the next handle allocateHandle would
    // return (persisted in the meta table so a load never collides).
    std::uint64_t nextHandleValue() const { return nextHandle_.value; }

    // ---- tables ----
    AcDbLayerTable &layerTable() { return layerTable_; }
    const AcDbLayerTable &layerTable() const { return layerTable_; }
    AcDbLinetypeTable &linetypeTable() { return linetypeTable_; }
    const AcDbLinetypeTable &linetypeTable() const { return linetypeTable_; }
    AcDbTextStyleTable &textStyleTable() { return textStyleTable_; }
    const AcDbTextStyleTable &textStyleTable() const { return textStyleTable_; }
    AcDbBlockTable &blockTable() { return blockTable_; }
    const AcDbBlockTable &blockTable() const { return blockTable_; }

    // ---- viewports / named views (ObjectARX VPORT / VIEW tables) ----
    // Persistent viewport configuration: acgs::AcGsView is the live
    // session state of one record (bidirectional parameter transfer in
    // AcGsView::applyViewportRecord / writeToViewportRecord).
    AcDbViewportTable &viewportTable() { return viewportTable_; }
    const AcDbViewportTable &viewportTable() const { return viewportTable_; }
    AcDbViewTable &viewTable() { return viewTable_; }
    const AcDbViewTable &viewTable() const { return viewTable_; }

    // Active viewport number (ObjectARX CVPORT, stored in the database
    // header): 1 = the single default viewport.
    int cvport() const { return cvport_; }
    void setCvport(int number) { cvport_ = number > 0 ? number : 1; }

    // Model space is the record stored in the block table under
    // kModelSpaceName — not a copy — so membership edits via either
    // handle stay consistent.
    AcDbBlockTableRecord &modelSpace()
    {
        return *blockTable_.getMutable(kModelSpaceName);
    }
    const AcDbBlockTableRecord &modelSpace() const
    {
        return *blockTable_.get(kModelSpaceName);
    }

    // ---- entity space ----
    // Adds |payload| to the database and appends its handle to model
    // space (AcDbDatabase::addDbEntity).  A payload whose common handle
    // is unset receives a fresh one; an already-set handle is honored
    // (the DWG import path) and must not collide.
    template <typename EntityType>
    AcDbHandle addEntity(EntityType payload)
    {
        AcDbHandle handle = payload.common.handle;
        if (!handle.isValid())
        {
            handle = allocateHandle();
            payload.common.handle = handle;
        }
        AcDbEntityVariant stored = std::move(payload);
        const AcDbHandle inserted = common(stored).handle;
        if (entities_.count(inserted) != 0)
            return kNullHandle; // honored-but-colliding import handle
        // Change primitives record their own before-image while a
        // transaction is active (OpenCADStudio UndoRecording); for an
        // insertion that image is "absent".
        captureBefore(inserted);
        entities_.emplace(inserted, std::move(stored));
        modelSpace().appendEntityHandle(inserted);
        notifyAppended(inserted);
        return inserted;
    }

    const AcDbEntityVariant *getEntity(AcDbHandle handle) const;
    // Mutable edits are captured by the active transaction; commit publishes
    // a modification notification so scene mirrors refresh only committed data.
    AcDbEntityVariant *getEntityMutable(AcDbHandle handle);

    // Store/import path (AcDbStore::loadDatabase): inserts a fully
    // formed payload WITHOUT touching model-space membership — block
    // membership is restored from the block-table rows themselves.
    // Returns false when |payload|'s handle collides or is invalid.
    bool insertLoadedEntity(AcDbEntityVariant payload);
    // Replaces persisted document state without detaching live reactors.
    void replaceContents(AcDbDatabase &&loaded);

    // Store/import path: restores the monotonic handle counter so
    // handles allocated after a load never collide with persisted ones.
    void restoreNextHandle(std::uint64_t next);

    template <typename Fn> void forEachEntity(Fn &&fn) const
    {
        for (const auto &[handle, payload] : entities_)
            fn(handle, payload);
    }
    std::size_t entityCount() const { return entities_.size(); }

    // Erasure keeps the record resident with a flag (undo-friendly).
    void eraseEntity(AcDbHandle handle);
    void uneraseEntity(AcDbHandle handle);
    bool isErased(AcDbHandle handle) const;

    // True removal: drops the payload from the store and the handle from
    // model-space membership.  Auto-captures the before-image inside an
    // active transaction, so undo restores both.
    bool removeEntity(AcDbHandle handle);

    // ---- active draw settings (ObjectARX database defaults) ----
    const std::string &activeLayerName() const { return activeLayer_; }
    bool setActiveLayerName(const std::string &name);

    const glm::vec4 &activeColor() const { return activeColor_; }
    void setActiveColor(const glm::vec4 &color) { activeColor_ = color; }

    const std::string &activeLineType() const { return activeLineType_; }
    bool setActiveLineType(const std::string &name);

    double activeLineTypeScale() const { return activeLineTypeScale_; }
    void setActiveLineTypeScale(double scale) { activeLineTypeScale_ = scale; }

    double activeLineWeight() const { return activeLineWeight_; }
    void setActiveLineWeight(double weight) { activeLineWeight_ = weight; }

    // ---- blocks (AcDbBlockTableRecord / AcDbBlockReference) ----

    // Defines a block record from model-space-resident entities: the
    // members move out of model-space membership into the record
    // (OpenCADStudio create_block_from_entities); the payloads stay in
    // the flat store with ownerHandle pointing at the record.
    AcDbHandle createBlockDefinition(const std::string &name,
                                     const AcGePoint3d &basePoint,
                                     const std::vector<AcDbHandle> &members);

    // Adds an INSERT of |recordName| at |position| to model space.
    // Returns kNullHandle when the record does not exist.
    AcDbHandle addBlockReference(const std::string &recordName,
                                 const AcGePoint3d &position,
                                 double rotation = 0.0,
                                 const AcGeVector3d &scale =
                                     AcGeVector3d(1.0, 1.0, 1.0));

    // Resolves one block reference's placement matrix against its
    // record's base point; identity when the record is missing.
    AcGeMatrix3d referenceTransform(
        const AcDbBlockReference &reference) const;

    // ---- Named Objects Dictionary (ObjectARX NOD subtree) ----

    // The NOD root (created by the constructor alongside the standard
    // ACAD_GROUP / ACAD_MLINESTYLE / ACAD_LAYOUT child dictionaries).
    AcDbHandle namedObjectsDictionary() const
    {
        return namedObjectsHandle_;
    }
    AcDbDictionary *namedObjectsDictionaryMutable();
    const AcDbDictionary *namedObjectsDictionaryObject() const;

    const AcDbNonGraphicalObject *getNonGraphicalObject(
        AcDbHandle handle) const;
    AcDbNonGraphicalObject *getNonGraphicalObjectMutable(
        AcDbHandle handle);
    std::size_t nonGraphicalObjectCount() const
    {
        return nonGraphicalObjects_.size();
    }

    // CoreDB getDictionary(key, createIfNotFound): resolves a '/'
    // separated path of dictionary names from the NOD root, creating
    // missing levels when asked.  Returns kNullHandle on a missing
    // path (createIfNotFound=false) or when an intermediate entry is
    // not a dictionary.
    AcDbHandle getDictionary(const std::string &path,
                             bool createIfNotFound = true);

    // Creates a dictionary entry under |parentDictionaryHandle| and
    // returns the new dictionary's handle (kNullHandle when the parent
    // is not a dictionary or the name is taken).
    AcDbHandle createSubDictionary(AcDbHandle parentDictionaryHandle,
                                   const std::string &name);

    // Creates an xrecord entry under |parentDictionaryHandle| and
    // returns the xrecord's handle.
    AcDbHandle createXRecord(AcDbHandle parentDictionaryHandle,
                             const std::string &name);

    template <typename Fn> void forEachNonGraphicalObject(Fn &&fn) const
    {
        for (const auto &[handle, object] : nonGraphicalObjects_)
            fn(handle, object);
    }

    // Store/import path: inserts a fully formed non-graphical object
    // under a known handle (no allocation, no NOD entry — the parent
    // dictionary rows restore the entries themselves).
    bool insertNonGraphicalObject(AcDbHandle handle,
                                  AcDbNonGraphicalObject object)
    {
        if (!handle.isValid() || nonGraphicalObjects_.count(handle) != 0)
            return false;
        nonGraphicalObjects_.emplace(handle, std::move(object));
        return true;
    }

    // Store/import path: repoints the NOD root at the persisted handle.
    void restoreNamedObjectsDictionary(AcDbHandle handle)
    {
        namedObjectsHandle_ = handle;
    }

    // Store/import path: drops the constructor-created default NOD
    // subtree so the persisted rows can take their original handles.
    void clearNonGraphicalObjects()
    {
        nonGraphicalObjects_.clear();
        namedObjectsHandle_ = {};
    }

    // Depth-first expansion of a block record into leaf-entity instances
    // (ObjectARX: the Gs replay of block contents).  |fn| receives the
    // composed world transform, the member handle, and its payload for
    // every non-INSERT member.  Nesting is bounded (32 deep, OpenCADStudio
    // parity) and cycle-guarded by the record-name stack, so a block that
    // references itself terminates.
    template <typename Fn>
    void walkInsertInstances(const std::string &recordName,
                             const glm::dmat4 &parentTransform,
                             Fn &&fn) const
    {
        std::vector<std::string> nameStack;
        std::vector<AcDbHandle> insertPath;
        walkInsertInstances(recordName, parentTransform, nameStack,
                            insertPath, fn);
    }

private:
    template <typename Fn>
    void walkInsertInstances(const std::string &recordName,
                             const glm::dmat4 &parentTransform,
                             std::vector<std::string> &nameStack,
                             std::vector<AcDbHandle> &insertPath,
                             Fn &&fn) const
    {
        if (nameStack.size() >= kMaxInsertDepth)
            return;
        if (std::find(nameStack.begin(), nameStack.end(), recordName) !=
            nameStack.end())
            return; // circular block reference
        const AcDbBlockTableRecord *record = blockTable_.get(recordName);
        if (record == nullptr)
            return;
        nameStack.push_back(recordName);
        for (const AcDbHandle memberHandle : record->entityHandles())
        {
            const AcDbEntityVariant *payload = getEntity(memberHandle);
            if (payload == nullptr)
                continue;
            if (isErased(memberHandle))
                continue; // erased members leave the instance picture
            if (const auto *reference =
                    std::get_if<AcDbBlockReference>(payload))
            {
                insertPath.push_back(memberHandle);
                walkInsertInstances(reference->blockTableRecordName,
                                    parentTransform *
                                        glm::dmat4(referenceTransform(
                                            *reference)),
                                    nameStack, insertPath, fn);
                insertPath.pop_back();
            }
            else
            {
                fn(parentTransform, memberHandle, *payload);
            }
        }
        nameStack.pop_back();
    }

public:
    // ---- transactions / undo ----

    // Single active transaction (AcDbDatabase::startTransaction in ARX
    // hosts one per document scope); begin while one is active reuses it.
    void beginTransaction();
    // Captures the payload's before-image (first capture wins).
    void captureBefore(AcDbHandle handle);
    // Pairs before-images with after-images and returns the delta; the
    // transaction closes.  An empty transaction returns an empty delta.
    AcDbUndoDelta commitTransaction();
    bool transactionActive() const { return transaction_.isActive(); }

    // Applies one side of |delta| to the store and model-space
    // membership (undo takes before, redo takes after).  Returns the
    // number of entries applied.
    std::size_t applyUndoDelta(const AcDbUndoDelta &delta, bool forward);

private:
    void storeEntityState(AcDbHandle handle,
                          std::optional<AcDbEntityVariant> &slot) const;

    void notifyAppended(AcDbHandle handle);
    void notifyErased(AcDbHandle handle);
    void notifyUnerased(AcDbHandle handle);
    void notifyRemoved(AcDbHandle handle);
    void notifyModified(AcDbHandle handle);
    void notifyDatabaseReplaced();

public:

private:
    void createDefaults();

    AcDbHandle nextHandle_{1}; // allocateHandle() returns and increments

    AcDbLayerTable layerTable_;
    AcDbLinetypeTable linetypeTable_;
    AcDbTextStyleTable textStyleTable_;
    AcDbBlockTable blockTable_;
    AcDbViewportTable viewportTable_;
    AcDbViewTable viewTable_;
    int cvport_ = 1;

    std::unordered_map<AcDbHandle, AcDbEntityVariant> entities_;
    std::unordered_map<AcDbHandle, bool> erased_;

    // ---- Named Objects Dictionary subtree ----
    // The NOD root plus every dictionary / xrecord hanging under it.
    // Same bookkeeping split as entities: a flat handle-keyed store
    // here, name->handle entries inside each AcDbDictionary.
    AcDbHandle namedObjectsHandle_{};
    std::unordered_map<AcDbHandle, AcDbNonGraphicalObject>
        nonGraphicalObjects_;
    std::vector<AcDbDatabaseReactor *> reactors_;
    AcDbTransaction transaction_;

    static constexpr std::size_t kMaxInsertDepth = 32;

    std::string activeLayer_ = "0";
    glm::vec4 activeColor_{1.0f, 1.0f, 1.0f, 1.0f};
    std::string activeLineType_ = "ByLayer";
    double activeLineTypeScale_ = 1.0;
    double activeLineWeight_ = 0.0;
};

} // namespace acdb
