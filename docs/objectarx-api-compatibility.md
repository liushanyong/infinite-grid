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

`AcGeBoundBlock2d/3d` provide coordinate-aligned box operations for setting,
extending, swelling, querying extents, containment, and disjointness. They do
not yet represent the documented oriented parallelogram/parallelepiped forms.

The point/vector value types expose tolerance-based equality, vector angle and
direction predicates, and common axis constants. `AcGeTol` follows the
documented accessor-method spelling; only the basic tolerance contract is
implemented.

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
