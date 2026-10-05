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

#include "acdb/AcDbBlockReference.h"
#include "entities/arc.h"
#include "entities/circle.h"
#include "entities/ellipse.h"
#include "entities/hatch.h"
#include "entities/light.h"
#include "entities/line.h"
#include "entities/lwpolyline.h"
#include "entities/mesh.h"
#include "entities/mline.h"
#include "entities/mtext.h"
#include "entities/point.h"
#include "entities/polyline.h"
#include "entities/ray.h"
#include "entities/solid.h"
#include "entities/solid3d.h"
#include "entities/spline.h"
#include "entities/text.h"
#include "entities/xline.h"

namespace acdb
{

// ---- AcDbHandle: persistent object identifier (DXF/DWG handle) ----

struct AcDbHandle
{
    std::uint64_t value = 0;

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
    AcDbHandle handle() const { return handle_; }
    void setHandle(AcDbHandle handle) { handle_ = handle; }

    AcDbHandle ownerHandle() const { return ownerHandle_; }
    void setOwnerHandle(AcDbHandle owner) { ownerHandle_ = owner; }

    // Erase semantics (ObjectARX: erasure is a flag until the database
    // is compacted; undo can unerase with the original handle).
    void erase() { erased_ = true; }
    void unerase() { erased_ = false; }
    bool isErased() const { return erased_; }

private:
    AcDbHandle handle_;
    AcDbHandle ownerHandle_;
    bool erased_ = false;
};

// ---- AcDbEntity: common entity properties ----
//
// Field names intentionally mirror entities::EntityCommon (the demo
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

    // Payload fields (shared shape with entities::EntityCommon).
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

// Concrete entity payload set.  Each member is a lib/entities value type;
// geometry stays untouched while acdb owns identity and membership.
using AcDbEntityVariant = std::variant<
    entities::Line, entities::Arc, entities::Circle, entities::Ellipse,
    entities::Point, entities::Ray, entities::XLine, entities::Solid,
    entities::Hatch, entities::Polyline, entities::LwPolyline,
    entities::Spline, entities::Text, entities::MText, entities::MLine,
    entities::Mesh, entities::Solid3d, entities::Light,
    AcDbBlockReference>;

// Read-write access to the common-properties member shared by every
// payload type (all of them embed entities::EntityCommon as `common`).
entities::EntityCommon &common(AcDbEntityVariant &payload);
const entities::EntityCommon &common(const AcDbEntityVariant &payload);

// ARX view of a payload's common properties (migration seam until the
// lib/entities payloads embed acdb::AcDbEntity directly).
AcDbEntity toAcDbEntity(const entities::EntityCommon &payloadCommon);

} // namespace acdb
