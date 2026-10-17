# ObjectARX API compatibility types

The bundled `/docs/arxref.chm` is the AutoCAD 2026 ObjectARX Reference Guide.
It is used as a source-level API reference; it does not imply binary ABI
compatibility. The AcGe implementation is being expanded incrementally.

The source-level compatibility aliases in `lib/arx/ArxTypes.h` provide a
shared starting point for ObjectARX-style declarations:

| ObjectARX spelling | Project type | Notes |
|---|---|---|
| `ACHAR` | `wchar_t` | Unicode-facing character type |
| `AcString` | `std::basic_string<ACHAR>` | Wide string for ObjectARX-facing text |
| `AcArray<T>` | `std::vector<T>` | Dynamic-array storage; this does not reproduce every `AcArray` member |
| `Adesk::Boolean`, integer aliases | Corresponding fixed-width C++ types | `LongPtr`/`ULongPtr` follow pointer width |
| `AcGePoint2d/3d`, `AcGeVector2d/3d`, `AcGeMatrix3d` | Project's geometry types | Defined in `lib/ge`; already use ObjectARX names |

`AcGeMatrix2d` currently provides the core affine transform operations
documented in the bundled reference. This is a partial compatibility layer,
not a claim that all ObjectARX AcGe classes, methods, semantics, or ABI are
implemented.

The geometry layer also includes source-level subsets of `AcGeLine2d/3d`,
`AcGeRay2d/3d`, and `AcGeScale2d/3d`. These provide point/direction setup,
parameterized evaluation (rays clamp to their half-line), and scale-vector
composition/inversion/matrix conversion. They do not yet reproduce the full
`AcGeLinearEnt2d/3d` inheritance API or every documented overload.

`AcGeBoundBlock2d/3d` provide coordinate-aligned box operations and documented
oriented parallelogram/parallelepiped constructors, containment, frame queries,
and separating-axis disjointness tests. Extending or swelling an oriented block
converts it to its axis-aligned extents.

The point/vector value types expose tolerance-based equality, vector angle and
direction predicates, and common axis constants. `AcGeTol` follows the
documented accessor-method spelling; only the basic tolerance contract is
implemented.

`AcGePosition2d/3d` are point-derived value types with ObjectARX-style point
and coordinate constructors/setters; the broad `AcGePointEnt` hierarchy is not
implemented.

`AcGePlane` includes signed distance and point projection helpers, and
`AcGeSphere` supports spherical surface parameterization and angular patches.
These are geometric subsets; they do not provide the full AcGe surface
intersection and hierarchy APIs.

`AcGeBoundedPlane` adds a finite rectangular planar domain with an orthonormal
parameter frame, point evaluation, projection, containment, closest-point, and
area queries. Its ranges must be finite and bounded.

`AcGeConvexHull2d` builds a counter-clockwise hull from finite 2D points,
removes near-duplicates using `AcGeTol`, and uses the exact orientation
predicate for turn decisions. Collinear inputs reduce to their two endpoints.

`AcGePlane` also supports construction from three points, spanning vectors,
or equation coefficients, plus tolerance-aware line-plane and plane-plane
intersection.

`AcGeCylinder` supports an axis/reference frame, angular sweep, height
interval, and parametric point/normal evaluation; it does not implement
surface intersection or the full `AcGeSurface` interface.

`AcGeTorus` supports major/minor radii, a normalized axis/reference frame,
angular segment domains, parametric evaluation, and basic topology predicates.
It does not provide the complete torus classification or intersection API.

`AcGeCone` provides base/apex geometry, an angular domain, an axial height
interval, and parametric point/normal evaluation. Cone/surface intersections
and advanced domain semantics remain outside this subset.

`AcGeInterval` provides finite and one-sided/unbounded parameter domains,
tolerance-aware containment, intersection, merge, extent queries, subtraction
into up to two remainder intervals, and upper-bound overlap queries.
Periodic parameter normalization remains unimplemented.

