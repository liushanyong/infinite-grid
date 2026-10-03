#pragma once

// AcGi-compatible text style (ObjectARX signature subset used by the
// worldText callback).  The glyph engine consumes these fields; until it
// lands, the text callback uses textSize and xScale for layout framing.

#include "../ge/gepoint.h"

struct AcGiTextStyle
{
    const char *fileName = "txt.shx";
    const char *bigFontFileName = "";
    double textSize = 1.0;
    double xScale = 1.0;      // width factor
    double obliqueAngle = 0.0;
    bool upsideDown = false;
    bool backwards = false;
    bool vertical = false;
    bool underlines = false;
    bool overlines = false;

    AcGiTextStyle &setTextSize(double size) { textSize = size; return *this; }
    AcGiTextStyle &setXScale(double scale) { xScale = scale; return *this; }
    AcGiTextStyle &setObliqueAngle(double angle) { obliqueAngle = angle; return *this; }
    AcGiTextStyle &setFileName(const char *name) { fileName = name; return *this; }
    AcGiTextStyle &setBigFontFileName(const char *name) { bigFontFileName = name; return *this; }
};
