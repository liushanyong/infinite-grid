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
tolerance-aware containment, intersection, merge, and extent queries.
Advanced periodicity and interval subtraction APIs remain unimplemented.

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

`AcDbText::text` and `AcDbMText::text` use `AcString`. Convert only at
UTF-8 boundaries: `arxcompat::fromUtf8` when importing or reading persisted text
and `arxcompat::toUtf8` when writing DWG or serializing JSON. Other CAD string
fields are being migrated incrementally; UTF-8-oriented renderer, codec, and
storage interfaces remain narrow strings.

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
world lines, polylines, points, and triangles against strictly convex planar
boundaries in either winding. Clip-space matrices and front/back depth limits
are retained in the port structure but are not interpreted.

`polyPolygon` expands the documented grouped shape counts and insertion
positions, ear-clips simple planar concave outlines, and carries fill/outline
colors and alpha into the existing primitives. Mesh/shell callbacks consume
true-color and visibility arrays, apply vertex colors/normals to generated
triangles, and emit explicitly visible edge strokes; face transparency remains
ignored as the reference currently specifies. `image` copies BGRA pixels and
approximates them with two colored triangles per pixel, limited to 65,536
pixels per call. This is a CPU geometry fallback, not filtered GPU texture
upload. Generic `AcGeCurve2d` edge loops and native GDI owner-draw uploads are
not implemented; the latter remains a no-op, matching ObjectARX's default
implementation. `AcDbPolyline` already has an entity-level AcGi callback path,
so `pline` is not a missing renderer algorithm.
