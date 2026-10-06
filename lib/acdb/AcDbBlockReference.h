#pragma once

// AcDbBlockReference (namespace acdb) — the INSERT entity, aligned with
// ObjectARX AcDbBlockReference and the wecad_sdk CoreDB
// AcDbBlockReference: a named reference to an AcDbBlockTableRecord plus
// the placement transform.  Like DXF INSERT (and OpenCADStudio's
// EntityType::Insert) the reference names the record by string; the
// record owns the member entity handles.

#include <glm/glm.hpp>

#include "acdb/AcDbCore.h"
#include "ge/gematrix.h"

namespace acdb
{

class AcDbBlockTableRecord;

class AcDbBlockReference
{
public:
    // Common entity properties (same payload shape as the lib/entities
    // value types so the database variant's common() accessor applies).
    acdb::AcDbEntity common;

    // Name of the referenced AcDbBlockTableRecord.
    std::string blockTableRecordName;

    AcGePoint3d position{0.0, 0.0, 0.0};
    double rotation = 0.0; // radians, right-hand about |normal|
    AcGeVector3d scale{1.0, 1.0, 1.0};
    AcGeVector3d normal{0.0, 0.0, 1.0};

    // Local placement matrix relative to the referenced record's base
    // point: translate(position) * rotate * scale * translate(-basePoint).
    AcGeMatrix3d toMatrix(const AcGePoint3d &basePoint) const
    {
        AcGeMatrix3d local =
            AcGeMatrix3d::setToTranslation(position.asVector());
        const AcGeVector3d axis =
            normal.length() > 1.0e-12
                ? normal.normal()
                : AcGeVector3d(0.0, 0.0, 1.0);
        if (rotation != 0.0)
            local = local * AcGeMatrix3d::setToRotation(
                                rotation, axis,
                                AcGePoint3d(0.0, 0.0, 0.0));
        if (scale != AcGeVector3d(1.0, 1.0, 1.0))
            local = local * AcGeMatrix3d::setToScaling(scale);
        local = local * AcGeMatrix3d::setToTranslation(
                            (basePoint * -1.0).asVector());
        return local;
    }
};

} // namespace acdb
