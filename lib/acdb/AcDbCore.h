#pragma once

// AcDb — the database layer (namespace acdb), aligned with ObjectARX AcDb.
//
// ObjectARX organizes a drawing as AcDbDatabase owning symbol tables
// (layer / linetype / text style / block), a model-space block table
// record, and a flat object space where every resident object carries a
// persistent AcDbHandle.  This module reproduces that shape over the
// project's value-type entity payloads (lib/entities): the database stores
// them in a handle-indexed variant, assigns handles on insertion, and
// tracks block membership through AcDbBlockTableRecord entity lists.
//
// Naming follows the ObjectARX alignment law; the storage style is
// value-semantic (variant + unordered_map) instead of ARX's pointer
// graph, matching the rest of this codebase.

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace acdb
{

// ---- AcDbHandle: persistent object identifier (DXF/DWG handle) ----

struct AcDbHandle
{
    std::uint64_t value = 0;

    // Implicit conversions so payload field reads/writes
    // (entity.common.handle = 0xCAFE) keep compiling against u64 handles.
    AcDbHandle() = default;
    constexpr AcDbHandle(std::uint64_t v) : value(v) {}
    operator std::uint64_t() const { return value; }

    bool isValid() const { return value != 0; }
    bool operator==(const AcDbHandle &other) const = default;
};

constexpr AcDbHandle kNullHandle{};

} // namespace acdb

template <> struct std::hash<acdb::AcDbHandle>
{
    std::size_t operator()(const acdb::AcDbHandle &handle) const noexcept
    {
        return std::hash<std::uint64_t>()(handle.value);
    }
};

namespace acdb
{

// ---- AcDbObject: base state shared by every database-resident object ----

class AcDbObject
{
public:
    // Public fields: the entity payloads embed these by value and the
    // render pipeline reads them directly (ObjectARX's open/close protocol
    // is out of scope for the value-type layer).
    AcDbHandle handle;
    AcDbHandle ownerHandle;
    bool erased = false;
};

// ---- AcDbEntity: common entity properties ----
//
// Field names intentionally mirror acdb::AcDbEntity (the demo
// payload reads them directly); the accessors carry the ObjectARX
// spellings (setLayerName/getLineWeight/...).  setDatabaseDefaults
// applies the database's active settings exactly like
// AcDbEntity::setDatabaseDefaults.

class AcDbEntity : public AcDbObject
{
public:
    // AcDbEntity::setLayerName / getLayerName
    void setLayerName(const std::string &layerName) { layer = layerName; }
    const std::string &getLayerName() const { return layer; }

    // AcDbEntity::setColor / getColor
    void setColor(const glm::vec4 &value) { color = value; }
    const glm::vec4 &getColor() const { return color; }

    // AcDbEntity::setLineType / getLineType
    void setLineType(const std::string &value) { lineType = value; }
    const std::string &getLineType() const { return lineType; }

    // AcDbEntity::setLineWeight / getLineWeight
    void setLineWeight(double value) { lineWeight = value; }
    double getLineWeight() const { return lineWeight; }

    // AcDbEntity::setVisibility
    void setVisibility(bool value) { visible = value; }
    bool getVisibility() const { return visible; }

    // AcDbEntity::setName (DXF group 1 for non-graphical naming)
    void setName(const std::string &value) { name = value; }
    const std::string &getName() const { return name; }

    // Applies the database's active defaults to the unset properties
    // (ByLayer-style fields inherit; values already set stay).
    void setDatabaseDefaults(const class AcDbDatabase &database);

    // Payload fields (shared shape with acdb::AcDbEntity).
    std::string name;
    std::string layer = "0";
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "ByLayer";
    double lineWeight = 0.0;
    bool visible = true;
};

// ---- Symbol tables (AcDbLayerTable / AcDbLinetypeTable / ...) ----

class AcDbSymbolTableRecord : public AcDbObject
{
public:
    const std::string &getRecordName() const { return name_; }
    void setRecordName(const std::string &name) { name_ = name; }

private:
    std::string name_;
};

struct AcDbLayerTableRecord : AcDbSymbolTableRecord
{
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "Continuous";
    double lineWeight = 0.0;
    bool isOff = false;
    bool isFrozen = false;
    bool isPlottable = true;
};

struct AcDbLinetypeTableRecord : AcDbSymbolTableRecord
{
    std::string description;
};

struct AcDbTextStyleTableRecord : AcDbSymbolTableRecord
{
    std::string fileName = "txt.shx";
    std::string bigFontFileName;
    double textSize = 0.0;
    double obliqueAngle = 0.0;
};

// ---- AcDbBlockTableRecord: named container of entity handles ----

class AcDbBlockTableRecord : public AcDbSymbolTableRecord
{
public:
    const glm::dvec3 &basePoint() const { return basePoint_; }
    void setBasePoint(const glm::dvec3 &point) { basePoint_ = point; }

    const std::vector<AcDbHandle> &entityHandles() const
    {
        return entityHandles_;
    }
    std::vector<AcDbHandle> &entityHandles() { return entityHandles_; }

    void appendEntityHandle(AcDbHandle handle)
    {
        entityHandles_.push_back(handle);
    }

private:
    glm::dvec3 basePoint_{0.0};
    std::vector<AcDbHandle> entityHandles_;
};


} // namespace acdb
