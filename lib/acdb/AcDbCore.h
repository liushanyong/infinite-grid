#pragma once

#include "AcDbPort.h"

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

#include "arx/ArxTypes.h"
#include "arx/AcCmEntityColor.h"
#include "acge/AcGePoint.h"
#include "acdb/AcDbHandle.h"
#include "acgi/AcGiDrawable.h"

class AcGiWorldDraw;
class AcGiViewportDraw;
class AcGiDrawableTraits;

namespace acdb
{

// ---- AcDbObject: base state shared by every database-resident object ----

class ACDB_PORT AcDbObject
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
// ObjectARX shape: AcDbEntity derives AcDbObject and AcGiDrawable, so
// every concrete entity (AcDbPolyFaceMesh included) is-a AcGiDrawable and
// the graphics system drives it through the sealed regen protocol.  The
// base stays concrete (soup records embed AcDbEntity by value) — the
// deviation from ARX, where the sub* hooks are pure and the class
// abstract.  Concrete entities override subWorldDraw (the ARX
// custom-entity contract); the default implementations live in
// AcDbEntityWorldDraw.cpp.
//
// Field names intentionally mirror acdb::AcDbEntity (the demo
// payload reads them directly); the accessors carry the ObjectARX
// spellings (setLayerName/getLineWeight/...).  setDatabaseDefaults
// applies the database's active settings exactly like
// AcDbEntity::setDatabaseDefaults.

class ACDB_PORT AcDbEntity : public AcDbObject, public AcGiDrawable
{
public:
    // ---- AcGiDrawable identity ----

    Adesk::Boolean isPersistent() const override
    {
        return handle.isValid();
    }

    acdb::AcDbObjectId id() const override
    {
        return isPersistent() ? acdb::AcDbObjectId(handle)
                              : acdb::AcDbObjectId();
    }

    // AcDbEntity::setLayerName / getLayerName
    void setLayerName(const std::string &layerName) { layer = layerName; }
    const std::string &getLayerName() const { return layer; }

    // AcDbEntity::setColor / getColor
    void setColor(const AcCmEntityColor &value) { color = value; }
    void setColor(const glm::vec4 &value) { color = value; }
    const AcCmEntityColor &getColor() const { return color; }
    void setColorIndex(int value) { color.setColorIndex(value); }
    int colorIndex() const { return color.colorIndex(); }

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
    AcCmEntityColor color;
    std::string lineType = "ByLayer";
    double lineWeight = 0.0;
    bool visible = true;

protected:
    // Protocol hooks.  The base defaults: attributes stamp the entity
    // common (kDrawableIsAnEntity), worldDraw emits nothing (concrete
    // entities override), viewportDraw replays worldDraw.  Definitions in
    // AcDbEntityWorldDraw.cpp.
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    void subViewportDraw(AcGiViewportDraw *vd) override;
};

// ---- Symbol tables (AcDbLayerTable / AcDbLinetypeTable / ...) ----

class ACDB_PORT AcDbSymbolTableRecord : public AcDbObject
{
public:
    const std::string &getRecordName() const { return name_; }
    void setRecordName(const std::string &name) { name_ = name; }

private:
    std::string name_;
};

struct ACDB_PORT AcDbLayerTableRecord : AcDbSymbolTableRecord
{
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "Continuous";
    double lineWeight = 0.0;
    bool isOff = false;
    bool isFrozen = false;
    bool isPlottable = true;
};

struct ACDB_PORT AcDbLinetypeTableRecord : AcDbSymbolTableRecord
{
    std::string description;
};

struct ACDB_PORT AcDbTextStyleTableRecord : AcDbSymbolTableRecord
{
    std::string fileName = "txt.shx";
    std::string bigFontFileName;
    double textSize = 0.0;
    double obliqueAngle = 0.0;
};

// ---- AcDbBlockTableRecord: named container of entity handles ----

class ACDB_PORT AcDbBlockTableRecord : public AcDbSymbolTableRecord
{
public:
    const AcGePoint3d &basePoint() const { return basePoint_; }
    void setBasePoint(const AcGePoint3d &point) { basePoint_ = point; }

    const AcArray<AcDbHandle> &entityHandles() const
    {
        return entityHandles_;
    }
    AcArray<AcDbHandle> &entityHandles() { return entityHandles_; }

    void appendEntityHandle(AcDbHandle handle)
    {
        entityHandles_.push_back(handle);
    }

private:
    AcGePoint3d basePoint_{0.0, 0.0, 0.0};
    AcArray<AcDbHandle> entityHandles_;
};


} // namespace acdb
