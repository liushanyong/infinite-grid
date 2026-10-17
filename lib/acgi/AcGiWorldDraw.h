#pragma once

#include "AcGiPort.h"

// AcGi-compatible graphics interface: the draw-callback protocol entities
// use to emit primitives (see ObjectARX acgi.h, "AcGiGeometry" /
// "AcGiWorldGeometry" / "AcGiWorldDraw"; arxref.chm, "AcGi Overview").
// The implementation records the callbacks into the scene geometry sink
// with the current sub-entity traits applied per call, exactly like the
// traits-state semantics of AcGi.
//
// Hierarchy (ObjectARX shape):
//   AcGiCommonDraw         draw-state base (regen/dragging/deviation/context)
//   ├─ AcGiWorldDraw       : AcGiWorldGeometry + AcGiSubEntityTraits facet
//   │                        (view-independent world recording)
//   └─ AcGiViewportDraw    : + AcGiViewportGeometry facet
//                             (eye/DC protocol, raster images, screen
//                             density, AcGiViewport queries)
//   AcGiGeometry is a virtual base shared by AcGiWorldGeometry and
//   AcGiViewportGeometry so the concrete viewport draw exposes one
//   primitive toolkit.
//
// Return convention (ObjectARX contract, arxref.chm "AcGiGeometry"): the
// primitive callbacks return kFalse (false) when the primitive was stored
// in the graphics database, kTrue (true) when the operation was terminated
// and the application wants control back.  AcGiDrawable::worldDraw keeps
// the other ObjectARX convention (kTrue = viewport-independent).
//
// Port deviations from ObjectARX, all consequences of the value-type layer:
// callbacks are non-const (the sink records through a reference); primitive
// spellings keep the port's established forms (worldCircle/worldPoint/
// worldPolyline with a closed flag, worldTriangle, worldInfiniteLine for
// Ray/XLine overlay semantics); behavior transforms resolve viewport/screen inputs into
// world soup at push time; clip boundaries
// support simple concave polygons, clip-space transforms, depth limits, and
// optional outlines; mesh/shell consume color, layer, linetype, selection,
// material, transparency, visibility, and normal attributes into the sink;
// image is rasterized as a bounded per-pixel triangle
// approximation; polypoint emits real point primitives instead of ARX's
// degenerate two-point strokes.
// edge() accepts a value-semantic subset of planar curves rather than native
// AcGeCurve2d pointers; native GDI owner-draw uploads are not implemented.

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "arx/ArxTypes.h"
#include "arx/AcCmEntityColor.h"
#include "arx/AcCmTransparency.h"
#include "../acge/AcGe.h"
#include "../acgs/model/DrawContext.h"
#include "AcGiLineType.h"
#include "AcGiTextStyle.h"
#include "AcGiTextQueue.h"

class AcGiDrawable;

class AcGiMapper
{
public:
    enum Projection
    {
        kInheritProjection = 0,
        kPlanar,
        kBox,
        kCylinder,
        kSphere
    };
    enum Tiling
    {
        kInheritTiling = 0,
        kTile,
        kCrop,
        kClamp,
        kMirror
    };
    enum AutoTransform
    {
        kInheritAutoTransform = 0,
        kNone = 0x1,
        kObject = 0x2,
        kModel = 0x4
    };

    void set(const AcGiMapper &mapper) { *this = mapper; }
    void setProjection(Projection value) { projection_ = value; }
    void setAutoTransform(AutoTransform value) { autoTransform_ = value; }
    void setTransform(const AcGeMatrix3d &value) { transform_ = value; }
    void setUTiling(Tiling value) { uTiling_ = value; }
    void setVTiling(Tiling value) { vTiling_ = value; }
    Projection projection() const { return projection_; }
    AutoTransform autoTransform() const { return autoTransform_; }
    const AcGeMatrix3d &transform() const { return transform_; }
    Tiling uTiling() const { return uTiling_; }
    Tiling vTiling() const { return vTiling_; }

private:
    Projection projection_ = kInheritProjection;
    AutoTransform autoTransform_ = kInheritAutoTransform;
    AcGeMatrix3d transform_;
    Tiling uTiling_ = kInheritTiling;
    Tiling vTiling_ = kInheritTiling;
};

// AcGeCurve3d is the analytic curve base provided by the acge layer;
// AcGi uses it to implement the generic curve callback.

// ---- enums (acgidefs.h / acgi.h) ----

enum AcGiArcType
{
    kAcGiArcSimple = 0, // just the arc (not fillable)
    kAcGiArcSector,     // area bounded by the arc and its center of curvature
    kAcGiArcChord       // area bounded by the arc and its end points
};

enum AcGiFillType
{
    kAcGiFillAlways = 1,
    kAcGiFillNever
};

// ObjectARX AcGiRegenType / AcGiDeviationType (acgidefs.h): draw-context
// state consumed by AcGiCommonDraw queries.
enum AcGiRegenType
{
    kAcGiRegenDisabled = 0,
    kAcGiRegenQuickDraw,
    kAcGiRegen,
    kAcGiRegenAll
};

enum AcGiDeviationType
{
    kAcGiDeviationCircle = 0,
    kAcGiDeviationCurve,
    kAcGiDeviationSurface
};

// Value-segment subset used by the port's edge() callback.  The separate
// edge(const AcGeCurve2d*, count) overload accepts the analytic base hierarchy.
struct AcGiEdge2d
{
    std::variant<AcGeLineSeg2d, AcGeCircArc2d, AcGeEllipArc2d> curve;
};

struct resbuf;

enum AcGiPositionTransformBehavior
{
    kAcGiWorldPosition,
    kAcGiViewportPosition,
    kAcGiScreenPosition,
    kAcGiScreenLocalOriginPosition,
    kAcGiWorldWithScreenOffsetPosition
};

enum AcGiScaleTransformBehavior
{
    kAcGiWorldScale,
    kAcGiViewportScale,
    kAcGiScreenScale,
    kAcGiViewportLocalOriginScale,
    kAcGiScreenLocalOriginScale
};

enum AcGiOrientationTransformBehavior
{
    kAcGiWorldOrientation,
    kAcGiScreenOrientation,
    kAcGiZAxisOrientation
};

enum AcGiOrientationType
{
    kAcGiCounterClockwise = -1,
    kAcGiNoOrientation = 0,
    kAcGiClockwise = 1
};

// ---- port value types for the toolkit signatures ----

struct AcGiPolyline
{
    std::vector<AcGePoint3d> vertices;
    bool closed = false;
};

struct AcGiEdgeData
{
    std::vector<short> colors;
    std::vector<AcCmEntityColor> trueColors;
    std::vector<acdb::AcDbObjectId> layers;
    std::vector<acdb::AcDbObjectId> linetypeIdValues;
    std::vector<Adesk::LongPtr> selectionMarkers;
    std::vector<std::string> lineTypes;
    std::vector<Adesk::UInt8> visibility;
    const acdb::AcDbObjectId *borrowedLinetypeIds = nullptr;

