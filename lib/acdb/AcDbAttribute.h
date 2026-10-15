#pragma once

#include "AcDbPort.h"

// AcDbAttribute (namespace acdb) — the ATTRIB entity, aligned with
// ObjectARX AcDbAttribute: a text value stamped from an
// AcDbAttributeDefinition when an INSERT referencing that block is
// placed.  The attribute is owned by its INSERT (never a model-space
// resident); AcDbBlockReference::attributeHandles carries the
// membership and the INSERT's placement transforms the text.

#include <glm/glm.hpp>

#include <string>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct ACDB_PORT AcDbAttribute
{
    AcDbEntity common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    double height = 1.0;
    double rotation = 0.0;
    double widthFactor = 1.0;
    std::string tag;
    std::string textValue;
    std::string styleName = "Standard";
    bool invisible = false;
};

} // namespace acdb
