#pragma once

// File-IO facade over AcDbFileCodec (namespace acdb). The four
// ObjectARX entry points live as AcDbDatabase methods — readDwgFile /
// writeDwgFile for the binary DWG container, dxfIn / dxfOut for the DXF
// interchange format:
//
// Current default-codec mapping:
//   readDwgFile  dwg_read_file  -> Dwg_Data -> AcDbDwgBridge::addDwgEntities
//   dxfIn        dxf_read_file  -> Dwg_Data -> AcDbDwgBridge::addDwgEntities
//   writeDwgFile AcDbDwgWriteBridge::fillDwgFromDatabase -> dwg_write_file
//   dxfOut       AcDbDwgWriteBridge::fillDwgFromDatabase -> dwg_write_dxf
//
// The default codec currently decodes both formats into the same
// intermediate document. readDwgFile/dxfIn APPEND into this
// database (callers wanting a fresh document start from an empty one);
// writeDwgFile overwrites an existing target (the codec refuses to).

#include <cstddef>

namespace acdb
{

// Counts reported by readers; errorCode is the selected codec's native code.
struct AcDbFileIoReport
{
    std::size_t entitiesAdded = 0;
    std::size_t unsupportedSkipped = 0;
    unsigned int errorCode = 0;
};

} // namespace acdb