    void setColors(const short *values, size_t count)
    {
        if (values == nullptr)
        {
            colors.clear();
            return;
        }
        colors.assign(values, values + count);
    }
    short *colorsData() { return colors.data(); }
    short *colorsData() const
    {
        return const_cast<short *>(colors.data());
    }
    void setLayers(const acdb::AcDbObjectId *values, size_t count)
    {
        if (values == nullptr)
            layers.clear();
        else
            layers.assign(values, values + count);
    }
    acdb::AcDbObjectId *layerIds() { return layers.data(); }
    acdb::AcDbObjectId *layerIds() const
    {
        return const_cast<acdb::AcDbObjectId *>(layers.data());
    }
    void setLinetypeIds(const acdb::AcDbObjectId *values, size_t count)
    {
        if (values == nullptr)
        {
            linetypeIdValues.clear();
            borrowedLinetypeIds = nullptr;
        }
        else
        {
            linetypeIdValues.assign(values, values + count);
            borrowedLinetypeIds = nullptr;
        }
    }
    void setLineTypes(const acdb::AcDbObjectId *values)
    {
        linetypeIdValues.clear();
        borrowedLinetypeIds = values;
    }
    void setLineTypes(const acdb::AcDbObjectId *values, size_t count)
    {
        setLinetypeIds(values, count);
    }
    acdb::AcDbObjectId *linetypeIds() const
    {
        return const_cast<acdb::AcDbObjectId *>(
            linetypeIdValues.empty() ? borrowedLinetypeIds
                                     : linetypeIdValues.data());
    }
    acdb::AcDbObjectId *linetypeIdsData() { return linetypeIds(); }
    acdb::AcDbObjectId *linetypeIdsData() const
    {
        return linetypeIds();
    }
    void setSelectionMarkers(const Adesk::LongPtr *values, size_t count)
    {
        if (values == nullptr)
            selectionMarkers.clear();
        else
            selectionMarkers.assign(values, values + count);
    }
    Adesk::LongPtr *selectionMarkersData() { return selectionMarkers.data(); }
    Adesk::LongPtr *selectionMarkersData() const
    {
        return const_cast<Adesk::LongPtr *>(selectionMarkers.data());
    }
    void setLineTypes(const AcGiLineType *values, size_t count)
    {
        lineTypes.clear();
        if (values == nullptr)
            return;
        lineTypes.reserve(count);
        for (size_t index = 0; index < count; ++index)
            lineTypes.emplace_back(values[index].name == nullptr
                                       ? ""
                                       : values[index].name);
    }
    void setTrueColors(const AcCmEntityColor *values, size_t count)
    {
        if (values == nullptr)
            trueColors.clear();
        else
            trueColors.assign(values, values + count);
    }
    void setVisibility(const Adesk::UInt8 *values, size_t count)
    {
        if (values == nullptr)
            visibility.clear();
        else
            visibility.assign(values, values + count);
    }
};
struct AcGiFaceData
{
    std::vector<short> colors;
    std::vector<AcCmEntityColor> trueColors;
    std::vector<acdb::AcDbObjectId> layers;
    std::vector<Adesk::LongPtr> selectionMarkers;
    std::vector<Adesk::UInt8> visibility;
    std::vector<AcGeVector3d> normals;
    std::vector<acdb::AcDbObjectId> materials;
    std::vector<AcGiMapper> ownedMappers;
    const AcGiMapper *mapperValues = nullptr;
    size_t mapperCount = 0;
    bool mapperCountKnown = false;
    std::vector<AcCmTransparency> transparency;

    void setColors(const short *values, size_t count)
    {
        if (values == nullptr)
        {
            colors.clear();
            return;
        }
        colors.assign(values, values + count);
    }
    short *colorsData() { return colors.data(); }
    short *colorsData() const
    {
        return const_cast<short *>(colors.data());
    }
    void setTrueColors(const AcCmEntityColor *values, size_t count)
    {
        if (values == nullptr)
            trueColors.clear();
        else
            trueColors.assign(values, values + count);
    }
    AcCmEntityColor *trueColorsData() { return trueColors.data(); }
    AcCmEntityColor *trueColorsData() const
    {
        return const_cast<AcCmEntityColor *>(trueColors.data());
    }
    void setLayers(const acdb::AcDbObjectId *values, size_t count)
    {
        if (values == nullptr)
            layers.clear();
        else
            layers.assign(values, values + count);
    }
    acdb::AcDbObjectId *layerIds() { return layers.data(); }
    acdb::AcDbObjectId *layerIds() const
    {
        return const_cast<acdb::AcDbObjectId *>(layers.data());
    }
    void setSelectionMarkers(const Adesk::LongPtr *values, size_t count)
    {
        if (values == nullptr)
            selectionMarkers.clear();
        else
            selectionMarkers.assign(values, values + count);
    }
    Adesk::LongPtr *selectionMarkersData()
    {
        return selectionMarkers.data();
    }
    Adesk::LongPtr *selectionMarkersData() const
    {
        return const_cast<Adesk::LongPtr *>(selectionMarkers.data());
    }
    void setMaterials(const acdb::AcDbObjectId *values, size_t count)
    {
        if (values == nullptr)
            materials.clear();
        else
            materials.assign(values, values + count);
    }
    acdb::AcDbObjectId *materialsData() { return materials.data(); }
    acdb::AcDbObjectId *materialsData() const
    {
        return const_cast<acdb::AcDbObjectId *>(materials.data());
    }
    // Counted setters own a copy; this is the safe lifetime semantics used
    // when an entity has already left the mapper array on the stack.
    void setMappers(const AcGiMapper *values, size_t count)
    {
        ownedMappers.clear();
        mapperValues = nullptr;
        if (values != nullptr && count != 0)
        {
            ownedMappers.assign(values, values + count);
            mapperValues = ownedMappers.data();
            mapperCount = count;
        }
        else
        {
            mapperCount = 0;
        }
        mapperCountKnown = true;
    }
    void setMappers(std::vector<AcGiMapper> values)
    {
        ownedMappers = std::move(values);
        mapperValues = ownedMappers.empty() ? nullptr : ownedMappers.data();
        mapperCount = ownedMappers.size();
        mapperCountKnown = true;
    }

    // ARX-compatible borrowed overload: the caller must keep |values| alive
    // through the mesh/shell callback.  It intentionally replaces any owned
    // mapper vector so the two lifetime modes cannot silently mix.
    void setMappers(const AcGiMapper *values)
    {
        ownedMappers.clear();
        mapperValues = values;
        mapperCount = 0;
        mapperCountKnown = false;
    }
    AcGiMapper *mappers() const
    {
        return const_cast<AcGiMapper *>(mapperValues);
    }
    size_t mapperCountData() const { return mapperCount; }
    bool mapperCountKnownData() const { return mapperCountKnown; }
    void setVisibility(const Adesk::UInt8 *values, size_t count)
    {
        if (values == nullptr)
            visibility.clear();
        else
            visibility.assign(values, values + count);
    }
    Adesk::UInt8 *visibilityData() { return visibility.data(); }
    Adesk::UInt8 *visibilityData() const
    {
        return const_cast<Adesk::UInt8 *>(visibility.data());
    }
    void setNormals(const AcGeVector3d *values, size_t count)
    {
        if (values == nullptr)
            normals.clear();
        else
            normals.assign(values, values + count);
    }
    AcGeVector3d *normalsData() { return normals.data(); }
    AcGeVector3d *normalsData() const
    {
        return const_cast<AcGeVector3d *>(normals.data());
    }
    void setTransparency(const AcCmTransparency *values, size_t count)
    {
        if (values == nullptr)
            transparency.clear();
        else
            transparency.assign(values, values + count);
    }
    AcCmTransparency *transparencyData() { return transparency.data(); }
    AcCmTransparency *transparencyData() const
    {
        return const_cast<AcCmTransparency *>(transparency.data());
    }
};
struct AcGiVertexData
{
    enum MapChannel
    {
        kAllChannels,
        kMapChannels
    };

