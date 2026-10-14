#pragma once

#include "acdb/AcDbFileIo.h"

namespace acdb
{

class AcDbDatabase;

// Format codec boundary. Implementations own format-specific decoding,
// encoding, and conversion between their document model and AcDbDatabase.
class AcDbFileCodec
{
public:
    virtual ~AcDbFileCodec() = default;

    virtual bool readDwg(AcDbDatabase &database, const char *filePath,
                         AcDbFileIoReport *report) const = 0;
    virtual bool readDxf(AcDbDatabase &database, const char *filePath,
                         AcDbFileIoReport *report) const = 0;
    virtual bool writeDwg(const AcDbDatabase &database,
                          const char *filePath) const = 0;
    virtual bool writeDxf(const AcDbDatabase &database, const char *filePath,
                          bool binary) const = 0;
};

const AcDbFileCodec &defaultAcDbFileCodec();

} // namespace acdb
