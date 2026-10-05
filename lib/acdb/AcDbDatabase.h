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

#include <functional>
#include <string>
#include <unordered_map>

#include "acdb/AcDbCore.h"

namespace acdb
{

// ObjectARX: ACDB_MODEL_SPACE.
inline constexpr const char *kModelSpaceName = "*Model_Space";
inline constexpr const char *kPaperSpaceName = "*Paper_Space";

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

    // ---- tables ----
    AcDbLayerTable &layerTable() { return layerTable_; }
    const AcDbLayerTable &layerTable() const { return layerTable_; }
    AcDbLinetypeTable &linetypeTable() { return linetypeTable_; }
    const AcDbLinetypeTable &linetypeTable() const { return linetypeTable_; }
    AcDbTextStyleTable &textStyleTable() { return textStyleTable_; }
    const AcDbTextStyleTable &textStyleTable() const { return textStyleTable_; }
    AcDbBlockTable &blockTable() { return blockTable_; }
    const AcDbBlockTable &blockTable() const { return blockTable_; }

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
        AcDbHandle handle{payload.common.handle};
        if (!handle.isValid())
        {
            handle = allocateHandle();
            payload.common.handle = static_cast<std::uint32_t>(handle.value);
        }
        AcDbEntityVariant stored = std::move(payload);
        const AcDbHandle inserted =
            std::visit([](auto &entity) {
                return AcDbHandle{entity.common.handle};
            }, stored);
        if (entities_.count(inserted) != 0)
            return kNullHandle; // honored-but-colliding import handle
        entities_.emplace(inserted, std::move(stored));
        modelSpace().appendEntityHandle(inserted);
        return inserted;
    }

    const AcDbEntityVariant *getEntity(AcDbHandle handle) const;
    AcDbEntityVariant *getEntityMutable(AcDbHandle handle);

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

private:
    void createDefaults();

    AcDbHandle nextHandle_{1}; // allocateHandle() returns and increments

    AcDbLayerTable layerTable_;
    AcDbLinetypeTable linetypeTable_;
    AcDbTextStyleTable textStyleTable_;
    AcDbBlockTable blockTable_;

    std::unordered_map<AcDbHandle, AcDbEntityVariant> entities_;
    std::unordered_map<AcDbHandle, bool> erased_;

    std::string activeLayer_ = "0";
    glm::vec4 activeColor_{1.0f, 1.0f, 1.0f, 1.0f};
    std::string activeLineType_ = "ByLayer";
    double activeLineTypeScale_ = 1.0;
    double activeLineWeight_ = 0.0;
};

} // namespace acdb