    std::vector<AcCmEntityColor> trueColors;
    std::vector<AcGeVector3d> normals;

    void setOrientationFlag(AcGiOrientationType value)
    {
        orientation = value;
    }
    AcGiOrientationType orientationFlag() const { return orientation; }

    void setMappingCoords(MapChannel channel, const AcGePoint3d *values)
    {
        switch (channel)
        {
        case kAllChannels:
            allMappingCoords = values;
            break;
        case kMapChannels:
            mapMappingCoords = values;
            break;
        }
    }
    AcGePoint3d *mappingCoords(MapChannel channel) const
    {
        switch (channel)
        {
        case kAllChannels:
            return const_cast<AcGePoint3d *>(allMappingCoords);
        case kMapChannels:
            return const_cast<AcGePoint3d *>(mapMappingCoords);
        }
        return nullptr;
    }

    void setTrueColors(const AcCmEntityColor *values, size_t count)
    {
        if (values == nullptr)
            trueColors.clear();
        else
            trueColors.assign(values, values + count);
    }
    void setNormals(const AcGeVector3d *values, size_t count)
    {
        if (values == nullptr)
            normals.clear();
        else
            normals.assign(values, values + count);
    }

private:
    AcGiOrientationType orientation = kAcGiNoOrientation;
    const AcGePoint3d *allMappingCoords = nullptr;
    const AcGePoint3d *mapMappingCoords = nullptr;
};

// Clip boundary (port subset: a convex polygonal clip plane).
struct AcGiClipBoundary
{
    // The ObjectARX clip-plane contract. The lower-case `points` member is
    // retained as a convenience for existing port callers.
    AcGeVector3d m_vNormal{0.0, 0.0, 1.0};
    AcGePoint3d m_ptPoint{0.0, 0.0, 0.0};
    std::vector<AcGePoint2d> m_aptPoints;
    AcGeMatrix3d m_xToClipSpace;
    AcGeMatrix3d m_xInverseBlockRefXForm;
    bool m_bClippingFront = false;
    bool m_bClippingBack = false;
    double m_dFrontClipZ = 0.0;
    double m_dBackClipZ = 0.0;
    bool m_bDrawBoundary = false;
    std::vector<AcGePoint2d> points;
};

struct AcGiGdiDrawObject
{
};

// Minimal owner-draw tokens for the port.  Native GDI upload remains outside
// the renderer, but AcGiViewportDraw now has protocol slots for these calls.
class AcGiOwnerDraw {};
class AcGiOwnerDraw3d {};

struct AcGiPixelBGRA32
{
    Adesk::UInt8 b = 0;
    Adesk::UInt8 g = 0;
    Adesk::UInt8 r = 0;
    Adesk::UInt8 a = 255;
};

class AcGiImageBGRA32
{
public:
    AcGiImageBGRA32() = default;
    AcGiImageBGRA32(Adesk::UInt32 width, Adesk::UInt32 height,
                    const AcGiPixelBGRA32 *pixels)
    {
        setImage(width, height, pixels);
    }

    const AcGiPixelBGRA32 *image() const { return pixels_.data(); }
    Adesk::UInt32 width() const { return width_; }
    Adesk::UInt32 height() const { return height_; }
    void setImage(Adesk::UInt32 width, Adesk::UInt32 height,
                  const AcGiPixelBGRA32 *pixels)
    {
        pixels_.clear();
        if (pixels != nullptr && width != 0 && height != 0 &&
            size_t(height) <= std::numeric_limits<size_t>::max() / width)
        {
            const size_t count = size_t(width) * height;
            pixels_.assign(pixels, pixels + count);
            width_ = width;
            height_ = height;
        }
        else
        {
            width_ = 0;
            height_ = 0;
        }
    }

private:
    Adesk::UInt32 width_ = 0;
    Adesk::UInt32 height_ = 0;
    std::vector<AcGiPixelBGRA32> pixels_;
};

// ---- AcGiGeometry: the base class for geometry toolkits ----

class ACGI_PORT AcGiGeometry
{
public:
    enum TransparencyMode
    {
        kTransparencyOff,  // pixel alpha ignored, all pixels are opaque
        kTransparency1Bit, // pixel alpha determines transparency on/off,
        kTransparency8Bit  // pixel alpha determines transparency level
    };

    virtual ~AcGiGeometry() = default;

    // Coordinate transformations: subsequent callbacks are expressed in the
    // pushed frame (ObjectARX AcGiGeometry::pushModelTransform).
    virtual Adesk::Boolean pushModelTransform(const AcGeMatrix3d &matrix) = 0;
    virtual Adesk::Boolean pushModelTransform(const AcGeVector3d &translation) = 0;
    virtual Adesk::Boolean popModelTransform() = 0;
    // The net model-to-world transform (block insert chain); its inverse
    // maps world coordinates back into the pushed model frame.
    virtual void getModelToWorldTransform(AcGeMatrix3d &matrix) const = 0;
    virtual void getWorldToModelTransform(AcGeMatrix3d &matrix) const = 0;

    // Nested-drawable replay (compound owners expand their members through
    // this).  Defined in AcGiDrawable.h once the drawable protocol is
    // complete; the null pointer draws nothing.  Returns kTrue when the
    // replay was terminated.
    virtual Adesk::Boolean draw(AcGiDrawable *nested) = 0;

    // ---- primitives (concrete emitters) ----

    virtual Adesk::Boolean worldLine(const AcGePoint3d &from,
                                     const AcGePoint3d &to) = 0;

    // ObjectARX array form; forwards to the two-point form.
    virtual Adesk::Boolean worldLine(const AcGePoint3d points[2])
    {
        return worldLine(points[0], points[1]);
    }

    virtual Adesk::Boolean worldPolyline(const AcGePoint3d *points, int count,
                                         bool closed) = 0;

    virtual Adesk::Boolean worldTriangle(const AcGePoint3d &a,
                                         const AcGePoint3d &b,
                                         const AcGePoint3d &c,
                                         bool fillIs3DFace = false) = 0;

    // A full circle when the arc spans two pi, an open arc otherwise.
    virtual Adesk::Boolean worldCircle(const AcGeCircArc3d &arc) = 0;

    virtual Adesk::Boolean worldPoint(
        const AcGePoint3d &position,
        acge::PointDisplayStyle displayStyle = acge::PointDisplayStyle::Circle) = 0;

    // Semi-infinite stroke: the stored endpoint is only a proxy; renderers
    // and pickers treat the geometry as extending forever along the
    // direction.  This is an extension beyond classic AcGi, needed because
    // Ray/XLine overlay semantics live in the sink payload.
    virtual Adesk::Boolean worldInfiniteLine(const AcGePoint3d &start,
                                             const AcGeVector3d &direction) = 0;

    // Text callback; the style carries the text height.
    virtual Adesk::Boolean text(const AcGePoint3d &position,
                                const AcGeVector3d &normal,
                                const AcGeVector3d &direction,
                                const char *message,
                                const AcGiTextStyle &style) = 0;

    // ObjectARX draws ellipses as polylines; this convenience keeps the
    // same tessellation policy in one place.
    virtual Adesk::Boolean worldEllipse(const AcGeEllip3d &ellipse) = 0;

    virtual Adesk::Boolean circle(const AcGePoint3d &center,
                                  const double radius,
                                  const AcGeVector3d &normal) = 0;

