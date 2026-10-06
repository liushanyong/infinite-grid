#include "acdb/AcDbDatabase.h"

#include <algorithm>

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
}

AcDbHandle AcDbDatabase::allocateHandle()
{
    const AcDbHandle allocated = nextHandle_;
    nextHandle_.value += 1;
    return allocated;
}

const AcDbEntityVariant *AcDbDatabase::getEntity(AcDbHandle handle) const
{
    const auto found = entities_.find(handle);
    return found != entities_.end() ? &found->second : nullptr;
}

AcDbEntityVariant *AcDbDatabase::getEntityMutable(AcDbHandle handle)
{
    const auto found = entities_.find(handle);
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

void AcDbDatabase::restoreNextHandle(std::uint64_t next)
{
    if (next > nextHandle_.value)
        nextHandle_.value = next;
}

void AcDbDatabase::eraseEntity(AcDbHandle handle)
{
    if (entities_.count(handle) != 0)
        erased_[handle] = true;
}

void AcDbDatabase::uneraseEntity(AcDbHandle handle)
{
    erased_.erase(handle);
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
    std::vector<AcDbHandle> remaining;
    remaining.reserve(space.entityHandles().size());
    for (const AcDbHandle handle : space.entityHandles())
    {
        const bool isMember =
            std::find(members.begin(), members.end(), handle) !=
            members.end();
        if (isMember)
        {
            record.appendEntityHandle(handle);
        }
        else
        {
            remaining.push_back(handle);
        }
    }
    space.entityHandles() = std::move(remaining);
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
            auto stored = entities_.find(entry.handle);
            if (stored != entities_.end())
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
    return true;
}

} // namespace acdb
