#pragma once

// AcDbViewportTable / AcDbViewTable (namespace acdb) — viewport and
// named-view configuration as drawing data, aligned with ObjectARX:
//
//   AcDbViewportTableRecord  one tiled model-space viewport (the VPORT
//                            table): view target/direction, height,
//                            lens, twist, grid bounds.  The default
//                            single-viewport record is "*Active".
//   AcDbViewTableRecord      one named view (the VIEW table).
//   AcDbViewportTable        the VPORT container (name-keyed records).
//   AcDbViewTable            the VIEW container.
//
// The records carry handles like every other resident (AcDbObject base
// through AcDbSymbolTableRecord), so the identity split applies: SQLite
// persists them by handle, the ECS mirror keys them by AcDbObjectId,
// and acgs::AcGsView is their live session state.  The GS↔DB parameter
// transfer lives in acgs::AcGsView (applyViewportRecord /
// writeToViewportRecord).

#include <string>
#include <unordered_map>

#include "acdb/AcDbCore.h"
#include "ge/gematrix.h"
#include "ge/gepoint.h"

namespace acdb
{

// ObjectARX: the default model-space viewport record (DXF VPORT "*Active").
inline constexpr const char *kActiveViewportName = "*Active";

// ---- AcDbAbstractViewTableRecord: fields shared by VPORT and VIEW ----

class AcDbAbstractViewTableRecord : public AcDbSymbolTableRecord
{
public:
    // View direction and aim point (ObjectARX: setViewDirection /
    // setTarget).  The direction points from the camera toward the
    // target.
    const AcGeVector3d &viewDirection() const { return viewDirection_; }
    void setViewDirection(const AcGeVector3d &direction)
    {
        viewDirection_ = direction;
    }
    const AcGePoint3d &target() const { return target_; }
    void setTarget(const AcGePoint3d &point) { target_ = point; }

    // Vertical extent of the view in drawing units (ObjectARX:
    // setHeight); the horizontal extent rides the viewport aspect.
    double height() const { return height_; }
    void setHeight(double height) { height_ = height > 0.0 ? height : 1.0; }

    // ObjectARX: perspective toggle (false = orthographic) and the lens
    // length in millimetres (50mm default, ObjectARX convention).
    bool isPerspectiveEnabled() const { return perspective_; }
    void setPerspectiveEnabled(bool enabled) { perspective_ = enabled; }
    double lensLength() const { return lensLength_; }
    void setLensLength(double millimetres)
    {
        lensLength_ = millimetres > 0.0 ? millimetres : 50.0;
    }

    // Counter-clockwise view twist in radians.  The demo camera keeps no
    // roll, so the GS sync always writes 0 (AcGsView notes the limit).
    double twistAngle() const { return twistAngle_; }
    void setTwistAngle(double radians) { twistAngle_ = radians; }

    // Viewport grid bounds in normalized 0..1 coordinates (ObjectARX:
    // lowerLeftCorner / upperRightCorner); the ViewTableRecord carries
    // the same fields for symmetry.
    const AcGePoint2d &lowerLeft() const { return lowerLeft_; }
    void setLowerLeft(const AcGePoint2d &point) { lowerLeft_ = point; }
    const AcGePoint2d &upperRight() const { return upperRight_; }
    void setUpperRight(const AcGePoint2d &point) { upperRight_ = point; }

protected:
    AcGeVector3d viewDirection_{0.0, 0.0, 1.0};
    AcGePoint3d target_{0.0, 0.0, 0.0};
    double height_ = 1.0;
    bool perspective_ = false;
    double lensLength_ = 50.0;
    double twistAngle_ = 0.0;
    AcGePoint2d lowerLeft_{0.0, 0.0};
    AcGePoint2d upperRight_{1.0, 1.0};
};

struct AcDbViewportTableRecord : AcDbAbstractViewTableRecord
{
};

struct AcDbViewTableRecord : AcDbAbstractViewTableRecord
{
};

// ---- containers (same shape as AcDbLayerTable) ----

class AcDbViewportTable
{
public:
    const AcDbViewportTableRecord *get(const std::string &name) const;
    AcDbViewportTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const
    {
        return records_.count(name) != 0;
    }
    AcDbViewportTableRecord &add(const std::string &name, AcDbHandle handle);
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbViewportTableRecord> records_;
};

class AcDbViewTable
{
public:
    const AcDbViewTableRecord *get(const std::string &name) const;
    AcDbViewTableRecord *getMutable(const std::string &name);
    bool contains(const std::string &name) const
    {
        return records_.count(name) != 0;
    }
    AcDbViewTableRecord &add(const std::string &name, AcDbHandle handle);
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[name, record] : records_)
            fn(name, record);
    }

private:
    std::unordered_map<std::string, AcDbViewTableRecord> records_;
};

} // namespace acdb