    // The full circle through the three points.
    virtual Adesk::Boolean circle(const AcGePoint3d &first,
                                  const AcGePoint3d &second,
                                  const AcGePoint3d &third) = 0;

    // startVector sets the arc's start direction inside the plane; the
    // sweep runs CCW around |normal|.  kAcGiArcSector / kAcGiArcChord add
    // the closure strokes and the filled fan (fillType default kFill).
    virtual Adesk::Boolean circularArc(const AcGePoint3d &center,
                                       const double radius,
                                       const AcGeVector3d &normal,
                                       const AcGeVector3d &startVector,
                                       const double sweepAngle,
                                       const AcGiArcType arcType = kAcGiArcSimple) = 0;

    // The arc from |start| through |point| to |end|.
    virtual Adesk::Boolean circularArc(const AcGePoint3d &start,
                                       const AcGePoint3d &point,
                                       const AcGePoint3d &end,
                                       const AcGiArcType arcType = kAcGiArcSimple) = 0;

    virtual Adesk::Boolean ellipticalArc(
        const AcGePoint3d &center, const AcGeVector3d &normal,
        double majorAxisLength, double minorAxisLength,
        double startAngleInRad, double endAngleInRad, double tiltAngleInRad,
        AcGiArcType arcType = kAcGiArcSimple) = 0;

    // rows x columns vertex grid; each cell becomes two triangles.
    virtual Adesk::Boolean mesh(const Adesk::UInt32 rows,
                                const Adesk::UInt32 columns,
                                const AcGePoint3d *vertexList,
                                const AcGiEdgeData *edgeData = nullptr,
                                const AcGiFaceData *faceData = nullptr,
                                const AcGiVertexData *vertexData = nullptr,
                                const bool autoGenerateNormals = true) = 0;

    // ARX face-list encoding: positive counts start faces and following
    // negative counts describe holes in the preceding face.
    virtual Adesk::Boolean shell(const Adesk::UInt32 nbVertex,
                                 const AcGePoint3d *vertexList,
                                 const Adesk::UInt32 faceListSize,
                                 const Adesk::Int32 *faceList,
                                 const AcGiEdgeData *edgeData = nullptr,
                                 const AcGiFaceData *faceData = nullptr,
                                 const AcGiVertexData *vertexData = nullptr,
                                 const struct resbuf *resbufData = nullptr,
                                 bool autoGenerateNormals = true) = 0;

    // ---- primitives (ObjectARX contract: composed over the emitters) ----

    virtual Adesk::Boolean polyline(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList,
        const AcGeVector3d * /*normal*/ = nullptr,
        Adesk::LongPtr /*baseSubEntMarker*/ = -1)
    {
        return worldPolyline(vertexList, static_cast<int>(nbPoints), false);
    }

    virtual Adesk::Boolean polyline(const AcGiPolyline &polylineObj)
    {
        return worldPolyline(polylineObj.vertices.data(),
                             static_cast<int>(polylineObj.vertices.size()),
                             polylineObj.closed);
    }

    virtual Adesk::Boolean polyPolyline(const Adesk::UInt32 nbPolylines,
                                        const AcGiPolyline *polylines)
    {
        if (polylines == nullptr)
            return true;
        for (Adesk::UInt32 index = 0; index < nbPolylines; ++index)
        {
            if (polyline(polylines[index]))
                return true;
        }
        return false;
    }

    // Closed automatically; fillType() controls whether its interior is emitted.
    virtual Adesk::Boolean polygon(const Adesk::UInt32 nbPoints,
                                   const AcGePoint3d *vertexList)
    {
        return worldPolyline(vertexList, static_cast<int>(nbPoints), true);
    }

    virtual Adesk::Boolean polyPolygon(
        const Adesk::UInt32 numPolygonIndices,
        const Adesk::UInt32 *numPolygonPositions,
        const AcGePoint3d *polygonPositions,
        const Adesk::UInt32 *numPolygonPoints,
        const AcGePoint3d *polygonPoints,
        const AcCmEntityColor *outlineColors = nullptr,
        const AcGiLineType *outlineTypes = nullptr,
        const AcCmEntityColor *fillColors = nullptr,
        const AcCmTransparency *fillOpacities = nullptr) = 0;

    virtual Adesk::Boolean image(
        const AcGiImageBGRA32 &imageSource, const AcGePoint3d &position,
        const AcGeVector3d &u, const AcGeVector3d &v,
        TransparencyMode transparencyMode = kTransparency8Bit) = 0;

    virtual Adesk::Boolean edge(const std::vector<AcGiEdge2d> &edges) = 0;

    // ObjectARX-style planar curve array.  The concrete AcGiWorldDraw adapts
    // these type-erased value curves to the edge-value callback above.
    virtual Adesk::Boolean edge(const AcGeCurve2d *const *edges,
                                Adesk::UInt32 nbEdges)
    {
        (void)edges;
        (void)nbEdges;
        return true;
    }

    virtual Adesk::Boolean polypoint(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList,
        const AcGeVector3d * /*normal*/ = nullptr,
        const Adesk::LongPtr * /*subEntMarkers*/ = nullptr)
    {
        if (vertexList == nullptr)
            return true;
        for (Adesk::UInt32 index = 0; index < nbPoints; ++index)
        {
            if (worldPoint(vertexList[index]))
                return true;
        }
        return false;
    }

    virtual Adesk::Boolean rowOfDots(int count, const AcGePoint3d &start,
                                     const AcGeVector3d &step)
    {
        for (int index = 0; index < count; ++index)
        {
            if (worldPoint(start + step * static_cast<double>(index)))
                return true;
        }
        return false;
    }

    // Semi-infinite: starts at |start| and passes through |throughPoint|.
    virtual Adesk::Boolean ray(const AcGePoint3d &start,
                               const AcGePoint3d &throughPoint)
    {
        return worldInfiniteLine(start, throughPoint - start);
    }

    // Infinite in both directions through the two points.
    virtual Adesk::Boolean xline(const AcGePoint3d &first,
                                 const AcGePoint3d &second)
    {
        if (worldInfiniteLine(first, second - first))
            return true;
        return worldInfiniteLine(first, first - second);
    }

    // Behavior transforms.  Concrete world draw resolves viewport/screen
    // input using its optional viewport frame; the base fallback remains
    // world-space.  Each push returns the resolved matrix.
    virtual AcGeMatrix3d pushPositionTransform(
        AcGiPositionTransformBehavior /*behavior*/, const AcGePoint3d &offset)
    {
        const AcGeMatrix3d matrix =
            AcGeMatrix3d::setToTranslation(offset.asVector());
        pushModelTransform(matrix);
        return matrix;
    }

    virtual AcGeMatrix3d pushPositionTransform(
        AcGiPositionTransformBehavior behavior, const AcGePoint2d &offset)
    {
        return pushPositionTransform(
            behavior, AcGePoint3d(offset.x, offset.y, 0.0));
    }

    virtual AcGeMatrix3d pushScaleTransform(
        AcGiScaleTransformBehavior /*behavior*/, const AcGePoint3d &extents)
    {
        const AcGeMatrix3d matrix = AcGeMatrix3d::setToScaling(
            AcGeVector3d(extents.x, extents.y, extents.z));
        pushModelTransform(matrix);
        return matrix;
    }

