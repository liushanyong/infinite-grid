#include "acdb/AcDbDatabase.h"

#include <algorithm>

#include "acgi/AcGiLineType.h"

namespace acdb
{

// ---- common(payload) accessors ----

entities::EntityCommon &common(AcDbEntityVariant &payload)
{
    return std::visit([](auto &entity) -> entities::EntityCommon & {
        return entity.common;
    }, payload);
}

const entities::EntityCommon &common(const AcDbEntityVariant &payload)
{
    return std::visit([](const auto &entity)
                          -> const entities::EntityCommon & {
        return entity.common;
    }, payload);
}

AcDbEntity toAcDbEntity(const entities::EntityCommon &payloadCommon)
{
    AcDbEntity entity;
    entity.setHandle(AcDbHandle{payloadCommon.handle});
    entity.setName(payloadCommon.name);
    entity.setLayerName(payloadCommon.layer);
    entity.setColor(payloadCommon.color);
    entity.setLineType(payloadCommon.lineType);
    entity.setLineWeight(payloadCommon.lineWeight);
    entity.setVisibility(payloadCommon.visible);
    return entity;
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
    record.setHandle(handle);
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
    record.setHandle(handle);
    record.description = description;
}

// ---- AcDbTextStyleTable ----

const AcDbTextStyleTableRecord *AcDbTextStyleTable::get(
    const std::string &name) const
{
    const auto found = records_.find(name);
    return found != records_.end() ? &found->second : nullptr;
}

AcDbTextStyleTableRecord &AcDbTextStyleTable::add(const std::string &name,
                                                  AcDbHandle handle)
{
    AcDbTextStyleTableRecord &record = records_[name];
    record.setRecordName(name);
    record.setHandle(handle);
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
    record.setHandle(handle);
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

} // namespace acdb
