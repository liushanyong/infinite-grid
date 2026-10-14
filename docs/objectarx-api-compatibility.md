# ObjectARX API compatibility types

The source-level compatibility aliases in `lib/arx/ArxTypes.h` provide a
shared starting point for ObjectARX-style declarations:

| ObjectARX spelling | Project type | Notes |
|---|---|---|
| `ACHAR` | `wchar_t` | Unicode-facing character type |
| `AcString` | `std::basic_string<ACHAR>` | Wide string for ObjectARX-facing text |
| `AcArray<T>` | `std::vector<T>` | Dynamic-array storage; this does not reproduce every `AcArray` member |
| `Adesk::Boolean`, integer aliases | Corresponding fixed-width C++ types | `LongPtr`/`ULongPtr` follow pointer width |
| `AcGePoint2d/3d`, `AcGeVector2d/3d`, `AcGeMatrix3d` | Project's geometry types | Defined in `lib/ge`; already use ObjectARX names |

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
