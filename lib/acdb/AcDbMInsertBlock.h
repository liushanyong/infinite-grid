#pragma once

// AcDbMInsertBlock (namespace acdb) — the MINSERT entity, aligned with
// ObjectARX AcDbMInsertBlock (derived from AcDbBlockReference): a block
// placement repeated on a rectangular row/column grid.  The fields
// mirror AcDbBlockReference plus the array counts/spacings; the replay
// expands the grid into per-cell instance walks (nested MINSERTs inside
// a block render as their base placement only).

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbMInsertBlock
{
    AcDbEntity common;

    std::string blockTableRecordName;

    AcGePoint3d position{0.0, 0.0, 0.0};
    double rotation = 0.0; // radians, right-hand about |normal|
    AcGeVector3d scale{1.0, 1.0, 1.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};

    int rowCount = 1;
    int columnCount = 1;
    double rowSpacing = 0.0;
    double columnSpacing = 0.0;

    // ATTRIB entities owned by this reference (stamped values).
    std::vector<AcDbHandle> attributeHandles;
};

} // namespace acdb
