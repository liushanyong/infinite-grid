#pragma once

#include "AcDbPort.h"

// AcDbHandle / AcDbObjectId — the two identities of a database-resident
// object, extracted from AcDbCore.h so headers outside acdb (notably
// acgi/AcGiDrawable.h, whose id() returns the session identity) can depend
// on identity without pulling in the entity definitions.

#include <cstdint>
#include <functional>

namespace acdb
{

// ---- AcDbHandle: persistent object identifier (DXF/DWG handle) ----

struct ACDB_PORT AcDbHandle
{
    std::uint64_t value = 0;

    // Implicit conversions so payload field reads/writes
    // (entity.handle = 0xCAFE) keep compiling against u64 handles.
    AcDbHandle() = default;
    constexpr AcDbHandle(std::uint64_t v) : value(v) {}
    operator std::uint64_t() const { return value; }

    bool isValid() const { return value != 0; }
    bool operator==(const AcDbHandle &other) const = default;
};

constexpr AcDbHandle kNullHandle{};

// ---- AcDbObjectId: session-scoped object identifier ----
//
// ObjectARX hands every database-resident object two identities: the
// persistent AcDbHandle (file anchor, survives save/load through
// AcDbStore) and the AcDbObjectId (session identity, what call sites
// pass around and what the in-memory graphics scene resolves).  This
// layer reproduces the split: AcDbObjectId wraps the handle 1:1 today,
// but is a distinct type so runtime identity and file identity can
// never blur at a call site.  The in-memory mirror (acgs::SceneStore)
// is keyed by this; SQLite stays handle-keyed end to end.

struct ACDB_PORT AcDbObjectId
{
    AcDbHandle handle{};

    constexpr AcDbObjectId() = default;
    constexpr explicit AcDbObjectId(AcDbHandle persistentHandle)
        : handle(persistentHandle)
    {
    }

    bool isValid() const { return handle.isValid(); }
    // The file anchor this session identity currently maps to.
    constexpr AcDbHandle persistentHandle() const { return handle; }

    bool operator==(const AcDbObjectId &other) const = default;
};

constexpr AcDbObjectId kNullObjectId{};

} // namespace acdb

template <> struct std::hash<acdb::AcDbHandle>
{
    std::size_t operator()(const acdb::AcDbHandle &handle) const noexcept
    {
        return std::hash<std::uint64_t>()(handle.value);
    }
};

template <> struct std::hash<acdb::AcDbObjectId>
{
    std::size_t operator()(const acdb::AcDbObjectId &id) const noexcept
    {
        return std::hash<acdb::AcDbHandle>()(id.handle);
    }
};
