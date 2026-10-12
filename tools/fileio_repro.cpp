// Minimal file-IO repro for ASAN: build the self-test sample document,
// write DXF, read it back, write a second generation — the second
// dxfOut is where the fills start seeing empty membership lists.
#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbFileIo.h"
#include "acdb/FileIoSelfTest.h"

#include <cstdio>
#include <cstring>

#include "libredwg/include/dwg.h"

extern "C" int dwg_read_file(const char *filename, Dwg_Data *dwg);
extern "C" void dwg_free(Dwg_Data *dwg);

int main()
{
    acdb::AcDbDatabase sample =
        acdb::fileio_selftest::buildSampleDocument();
    std::printf("sample: mspace=%zu entities=%zu\n",
                sample.modelSpace().entityHandles().size(),
                sample.entityCount());

    if (!sample.dxfOut("repro_gen1.dxf"))
    {
        std::printf("gen1 dxfOut FAILED\n");
        return 1;
    }
    std::printf("gen1 written; sample mspace=%zu\n",
                sample.modelSpace().entityHandles().size());

    acdb::AcDbDatabase fromDxf;
    acdb::AcDbFileIoReport report;
    if (!fromDxf.dxfIn("repro_gen1.dxf", &report))
    {
        std::printf("gen1 dxfIn FAILED error=%u\n", report.errorCode);
        return 1;
    }
    std::printf("fromDxf: mspace=%zu entities=%zu\n",
                fromDxf.modelSpace().entityHandles().size(),
                fromDxf.entityCount());
    {
        int slot = 0;
        for (const acdb::AcDbHandle handle :
             fromDxf.modelSpace().entityHandles())
        {
            std::printf("  slot %d: handle=%llu resolved=%d erased=%d\n",
                        slot++, (unsigned long long)handle.value,
                        fromDxf.getEntity(handle) != nullptr ? 1 : 0,
                        fromDxf.isErased(handle) ? 1 : 0);
            if (slot >= 6)
                break;
        }
    }

    if (!fromDxf.dxfOut("repro_gen2.dxf"))
    {
        std::printf("gen2 dxfOut FAILED\n");
        return 1;
    }
    std::printf("gen2 written; fromDxf mspace=%zu\n",
                fromDxf.modelSpace().entityHandles().size());

    acdb::AcDbDatabase gen2;
    if (!gen2.dxfIn("repro_gen2.dxf", &report))
    {
        std::printf("gen2 dxfIn FAILED error=%u\n", report.errorCode);
        return 1;
    }
    std::printf("gen2: mspace=%zu entities=%zu\n",
                gen2.modelSpace().entityHandles().size(),
                gen2.entityCount());

    // Probe the binary DWG: how did the POLYLINE_3D children survive the
    // encoder?
    {
        Dwg_Data dwg;
        memset(&dwg, 0, sizeof(dwg));
        const int readError = dwg_read_file("fileio_selftest.dwg", &dwg);
        std::printf("probe: dwg_read_file error=%d objects=%u\n", readError,
                    (unsigned)dwg.num_objects);
        if (readError < 128)
        {
            int vertices = 0;
            for (BITCODE_BL i = 0; i < dwg.num_objects; ++i)
            {
                const Dwg_Object &object = dwg.object[i];
                if (object.fixedtype == DWG_TYPE_POLYLINE_3D &&
                    object.tio.entity != nullptr)
                {
                    const Dwg_Entity_POLYLINE_3D &poly =
                        *object.tio.entity->tio.POLYLINE_3D;
                    std::printf(
                        "probe POLYLINE_3D: num_owned=%u has_vertex=%d "
                        "first=%d vertex0=%d flag=%d\n",
                        (unsigned)poly.num_owned, (int)poly.has_vertex,
                        poly.first_vertex != nullptr ? 1 : 0,
                        (poly.vertex != nullptr && poly.vertex[0] != nullptr)
                            ? 1
                            : 0,
                        (int)poly.flag);
                }
                else if (object.fixedtype == DWG_TYPE_VERTEX_3D)
                    ++vertices;
                else if (object.fixedtype == DWG_TYPE_SEQEND)
                    std::printf("probe SEQEND at obj %u\n", (unsigned)i);
            }
            std::printf("probe: %d VERTEX_3D in file, num_objects=%u\n",
                        vertices, (unsigned)dwg.num_objects);
            dwg_free(&dwg);
        }
    }
    return gen2.entityCount() == fromDxf.entityCount() ? 0 : 2;
}
