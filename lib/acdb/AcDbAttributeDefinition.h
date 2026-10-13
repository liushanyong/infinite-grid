#pragma once

// AcDbAttributeDefinition (namespace acdb) — the ATTDEF entity, aligned
// with ObjectARX AcDbAttributeDefinition: the template that lives in a
// block definition and stamps AcDbAttribute values onto INSERTs.  In the
// drawing an ATTDEF displays its tag; the default value shows up in the
// stamped attributes.

#include <glm/glm.hpp>

#include <string>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbAttributeDefinition
{
    AcDbEntity common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    double height = 1.0;
    double rotation = 0.0;
    double widthFactor = 1.0;
    double obliqueAngle = 0.0;
    std::string tag;
    std::string prompt;
    std::string textValue; // the default stamped into new attributes
    std::string styleName = "Standard";
    // DXF 70 flag bits (ObjectARX AcDbAttributeDefinition::isInvisible /
    // isConstant / isVerifiable / isPreset).
    bool invisible = false;
    bool constant = false;
    bool verify = false;
    bool preset = false;
};

} // namespace acdb
