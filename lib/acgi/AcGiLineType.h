#pragma once

// AcGi-compatible line type definitions.  Elements follow the LTYPE
// dash/dot/gap semantics: a positive length is a stroke (dash), a negative
// length is a gap, and zero is a dot.  Lengths are world units and the
// pattern repeats along the line, mirroring the AutoCAD .lin format that
// libredwg/LTYPE data models.
#include <vector>

struct AcGiLineType
{
    const char *name;
    const double *elements;
    size_t elementCount;
};

namespace acgi_line_types
{
namespace
{
constexpr double kDashed[] = {96.0, -48.0};
constexpr double kHidden[] = {24.0, -12.0};
constexpr double kCenter[] = {96.0, -12.0, 24.0, -24.0};
constexpr double kDashDot[] = {48.0, -12.0, 8.0, -12.0};
constexpr double kDot[] = {0.0, -8.0};
constexpr double kPhantom[] = {48.0, -8.0, 12.0, -8.0, 12.0, -24.0};

constexpr AcGiLineType kLineTypes[] = {
    {"DASHED", kDashed, 2},
    {"HIDDEN", kHidden, 2},
    {"CENTER", kCenter, 4},
    {"DASHDOT", kDashDot, 4},
    {"DOT", kDot, 2},
    {"PHANTOM", kPhantom, 6},
};
} // namespace
} // namespace acgi_line_types

// Case-sensitive lookup matching the entity lineType string.  A missing
// name (or "ByLayer"/"CONTINUOUS") means a solid line.
inline const AcGiLineType *acgiFindLineType(const char *name)
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

struct AcGiLineTypeMark
{
    double length;
    bool stroke;
};

// Flatten the dash/dot/gap elements into render marks.  A dot carries no
// length by definition; it renders as a short stroke so it stays visible
// at any zoom.
inline std::vector<AcGiLineTypeMark> acgiLineTypeMarks(
    const AcGiLineType &type)
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
