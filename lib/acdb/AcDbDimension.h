#pragma once

#include "AcDbPort.h"

// AcDbDimension family (namespace acdb), aligned with the ObjectARX
// dimension class tree: one AcDbDimension common-annotation payload
// embedded per concrete type, exactly like AcDbEntity rides along as
// `common`.  The concrete types mirror the DXF/DWG dimension sub-formats
// (DIMENSION_LINEAR = AcDbRotatedDimension, DIMENSION_ALIGNED =
// AcDbAlignedDimension, DIMENSION_ANG2LN = AcDb2LineAngularDimension,
// DIMENSION_ANG3PT = AcDb3PointAngularDimension, DIMENSION_RADIUS =
// AcDbRadialDimension, DIMENSION_DIAMETER = AcDbDiametricDimension,
// DIMENSION_ORDINATE = AcDbOrdinateDimension).
//
// Appearance (arrow size, text height, colors) normally lives in the
// referenced DIMSTYLE table record; this value-type layer carries only
// the style NAME plus a per-entity textHeight fallback, and worldDraw
// derives the rest from geometry so unnamed styles still draw.

#include <glm/glm.hpp>

#include <string>

#include "../acge/AcGe.h"
#include "acdb/AcDbCore.h"

namespace acdb
{

// Shared annotation payload (ObjectARX: AcDbDimension accessors).
struct ACDB_PORT AcDbDimension
{
    // Dimension text placement (DXF 11); empty when the text sits at the
    // dimension-line midpoint.
    AcGePoint3d textPosition{0.0, 0.0, 0.0};
    // User-authored text override (DXF 1); empty = drawn measurement.
    std::string dimensionText;
    double textRotation = 0.0;
    double actualMeasurement = 0.0;
    // Referenced DIMSTYLE table record by name (resolved from the file's
    // dimstyle handle; empty when unresolvable).
    std::string dimensionStyleName;
    // Per-entity fallback for DIMSTYLE.DIMTXT (0 = derive from geometry).
    double textHeight = 0.0;
};

// AcDbRotatedDimension (DXF/DWG DIMENSION_LINEAR): measures the distance
// between two points along a rotated direction.
struct ACDB_PORT AcDbRotatedDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d xLine1Point{0.0, 0.0, 0.0};
    AcGePoint3d xLine2Point{0.0, 0.0, 0.0};
    // A point on the dimension line (DXF 10, "definition point").
    AcGePoint3d dimensionLinePoint{0.0, 0.0, 0.0};
    // Dimension line direction angle (DXF 52 "oblique" is separate).
    double dimensionLineAngle = 0.0;
    double obliqueAngle = 0.0;
};

// AcDbAlignedDimension (DIMENSION_ALIGNED): measures the true distance
// between two points; the dimension line stays parallel to them.
struct ACDB_PORT AcDbAlignedDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d xLine1Point{0.0, 0.0, 0.0};
    AcGePoint3d xLine2Point{0.0, 0.0, 0.0};
    AcGePoint3d dimensionLinePoint{0.0, 0.0, 0.0};
    double obliqueAngle = 0.0;
};

// AcDb2LineAngularDimension (DIMENSION_ANG2LN): angle between two lines,
// each authored as a segment; the arc center is the lines' intersection
// (DXF 10 carries it pre-computed).
struct ACDB_PORT AcDb2LineAngularDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d xLine1StartPoint{0.0, 0.0, 0.0};
    AcGePoint3d xLine1EndPoint{0.0, 0.0, 0.0};
    AcGePoint3d xLine2StartPoint{0.0, 0.0, 0.0};
    AcGePoint3d xLine2EndPoint{0.0, 0.0, 0.0};
    // Center of the dimension arc (DXF 10 definition point).
    AcGePoint3d arcPoint{0.0, 0.0, 0.0};
};

// AcDb3PointAngularDimension (DIMENSION_ANG3PT): angle at an explicit
// center between two ray targets.
struct ACDB_PORT AcDb3PointAngularDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d centerPoint{0.0, 0.0, 0.0};
    AcGePoint3d xLine1Point{0.0, 0.0, 0.0};
    AcGePoint3d xLine2Point{0.0, 0.0, 0.0};
};

// AcDbRadialDimension (DIMENSION_RADIUS): radius callout from the circle
// center out to a point on the circle with a landing of leaderLength.
struct ACDB_PORT AcDbRadialDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d centerPoint{0.0, 0.0, 0.0};
    AcGePoint3d radiusPoint{0.0, 0.0, 0.0};
    double leaderLength = 0.0;
};

// AcDbDiametricDimension (DIMENSION_DIAMETER): a full chord through the
// circle with arrows on both ends (DXF 10 = chord point, 15 = far point).
struct ACDB_PORT AcDbDiametricDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d chordPoint{0.0, 0.0, 0.0};
    AcGePoint3d farChordPoint{0.0, 0.0, 0.0};
    double leaderLength = 0.0;
};

// AcDbOrdinateDimension (DIMENSION_ORDINATE): X/Y datum coordinate of a
// feature point with a jogged leader.
struct ACDB_PORT AcDbOrdinateDimension : AcDbEntity
{
    // AcGiDrawable protocol: the entity draws itself (ARX worldDraw
    // contract); definitions live in AcDbEntityWorldDraw.cpp.
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    // Dimensions report kDrawableIsDimension (ObjectARX parity).
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    AcDbDimension dimension;
    AcGePoint3d featureLocationPoint{0.0, 0.0, 0.0};
    AcGePoint3d leaderEndPoint{0.0, 0.0, 0.0};
    bool useXAxis = true;
};

} // namespace acdb
