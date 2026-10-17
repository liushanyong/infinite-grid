#pragma once

#include "AcDbPort.h"

// AcDbDictionary / AcDbXrecord (namespace acdb) — the Named Objects
// Dictionary subtree, the naming container that sits BESIDE the four
// symbol tables: symbol tables hold the table-keyed records (layer /
// linetype / text style / block), and every other "locate-by-name,
// not-a-table" object (groups, mline styles, layouts, materials, ...)
// hangs under the database's NOD root as ObjectARX prescribes
// (ACAD_GROUP / ACAD_MLINESTYLE / ACAD_LAYOUT ...).
//
// Design follows the wecad_sdk CoreDB reference (AcDbDictionary.h /
// AcDbXrecord.h) translated into the value-semantic architecture:
//   * a dictionary ENTRY is {name -> object handle}; the object itself
//     lives in the database's flat non-graphical store (the same
//     bookkeeping split as entities vs. model-space membership), so a
//     dictionary never holds pointers and nested dictionaries are just
//     more entries;
//   * AcDbXrecord carries tagged data slots keyed by integer id
//     (CoreDB's setValue/getValue overload set) — the resbuf payload
//     of ObjectARX XRecords in value form.
//
// Payload variant is open-ended: AcDbGroup / AcDbMlineStyle /
// AcDbLayout record types slot into it when they arrive.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "acdb/AcDbCore.h"
#include "acdb/AcDbMaterial.h"
#include "acge/AcGePoint.h"

namespace acdb
{

// Standard NOD subtree keys (ObjectARX fixed names).
inline constexpr const char *kAcadGroupDictionary = "ACAD_GROUP";
inline constexpr const char *kAcadMlineStyleDictionary =
    "ACAD_MLINESTYLE";
inline constexpr const char *kAcadLayoutDictionary = "ACAD_LAYOUT";
inline constexpr const char *kAcadMaterialDictionary = "ACAD_MATERIAL";

// ---- AcDbDictionary: ordered name -> object-handle entries ----

class ACDB_PORT AcDbDictionary : public AcDbObject
{
public:
    // AcDbDictionary::numEntries
    int numEntries() const;

    // AcDbDictionary::has
    bool has(const std::string &name) const;

    // AcDbDictionary::getAt — the referenced object's handle, or
    // kNullHandle when absent.
    AcDbHandle getAt(const std::string &name) const;

    // AcDbDictionary::setAt — inserts or repoints |name| at
    // |objectHandle|.  Returns false when the handle is invalid.
    bool setAt(const std::string &name, AcDbHandle objectHandle);

    // AcDbDictionary::setName — renames an entry in place (the object
    // handle and its identity are untouched).
    bool setName(const std::string &oldName, const std::string &newName);

    // AcDbDictionary::remove
    bool remove(const std::string &name);

    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, handle] : entries_)
            fn(name, handle);
    }

private:
    std::map<std::string, AcDbHandle> entries_;
};

// ---- AcDbMaterialRecord: a named material resident in ACAD_MATERIAL ----

struct ACDB_PORT AcDbMaterialRecord : AcDbObject
{
    // Dictionary entries point at records; the local name makes a material
    // record independently inspectable while still preserving its NOD key.
    std::string name;
    AcDbMaterial material;
};

// ---- AcDbXrecord: tagged data slots keyed by integer id ----

class ACDB_PORT AcDbXrecord : public AcDbObject
{
public:
    // Tagged slot values (the value-semantic form of a resbuf chain).
    using Value = std::variant<bool, std::int32_t, double, std::string,
                               AcGePoint3d, AcGeVector3d,
                               std::vector<double>,
                               std::vector<std::int32_t>,
                               std::vector<std::uint8_t>,
                               std::vector<AcGePoint3d>>;

    // CoreDB-style typed writers.
    void setValue(int id, bool v);
    void setValue(int id, int v);
    void setValue(int id, double v);
    void setValue(int id, const std::string &v);
    void setValue(int id, const AcGePoint3d &v);
    void setValue(int id, const AcGeVector3d &v);
    void setValue(int id, const std::vector<double> &v);
    void setValue(int id, const std::vector<int> &v);
    void setValue(int id, const std::vector<std::uint8_t> &v);
    void setValue(int id, const std::vector<AcGePoint3d> &v);
    void setValue(int id, const Value &v);

    // CoreDB-style typed readers: true when the slot exists and holds
    // the requested type.
    bool getValue(int id, bool &v) const;
    bool getValue(int id, int &v) const;
    bool getValue(int id, double &v) const;
    bool getValue(int id, std::string &v) const;
    bool getValue(int id, AcGePoint3d &v) const;
    bool getValue(int id, AcGeVector3d &v) const;
    bool getValue(int id, std::vector<double> &v) const;
    bool getValue(int id, std::vector<int> &v) const;
    bool getValue(int id, std::vector<std::uint8_t> &v) const;
    bool getValue(int id, std::vector<AcGePoint3d> &v) const;
    bool getValue(int id, Value &v) const;

    int numValues() const;
    void removeValue(int id);

    template <typename Fn> void forEachValue(Fn &&fn) const
    {
        for (const auto &[id, value] : values_)
            fn(id, value);
    }

private:
    template <typename T> bool readValue(int id, T &v) const
    {
        const auto found = values_.find(id);
        if (found == values_.end())
            return false;
        const T *slot = std::get_if<T>(&found->second);
        if (slot == nullptr)
            return false;
        v = *slot;
        return true;
    }

    std::map<int, Value> values_;
};

// ---- the non-graphical object payload variant ----

using AcDbNonGraphicalObject = std::variant<AcDbDictionary, AcDbXrecord, AcDbMaterialRecord>;

} // namespace acdb