    virtual AcGeMatrix3d pushScaleTransform(
        AcGiScaleTransformBehavior behavior, const AcGePoint2d &extents)
    {
        return pushScaleTransform(
            behavior, AcGePoint3d(extents.x, extents.y, 0.0));
    }

    virtual AcGeMatrix3d pushOrientationTransform(
        AcGiOrientationTransformBehavior /*behavior*/)
    {
        const AcGeMatrix3d matrix;
        pushModelTransform(matrix);
        return matrix;
    }

    // Clip boundaries are stacked and applied to subsequent world geometry.
    // The port supports two-corner rectangles and simple polygons in the
    // documented clip coordinate system, including front/back depth limits.
    virtual Adesk::Boolean pushClipBoundary(AcGiClipBoundary *boundary) = 0;
    virtual void popClipBoundary() = 0;

    // GDI owner-draw object: no-op default per ObjectARX (kTrue).
    virtual Adesk::Boolean ownerDraw(AcGiGdiDrawObject * /*object*/,
                                     const AcGePoint3d & /*position*/,
                                     const AcGeVector3d & /*u*/,
                                     const AcGeVector3d & /*v*/)
    {
        return true;
    }

    // Generic 3D curve: ARX default stores nothing (kFalse).  The concrete
    // AcGiWorldDraw below tessellates the supported value curves.
    virtual Adesk::Boolean curve(const AcGeCurve3d * /*curve*/)
    {
        return false;
    }
};

// ---- AcGiWorldGeometry: view-independent, model-coordinate methods ----

class ACGI_PORT AcGiWorldGeometry : public virtual AcGiGeometry
{
public:
    // ObjectARX AcGiWorldGeometry::setExtents: extents hint during
    // worldDraw.  The sink derives extents from the emitted primitives, so
    // the hint is accepted and unused until a consumer exists.
    virtual void setExtents(AcGePoint3d * /*pNewExtents*/) const {}

    // If this drawable is a block with AcDbAttributes it must call this
    // exactly once before sending the attributes to draw(); attributes are
    // the last objects rendered.  (Hook parity; the replay currently keeps
    // attributes in the same segment.)
    virtual void startAttributesSegment() {}
};

// ---- AcGiViewportGeometry: view-dependent eye/DC-space methods ----
// ObjectARX shape: AcGiGeometry <- AcGiViewportGeometry.  Both geometry
// facets share the AcGiGeometry virtual base so the concrete
// AcGiViewportDraw keeps one unambiguous primitive toolkit.

class ACGI_PORT AcGiViewportGeometry : public virtual AcGiGeometry
{
public:
    // ObjectARX AcGiViewportGeometry::ImageSource.
    enum ImageSource
    {
        kFromDwg,
        kFromOleObject,
        kFromRender
    };

    virtual Adesk::Boolean polylineEye(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) = 0;
    virtual Adesk::Boolean polygonEye(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) = 0;
    virtual Adesk::Boolean polylineDc(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) = 0;
    virtual Adesk::Boolean polygonDc(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) = 0;

    virtual Adesk::Boolean rasterImageDc(
        const AcGePoint3d &origin, const AcGeVector3d &u,
        const AcGeVector3d &v, const AcGiImageBGRA32 &imageSource,
        TransparencyMode transparencyMode = kTransparency8Bit) = 0;

    // Simplified entity-reference form of the ARX signature (pixel-to-DC
    // and unrotated-clone parameters are not modeled by the value port).
    virtual Adesk::Boolean rasterImageDc(
        const AcGePoint3d &origin, const AcGeVector3d &u,
        const AcGeVector3d &v, const acdb::AcDbObjectId &imageEntityId,
        ImageSource source, Adesk::UInt32 imageWidth,
        Adesk::UInt32 imageHeight, Adesk::Int16 imageColorDepth,
        bool transparency) = 0;

    virtual Adesk::Boolean ownerDrawDc(
        Adesk::Int32 viewportNumber, Adesk::Int32 left, Adesk::Int32 top,
        Adesk::Int32 right, Adesk::Int32 bottom,
        AcGiOwnerDraw *ownerDraw) = 0;
    virtual Adesk::Boolean ownerDraw3d(
        AcGePoint3d &minimum, AcGePoint3d &maximum,
        AcGiOwnerDraw3d *ownerDraw) = 0;
};

// ---- AcGiSubEntityTraits: the trait-state facet of a draw context ----

class ACGI_PORT AcGiSubEntityTraits
{
public:
    virtual ~AcGiSubEntityTraits() = default;

    virtual void setColor(const glm::vec4 &color) = 0;
    virtual void setLayer(std::string layer) = 0;
    virtual void setLineType(std::string lineType) = 0;
    virtual void setLineWeight(double lineWeight) = 0;
    virtual void setVisibility(bool visible) = 0;
    virtual void setFillType(AcGiFillType fillType) = 0;
    virtual AcGiFillType fillType() const = 0;
    virtual void setMaterial(const acdb::AcDbObjectId &materialId) = 0;
    virtual const acdb::AcDbObjectId &materialId() const = 0;

    // Port extension: applies the entity-common attribute set in one call.
    virtual void setFrom(const acdb::AcDbEntity &common) = 0;
};

// ---- AcGiContext: draw-context query (ObjectARX AcGiContext subset) ----

namespace acdb
{
class AcDbDatabase;
}

class ACGI_PORT AcGiContext
{
public:
    virtual ~AcGiContext() = default;

    virtual AcGiRegenType regenType() const = 0;
    virtual Adesk::Boolean isDragging() const = 0;

    // ObjectARX AcGiContext subset supported by the port.  PsOut / plot
    // generation are fixed false (no plot pipeline); the supports* queries
    // report the port's real capabilities.
    virtual Adesk::Boolean isPsOut() const = 0;
    virtual Adesk::Boolean isPlotGeneration() const = 0;
    virtual acdb::AcDbDatabase *database() const = 0;
    virtual bool isBoundaryClipping() const = 0;
    virtual bool isNesting() const = 0;
    virtual AcCmEntityColor effectiveColor() const = 0;
    virtual double byBlockLineWeight() const = 0;
    virtual bool supportsTrueTypeText() { return false; }
    virtual bool supportsOwnerDraw() { return false; }
    virtual bool supportsFillTrait() const { return false; }
    virtual bool supportsCurveTrait() const { return false; }
};

// ---- AcGiCommonDraw: base for AcGiWorldDraw / AcGiViewportDraw ----

class ACGI_PORT AcGiCommonDraw
{
public:
    virtual ~AcGiCommonDraw() = default;

    virtual AcGiRegenType regenType() const = 0;
    virtual Adesk::Boolean regenAbort() const = 0;
    // ObjectARX returns the mutable facet through a const context; the port
    // mirrors that signature with a const_cast.
    virtual AcGiSubEntityTraits &subEntityTraits() const = 0;
    virtual AcGiGeometry *rawGeometry() const = 0;
    virtual Adesk::Boolean isDragging() const = 0;
    virtual double deviation(AcGiDeviationType type,
                             const AcGePoint3d &point) const = 0;
    virtual Adesk::UInt32 numberOfIsolines() const = 0;
    virtual AcGiContext *context() = 0;
    virtual bool secondaryCall() const { return false; }
};

// ---- AcGiWorldDraw: shared-world draw context ----

