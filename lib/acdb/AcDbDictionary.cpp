#include "acdb/AcDbDictionary.h"

namespace acdb
{

int AcDbDictionary::numEntries() const
{
    return int(entries_.size());
}

bool AcDbDictionary::has(const std::string &name) const
{
    return entries_.count(name) != 0;
}

AcDbHandle AcDbDictionary::getAt(const std::string &name) const
{
    const auto found = entries_.find(name);
    return found != entries_.end() ? found->second : kNullHandle;
}

bool AcDbDictionary::setAt(const std::string &name, AcDbHandle objectHandle)
{
    if (!objectHandle.isValid())
        return false;
    entries_[name] = objectHandle;
    return true;
}

bool AcDbDictionary::setName(const std::string &oldName,
                             const std::string &newName)
{
    const auto found = entries_.find(oldName);
    if (found == entries_.end() || oldName == newName)
        return false;
    const AcDbHandle handle = found->second;
    entries_.erase(found);
    entries_[newName] = handle;
    return true;
}

bool AcDbDictionary::remove(const std::string &name)
{
    return entries_.erase(name) != 0;
}

void AcDbXrecord::setValue(int id, bool v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, int v)
{
    values_[id] = Value(std::int32_t(v));
}

void AcDbXrecord::setValue(int id, double v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const std::string &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const AcGePoint3d &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const AcGeVector3d &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const std::vector<double> &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const std::vector<int> &v)
{
    Value out(std::vector<std::int32_t>{});
    auto &target = std::get<std::vector<std::int32_t>>(out);
    target.assign(v.begin(), v.end());
    values_[id] = std::move(out);
}

void AcDbXrecord::setValue(int id, const std::vector<std::uint8_t> &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const std::vector<AcGePoint3d> &v)
{
    values_[id] = Value(v);
}

void AcDbXrecord::setValue(int id, const Value &v)
{
    values_[id] = v;
}

bool AcDbXrecord::getValue(int id, bool &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, int &v) const
{
    std::int32_t stored = 0;
    if (!readValue(id, stored))
        return false;
    v = stored;
    return true;
}

bool AcDbXrecord::getValue(int id, double &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, std::string &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, AcGePoint3d &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, AcGeVector3d &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, std::vector<double> &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, std::vector<int> &v) const
{
    std::vector<std::int32_t> stored;
    if (!readValue(id, stored))
        return false;
    v.assign(stored.begin(), stored.end());
    return true;
}

bool AcDbXrecord::getValue(int id, std::vector<std::uint8_t> &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, std::vector<AcGePoint3d> &v) const
{
    return readValue(id, v);
}

bool AcDbXrecord::getValue(int id, Value &v) const
{
    const auto found = values_.find(id);
    if (found == values_.end())
        return false;
    v = found->second;
    return true;
}

} // namespace acdb