The existing 2D segment implementation is also exported as global
`AcGeLineSeg2d` and now includes the documented endpoint, midpoint, blend,
full/subrange length, closest-point, point distance/membership, point-pair and
point/vector set, and bisector operations. Its robust intersection helpers
remain available in namespace `ge`.

`AcGeLineSeg3d` now exposes endpoint-pair and start-point/vector construction
and setters, midpoint, blend, full/subrange length, closest-point, point
distance/membership, and bisector-plane operations.

The existing planar circular arc is exported as global `AcGeCircArc2d` and
supports reference vectors, clockwise/CCW parameterization, endpoints, and
point-on-arc tests. Curve-curve intersection remains the existing focused
`ge::intersect` subset.

`AcGeEllipArc2d` provides orthogonal axis/radius storage, angular evaluation,
endpoints, and point-on-ellipse-arc testing. Ellipse intersection and broader
planar-entity APIs are not yet implemented.

`AcGeOffsetPolyline2d` and `AcGeOffsetPolyline3d` provide miter-joined
parallel offsets for directed polylines. The 3D variant requires coplanar
vertices and an explicit plane normal; neither variant trims self-intersections
or substitutes for the full parametric `AcGeOffsetCurve` API.

`AcGeCircArc3d` and the newly exposed `AcGeEllipArc3d` name provide endpoint,
plane, and point-on-curve helpers alongside parameter evaluation. The legacy
`AcGeEllip3d` spelling remains an alias for the ellipse arc value type.

`AcGeTriangle2d/3d` provide area, centroid, barycentric coordinates,
tolerance-aware containment, and closest-point queries. `AcGeMesh3d` provides
indexed triangle storage, validated indices, bounds, and area-weighted vertex
normals; mesh topology editing and boolean operations are not included.

`AcGeCompositeCurve2d/3d` provide owning, heterogeneous curve segments with
explicit finite local parameter intervals. Each segment occupies one unit of
the global parameter domain; interior integer parameters select the next
segment, while the final parameter selects the last endpoint. They expose
local/global parameter conversion, endpoints, positional continuity, and
closedness checks. Disconnected segments are allowed, but are not reported as
continuous. These value adapters do not implement `AcGeCurve` inheritance,
native pointer-array ownership, derivatives, arc length, or nearest-point queries.

`AcGeOffsetCurve2d/3d` evaluate signed, bounded parametric offsets from an owned
curve and its analytic tangent, or explicit point/tangent callbacks. Positive
offset is left of the increasing-parameter tangent in 2D, and in the direction
`planeNormal × tangent` in 3D. For planar 3D offsets the caller must supply a
normal perpendicular to the source plane; global planarity is not inferred.
Zero-distance offsets evaluate only the source point, allowing stationary
points. Nonzero offsets reject zero/ill-conditioned tangents. These wrappers do
not trim self-intersections, remove cusps, or implement the complete native
offset-curve API. Callback copies do not extend the lifetime of referenced data.

`AcGeClipBoundary2d` provides rectangular or strictly convex polygonal clipping
boundaries, point containment, segment clipping, convex polygon clipping, and
polyline clipping into separate visible chains. Either winding and an optional
repeated closing vertex are accepted. Nonfinite coordinates, degenerate,
concave, collinear-edged, and self-intersecting boundaries are rejected.
Clipping expands each boundary half-plane by `AcGeTol::equalPoint()` in world
units; intersection coordinates remain subject to floating-point roundoff.
This subset uses standard containers and exceptions rather than ObjectARX
array/error-status signatures, and does not support concave subject polygons.

Clipping remains value-based: `AcGeClipBoundary2d` clips points, segments,
polygons, and sampled polylines directly. The port intentionally does not add
`AcGeEntity2d/3d` base classes merely to reproduce entity-oriented clipping
overloads.

