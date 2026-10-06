#pragma once

// AcDbStore (namespace acdb) — SQLite persistence for the drawing
// document, backed by SQLiteCpp.  Layout follows the roadmap row
// "存储：SQLite 索引 + 操作日志，不存 BLOB 快照":
//
//   meta(key,value)                format version, handle counter,
//                                  active draw settings
//   layers/linetypes/text_styles   symbol tables, natural-key + handle
//   blocks                         block records with member handle lists
//   entities                       handle-keyed rows; class identity and
//                                  erased flag in columns, payload JSON
//   ops                            append-only operation journal (the
//                                  command-bus seed; a 'save' row lands
//                                  on every saveDatabase)
//
// Handles are exact INTEGERs (never JSON doubles); payload geometry
// rides in JSON via AcDbJson.  Brep bodies owned by AcDb3dSolid payloads
// are runtime tessellation state and stay out of the store — kernel
// solids persist through brep::FeatureHistory journals, not blobs.

#include <string>

#include "acdb/AcDbDatabase.h"

namespace acdb
{

struct StoreResult
{
    bool ok = false;
    std::string error;
    std::size_t entities = 0;
};

// Writes the whole document to |path| (creates/overwrites the file;
// the ops journal is append-only and survives re-saves).
StoreResult saveDatabase(const AcDbDatabase &database, const char *path);

// Reads |path| into |database| (replaces its content entirely).
StoreResult loadDatabase(const char *path, AcDbDatabase &database);

} // namespace acdb
