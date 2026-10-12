// AcDbFileIo.cpp — the AcDbDatabase file-IO entry points implemented on
// the libredwg codec.  Declarations live in AcDbDatabase.h (ObjectARX
// spellings); the format plumbing is:
//
//   read:  dwg_read_file / dxf_read_file -> Dwg_Data -> AcDbDwgBridge
//   write: AcDbDwgWriteBridge -> Dwg_Data -> dwg_write_file / dwg_write_dxf

#include "acdb/AcDbFileIo.h"

#include <cstdio>
#include <cstring>

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbDwgBridge.h"
#include "acdb/AcDbDwgWriteBridge.h"

// The DXF writers live behind the library-private bits.h (which pulls in
// the generated config.h — its build dir is on the include path).
// out_dxf.h itself carries no extern "C" guard (library-internal), so the
// two writer entry points are declared here with C linkage, matching
// out_dxf.c.
#include "bits.h"

extern "C" {
int dwg_write_dxf(Bit_Chain *dat, Dwg_Data *dwg);
int dwg_write_dxfb(Bit_Chain *dat, Dwg_Data *dwg);
}

namespace acdb
{

namespace
{

// Shared decode+fill: both readers land in one Dwg_Data.
bool fillFromFile(AcDbDatabase &database, const char *filePath,
                  AcDbFileIoReport *report, bool asDxf)
{
    Dwg_Data dwg;
    memset(&dwg, 0, sizeof(dwg));
    const int error =
        asDxf ? dxf_read_file(filePath, &dwg) : dwg_read_file(filePath, &dwg);
    if (report != nullptr)
        report->errorCode = static_cast<unsigned int>(error);
    // The codec returns a DWG_ERR_* bitmask; only the critical range
    // (classes not found, invalid DWG, IO, OOM) means the decode is
    // unusable — warnings like VALUEOUTOFBOUNDS still yield content.
    if (error >= DWG_ERR_CRITICAL)
    {
        dwg_free(&dwg);
        return false;
    }
    const AcDbDwgImportReport imported = addDwgEntities(database, dwg);
    if (report != nullptr)
    {
        report->entitiesAdded = imported.added;
        report->unsupportedSkipped = imported.unsupported;
    }
    dwg_free(&dwg);
    return true;
}

} // namespace

// ObjectARX AcDbDatabase::readDwgFile: decode the binary DWG container
// and fill this database.
bool AcDbDatabase::readDwgFile(const char *filePath,
                               AcDbFileIoReport *report)
{
    if (filePath == nullptr)
        return false;
    if (report != nullptr)
        *report = {};
    return fillFromFile(*this, filePath, report, /*asDxf=*/false);
}

// ObjectARX AcDbDatabase::dxfIn: DXF text or binary (the codec sniffs
// the "AutoCAD Binary DXF" signature by content).
bool AcDbDatabase::dxfIn(const char *filePath, AcDbFileIoReport *report)
{
    if (filePath == nullptr)
        return false;
    if (report != nullptr)
        *report = {};
    return fillFromFile(*this, filePath, report, /*asDxf=*/true);
}

// ObjectARX AcDbDatabase::writeDwgFile: fill a Dwg_Data and encode the
// binary container (R13..R2000 targets; R_2000 chosen by the bridge).
bool AcDbDatabase::writeDwgFile(const char *filePath) const
{
    if (filePath == nullptr)
        return false;
    DwgWriteContext ctx;
    // Crash-position forensics (GUI processes have no valid stderr).
    // Append mode: writeDwgFile and dxfOut both trace into one file.
    if (const char *trace = std::getenv("GRID_DWGWRITE_TRACE");
        trace != nullptr && *trace != '\0')
    {
        ctx.trace = std::fopen(trace, "a");
        if (ctx.trace != nullptr)
        {
            std::fprintf(ctx.trace, "=== writeDwgFile this=%p\n",
                         (const void *)this);
            std::fflush(ctx.trace);
        }
    }
    const AcDbDwgWriteReport built = fillDwgFromDatabase(ctx, *this);
    if (ctx.trace != nullptr)
        std::fclose(ctx.trace);
    if (!built.ok)
        return false;
    // dwg_write_file refuses to overwrite an existing target.
    std::remove(filePath);
    const int error = dwg_write_file(filePath, &ctx.dwg);
    dwg_free(&ctx.dwg);
    return error == 0;
}

// ObjectARX AcDbDatabase::dxfOut: fill a Dwg_Data and write DXF ASCII
// (or binary with |binary|) through the library's own writer, which
// streams straight into the FILE* it finds on the Bit_Chain.
bool AcDbDatabase::dxfOut(const char *filePath, bool binary) const
{
    if (filePath == nullptr)
        return false;
    DwgWriteContext ctx;
    // Crash-position forensics (GUI processes have no valid stderr).
    // Append mode: writeDwgFile and dxfOut both trace into one file.
    if (const char *trace = std::getenv("GRID_DWGWRITE_TRACE");
        trace != nullptr && *trace != '\0')
    {
        ctx.trace = std::fopen(trace, "a");
        if (ctx.trace != nullptr)
        {
            std::fprintf(ctx.trace,
                         "=== dxfOut this=%p entry mspace=%zu\n",
                         (const void *)this,
                         modelSpace().entityHandles().size());
            std::fflush(ctx.trace);
        }
    }
    const AcDbDwgWriteReport built = fillDwgFromDatabase(ctx, *this);
    if (ctx.trace != nullptr)
        std::fclose(ctx.trace);
    if (!built.ok)
        return false;
    Bit_Chain dat;
    memset(&dat, 0, sizeof(dat));
    dat.version = ctx.dwg.header.version;
    dat.from_version = ctx.dwg.header.from_version;
    dat.fh = std::fopen(filePath, "wb");
    if (dat.fh == nullptr)
    {
        dwg_free(&ctx.dwg);
        return false;
    }
    const int error =
        binary ? dwg_write_dxfb(&dat, &ctx.dwg) : dwg_write_dxf(&dat, &ctx.dwg);
    std::fclose(dat.fh);
    dwg_free(&ctx.dwg);
    return error < DWG_ERR_CRITICAL;
}

} // namespace acdb
