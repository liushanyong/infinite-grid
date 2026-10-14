#pragma once

#include <glm/glm.hpp>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbText
{
    AcDbEntity common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    double height = 1.0;
    double rotation = 0.0;
    AcString text;
    // Text style reference (ObjectARX: AcDbTextStyleTableRecord by name).
    // The demo reserves the name "SHX" for stroke-font rendering.
    std::string styleName = "Standard";
};

} // namespace acdb
