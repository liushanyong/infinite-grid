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
//   AcGiGeometry          base geometry toolkit (transforms + primitives)
//   └─ AcGiWorldGeometry  view-independent, model-coordinate methods
//      └─ AcGiWorldDraw   + AcGiSubEntityTraits facet, sink recording
//         └─ AcGiViewportDraw  + per-viewport screen-density channel
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
// Ray/XLine overlay semantics); viewport/screen transform behaviors and the
// behaviors-only APIs collapse to their world-space form; clip boundaries
// support convex polygons, clip-space transforms, depth limits, and optional
// outlines; mesh/shell consume supported color, linetype, visibility, and
// normal arrays; image is rasterized as a bounded per-pixel triangle
// approximation; polypoint emits real point primitives instead of ARX's
// degenerate two-point strokes.
// edge() accepts a value-semantic subset of planar curves rather than native
// AcGeCurve2d pointers; native GDI owner-draw uploads are not implemented.

#include <cmath>
#include <limits>
#include <string>
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

// The abstract curve base is not ported yet (acge ships concrete value
// types); the generic-curve callback binds to the future class through the
// forward declaration.
struct AcGeCurve3d;

// ---- enums (acgidefs.h / acgi.h) ----

enum AcGiArcType
{
    kAcGiArcSimple = 0, // just the arc (not fillable)
    kAcGiArcSector,     // area bounded by the arc and its center of curvature
    kAcGiArcChord       // area bounded by the arc and its end points
};

// Value-segment subset used by the port's edge() callback; ObjectARX exposes
// an AcGeCurve2d pointer hierarchy, which this value-semantic AcGe layer lacks.
struct AcGiEdge2d
{
    std::variant<AcGeLineSeg2d, AcGeCircArc2d, AcGeEllipArc2d> curve;
};

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

// ---- port value types for the toolkit signatures ----

struct AcGiPolyline
{
    std::vector<AcGePoint3d> vertices;
    bool closed = false;
};

struct AcGiEdgeData
{
    std::vector<AcCmEntityColor> trueColors;
    std::vector<std::string> lineTypes;
    std::vector<Adesk::UInt8> visibility;

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
    std::vector<AcCmEntityColor> trueColors;
    std::vector<Adesk::UInt8> visibility;
    std::vector<AcGeVector3d> normals;
    std::vector<AcCmTransparency> transparency;

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
    void setNormals(const AcGeVector3d *values, size_t count)
    {
        if (values == nullptr)
            normals.clear();
        else
            normals.assign(values, values + count);
    }
    void setTransparency(const AcCmTransparency *values, size_t count)
    {
        if (values == nullptr)
            transparency.clear();
        else
            transparency.assign(values, values + count);
    }
};
struct AcGiVertexData
{
    std::vector<AcCmEntityColor> trueColors;
    std::vector<AcGeVector3d> normals;

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

    // ARX face-list encoding: [n, i0 .. i(n-1), m, j0 ..], fans per face; a
    // negative count hides edges (the sign is consumed, edges are the
    // caller's business in this port).
    virtual Adesk::Boolean shell(const Adesk::UInt32 nbVertex,
                                 const AcGePoint3d *vertexList,
                                 const Adesk::UInt32 faceListSize,
                                 const Adesk::Int32 *faceList,
                                 const AcGiEdgeData *edgeData = nullptr,
                                 const AcGiFaceData *faceData = nullptr,
                                 const AcGiVertexData *vertexData = nullptr) = 0;

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

    // Closed automatically; fill type support pending in the sink, so the
    // boundary stroke is what stores today.
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

    // Behavior transforms.  Port subset: every behavior collapses to its
    // world-space form until per-viewport transform plumbing lands; each
    // returns the pushed matrix.
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
    // The port supports two-corner rectangles and convex polygons in the
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

    // Generic 3D curve: ARX default stores nothing (kFalse).
    virtual Adesk::Boolean curve(const AcGeCurve3d * /*curve*/)
    {
        return false;
    }
};