class ACGI_PORT AcGiWorldDraw : public AcGiCommonDraw,
                                public AcGiWorldGeometry,
                                public AcGiSubEntityTraits
{
public:
    explicit AcGiWorldDraw(acgs::ViewportDraw &draw) : draw_(draw) {}

    // The renderer sink context this facade records into; the entity
    // drawables call the acdb::worldDraw overload set through it.  (ARX
    // reserves context() for AcGiCommonDraw::context, so the port-level
    // accessor carries a gs prefix.)
    acgs::ViewportDraw &gsContext() { return draw_; }
    [[nodiscard]] const acgs::ViewportDraw &gsContext() const
    {
        return draw_;
    }

    // The trait state the sink applies to subsequently emitted primitives.
    acgs::SubEntityTraits &gsSubEntityTraits()
    {
        return draw_.subEntityTraits();
    }
    const acgs::SubEntityTraits &gsSubEntityTraits() const
    {
        return draw_.subEntityTraits();
    }

    // AcGiSubEntityTraits facet: forwards to the trait state above.
    void setColor(const glm::vec4 &color) override
    {
        gsSubEntityTraits().setColor(color);
    }
    void setLayer(std::string layer) override
    {
        gsSubEntityTraits().setLayer(std::move(layer));
    }
    void setLineType(std::string lineType) override
    {
        gsSubEntityTraits().setLineType(std::move(lineType));
    }
    void setLineWeight(double lineWeight) override
    {
        gsSubEntityTraits().setLineWeight(lineWeight);
    }
    void setVisibility(bool visible) override
    {
        gsSubEntityTraits().setVisibility(visible);
    }
    void setFillType(AcGiFillType fillType) override
    {
        gsSubEntityTraits().setFillEnabled(fillType == kAcGiFillAlways);
    }
    AcGiFillType fillType() const override
    {
        return gsSubEntityTraits().traits().fillEnabled ? kAcGiFillAlways
                                                       : kAcGiFillNever;
    }
    void setMaterial(const acdb::AcDbObjectId &value) override
    {
        gsSubEntityTraits().setMaterial(value);
    }
    const acdb::AcDbObjectId &materialId() const override
    {
        return gsSubEntityTraits().materialId();
    }
    void setFrom(const acdb::AcDbEntity &common) override
    {
        gsSubEntityTraits().setFrom(common);
    }

    // ---- AcGiCommonDraw state (ObjectARX AcGiCommonDraw) ----

    AcGiRegenType regenType() const override
    {
        return static_cast<AcGiRegenType>(draw_.regenType());
    }
    Adesk::Boolean regenAbort() const override { return draw_.regenAbort(); }
    AcGiSubEntityTraits &subEntityTraits() const override
    {
        return const_cast<AcGiSubEntityTraits &>(
            static_cast<const AcGiSubEntityTraits &>(*this));
    }
    AcGiGeometry *rawGeometry() const override
    {
        return const_cast<AcGiWorldDraw *>(this);
    }
    Adesk::Boolean isDragging() const override { return draw_.dragging(); }
    double deviation(AcGiDeviationType type,
                     const AcGePoint3d &point) const override;
    Adesk::UInt32 numberOfIsolines() const override
    {
        return draw_.numberOfIsolines();
    }
    AcGiContext *context() override;
    bool secondaryCall() const override { return draw_.secondaryCall(); }

    // AcGiContext support state.
    [[nodiscard]] Adesk::UInt32 nestingDepth() const { return drawDepth_; }
    [[nodiscard]] bool hasActiveClipBoundary() const
    {
        return !clipStack_.empty();
    }

    // Persistent AcGi id minting (AcGiViewportDraw::isValidId contract).
    Adesk::ULongPtr mintAcgiId() { return draw_.mintAcgiId(); }
    [[nodiscard]] Adesk::ULongPtr mintedAcgiIdCount() const
    {
        return draw_.mintedAcgiIdCount();
    }

    // AcDbRasterImage consumption protocol: the host installs a resolver
    // that maps an image-entity object id to its raster pixels.  When set,
    // the entity-reference rasterImageDc resolves and emits real pixels;
    // without it the call stays a no-op slot.
    using RasterImageResolver = std::function<const AcGiImageBGRA32 *(
        const acdb::AcDbObjectId &)>;
    void setRasterImageResolver(RasterImageResolver resolver)
    {
        rasterImageResolver_ = std::move(resolver);
    }

    // ---- AcGiGeometry: concrete emitters (implemented against the sink) ----

    Adesk::Boolean worldLine(const AcGePoint3d &from,
                             const AcGePoint3d &to) override;
    Adesk::Boolean worldPolyline(const AcGePoint3d *points, int count,
                                 bool closed) override;
    Adesk::Boolean polygon(Adesk::UInt32 nbPoints,
                           const AcGePoint3d *vertexList) override;
    Adesk::Boolean polyPolygon(
        const Adesk::UInt32 numPolygonIndices,
        const Adesk::UInt32 *numPolygonPositions,
        const AcGePoint3d *polygonPositions,
        const Adesk::UInt32 *numPolygonPoints,
        const AcGePoint3d *polygonPoints,
        const AcCmEntityColor *outlineColors = nullptr,
        const AcGiLineType *outlineTypes = nullptr,
        const AcCmEntityColor *fillColors = nullptr,
        const AcCmTransparency *fillOpacities = nullptr) override;
    Adesk::Boolean image(
        const AcGiImageBGRA32 &imageSource, const AcGePoint3d &position,
        const AcGeVector3d &u, const AcGeVector3d &v,
        TransparencyMode transparencyMode = kTransparency8Bit) override;
    Adesk::Boolean edge(const std::vector<AcGiEdge2d> &edges) override;
    Adesk::Boolean edge(const AcGeCurve2d *const *edges,
                        Adesk::UInt32 nbEdges) override;
    Adesk::Boolean worldTriangle(const AcGePoint3d &a, const AcGePoint3d &b,
                                 const AcGePoint3d &c,
                                 bool fillIs3DFace = false) override;
    Adesk::Boolean worldCircle(const AcGeCircArc3d &arc) override;
    Adesk::Boolean worldPoint(
        const AcGePoint3d &position,
        acge::PointDisplayStyle displayStyle =
            acge::PointDisplayStyle::Circle) override;
    Adesk::Boolean worldInfiniteLine(const AcGePoint3d &start,
                                     const AcGeVector3d &direction) override;
    Adesk::Boolean pushClipBoundary(AcGiClipBoundary *boundary) override
    {
        return pushClipBoundaryImpl(boundary);
    }
    void popClipBoundary() override { popClipBoundaryImpl(); }
    Adesk::Boolean text(const AcGePoint3d &position, const AcGeVector3d &normal,
                        const AcGeVector3d &direction, const char *message,
                        const AcGiTextStyle &style) override;
    Adesk::Boolean worldEllipse(const AcGeEllip3d &ellipse) override;

    // Wide-character overload; forwards to the UTF-8 form.
    Adesk::Boolean text(const AcGePoint3d &position, const AcGeVector3d &normal,
                        const AcGeVector3d &direction, const ACHAR *message,
                        const AcGiTextStyle &style);

    Adesk::Boolean circle(const AcGePoint3d &center, const double radius,
                          const AcGeVector3d &normal) override;
    Adesk::Boolean circle(const AcGePoint3d &first, const AcGePoint3d &second,
                          const AcGePoint3d &third) override;
    Adesk::Boolean circularArc(const AcGePoint3d &center, const double radius,
                               const AcGeVector3d &normal,
                               const AcGeVector3d &startVector,
                               const double sweepAngle,
                               const AcGiArcType arcType =
                                   kAcGiArcSimple) override;
    Adesk::Boolean circularArc(const AcGePoint3d &start,
                               const AcGePoint3d &point,
                               const AcGePoint3d &end,
                               const AcGiArcType arcType =
                                   kAcGiArcSimple) override;
    Adesk::Boolean ellipticalArc(const AcGePoint3d &center,
                                 const AcGeVector3d &normal,
                                 double majorAxisLength,
                                 double minorAxisLength,
                                 double startAngleInRad,
                                 double endAngleInRad, double tiltAngleInRad,
                                 AcGiArcType arcType =
                                     kAcGiArcSimple) override;
    Adesk::Boolean mesh(const Adesk::UInt32 rows, const Adesk::UInt32 columns,
                        const AcGePoint3d *vertexList,
                        const AcGiEdgeData *edgeData = nullptr,
                        const AcGiFaceData *faceData = nullptr,
                        const AcGiVertexData *vertexData = nullptr,
                        const bool autoGenerateNormals = true) override;
    Adesk::Boolean shell(const Adesk::UInt32 nbVertex,
                         const AcGePoint3d *vertexList,
                         const Adesk::UInt32 faceListSize,
                         const Adesk::Int32 *faceList,
                         const AcGiEdgeData *edgeData = nullptr,
                         const AcGiFaceData *faceData = nullptr,
                         const AcGiVertexData *vertexData = nullptr,
                         const struct resbuf *resbufData = nullptr,
                         bool autoGenerateNormals = true) override;

    Adesk::Boolean pushModelTransform(const AcGeMatrix3d &matrix) override
    {
        transformStack_.push_back(currentTransform() * matrix);
        return true;
    }

    // Behavior-aware AcGi transforms resolve viewport/screen input into the
    // world soup at push time.  The returned matrix is the resolved model
    // transform; subsequent local geometry is still composed normally.
    AcGeMatrix3d pushPositionTransform(
        AcGiPositionTransformBehavior behavior,
        const AcGePoint3d &offset) override;
    AcGeMatrix3d pushScaleTransform(
        AcGiScaleTransformBehavior behavior,
        const AcGePoint3d &extents) override;
    AcGeMatrix3d pushOrientationTransform(
        AcGiOrientationTransformBehavior behavior) override;

    Adesk::Boolean curve(const AcGeCurve3d *curve) override;

    Adesk::Boolean pushModelTransform(const AcGeVector3d &translation) override
    {
        return pushModelTransform(AcGeMatrix3d::setToTranslation(translation));
    }

    // kTrue when a frame was popped; kFalse on an empty stack.
    Adesk::Boolean popModelTransform() override
    {
        if (transformStack_.empty())
            return false;
        transformStack_.pop_back();
        return true;
    }

    void getModelToWorldTransform(AcGeMatrix3d &matrix) const override
    {
        matrix = currentTransform();
    }

    void getWorldToModelTransform(AcGeMatrix3d &matrix) const override
    {
        matrix = currentTransform().inverted();
    }

    // Nested-drawable replay; defined in AcGiDrawable.h.
    Adesk::Boolean draw(AcGiDrawable *nested) override;

private:
    struct ClipFrame
    {
        std::vector<AcGePoint2d> boundary;
        std::vector<std::array<size_t, 3>> triangles;
        AcGeMatrix3d toClipSpace;
        bool clippingFront = false;
        bool clippingBack = false;
        double frontClipZ = 0.0;
        double backClipZ = 0.0;
        double winding = 1.0;
        bool convex = true;

        bool coordinates(const AcGePoint3d &world,
                         AcGePoint3d &clip) const
        {
            clip = transformBy(world, toClipSpace);
            return std::isfinite(clip.x) && std::isfinite(clip.y) &&
                   std::isfinite(clip.z);
        }
    };

    bool pushClipBoundaryImpl(const AcGiClipBoundary *boundary);
    void popClipBoundaryImpl();
    bool clipPoint(AcGePoint3d &point) const;
    std::vector<std::pair<AcGePoint3d, AcGePoint3d>>
    clipSegment(const AcGePoint3d &start, const AcGePoint3d &end) const;
    std::vector<std::vector<AcGePoint3d>> clipPolygon(
        std::vector<AcGePoint3d> polygon) const;

    AcGeMatrix3d currentTransform() const
    {
        return transformStack_.empty() ? AcGeMatrix3d()
                                       : transformStack_.back();
    }

    AcGePoint3d apply(const AcGePoint3d &point) const
    {
        return transformBy(point, currentTransform());
    }

protected:
    // Maps an absolute world point back through the active model transform so
    // converted eye/DC points can re-enter worldPolyline/worldPolygon without
    // being transformed a second time.  Protected because the AcGiViewportDraw
    // eye/DC emitters need the same conversion.
    AcGePoint3d modelPointFromWorld(const AcGePoint3d &world) const
    {
        return transformBy(world, currentTransform().inverted());
    }
    [[nodiscard]] bool behaviorNeedsViewport(
        AcGiPositionTransformBehavior behavior) const;
    [[nodiscard]] bool behaviorNeedsViewport(
        AcGiScaleTransformBehavior behavior) const;
    [[nodiscard]] AcGeVector3d viewportWorldOffset(
        const AcGePoint3d &offset) const;
    [[nodiscard]] AcGeVector3d viewportWorldScaleAxis(
        double value, size_t axis) const;
    [[nodiscard]] AcGeMatrix3d screenOrientationMatrix() const;

    acgs::GeometrySink &sink() { return draw_.sink(); }

    void applyTraits(acdb::Stroke &stroke) const;
    void applyTraits(acdb::Triangle &triangle, bool fillIs3DFace) const;

    acgs::ViewportDraw &draw_;
    std::unique_ptr<AcGiContext> commonContext_;
    Adesk::UInt32 drawDepth_ = 0;
    RasterImageResolver rasterImageResolver_;
    std::vector<AcGeMatrix3d> transformStack_;
    std::vector<ClipFrame> clipStack_;
};

