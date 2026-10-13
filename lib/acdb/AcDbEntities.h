#pragma once

// The concrete entity payload set (namespace acdb).  Each member is an
// ObjectARX-aligned value type; geometry stays plain data while AcDb
// owns identity and membership.  Kept separate from AcDbCore.h so the
// payload headers can include the core without an include cycle.

#include <variant>

#include "acdb/AcDb3dPolyline.h"
#include "acdb/AcDb3dSolid.h"
#include "acdb/AcDbArc.h"
#include "acdb/AcDbAttribute.h"
#include "acdb/AcDbAttributeDefinition.h"
#include "acdb/AcDbBlockReference.h"
#include "acdb/AcDbCircle.h"
#include "acdb/AcDbDimension.h"
#include "acdb/AcDbEllipse.h"
#include "acdb/AcDbFcf.h"
#include "acdb/AcDbHatch.h"
#include "acdb/AcDbLeader.h"
#include "acdb/AcDbLight.h"
#include "acdb/AcDbLine.h"
#include "acdb/AcDbMInsertBlock.h"
#include "acdb/AcDbMText.h"
#include "acdb/AcDbMline.h"
#include "acdb/AcDbPoint.h"
#include "acdb/AcDbPolyFaceMesh.h"
#include "acdb/AcDbPolyline.h"
#include "acdb/AcDbRay.h"
#include "acdb/AcDbShape.h"
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
    AcDb3dSolid, AcDbLight, AcDbBlockReference,
    // Annotation family appended last: the variant index doubles as the
    // persisted class key in AcDbStore, so existing indexes must never
    // shift.
    AcDbRotatedDimension, AcDbAlignedDimension, AcDb2LineAngularDimension,
    AcDb3PointAngularDimension, AcDbRadialDimension,
    AcDbDiametricDimension, AcDbOrdinateDimension, AcDbLeader, AcDbFcf,
    AcDbShape, AcDbAttributeDefinition, AcDbAttribute, AcDbMInsertBlock>;

// Read-write access to the common-properties member shared by every
// payload type (all of them embed acdb::AcDbEntity as `common`).
AcDbEntity &common(AcDbEntityVariant &payload);
const AcDbEntity &common(const AcDbEntityVariant &payload);

} // namespace acdb