`AcGeSurfSurfInt` provides a type-erased, sampled subset for transverse
intersections over finite surface parameter domains. `intCurve()` returns a
polyline approximation and `intParamCurve()` returns corresponding sampled
parameters on each input surface. Coincident regions, tangent components,
classification/configuration queries, and analytic intersection curves are
not implemented.

`AcGeExternalCurve2d`, `AcGeExternalCurve3d`, and `AcGeExternalSurface` are
callback-backed value adapters with explicit parameter ranges. They do not
own opaque kernel objects or provide ObjectARX entity inheritance; the
ownership flag only records the requested compatibility state. A missing
evaluator is reported by `isDefined()` and evaluation throws.

`AcGeRuledSurface3d`, `AcGeExtrudedSurface3d`, and `AcGeRevolvedSurface3d` are
**project extensions**, not native ObjectARX class/API compatibility claims.
They provide finite parameter domains, owning `AcGeSweptCurve3d` source adapters,
point evaluation, analytic partial derivatives, and normals oriented as
`partialU × partialV`. Ruled surfaces interpolate between two source curves;
extrusions use displacement per parameter unit; revolutions use radians and
right-handed rotation about an arbitrary axis. Missing analytic derivatives
and singular normals throw rather than silently approximating geometry.
They do not implement trimming, tessellation, solid topology, intersections,
or the native surface hierarchy.

The new headers are exported by `lib/acge/AcGe.h`. Standalone regression
executables follow the existing assert-based test convention and can be enabled
with `-DACGE_BUILD_TESTS=ON` in the root CMake build. Build targets
`AcGeClipBoundaryTests`, `AcGeCompositeCurveTests`, `AcGeOffsetCurveTests`, and
`AcGeSweptSurfaceTests`, then run `ctest --test-dir <build-dir> -R "^AcGe"`.
Assertions remain enabled for these tests in Release builds as well.

`AcGeKnotVector` provides the documented ordered non-decreasing knot
sequence: uniform `set(size, start, end)` and array construction, ordered
`insert`/`append` with tolerance-based multiplicity grouping, distinct-knot
extraction, `multiplicityAt`, interval lookup for a spline order, inclusive
subvector removal, range reversal/remapping, and the documented
`split`/`append(tail, knotRatio)` operations. `physicalLength` maps to
vector capacity and `growLength` is tracked for API compatibility.

`AcGeCubicSplineCurve2d/3d` are interpolation cubic splines through every
fit point. The fit-point constructor builds a closed periodic C2 spline with
chord-length parameterization (including the wrap segment); the
start/end-derivative constructor builds a clamped C2 spline; and the
knot/derivative constructor stores explicit per-interval cubic Hermite data.
Evaluation clamps to `[startParam, endParam]`, periodic curves wrap the
parameter around the span, and `setFitPointAt` re-solves the tangent system
for periodic/clamped splines while `setFirstDerivAt` pins the curve to
explicit derivative data. These value types do not implement the
`AcGeCurve2d/3d` inheritance hierarchy, generic-curve fitting, or the
filer/IO protocols.