// Value-layer AcGiViewport (ObjectARX acgi.h): camera / viewport-space query
// object driven by the acgs::ViewportFrame configured by AcGsView.  Matrices
// use the same conventions as the frame: worldToEye maps world points to eye
// space and eyeToDc maps eye points (with perspective divide) to device
// coordinates whose origin is (left, top).
class ACGI_PORT AcGiViewport
{
public:
    AcGiViewport() = default;
    explicit AcGiViewport(const acgs::ViewportFrame &frame) : frame_(frame) {}

    explicit operator bool() const { return frame_.has_value(); }
    [[nodiscard]] bool hasFrame() const { return frame_.has_value(); }
    [[nodiscard]] const acgs::ViewportFrame &frame() const
    {
        return frame_.value();
    }

    // "Model" and "world" are the same coordinate system in this port.
    bool getModelToEyeTransform(AcGeMatrix3d &matrix) const
    {
        if (!frame_) return false;
        matrix = frame_->worldToEye;
        return true;
    }
    bool getEyeToModelTransform(AcGeMatrix3d &matrix) const
    {
        if (!frame_) return false;
        matrix = frame_->eyeToWorld;
        return true;
    }
    bool getWorldToEyeTransform(AcGeMatrix3d &matrix) const
    {
        return getModelToEyeTransform(matrix);
    }
    bool getEyeToWorldTransform(AcGeMatrix3d &matrix) const
    {
        return getEyeToModelTransform(matrix);
    }

