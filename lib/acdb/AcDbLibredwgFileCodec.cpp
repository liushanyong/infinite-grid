// LibreDWG-backed implementation of the AcDb file codec boundary.

#include "acdb/AcDbFileCodec.h"

#include <cstdio>
#include <cstdlib>
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
    std::memset(&dwg, 0, sizeof(dwg));
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

bool writeDwgFile(const AcDbDatabase &database, const char *filePath)
{
    if (filePath == nullptr)
        return false;
    DwgWriteContext ctx;
    if (const char *trace = std::getenv("GRID_DWGWRITE_TRACE");
        trace != nullptr && *trace != '\0')
    {
        ctx.trace = std::fopen(trace, "a");
        if (ctx.trace != nullptr)
        {
            std::fprintf(ctx.trace, "=== writeDwgFile this=%p\n",
                         static_cast<const void *>(&database));
            std::fflush(ctx.trace);
        }
    }
    const AcDbDwgWriteReport built = fillDwgFromDatabase(ctx, database);
    if (!built.ok)
    {
        if (ctx.trace != nullptr)
            std::fclose(ctx.trace);
        return false;
    }
    // dwg_write_file refuses to overwrite an existing target.
    std::remove(filePath);
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dwg_write_file enter\n");
        std::fflush(ctx.trace);
    }
    const int error = dwg_write_file(filePath, &ctx.dwg);
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dwg_write_file done error=%d\n", error);
        std::fflush(ctx.trace);
    }
    // dwg_free deliberately skipped: libredwg's free path releases the
    // first_vertex/last_vertex/seqend refs per entity while the closing
    // object-ref pool pass frees them again, which double-frees once the
    // filled graph grows past the allocator's small-block tolerance (a
    // ~550-child POLYLINE_PFACE reproducibly aborts in dwg_free after the
    // file itself wrote cleanly). The Dwg_Data is one-shot per export,
    // so the current implementation intentionally retains this workaround.
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dwg_free skipped (leak)\n");
        std::fflush(ctx.trace);
        std::fclose(ctx.trace);
    }
    return error == 0;
}

bool writeDxfFile(const AcDbDatabase &database, const char *filePath,
                  bool binary)
{
    if (filePath == nullptr)
        return false;
    DwgWriteContext ctx;
    if (const char *trace = std::getenv("GRID_DWGWRITE_TRACE");
        trace != nullptr && *trace != '\0')
    {
        ctx.trace = std::fopen(trace, "a");
        if (ctx.trace != nullptr)
        {
            std::fprintf(ctx.trace,
                         "=== dxfOut this=%p entry mspace=%zu\n",
                         static_cast<const void *>(&database),
                         database.modelSpace().entityHandles().size());
            std::fflush(ctx.trace);
        }
    }
    const AcDbDwgWriteReport built = fillDwgFromDatabase(ctx, database);
    if (!built.ok)
    {
        if (ctx.trace != nullptr)
            std::fclose(ctx.trace);
        return false;
    }
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dwg_write_dxf enter\n");
        std::fflush(ctx.trace);
    }
    Bit_Chain dat;
    std::memset(&dat, 0, sizeof(dat));
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
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dwg_write_dxf done error=%d\n", error);
        std::fflush(ctx.trace);
    }
    // See writeDwgFile: this is a known workaround for the current
    // LibreDWG graph-cleanup double-free.
    if (ctx.trace != nullptr)
    {
        std::fprintf(ctx.trace, "dxf dwg_free skipped (leak)\n");
        std::fflush(ctx.trace);
        std::fclose(ctx.trace);
    }
    return error < DWG_ERR_CRITICAL;
}

class LibreDwgFileCodec final : public AcDbFileCodec
{
public:
    bool readDwg(AcDbDatabase &database, const char *filePath,
                 AcDbFileIoReport *report) const override
    {
        if (filePath == nullptr)
            return false;
        if (report != nullptr)
            *report = {};
        return fillFromFile(database, filePath, report, /*asDxf=*/false);
    }

    bool readDxf(AcDbDatabase &database, const char *filePath,
                 AcDbFileIoReport *report) const override
    {
        if (filePath == nullptr)
            return false;
        if (report != nullptr)
            *report = {};
        return fillFromFile(database, filePath, report, /*asDxf=*/true);
    }

    bool writeDwg(const AcDbDatabase &database,
                  const char *filePath) const override
    {
        return writeDwgFile(database, filePath);
    }

    bool writeDxf(const AcDbDatabase &database, const char *filePath,
                  bool binary) const override
    {
        return writeDxfFile(database, filePath, binary);
    }
};

} // namespace

const AcDbFileCodec &defaultAcDbFileCodec()
{
    static const LibreDwgFileCodec codec;
    return codec;
}

} // namespace acdb
