#include "acgi/AcGiLineType.h"

#include <string_view>
#include <vector>

const AcGiLineType *acgiFindLineType(const char *name)
{
    if (!name)
        return nullptr;
    for (const AcGiLineType &type : acgi_line_types::kLineTypes)
    {
        if (std::string_view(type.name) == name)
            return &type;
    }
    return nullptr;
}

std::vector<AcGiLineTypeMark> acgiLineTypeMarks(const AcGiLineType &type)
{
    constexpr double kDotRenderLength = 2.0;
    std::vector<AcGiLineTypeMark> marks;
    marks.reserve(type.elementCount);
    for (size_t i = 0; i < type.elementCount; ++i)
    {
        const double element = type.elements[i];
        if (element > 0.0)
            marks.push_back({element, true});
        else if (element < 0.0)
            marks.push_back({-element, false});
        else
            marks.push_back({kDotRenderLength, true});
    }
    return marks;
}