The documented `AcGeNurbCurve2d/3d` are non-uniform rational B-spline curves backed by a
shared NURBS evaluation core. Non-periodic curves use the documented
`knotCount == ctrlCount + degree + 1` clamped layout; periodic curves accept
both ARX layouts (wrapped format 1 with `knotCount == ctrlCount + 1` and
format 2 with a duplicated endpoint control point, which is normalized to
format 1) and are unrolled to an unclamped uniform curve with wrapped
control points. Evaluation, derivatives up to the degree, and curvature use
the standard Cox-de Boor basis recursions with the A4.2 rational quotient
rule, so rational curves (circle arcs, helices) evaluate exactly.
`getDefinitionData`/`setControlPointAt`/`setWeightAt` round-trip and edit
the definition. Structural editing (ObjectARX `insertKnot`, `addKnot`,
`deleteControlPointAt`, `elevateDegree`, `hardTrimByParams`,
`makeRational`, `joinWith`) is implemented for non-periodic curves with
exactness semantics: `insertKnot`/`addKnot` are shape-preserving Boehm
insertions (A5.1), `deleteControlPointAt` removes a control point only
when the backing knot occurrence can be removed without changing the
shape (A5.2 consistency check, otherwise `false`),
`elevateDegree` decomposes to Bezier spans and re-merges junctions at
(original multiplicity + increment), `hardTrimByParams` clamps both cut
parameters to full multiplicity and slices the control polygon, and
`joinWith` performs a C0 join of equal-degree open curves with coincident
endpoints (junction knot multiplicity = degree). `length()` and
`length(fromParam, toParam, tol)` integrate derivative magnitude with
24-point Gauss-Legendre quadrature split at knot spans; parameter intervals
are clipped to the curve domain, reversed bounds produce the same positive
length, and results no greater than `abs(tol)` are reported as zero. Not
implemented: the
`AcGeCurve2d/3d` inheritance hierarchy, curve fitting, extend, and
surface classes.
`AcGePointOnCurve2d/3d` and `AcGePointOnSurface` are parameterized point
wrappers in the ObjectARX sense: they bind any curve/surface value type and a
cached parameter, then expose `point`, `deriv`/`uDeriv`/`vDeriv` (orders 1-2),
`mixedPartial`, `normal`, `tangentVector`, `inverseTangentVector`,
`curvature` (including the `curvature(param, res)` Boolean overload), and
`isSingular`. Because this port keeps curve/surface entities as independent
value types (no `AcGeCurve2d/3d` or `AcGeSurface` hierarchy), the wrappers
type-erase any curve providing `pointAt(double)` or `pointAtAngle(double)` and
any surface providing `pointAt(double, double)` — the same protocol as the
swept-surface source curves. Analytic `firstDeriv(double)`/`derivativeAt(double)`
is used when present; otherwise derivatives come from central finite
differences.
`AcGeAugPolyline3d` stores per-vertex points, first-derivative vectors, and
second-derivative vectors over a `AcGeKnotVector` parameterization. It supports
direct construction from matching knot/point/vector bundles (missing second
derivatives default to zero) and adaptive curve approximation from any
type-erased curve so each chord stays within `approxTol` of the source curve,
matching the documented ARX contract.
Batch 5 adds the curve-curve intersection set, boundary, and context
helpers. `AcGeCurveCurveInt.h` implements `AcGeCurveCurveInt2d/3d` over
type-erased curve value types: the engine samples both curves, seeds from
coarse grid scans, and refines each seed with a 2x2/3x3 Gauss-Newton
solve of `C1(t1) - C2(t2)` (NURBS Book section 7.4 idea), falling back
to projection steps when the system is singular. Documented queries
`numIntPoints`, `intPoint`, `getPointOnCurve1/2`, and parameter
accessors are supported; the shared box overlap helper checks 2D points
without the 3D z sentinel (fixing a pre-existing bug that disabled the
2D engine). `AcGeCurveBoundary.h` implements the documented boundary
element loop: 3D space curves or degenerate positions paired one-to-one
with parameter-space (2D) curves or positions, per-element orientation
flags, degenerate-element queries, and the owner-of-curves flag (value
semantics; the callables are always owned by the boundary).
`AcGeContext.h` provides the documented `gTol` (the default `AcGeTol`
with `setEqualPoint`/`setEqualVector` storing values unclamped), the
`gErrorFunc` error-handler hook, and the `gOrthoVector` orthogonal-
direction helper used by tolerance-sensitive algorithms.

