#include "acdb/AcDbDatabase.h"

#include <algorithm>
#include <unordered_set>

#include "acgi/AcGiLineType.h"

namespace acdb
{

// ---- common(payload) accessors ----

AcDbEntity &common(AcDbEntityVariant &payload)
{
    return std::visit([](auto &entity) -> AcDbEntity & {
        return entity.common;
    }, payload);
}

const AcDbEntity &common(const AcDbEntityVariant &payload)
{
    return std::visit([](const auto &entity) -> const AcDbEntity & {
        return entity.common;
    }, payload);
}

// ---- AcDbEntity::setDatabaseDefaults ----

void AcDbEntity::setDatabaseDefaults(const AcDbDatabase &database)
{
    if (layer.empty() || layer == "0")
        layer = database.activeLayerName();
    if (lineType.empty() || lineType == "ByLayer")
        lineType = database.activeLineType();
    if (lineWeight <= 0.0)
        lineWeight = database.activeLineWeight();
    // The entity keeps its own color: ByLayer resolution happens at draw
    // time through AcGsView, mirroring ObjectARX.
}

// ---- AcDbLayerTable ----

const AcDbLayerTableRecord *AcDbLayerTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbLayerTableRecord *AcDbLayerTable::getMutable(const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbLayerTableRecord &AcDbLayerTable::add(const std::string &name,
                                          AcDbHandle handle)
{
    AcDbLayerTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    return record;
}

// ---- AcDbLinetypeTable ----

const AcDbLinetypeTableRecord *AcDbLinetypeTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

void AcDbLinetypeTable::add(const std::string &name, AcDbHandle handle,
                            const std::string &description)
{
    AcDbLinetypeTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    record.description = description;
}

AcDbLinetypeTableRecord *AcDbLinetypeTable::getMutable(
    const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

// ---- AcDbTextStyleTable ----

const AcDbTextStyleTableRecord *AcDbTextStyleTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbTextStyleTableRecord *AcDbTextStyleTable::getMutable(
    const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbTextStyleTableRecord &AcDbTextStyleTable::add(const std::string &name,
                                                  AcDbHandle handle)
{
    AcDbTextStyleTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    return record;
}

// ---- AcDbBlockTable ----

const AcDbBlockTableRecord *AcDbBlockTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbBlockTableRecord *AcDbBlockTable::getMutable(const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbBlockTableRecord &AcDbBlockTable::add(const std::string &name,
                                          AcDbHandle handle)
{
    AcDbBlockTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    return record;
}

// ---- AcDbViewportTable / AcDbViewTable ----

const AcDbViewportTableRecord *AcDbViewportTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbViewportTableRecord *AcDbViewportTable::getMutable(
    const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbViewportTableRecord &AcDbViewportTable::add(const std::string &name,
                                                AcDbHandle handle)
{
    AcDbViewportTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    return record;
}

const AcDbViewTableRecord *AcDbViewTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbViewTableRecord *AcDbViewTable::getMutable(const std::string &name)
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbViewTableRecord &AcDbViewTable::add(const std::string &name,
                                        AcDbHandle handle)
{
    AcDbViewTableRecord &record = records_[name];
    record.setRecordName(name);
    record.handle = handle;
    return record;
}

// ---- AcDbDatabase ----

AcDbDatabase::AcDbDatabase()
{
    createDefaults();
}

void AcDbDatabase::createDefaults()
{
    layerTable_.add("0", allocateHandle());

    linetypeTable_.add("ByLayer", allocateHandle(),
                       "Linetype determined by the layer");
    linetypeTable_.add("ByBlock", allocateHandle(),
                       "Linetype determined by the block reference");
    linetypeTable_.add("Continuous", allocateHandle(), "Solid line");
    // Seed the standard repeating patterns shared with the AcGi protocol
    // layer so document-resident entities can reference them by name.
    for (const AcGiLineType &preset : acgi_line_types::kLineTypes)
        linetypeTable_.add(preset.name, allocateHandle());

    textStyleTable_.add("Standard", allocateHandle());

    blockTable_.add(kModelSpaceName, allocateHandle());
    blockTable_.add(kPaperSpaceName, allocateHandle());

    // The default single model-space viewport (ObjectARX VPORT "*Active");
    // named views start empty.
    viewportTable_.add(kActiveViewportName, allocateHandle());

    // Named Objects Dictionary root + the standard ACAD_* subtrees
    // (group / mlinestyle / layout containers; materials arrive
    // with the material object type).
    AcDbDictionary root;
    namedObjectsHandle_ = allocateHandle();
    root.handle = namedObjectsHandle_;
    nonGraphicalObjects_.emplace(namedObjectsHandle_,
                                 std::move(root));
    for (const char *standardName :
         {kAcadGroupDictionary, kAcadMlineStyleDictionary,
          kAcadLayoutDictionary})
        createSubDictionary(namedObjectsHandle_, standardName);
}

AcDbHandle AcDbDatabase::allocateHandle()
{
    const AcDbHandle allocated = nextHandle_;
    nextHandle_.value += 1;
    return allocated;
}

// ---- reactors ----

void AcDbDatabase::addReactor(AcDbDatabaseReactor *reactor)
{
    if (reactor != nullptr &&
        std::find(reactors_.begin(), reactors_.end(), reactor) ==
            reactors_.end())
        reactors_.push_back(reactor);
}

void AcDbDatabase::removeReactor(AcDbDatabaseReactor *reactor)
{
    std::erase(reactors_, reactor);
}

void AcDbDatabaseReactor::objectAppended(const AcDbDatabase &, AcDbHandle) {}
void AcDbDatabaseReactor::objectErased(const AcDbDatabase &, AcDbHandle) {}
void AcDbDatabaseReactor::objectUnerased(const AcDbDatabase &, AcDbHandle) {}
void AcDbDatabaseReactor::objectRemoved(const AcDbDatabase &, AcDbHandle) {}
void AcDbDatabaseReactor::objectModified(const AcDbDatabase &, AcDbHandle) {}

void AcDbDatabase::notifyAppended(AcDbHandle handle)
{
    for (AcDbDatabaseReactor *reactor : reactors_)
        reactor->objectAppended(*this, handle);
}

void AcDbDatabase::notifyErased(AcDbHandle handle)
{
    for (AcDbDatabaseReactor *reactor : reactors_)
        reactor->objectErased(*this, handle);
}

void AcDbDatabase::notifyUnerased(AcDbHandle handle)
{
    for (AcDbDatabaseReactor *reactor : reactors_)
        reactor->objectUnerased(*this, handle);
}

void AcDbDatabase::notifyRemoved(AcDbHandle handle)
{
    for (AcDbDatabaseReactor *reactor : reactors_)
        reactor->objectRemoved(*this, handle);
}

void AcDbDatabase::notifyModified(AcDbHandle handle)
{
    for (AcDbDatabaseReactor *reactor : reactors_)
        reactor->objectModified(*this, handle);
}

const AcDbEntityVariant *AcDbDatabase::getEntity(AcDbHandle handle) const
{
    const auto found = entities_.find(handle);
    return found != entities_.end() ? &found->second : nullptr;
}

AcDbEntityVariant *AcDbDatabase::getEntityMutable(AcDbHandle handle)
{
    const auto found = entities_.find(handle);
    if (found != entities_.end())
        captureBefore(handle);
    return found != entities_.end() ? &found->second : nullptr;
}

bool AcDbDatabase::insertLoadedEntity(AcDbEntityVariant payload)
{
    const AcDbHandle handle = common(payload).handle;
    if (!handle.isValid() || entities_.count(handle) != 0)
        return false;
    entities_.emplace(handle, std::move(payload));
    erased_[handle] = false;
    return true;
}

// ---- Named Objects Dictionary ----

AcDbDictionary *AcDbDatabase::namedObjectsDictionaryMutable()
{
    const auto found = nonGraphicalObjects_.find(namedObjectsHandle_);
    return found != nonGraphicalObjects_.end()
               ? std::get_if<AcDbDictionary>(&found->second)
               : nullptr;
}

const AcDbDictionary *AcDbDatabase::namedObjectsDictionaryObject() const
{
    const auto found = nonGraphicalObjects_.find(namedObjectsHandle_);
    return found != nonGraphicalObjects_.end()
               ? std::get_if<AcDbDictionary>(&found->second)
               : nullptr;
}

const AcDbNonGraphicalObject *AcDbDatabase::getNonGraphicalObject(
    AcDbHandle handle) const
{
    const auto found = nonGraphicalObjects_.find(handle);
    return found != nonGraphicalObjects_.end() ? &found->second
                                               : nullptr;
}

AcDbNonGraphicalObject *AcDbDatabase::getNonGraphicalObjectMutable(
    AcDbHandle handle)
{
    const auto found = nonGraphicalObjects_.find(handle);
    return found != nonGraphicalObjects_.end() ? &found->second
                                               : nullptr;
}

AcDbHandle AcDbDatabase::getDictionary(const std::string &path,
                                       bool createIfNotFound)
{
    AcDbHandle current = namedObjectsHandle_;
    std::size_t start = 0;
    while (start <= path.size())
    {
        const std::size_t slash = path.find(char(47), start);
        const std::string name = path.substr(
            start, slash == std::string::npos ? std::string::npos
                                              : slash - start);
        if (!name.empty())
        {
            AcDbNonGraphicalObject *level =
                getNonGraphicalObjectMutable(current);
            AcDbDictionary *dictionary =
                level ? std::get_if<AcDbDictionary>(level) : nullptr;
            if (dictionary == nullptr)
                return kNullHandle;
            AcDbHandle next = dictionary->getAt(name);
            if (next == kNullHandle)
            {
                if (!createIfNotFound)
                    return kNullHandle;
                next = createSubDictionary(current, name);
                if (next == kNullHandle)
                    return kNullHandle;
            }
            current = next;
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return current;
}

AcDbHandle AcDbDatabase::createSubDictionary(
    AcDbHandle parentDictionaryHandle, const std::string &name)
{
    AcDbNonGraphicalObject *parent =
        getNonGraphicalObjectMutable(parentDictionaryHandle);
    AcDbDictionary *dictionary =
        parent ? std::get_if<AcDbDictionary>(parent) : nullptr;
    if (dictionary == nullptr || name.empty() ||
        dictionary->has(name))
        return dictionary != nullptr ? dictionary->getAt(name)
                                     : kNullHandle;
    AcDbDictionary created;
    const AcDbHandle handle = allocateHandle();
    created.handle = handle;
    created.ownerHandle = parentDictionaryHandle;
    nonGraphicalObjects_.emplace(handle, std::move(created));
    dictionary->setAt(name, handle);
    return handle;
}

AcDbHandle AcDbDatabase::createXRecord(
    AcDbHandle parentDictionaryHandle, const std::string &name)
{
    AcDbNonGraphicalObject *parent =
        getNonGraphicalObjectMutable(parentDictionaryHandle);
    AcDbDictionary *dictionary =
        parent ? std::get_if<AcDbDictionary>(parent) : nullptr;
    if (dictionary == nullptr || name.empty())
        return kNullHandle;
    AcDbXrecord created;
    const AcDbHandle handle = allocateHandle();
    created.handle = handle;
    created.ownerHandle = parentDictionaryHandle;
    nonGraphicalObjects_.emplace(handle, std::move(created));
    dictionary->setAt(name, handle);
    return handle;
}

void AcDbDatabase::restoreNextHandle(std::uint64_t next)
{
    if (next > nextHandle_.value)
        nextHandle_.value = next;
}

void AcDbDatabase::eraseEntity(AcDbHandle handle)
{
    if (entities_.count(handle) != 0)
    {
        erased_[handle] = true;
        notifyErased(handle);
    }
}

void AcDbDatabase::uneraseEntity(AcDbHandle handle)
{
    if (erased_.erase(handle) != 0)
        notifyUnerased(handle);
}

bool AcDbDatabase::isErased(AcDbHandle handle) const
{
    return erased_.count(handle) != 0;
}

bool AcDbDatabase::setActiveLayerName(const std::string &name)
{
    if (!layerTable_.contains(name))
        return false;
    activeLayer_ = name;
    return true;
}

bool AcDbDatabase::setActiveLineType(const std::string &name)
{
    if (!linetypeTable_.contains(name))
        return false;
    activeLineType_ = name;
    return true;
}

// ---- blocks ----

AcDbHandle AcDbDatabase::createBlockDefinition(
    const std::string &name, const AcGePoint3d &basePoint,
    const std::vector<AcDbHandle> &members)
{
    if (blockTable_.contains(name))
        return kNullHandle;
    const AcDbHandle recordHandle = allocateHandle();
    AcDbBlockTableRecord &record = blockTable_.add(name, recordHandle);
    record.setBasePoint(basePoint);

    // Move membership: out of model space, into the record; the payloads
    // stay resident with ownerHandle = record (OpenCADStudio's
    // create_block_from_entities bookkeeping).
    AcDbBlockTableRecord &space = modelSpace();
    const std::unordered_set<AcDbHandle> memberSet(members.begin(),
                                                   members.end());
    std::vector<AcDbHandle> &spaceHandles = space.entityHandles();
    std::vector<AcDbHandle> remaining;
    remaining.reserve(spaceHandles.size());
    for (const AcDbHandle handle : spaceHandles)
    {
        if (memberSet.contains(handle))
        {
            record.appendEntityHandle(handle);
            if (AcDbEntityVariant *payload = getEntityMutable(handle))
                common(*payload).ownerHandle = recordHandle;
        }
        else
        {
            remaining.push_back(handle);
        }
    }
    spaceHandles = std::move(remaining);
    return recordHandle;
}

AcDbHandle AcDbDatabase::addBlockReference(
    const std::string &recordName, const AcGePoint3d &position,
    double rotation, const AcGeVector3d &scale)
{
    if (!blockTable_.contains(recordName))
        return kNullHandle;
    AcDbBlockReference reference;
    reference.blockTableRecordName = recordName;
    reference.position = position;
    reference.rotation = rotation;
    reference.scale = scale;
    return addEntity(std::move(reference));
}

AcGeMatrix3d AcDbDatabase::referenceTransform(
    const AcDbBlockReference &reference) const
{
    const AcDbBlockTableRecord *record =
        blockTable_.get(reference.blockTableRecordName);
    return reference.toMatrix(record != nullptr
                                  ? record->basePoint()
                                  : AcGePoint3d(0.0, 0.0, 0.0));
}

// ---- transactions ----

void AcDbDatabase::storeEntityState(
    AcDbHandle handle, std::optional<AcDbEntityVariant> &slot) const
{
    const AcDbEntityVariant *payload = getEntity(handle);
    if (payload != nullptr)
        slot = *payload;
    else
        slot = std::nullopt;
}

void AcDbDatabase::beginTransaction()
{
    if (transaction_.isActive())
        return;
    transaction_.active_ = true;
    transaction_.captureOrder_.clear();
    transaction_.before_.clear();
}

void AcDbDatabase::captureBefore(AcDbHandle handle)
{
    if (!transaction_.isActive())
        return;
    if (transaction_.before_.count(handle) != 0)
        return; // first capture wins (OpenCADStudio UndoRecording)
    transaction_.captureOrder_.push_back(handle);
    storeEntityState(handle, transaction_.before_[handle]);
}

AcDbUndoDelta AcDbDatabase::commitTransaction()
{
    AcDbUndoDelta delta;
    if (!transaction_.isActive())
        return delta;
    delta.reserve(transaction_.captureOrder_.size());
    for (const AcDbHandle handle : transaction_.captureOrder_)
    {
        AcDbUndoEntry entry;
        entry.handle = handle;
        entry.before = transaction_.before_[handle];
        storeEntityState(handle, entry.after);
        delta.push_back(std::move(entry));
    }
    transaction_.active_ = false;
    for (const AcDbUndoEntry &entry : delta)
    {
        if (entry.before.has_value() && entry.after.has_value())
            notifyModified(entry.handle);
    }
    transaction_.captureOrder_.clear();
    transaction_.before_.clear();
    return delta;
}

std::size_t AcDbDatabase::applyUndoDelta(const AcDbUndoDelta &delta,
                                         bool forward)
{
    std::size_t applied = 0;
    for (const AcDbUndoEntry &entry : delta)
    {
        const std::optional<AcDbEntityVariant> &target =
            forward ? entry.after : entry.before;
        if (target.has_value())
        {
            AcDbEntityVariant payload = *target;
            // Keep the delta's original handle (OpenCADStudio
            // restore_entity_arc reinserts with the original handle).
            const bool wasErased = erased_.count(entry.handle) != 0;
            auto stored = entities_.find(entry.handle);
            const bool fresh = stored == entities_.end();
            if (!fresh)
            {
                stored->second = std::move(payload);
            }
            else
            {
                entities_.emplace(entry.handle, std::move(payload));
                // Restore the membership the state implies: an entity not
                // member of any block record lives in model space (block
                // membership itself is out of delta scope).
                bool blockMember = false;
                blockTable_.forEach([&](const std::string &,
                                        const AcDbBlockTableRecord &record) {
                    const std::vector<AcDbHandle> &handles =
                        record.entityHandles();
                    blockMember = blockMember ||
                        std::find(handles.begin(), handles.end(),
                                  entry.handle) != handles.end();
                });
                if (!blockMember)
                    modelSpace().appendEntityHandle(entry.handle);
            }
            erased_.erase(entry.handle);
            // Undo replays through the same lifecycle notifications:
            // a resurrected resident re-enters the mirror, one restored
            // from an erased flag turns visible again.
            if (fresh)
                notifyAppended(entry.handle);
            else if (wasErased)
                notifyUnerased(entry.handle);
            else
                notifyModified(entry.handle);
        }
        else
        {
            removeEntity(entry.handle);
        }
        ++applied;
    }
    return applied;
}

bool AcDbDatabase::removeEntity(AcDbHandle handle)
{
    captureBefore(handle);
    if (entities_.erase(handle) == 0)
        return false;
    erased_.erase(handle);
    AcDbBlockTableRecord &space = modelSpace();
    std::vector<AcDbHandle> &handles = space.entityHandles();
    handles.erase(std::remove(handles.begin(), handles.end(), handle),
                  handles.end());
    notifyRemoved(handle);
    return true;
}

} // namespace acdb
