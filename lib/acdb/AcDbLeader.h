#pragma once

// AcDbLeader (namespace acdb) — the LEADER entity, aligned with
// ObjectARX AcDbLeader: a polyline (optionally fitted) from an arrowhead
// to an annotation that lives as its own entity (MTEXT / AcDbFcf /
// INSERT), so this payload carries only the leader path itself.

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

struct AcDbLeader
{
    AcDbEntity common;
    // Leader vertices in world coordinates; points[0] is the arrow end.
    std::vector<AcGePoint3d> points;
    // LEADER path_type: 0 = straight segments, 1 = spline fitted.
    bool splinePath = false;
    bool hasArrowhead = true;
    bool hasHookLine = false;
    // Referenced DIMSTYLE table record by name (resolves arrow/annotation
    // conventions in a full implementation).
    std::string dimensionStyleName;
};

} // namespace acdb
