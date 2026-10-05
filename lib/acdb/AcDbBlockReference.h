#pragma once

// AcDbBlockReference (namespace acdb) — the INSERT entity, aligned with
// ObjectARX AcDbBlockReference and the wecad_sdk CoreDB
// AcDbBlockReference: a named reference to an AcDbBlockTableRecord plus
// the placement transform.  Like DXF INSERT (and OpenCADStudio's
// EntityType::Insert) the reference names the record by string; the
// record owns the member entity handles.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "acdb/AcDbCore.h"

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

    glm::dvec3 position{0.0};
    double rotation = 0.0; // radians, right-hand about |normal|
    glm::dvec3 scale{1.0};
    glm::dvec3 normal{0.0, 0.0, 1.0};

    // Local placement matrix relative to the referenced record's base
    // point: translate(position) * rotate * scale * translate(-basePoint).
    glm::dmat4 toMatrix(const glm::dvec3 &basePoint) const
    {
        glm::dmat4 local(1.0);
        local = glm::translate(local, position);
        const glm::dvec3 axis = glm::normalize(
            glm::length(normal) > 1.0e-12 ? normal : glm::dvec3(0.0, 0.0, 1.0));
        if (rotation != 0.0)
            local = glm::rotate(local, rotation, axis);
        if (glm::any(glm::notEqual(scale, glm::dvec3(1.0))))
            local = glm::scale(local, scale);
        local = glm::translate(local, -basePoint);
        return local;
    }
};

} // namespace acdb
