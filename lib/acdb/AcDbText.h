#pragma once

#include <glm/glm.hpp>

#include <string>

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
    std::string text;
};

} // namespace acdb