`AcGeIntArray`, `AcGeDoubleArray`, `AcGePoint2d/3dArray`,
`AcGeVector2d/3dArray`, and `AcGeVoidPointerArray` spellings are exported
as `std::vector` aliases from `AcGeArrays.h`, matching the `ArxTypes.h`
`AcArray` compatibility decision.
`AcDbText::text` and `AcDbMText::text` use `AcString`. Convert only at
UTF-8 boundaries: `arxcompat::fromUtf8` when importing or reading persisted text
and `arxcompat::toUtf8` when writing DWG or serializing JSON. Other CAD string
fields are being migrated incrementally; UTF-8-oriented renderer, codec, and
storage interfaces remain narrow strings.
Batch 6 adds the curve/surface intersection set and the documented NURBS
surface alias. `AcGeCurveSurfInt.h` implements `AcGeCurveSurfInt` over any
type-erased curve (`pointAt(double)`/`pointAtAngle(double)`, optional
analytic `firstDeriv`) and surface (`pointAt(double, double)`); parameter
domains are discovered through the documented protocol chain
(`minParameterU/maxParameterU/minParameterV/maxParameterV`, then
`uRange()/vRange()`, then `getAnglesInU/getAnglesInV` for sphere/torus,
then `getAngles`+`getHeight` for cylinder/cone, then `[0,1]^2`). The
engine seeds from curve samples with a coarse surface grid scan and
refines each seed with a 3x3 Gauss-Newton solve of `C(t) - S(u, v)`
(NURBS Book section 7.4); singular systems (tangential contact, surface
poles) fall back to alternating curve/surface projection. Documented
queries `numIntPoints`, `intPoint`, `getIntParams` (curve parameter plus
surface `(u, v)`), `getPointOnCurve`, `getPointOnSurface`, `tolerance`,
and `curve()`/`surface()`-equivalent parameter accessors are supported;
`getIntConfigs` (csiConfig) is out of scope. Fixing this also exposed and
repaired two pre-existing engine bugs: the shared Gauss-Newton right-hand
side had an inverted sign (curve-curve refinement was unaffected, but
`ge::intersectCurveSurface` silently returned nothing off-grid), and
`AcGeNurbsSurface3d::derivativesAt` differentiated the plain control
coordinates instead of the homogeneous ones and re-multiplied by the
weight during accumulation, returning zero first derivatives for any
non-rational patch. `AcGeNurbSurface.h` aliases the documented
`AcGeNurbSurface` spelling onto `ge::AcGeNurbsSurface3d`.

Batch 7 completes the bound-block API and its oriented-block semantics.
The existing `AcGeBoundBlock2d`/`AcGeBoundBlock3d` value types (in
`AcGeUtil.h`) gain the documented `setToBox` conversion, which replaces
an oriented block with the coordinate-aligned box of its corner points
(the reference HTML leaves the Boolean underdocumented, and `kFalse` is
treated as a no-op). Oriented blocks are now stored as a base corner
plus edge vectors and per-edge parameter ranges, so `extend(point)` and
`swell(distance)` preserve the oriented edges instead of collapsing the
block to its coordinate box: each edge range grows independently,
`get` returns the moved corner and scaled edge vectors, and the public
`min`/`max` extents keep tracking the block corners. Degenerate edge
bases fall back to the previous coordinate-box behavior. `contains`,
`isDisjoint` (separating-axis test with touching blocks not disjoint),
`isBox`, and `isEmpty` are unchanged in signature and covered by the
new `AcGeBoundBlockTests` regression tests.

Batch 8 adds the offset surface and the curve bounding-block queries.
`AcGeOffsetSurface.h` implements the documented offset value type over
type-erased base surfaces: `pointAt(u, v)` evaluates the base surface
plus `offsetDist` times the base normal (analytic `normalAt(u, v)` when
the base provides it, central finite differences otherwise), the offset
preserves the base normal, and `getConstructionSurface` returns the
erased base view. The `isPlane`/`isBoundedPlane`/`isSphere`/`isCylinder`/
`isCone`/`isTorus` flags are resolved from the base surface C++ type;
`getSurface` (fitted simple-surface conversion) has no value-type
equivalent and reports failure, and the ARX `makeCopy` ownership flag is
recorded for API parity. The curve bound-block queries arrive as free
functions in namespace `ge` (`boundBlock`, `orthoBoundBlock`, and the 2D
spellings `boundBlock2d`/`orthoBoundBlock2d`, with `AcGeInterval`
subrange overloads): per the reference HTML both spellings return the
coordinate-aligned box of the curve, discovered through the shared
domain protocol chain (`minParameter`/`maxParameter`, `startParam`/
`endParam`, the conic angle members/accessors, then `[0,1]`) and built
from uniform samples plus the domain endpoints.

