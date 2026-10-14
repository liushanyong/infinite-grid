// AcDbDatabase file-IO entry points. Codec-specific format handling lives
// behind AcDbFileCodec.

#include "acdb/AcDbFileIo.h"

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbFileCodec.h"

namespace acdb
{

bool AcDbDatabase::readDwgFile(const char *filePath,
                               AcDbFileIoReport *report)
{
    return defaultAcDbFileCodec().readDwg(*this, filePath, report);
}

bool AcDbDatabase::dxfIn(const char *filePath, AcDbFileIoReport *report)
{
    return defaultAcDbFileCodec().readDxf(*this, filePath, report);
}

bool AcDbDatabase::writeDwgFile(const char *filePath) const
{
    return defaultAcDbFileCodec().writeDwg(*this, filePath);
}

bool AcDbDatabase::dxfOut(const char *filePath, bool binary) const
{
    return defaultAcDbFileCodec().writeDxf(*this, filePath, binary);
}

} // namespace acdb
