#pragma once

// File-IO facade over the libredwg codec (namespace acdb).  The four
// ObjectARX entry points live as AcDbDatabase methods — readDwgFile /
// writeDwgFile for the binary DWG container, dxfIn / dxfOut for the DXF
// interchange format:
//
//   readDwgFile  dwg_read_file  -> Dwg_Data -> AcDbDwgBridge::addDwgEntities
//   dxfIn        dxf_read_file  -> Dwg_Data -> AcDbDwgBridge::addDwgEntities
//   writeDwgFile AcDbDwgWriteBridge::fillDwgFromDatabase -> dwg_write_file
//   dxfOut       AcDbDwgWriteBridge::fillDwgFromDatabase -> dwg_write_dxf
//
// Both formats decode into the same Dwg_Data, so one fill serves the
// two readers and one Dwg_Data feeds the two writers (OpenCADStudio's
// one-document-four-IOs shape).  readDwgFile/dxfIn APPEND into this
// database (callers wanting a fresh document start from an empty one);
// writeDwgFile overwrites an existing target (the codec refuses to).

#include <cstddef>

namespace acdb
{

// Counts reported by the readers (and error code from the codec, a
// DWG_ERR_* bitmask — >= DWG_ERR_CRITICAL means the decode aborted).
struct AcDbFileIoReport
{
    std::size_t entitiesAdded = 0;
    std::size_t unsupportedSkipped = 0;
    unsigned int errorCode = 0;
};

} // namespace acdb
