#pragma once

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
#include "ge/gepoint.h"

namespace acdb
{

// Standard NOD subtree keys (ObjectARX fixed names).
inline constexpr const char *kAcadGroupDictionary = "ACAD_GROUP";
inline constexpr const char *kAcadMlineStyleDictionary =
    "ACAD_MLINESTYLE";
inline constexpr const char *kAcadLayoutDictionary = "ACAD_LAYOUT";
inline constexpr const char *kAcadMaterialDictionary = "ACAD_MATERIAL";

// ---- AcDbDictionary: ordered name -> object-handle entries ----

class AcDbDictionary : public AcDbObject
{
public:
    // AcDbDictionary::numEntries
    int numEntries() const { return int(entries_.size()); }

    // AcDbDictionary::has
    bool has(const std::string &name) const
    {
        return entries_.count(name) != 0;
    }

    // AcDbDictionary::getAt — the referenced object's handle, or
    // kNullHandle when absent.
    AcDbHandle getAt(const std::string &name) const
    {
        const auto found = entries_.find(name);
        return found != entries_.end() ? found->second : kNullHandle;
    }

    // AcDbDictionary::setAt — inserts or repoints |name| at
    // |objectHandle|.  Returns false when the handle is invalid.
    bool setAt(const std::string &name, AcDbHandle objectHandle)
    {
        if (!objectHandle.isValid())
            return false;
        entries_[name] = objectHandle;
        return true;
    }

    // AcDbDictionary::setName — renames an entry in place (the object
    // handle and its identity are untouched).
    bool setName(const std::string &oldName, const std::string &newName)
    {
        const auto found = entries_.find(oldName);
        if (found == entries_.end() || oldName == newName)
            return false;
        const AcDbHandle handle = found->second;
        entries_.erase(found);
        entries_[newName] = handle;
        return true;
    }

    // AcDbDictionary::remove
    bool remove(const std::string &name)
    {
        return entries_.erase(name) != 0;
    }

    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, handle] : entries_)
            fn(name, handle);
    }

private:
    std::map<std::string, AcDbHandle> entries_;
};

// ---- AcDbXrecord: tagged data slots keyed by integer id ----

class AcDbXrecord : public AcDbObject
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
    void setValue(int id, bool v) { values_[id] = Value(v); }
    void setValue(int id, int v) { values_[id] = Value(std::int32_t(v)); }
    void setValue(int id, double v) { values_[id] = Value(v); }
    void setValue(int id, const std::string &v)
    {
        values_[id] = Value(v);
    }
    void setValue(int id, const AcGePoint3d &v) { values_[id] = Value(v); }
    void setValue(int id, const AcGeVector3d &v)
    {
        values_[id] = Value(v);
    }
    void setValue(int id, const std::vector<double> &v)
    {
        values_[id] = Value(v);
    }
    void setValue(int id, const std::vector<int> &v)
    {
        Value out(std::vector<std::int32_t>{});
        auto &target = std::get<std::vector<std::int32_t>>(out);
        target.assign(v.begin(), v.end());
        values_[id] = std::move(out);
    }
    void setValue(int id, const std::vector<std::uint8_t> &v)
    {
        values_[id] = Value(v);
    }
    void setValue(int id, const std::vector<AcGePoint3d> &v)
    {
        values_[id] = Value(v);
    }
    void setValue(int id, const Value &v) { values_[id] = v; }

    // CoreDB-style typed readers: true when the slot exists and holds
    // the requested type.
    bool getValue(int id, bool &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, int &v) const
    {
        std::int32_t stored = 0;
        if (!readValue(id, stored))
            return false;
        v = stored;
        return true;
    }
    bool getValue(int id, double &v) const { return readValue(id, v); }
    bool getValue(int id, std::string &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, AcGePoint3d &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, AcGeVector3d &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, std::vector<double> &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, std::vector<int> &v) const
    {
        std::vector<std::int32_t> stored;
        if (!readValue(id, stored))
            return false;
        v.assign(stored.begin(), stored.end());
        return true;
    }
    bool getValue(int id, std::vector<std::uint8_t> &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, std::vector<AcGePoint3d> &v) const
    {
        return readValue(id, v);
    }
    bool getValue(int id, Value &v) const
    {
        const auto found = values_.find(id);
        if (found == values_.end())
            return false;
        v = found->second;
        return true;
    }

    int numValues() const { return int(values_.size()); }
    void removeValue(int id) { values_.erase(id); }

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

using AcDbNonGraphicalObject = std::variant<AcDbDictionary, AcDbXrecord>;

} // namespace acdb
