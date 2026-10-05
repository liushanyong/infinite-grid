#include "acdb/AcDbTransaction.h"

#include "acdb/AcDbDatabase.h"

namespace acdb
{

void AcDbTransaction::captureBefore(AcDbHandle handle,
                                    const AcDbDatabase &database)
{
    if (before_.count(handle) != 0)
        return; // first capture wins
    captureOrder_.push_back(handle);
    const AcDbEntityVariant *payload = database.getEntity(handle);
    if (payload != nullptr)
        before_[handle] = *payload;
    else
        before_[handle] = std::nullopt;
}

void AcDbUndoStack::push(AcDbUndoDelta delta)
{
    if (delta.empty())
        return;
    undo_.push_back(std::move(delta));
    redo_.clear();
    if (undo_.size() > kMaxEntries)
        undo_.erase(undo_.begin(),
                    undo_.begin() + (undo_.size() - kMaxEntries));
}

bool AcDbUndoStack::undo(AcDbDatabase &database)
{
    if (undo_.empty())
        return false;
    AcDbUndoDelta delta = std::move(undo_.back());
    undo_.pop_back();
    database.applyUndoDelta(delta, false);
    redo_.push_back(std::move(delta));
    return true;
}

bool AcDbUndoStack::redo(AcDbDatabase &database)
{
    if (redo_.empty())
        return false;
    AcDbUndoDelta delta = std::move(redo_.back());
    redo_.pop_back();
    database.applyUndoDelta(delta, true);
    undo_.push_back(std::move(delta));
    return true;
}

void AcDbUndoStack::clear()
{
    undo_.clear();
    redo_.clear();
}

} // namespace acdb
