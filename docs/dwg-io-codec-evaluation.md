# DWG/DXF codec boundary and replacement evaluation

## Current design

`AcDbDatabase` exposes four file operations: DWG read/write and DXF read/write.
They now dispatch through `acdb::AcDbFileCodec`, while
`AcDbLibredwgFileCodec.cpp` is the current default implementation. That
adapter owns the LibreDWG calls and uses `AcDbDwgBridge.h` and
`AcDbDwgWriteBridge.h` to map between the codec's document model and
`AcDbDatabase`. The bridges remain format-library-specific; a new backend
would provide its own mapping behind the same database-facing interface.

This is a replacement seam, not a LibreDWG removal. LibreDWG is still built
and linked by the root CMake configuration, and all four operations still
use it by default. The existing round-trip self-test and `fileio_repro`
remain the compatibility checks when a backend is changed.

## Reference projects

- [OpenCADStudio](https://github.com/HakanSeven12/OpenCADStudio) delegates
  file operations to its separately maintained
  [`opencadcodec`](https://github.com/HakanSeven12/opencadcodec) Rust
  library. Its upstream documentation advertises DWG read/write through
  2018 and DXF read/write through 2018. This is the more relevant independent
  codec candidate, but it is not a drop-in C++/LibreDWG API replacement:
  integrating it here would require a Rust build/FFI boundary and a new
  mapping to the project's database types.
- [CADCraft](https://github.com/storytold/cadcraft) has its own DXF path, but
  its DWG crate uses `acadrust` to convert DWG through DXF. Its own
  [format-parity notes](https://github.com/storytold/cadcraft/blob/main/docs/file-format-parity.md)
  describe incomplete, lossy DWG round trips. It is useful as a reference
  for DXF architecture, but is not a stronger direct DWG backend candidate
  on the documented evidence.

## Recommendation

Keep LibreDWG as the default until a candidate passes a comparison against
the project's real drawing corpus. Evaluate `opencadcodec` first, separately
for read and write, and check target DWG/DXF versions, entity and metadata
retention, malformed-file behavior, round trips, build/deployment impact,
and license obligations. A trial backend should report unsupported or
lossy data explicitly and must not silently replace files until its output
has been reopened and compared. Do not adopt CADCraft's DWG bridge as a
replacement without evidence that its conversion losses meet this project's
requirements.
