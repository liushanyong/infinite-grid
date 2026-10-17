#pragma once

// ObjectARX-style dynamic array aliases.  Native AcGe uses AcArray<T>;
// this source-level layer maps the documented spellings onto std::vector,
// matching the ArxTypes.h AcArray compatibility decision.

#include <cstddef>
#include <vector>

#include "AcGePoint.h"

using AcGeIntArray = std::vector<int>;
using AcGeDoubleArray = std::vector<double>;
using AcGePoint2dArray = std::vector<AcGePoint2d>;
using AcGePoint3dArray = std::vector<AcGePoint3d>;
using AcGeVector2dArray = std::vector<AcGeVector2d>;
using AcGeVector3dArray = std::vector<AcGeVector3d>;
using AcGeVoidPointerArray = std::vector<void *>;
