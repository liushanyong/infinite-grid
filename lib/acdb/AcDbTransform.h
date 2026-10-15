#pragma once

// AcDbEntity transform helpers (namespace acdb) — ObjectARX
// AcDbEntity::transformBy over the value-type payloads.  Translation
// covers every supported type; the matrix overload applies to the types
// whose geometry is plain point sets (block-reference rotations and
// non-uniform scales of curves degrade through their sampled forms, so
// they are out of scope here).

#include "acdb/AcDbEntities.h"

namespace acdb
{

// AcDbEntity::transformBy (translation): shifts every geometric point of
// |payload| by |offset|.
void transformBy(AcDbEntityVariant &payload, const AcGeVector3d &offset);

} // namespace acdb
