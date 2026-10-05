#pragma once

// The concrete entity payload set (namespace acdb).  Each member is an
// ObjectARX-aligned value type; geometry stays plain data while AcDb
// owns identity and membership.  Kept separate from AcDbCore.h so the
// payload headers can include the core without an include cycle.

#include <variant>

#include "acdb/AcDb3dPolyline.h"
#include "acdb/AcDb3dSolid.h"
#include "acdb/AcDbArc.h"
#include "acdb/AcDbBlockReference.h"
#include "acdb/AcDbCircle.h"
#include "acdb/AcDbEllipse.h"
#include "acdb/AcDbHatch.h"
#include "acdb/AcDbLight.h"
#include "acdb/AcDbLine.h"
#include "acdb/AcDbMText.h"
#include "acdb/AcDbMline.h"
#include "acdb/AcDbPoint.h"
#include "acdb/AcDbPolyFaceMesh.h"
#include "acdb/AcDbPolyline.h"
#include "acdb/AcDbRay.h"
#include "acdb/AcDbSolid.h"
#include "acdb/AcDbSpline.h"
#include "acdb/AcDbText.h"
#include "acdb/AcDbXline.h"

namespace acdb
{

using AcDbEntityVariant = std::variant<
    AcDbLine, AcDbArc, AcDbCircle, AcDbEllipse, AcDbPoint, AcDbRay,
    AcDbXline, AcDbSolid, AcDbHatch, AcDb3dPolyline, AcDbPolyline,
    AcDbSpline, AcDbText, AcDbMText, AcDbMline, AcDbPolyFaceMesh,
    AcDb3dSolid, AcDbLight, AcDbBlockReference>;

// Read-write access to the common-properties member shared by every
// payload type (all of them embed acdb::AcDbEntity as `common`).
AcDbEntity &common(AcDbEntityVariant &payload);
const AcDbEntity &common(const AcDbEntityVariant &payload);

} // namespace acdb
