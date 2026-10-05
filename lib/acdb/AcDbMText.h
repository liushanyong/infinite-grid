#pragma once

#include <glm/glm.hpp>

#include <string>

#include "../ge/ge.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbMText
{
    AcDbEntity common;
    AcGePoint3d insertion{0.0, 0.0, 0.0};
    AcGeVector3d direction{1.0, 0.0, 0.0};
    double width = 0.0;
    double height = 1.0;
    std::string text;
};

} // namespace acdb