// ---- AcGiWorldGeometry: view-independent, model-coordinate methods ----

class ACGI_PORT AcGiWorldGeometry : public AcGiGeometry
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

    // Port extension: applies the entity-common attribute set in one call.
    virtual void setFrom(const acdb::AcDbEntity &common) = 0;
};

// ---- AcGiWorldDraw: shared-world draw context ----

class ACGI_PORT AcGiWorldDraw : public AcGiWorldGeometry,
                                public AcGiSubEntityTraits
{
public:
    explicit AcGiWorldDraw(acgs::ViewportDraw &draw) : draw_(draw) {}

    // The trait state the sink applies to subsequently emitted primitives.
    acgs::SubEntityTraits &subEntityTraits() { return draw_.subEntityTraits(); }

    // AcGiSubEntityTraits facet: forwards to the trait state above.
    void setColor(const glm::vec4 &color) override
    {
        subEntityTraits().setColor(color);
    }
    void setLayer(std::string layer) override
    {
        subEntityTraits().setLayer(std::move(layer));
    }
    void setLineType(std::string lineType) override
    {
        subEntityTraits().setLineType(std::move(lineType));
    }
    void setLineWeight(double lineWeight) override
    {
        subEntityTraits().setLineWeight(lineWeight);
    }
    void setVisibility(bool visible) override
    {
        subEntityTraits().setVisibility(visible);
    }
    void setFrom(const acdb::AcDbEntity &common) override
    {
        subEntityTraits().setFrom(common);
    }

    // ---- AcGiGeometry: concrete emitters (implemented against the sink) ----

    Adesk::Boolean worldLine(const AcGePoint3d &from,
                             const AcGePoint3d &to) override;
    Adesk::Boolean worldPolyline(const AcGePoint3d *points, int count,
                                 bool closed) override;
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
                         const AcGiVertexData *vertexData = nullptr) override;

    Adesk::Boolean pushModelTransform(const AcGeMatrix3d &matrix) override
    {
        transformStack_.push_back(currentTransform() * matrix);
        return true;
    }

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

    // The renderer sink context this facade records into; the entity
    // drawables call the acdb::worldDraw overload set through it.
    acgs::ViewportDraw &context() { return draw_; }
    [[nodiscard]] const acgs::ViewportDraw &context() const
    {
        return draw_;
    }

private:
    struct ClipFrame
    {
        std::vector<AcGePoint2d> boundary;
        AcGeMatrix3d toClipSpace;
        bool clippingFront = false;
        bool clippingBack = false;
        double frontClipZ = 0.0;
        double backClipZ = 0.0;
        double winding = 1.0;

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
    bool clipSegment(AcGePoint3d &start, AcGePoint3d &end) const;
    std::vector<AcGePoint3d> clipPolygon(
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

    acgs::GeometrySink &sink() { return draw_.sink(); }

    void applyTraits(acdb::Stroke &stroke) const;
    void applyTraits(acdb::Triangle &triangle, bool fillIs3DFace) const;

    acgs::ViewportDraw &draw_;
    std::vector<AcGeMatrix3d> transformStack_;
    std::vector<ClipFrame> clipStack_;
};

// The per-viewport counterpart of AcGiWorldDraw (ObjectARX AcGiViewportDraw).
// Both contexts record into the same sink in this port; the viewport form
// exposes the screen-density channel that curve tessellation adapts to.
class ACGI_PORT AcGiViewportDraw : public AcGiWorldDraw
{
public:
    explicit AcGiViewportDraw(acgs::ViewportDraw &draw) : AcGiWorldDraw(draw) {}

    [[nodiscard]] double pixelsPerUnit() const
    {
        return context().pixelsPerUnit();
    }

    // Maximum world-space distance from a chord to its curve at the
    // current viewport density.
    [[nodiscard]] double deviation(double curveRadius) const
    {
        return context().deviation(curveRadius);
    }
};