Batch 9 enables the AcGi image-texture rendering path. The filled
triangle shader binaries under `lib/shaders/cadPrimitives/filled` had
been checked in before the UV sampling uniforms existed, so the renderer
reported no AcGi image-texture support and every textured triangle fell
back to per-pixel color. `compile_shaders_all.bat` regenerated all
shader targets (its DX11 leg moved from the `s_4_0` profile to
`s_5_0` because `ps_4_0` rejects combined texture-sampler types),
and the new `fs_filled`/`vs_filled` binaries carry `s_albedo` and
`v_uv`. With the binaries current, `BgfxRenderer::supportsAcGiImageTextures`
now returns true, so `AcGsManager::uploadImageBGRA` uploads decoded
`AcGiImageBGRA32` pixels, `AcGsView` binds them per batch, and
`AcGiWorldDraw::image` quads render through the sampler instead of
the fallback path.
The wide `AcGiWorldDraw::text` overload accepts `ACHAR` text and converts it
to UTF-8 before queuing it for the existing text renderer. DWG/DXF codecs,
SQLite storage, and the current renderer remain UTF-8/narrow-string boundaries;
they should only be converted at explicit API edges, not by changing their
serialized representation.

These aliases target source portability, not ObjectARX binary ABI compatibility.
They do not emulate all native class methods, ownership rules, error statuses,
or the complete AcDb/AcGi/AcGs object model. Prefer the aliases in new public
declarations, and add narrow/wide adapters where a third-party or file-format
interface requires a different encoding.

## AcGi geometry coverage

`AcGiWorldDraw` records world-space primitives into the existing stroke,
triangle, and point sink. Its `pushClipBoundary`/`popClipBoundary` stack clips
world lines, polylines, points, and triangles against two-corner rectangular or
simple planar polygon boundaries in either winding. Concave boundaries are
triangulated for polygon clipping, and segments may produce multiple visible
strokes. The clip coordinates are computed with
`m_xToClipSpace * m_xInverseBlockRefXForm`, and active front/back Z planes clip
points, segments, and filled polygons. When `m_bDrawBoundary` is set, the clip
outline is emitted if its clip-to-world transform is finite.
`AcGiSubEntityTraits::setFillType`/`fillType` supports ObjectARX's always/never
modes. Closed polygons, full circles, sector/chord arc closures, mesh faces,
and shell faces omit their fill in `kAcGiFillNever` mode while retaining
applicable boundary and edge strokes.

`polyPolygon` expands the documented grouped shape counts and insertion
positions, ear-clips simple planar concave outlines, and carries fill/outline
colors and alpha into the existing primitives. Mesh/shell callbacks apply
true-color and face-transparency alpha to generated triangles, apply vertex
colors/normals, and emit explicitly visible edge strokes with optional color
and linetype overrides. Per-edge/per-face color indices, layer IDs, selection
markers, linetype IDs, and face material IDs are retained as metadata on the
tessellated sink primitives. `AcGsModel::setMaterial` binds a face-material ID
to an `AcGiMaterial` and optional 1-based raster-image index; the fill
submission resolves triangle material IDs (with a direct triangle image taking
priority), batches by material/texture/address, and passes the material to the
filled-triangle shader. Model `merge` appends images and rebases incoming
triangle and material image indices.

