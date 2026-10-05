#pragma once

// AcDbTransaction + undo (namespace acdb) — symmetric delta snapshots in
// the OpenCADStudio style: a transaction records the before-image of every
// entity handle it touches; commit pairs the before-images with after
// images into an AcDbUndoDelta where either side may be "absent" (entity
// created / removed inside the transaction).  Undo applies the before
// side, redo the after side.  The stack follows OpenCADStudio's capacity
// policy (256 entries; push clears the redo branch).
//
// Scope: the flat entity store and model-space membership.  Symbol-table
// and block-structure edits poison the transaction in OpenCADStudio and
// are out of scope here as well.

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "acdb/AcDbCore.h"

namespace acdb
{

class AcDbDatabase;

// One symmetric undo step: (handle, before, after) with either side
// nullopt when the entity did not exist / was removed in that state.
struct AcDbUndoEntry
{
    AcDbHandle handle;
    std::optional<AcDbEntityVariant> before;
    std::optional<AcDbEntityVariant> after;
};

using AcDbUndoDelta = std::vector<AcDbUndoEntry>;

class AcDbTransaction
{
public:
    bool isActive() const { return active_; }

    // Captures the current payload (or its absence) as the before-image.
    // Calling twice for one handle keeps the first capture.
    void captureBefore(AcDbHandle handle, const AcDbDatabase &database);

private:
    friend class AcDbDatabase;
    bool active_ = false;
    std::vector<AcDbHandle> captureOrder_;
    std::unordered_map<AcDbHandle, std::optional<AcDbEntityVariant>> before_;
};

// Bounded undo history (OpenCADStudio: 256 entries, push clears redo).
class AcDbUndoStack
{
public:
    static constexpr std::size_t kMaxEntries = 256;

    void push(AcDbUndoDelta delta);
    bool undo(AcDbDatabase &database);
    bool redo(AcDbDatabase &database);

    std::size_t undoDepth() const { return undo_.size(); }
    std::size_t redoDepth() const { return redo_.size(); }
    void clear();

private:
    std::vector<AcDbUndoDelta> undo_;
    std::vector<AcDbUndoDelta> redo_;
};

} // namespace acdb
