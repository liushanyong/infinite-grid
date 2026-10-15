#include "acdb/AcDbDictionary.h"

namespace acdb
{

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

void AcDbXrecord::setValue(int id, const std::vector<int> &v)
{
    Value out(std::vector<std::int32_t>{});
    auto &target = std::get<std::vector<std::int32_t>>(out);
    target.assign(v.begin(), v.end());
    values_[id] = std::move(out);
}

bool AcDbXrecord::getValue(int id, int &v) const
{
    std::int32_t stored = 0;
    if (!readValue(id, stored))
        return false;
    v = stored;
    return true;
}

bool AcDbXrecord::getValue(int id, std::vector<int> &v) const
{
    std::vector<std::int32_t> stored;
    if (!readValue(id, stored))
        return false;
    v.assign(stored.begin(), stored.end());
    return true;
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