`AcGiSubEntityTraits::setMaterial`/`materialId` completes the ObjectARX
subentity material API. The shared sink retains the id on `Stroke`,
`Triangle`, and `TessellatedPoint`, while fragment `append` preserves a more
specific per-primitive id and otherwise stamps the active trait state.
`AcDbMaterialRecord` is now a non-graphical NOD payload and the standard
`ACAD_MATERIAL` dictionary is seeded with the other ACAD subtrees.
`AcDbDatabase::setMaterial` / `findMaterial` / `forEachMaterial` maintain named,
database-resident material records; `AcDbEntity::materialId` and record/handle
bindings persist through the SQLite store. When replaying an entity whose
material id resolves to a dictionary record, replay uses that shared record;
otherwise it falls back to an entity-embedded `AcDbPolyFaceMesh::material`.
`AcGsMetafile` has the matching replay material map and stamps newly emitted
fills with that key. The glTF host bridge decodes
embedded `AcDbGltfTexture` bytes to BGRA raster images, appends them to replay
geometry, and binds `AcDbMaterial::diffuseTexture` by the 1-based replay image
index. The filled shader now consumes emissive RGB and alpha-mask cutoff in
addition to base color and texture sampling.
`AcGiEdgeData::setLineTypes(const AcDbObjectId *)` and `linetypeIds()` follow
the ObjectARX spelling; the pointer-only setter borrows the array until the
next synchronous `mesh`/`shell` callback, while the counted port setter copies
and validates its input.
`AcGiVertexData` mapping coordinates are validated, interpolated, and bound
as fill UVs. Face mappers are consumed per face: their projection generates
planar/box/cylindrical/spherical coordinates when no vertex mapping is
provided, and their transform is applied before UV submission. Inherited
projection uses the available vertex mapping channels. `AcGiMapper`
projection/transform/tiling/auto-transform declarations are available. Counted
`AcGiFaceData::setMappers` calls and the vector overload own copies; the
pointer-only overload remains ARX-compatible and borrows through the synchronous
mesh/shell callback. `kObject` and `kModel` auto-transform normalize generated
coordinates by the entity/model point bounds before applying the mapper
transform. Shell negative-count rings are
treated as holes in the preceding face and triangulated with simple planar
outer/hole contours; edge attributes follow face-list ring order, while face
attributes exclude holes. The shell `pResBuf` hatch-conversion payload is not
consumed.
`image` copies BGRA pixels and retains a UV-mapped quad plus a per-pixel
triangle fallback. The bgfx raw BGRA upload/cache path is implemented, but
image-texture submission remains capability-gated until the checked-in filled
shader binaries are regenerated with UV sampling; the current WebGPU backend
also uses the fallback. Calls are limited to 65,536 pixels; 1-bit alpha treats
only 255 as opaque, while 8-bit alpha is proportional.
`edge` accepts both the port's value-edge list and an ObjectARX-style
`AcGeCurve2d*` array; line-segment, circular-arc, elliptical-arc, and
type-erased curve values emit as strokes (curve tessellation is capped at 65,536
segments per edge). `AcGeCurve2d`/`AcGeCurve3d` are value-semantic facades over
the existing analytic curve values, and `AcGiGeometry::curve` tessellates line,
circle, circular-arc, and elliptical-arc inputs through the viewport deviation
policy. `polylineEye`, `polygonEye`, `polylineDc`, `polygonDc`,
`ownerDrawDc`, and `ownerDraw3d` are now protocol slots on `AcGiWorldDraw`;
eye/DC calls convert through `acgs::ViewportDraw::setViewportFrame` and reuse
the world sink. `AcGsView::configureViewportDraw` bridges the active frame's
true world-to-eye transform, projection-to-DC transform, viewport size, and
pixel density. Without a viewport frame they safely return `true`. Behavior
position/scale/orientation transforms resolve viewport and screen inputs into
world coordinates before cached soup submission; native GDI owner-draw uploads
remain no-ops. `AcDbPolyline` already has an entity-level AcGi callback path,
so `pline` is not a missing renderer algorithm.