    [[nodiscard]] bool isPerspective() const
    {
        return frame_ ? frame_->perspective : false;
    }

    // Applies the projection's perspective distortion to an eye-space point:
    // x / y become device-space coordinates at that depth, z stays in eye
    // depth.  This is the port's explicit convention for ARX's
    // doPerspective / doInversePerspective pair.
    bool doPerspective(AcGePoint3d &point) const
    {
        if (!frame_ || !frame_->perspective) return false;
        const AcGePoint3d device = transformBy(point, frame_->eyeToDc);
        point.x = device.x;
        point.y = device.y;
        return true;
    }
    bool doInversePerspective(AcGePoint3d &point) const
    {
        if (!frame_ || !frame_->perspective) return false;
        const double focalX = frame_->eyeToDc.m[0][0];
        const double focalY = frame_->eyeToDc.m[1][1];
        if (std::abs(focalX) < 1.0e-12 || std::abs(focalY) < 1.0e-12)
            return false;
        const double eyeZ = point.z;
        // eyeToDc is a projective matrix: its translation participates in
        // the perspective divide, so undo focal * x + cx over (-z), not
        // focal * (x - cx) over (-z).
        point.x = (point.x * (-eyeZ) -
                   0.5 * (frame_->left + frame_->right)) / focalX;
        point.y = (point.y * (-eyeZ) -
                   0.5 * (frame_->top + frame_->bottom)) / focalY;
        return true;
    }

    // Number of device pixels spanned by a unit world square at the given
    // world point.  The perspective query is always evaluated at the point,
    // which is the meaningful value for tessellation; includePerspective is
    // accepted for ARX signature parity.
    bool getNumPixelsInUnitSquare(const AcGePoint3d &point,
                                  AcGePoint2d &pixels,
                                  bool includePerspective = true) const
    {
        (void)includePerspective;
        if (!frame_) return false;
        const AcGePoint3d eye = transformBy(point, frame_->worldToEye);
        const AcGePoint3d center = transformBy(eye, frame_->eyeToDc);
        const AcGePoint3d stepX = transformBy(
            eye + AcGeVector3d(1.0, 0.0, 0.0), frame_->eyeToDc);
        const AcGePoint3d stepY = transformBy(
            eye + AcGeVector3d(0.0, 1.0, 0.0), frame_->eyeToDc);
        pixels = AcGePoint2d(
            std::hypot(stepX.x - center.x, stepX.y - center.y),
            std::hypot(stepY.x - center.x, stepY.y - center.y));
        return true;
    }

    bool getCameraLocation(AcGePoint3d &location) const
    {
        if (!frame_) return false;
        location = frame_->cameraLocation;
        return true;
    }
    bool getCameraTarget(AcGePoint3d &target) const
    {
        if (!frame_) return false;
        target = frame_->cameraTarget;
        return true;
    }
    bool getCameraUpVector(AcGeVector3d &up) const
    {
        if (!frame_) return false;
        up = frame_->cameraUp;
        return true;
    }

    [[nodiscard]] Adesk::Int32 viewportId() const
    {
        return frame_ ? frame_->viewportNumber : 0;
    }
    [[nodiscard]] Adesk::Int32 acadWindowId() const
    {
        return frame_ ? frame_->acadWindowId : 0;
    }

    bool getViewportDcCorners(AcGePoint2d &minimum,
                              AcGePoint2d &maximum) const
    {
        if (!frame_) return false;
        minimum = AcGePoint2d(frame_->left, frame_->bottom);
        maximum = AcGePoint2d(frame_->right, frame_->top);
        return true;
    }

    bool getFrontAndBackClipValues(bool &clipFront, bool &clipBack,
                                   double &front, double &back) const
    {
        if (!frame_) return false;
        clipFront = frame_->clipFront;
        clipBack = frame_->clipBack;
        front = frame_->frontClipDistance;
        back = frame_->backClipDistance;
        return true;
    }

private:
    std::optional<acgs::ViewportFrame> frame_;
};

// The per-viewport counterpart of AcGiWorldDraw (ObjectARX
// AcGiViewportDraw : AcGiCommonDraw with an AcGiViewportGeometry facet).
// Both contexts record into the same sink in this port; the viewport form
// exposes the screen-density channel, the AcGiViewportGeometry eye/DC and
// raster-image protocol, AcGiViewport queries, and the inherited
// AcGiCommonDraw draw state.
class ACGI_PORT AcGiViewportDraw : public AcGiWorldDraw,
                                   public AcGiViewportGeometry
{
public:
    explicit AcGiViewportDraw(acgs::ViewportDraw &draw) : AcGiWorldDraw(draw) {}

    [[nodiscard]] double pixelsPerUnit() const
    {
        return gsContext().pixelsPerUnit();
    }

    // Maximum world-space distance from a chord to its curve at the
    // current viewport density.  Keeps the AcGiCommonDraw point overload
    // visible alongside the radius overload.
    using AcGiWorldDraw::deviation;
    [[nodiscard]] double deviation(double curveRadius) const
    {
        return gsContext().deviation(curveRadius);
    }

    [[nodiscard]] AcGiViewport viewport() const
    {
        return gsContext().hasViewportFrame()
                   ? AcGiViewport(gsContext().viewportFrame())
                   : AcGiViewport();
    }

    // ---- AcGiViewportGeometry: eye/DC-space protocol ----
    // Eye/DC points are first mapped to world and then emitted through the
    // normal world sink (including current traits).

    Adesk::Boolean polylineEye(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) override;
    Adesk::Boolean polygonEye(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) override;
    Adesk::Boolean polylineDc(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) override;
    Adesk::Boolean polygonDc(
        const Adesk::UInt32 nbPoints, const AcGePoint3d *vertexList) override;

    // DC-space raster image with pixel data: the port converts the three DC
    // quad anchors to world space and reuses the world image() pipeline.
    Adesk::Boolean rasterImageDc(
        const AcGePoint3d &origin, const AcGeVector3d &u,
        const AcGeVector3d &v, const AcGiImageBGRA32 &imageSource,
        TransparencyMode transparencyMode = kTransparency8Bit) override;

    // ObjectARX entity-reference signature.  The value port has no
    // AcDbRasterImage protocol yet, so this is a documented no-op slot
    // (kTrue = nothing stored).
    Adesk::Boolean rasterImageDc(
        const AcGePoint3d &origin, const AcGeVector3d &u,
        const AcGeVector3d &v, const acdb::AcDbObjectId &imageEntityId,
        ImageSource source, Adesk::UInt32 imageWidth,
        Adesk::UInt32 imageHeight, Adesk::Int16 imageColorDepth,
        bool transparency) override;

    Adesk::Boolean ownerDrawDc(
        Adesk::Int32 viewportNumber, Adesk::Int32 left, Adesk::Int32 top,
        Adesk::Int32 right, Adesk::Int32 bottom,
        AcGiOwnerDraw *ownerDraw) override;
    Adesk::Boolean ownerDraw3d(
        AcGePoint3d &minimum, AcGePoint3d &maximum,
        AcGiOwnerDraw3d *ownerDraw) override;

    // ---- ObjectARX AcGiViewportDraw extras ----
    [[nodiscard]] Adesk::UInt32 sequenceNumber() const;
    [[nodiscard]] Adesk::Boolean isValidId(Adesk::ULongPtr acgiId) const;
    [[nodiscard]] acdb::AcDbObjectId viewportObjectId() const;
};
