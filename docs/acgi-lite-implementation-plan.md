# AcGi-lite implementation plan

## Goal

Status: P1-P3 and the full demo submission migration are implemented in this
pass. ECS projection and multi-document execution remain follow-on phases
P4/P5.

Bring the reusable part of the ObjectARX rendering protocol into this project without
copying the `AcRx`/`AcDb` heap-object graph. CAD entities remain value types, while
the renderer gains an explicit, cacheable draw protocol similar in shape to
`AcGiWorldDraw`, `AcGiViewportDraw`, `AcGiSubEntityTraits`, and `AcGiGeometry`.

This plan implements phases P1 through P3 from
[cad-rendering-porting-notes.md](cad-rendering-porting-notes.md). ECS projection and
multi-document execution remain later phases P4/P5.

## Architecture

```text
entity value (Line, Arc, Polyline, ...)
  -> entities::worldDraw(entity, WorldDraw&)
       -> GeometrySink records Stroke / Triangle / TessellatedPoint
       -> SubEntityTraits is applied while the draw list is collected
  -> VectorPrimitives draw list
  -> DrawListCache keyed by revision, render origin, traits version,
      and chord-tolerance bucket
  -> SceneDrawList (mesh batches, infinite-grid command, dynamic overlay)
  -> existing renderer back ends
```

The first implementation intentionally keeps the existing
`entities::tessellate()` geometry functions. They remain the mathematical
tessellation layer. `worldDraw()` wraps each result in an entity-local fragment and
transfers it through `GeometrySink`, so traits are applied at draw-list collection
time instead of by hand in `main.cpp`. This is a protocol migration, not a rewrite
of the geometry kernel.

`ViewportDraw` extends `WorldDraw` with `pixelsPerUnit()` and `deviation(radius)`.
Analytic arc, circle, and ellipse draws translate the deviation into a
`TesselationOptions::angleStep`. Other curve types continue to use their current
tessellation policy until the geometry functions expose per-span curvature.

## Work Breakdown

### P1.1 Draw context

Add `lib/scene/DrawContext.h`:

* `DrawTraits`: CAD-visible subset of `EntityCommon` (handle, name, layer, color,
  line type, line weight, visibility).
* `GeometrySink`: owns or appends to `TessellatedEntity`; transfers stroke, fill,
  and point traits at collection time.
* `SubEntityTraits`: mutable current traits state, with `setFrom(EntityCommon)`.
* `WorldDraw`: shared-world draw context containing the sink, tessellation options,
  and current subentity traits.
* `ViewportDraw`: adds `pixelsPerUnit`, `chordTolerance`, and `deviation(radius)`.

Acceptance:

* No renderer code copies `entity.common` into generated primitives.
* Sink output remains binary-compatible with the existing draw and picking paths.

### P1.2 Entity world-draw protocol

Add `lib/entities/world_draw.h`:

* Generic `worldDraw(entity, WorldDraw&)` maps the existing tessellation overload to
  the draw context.
* `ViewportDraw` overloads for arc, circle, and ellipse derive a finite chord-angle
  step from `deviation(radius)`.
* Preserve the existing `fillIs3DFace` behavior for 3D faces and mesh surfaces.

Acceptance:

* CAD entity call sites express intent as `worldDraw`, not direct tessellation plus
  trait patching.

### P2.1 Draw-list cache

Add `DrawListCache` to `lib/scene/DrawContext.h`.

Cache key:

| Field | Purpose |
|---|---|
| `entityRevision` | Invalidate after document/entity mutation. |
| `renderOrigin` | Separate large-coordinate rebase domains. |
| `traitsVersion` | Invalidate layer/color/style table changes. |
| `chordToleranceBucket` | Reuse geometry within a bounded viewport tolerance. |

Replace the immutable static `VectorPrimitivesTessellation` singleton with a cached
build. The demo uses revision `1`, its stable CAD anchor as render origin, traits
version `1`, and tolerance bucket `0`. Dynamic documents will increment revision or
invalidate by handle through a document dirty notification.

Acceptance:

* Repeat frame access returns the same draw-list object.
* Cache invalidation does not depend on immutable demo assumptions.

### P3.1 Viewport tolerance channel

Implement viewport-to-world conversion:

```text
pixelsPerUnit       = viewportHeight / worldViewportHeight
worldPerPixel       = 1 / pixelsPerUnit
chordTolerance      = fraction * worldPerPixel
deviation(radius)   = clamp(chordTolerance, minimum curve-dependent,
                            fraction of radius)
curveAngleStep      = 2 * acos(1 - deviation / radius)
```

Keep angle step inside conservative bounds so pathological camera values or tiny
radius values cannot create zero, infinite, or unstable segment counts.

Acceptance:

* `ViewportDraw` exposes a real tolerance channel.
* Arc, circle, and ellipse geometry can respond to it without changing entity data.

### P3.2 Demo integration

Refactor `appendVectorPrimitive()` and `getVectorPrimitivesTessellation()` in
`main.cpp` to:

1. Construct a `ViewportDraw` context.
2. Set subentity traits from the entity value.
3. Call `entities::worldDraw()`.
4. Record the same stroke/fill/point ranges as before.
5. Retrieve the complete vector draw list through `DrawListCache`.

Rendering and CPU/GPU picking continue to consume `Stroke`, `Triangle`, and
`TessellatedPoint`. Range indices and names remain stable so autofocus, visibility
culling, audit diagnostics, and GPU picking do not change behavior.

Acceptance:

* Release build succeeds.
* CAD stroke, fill, and point ranges retain the same names and ordering.
* Existing render and picking behavior remains source-compatible.

### P3.3 Full demo submission migration

Add `lib/scene/SceneDrawList.h` as the shared command list for renderer-visible
demo content:

* `TessellatedEntity` carries strokes, fills, and points collected through
  `WorldDraw`.
* `MeshBatchCommand` preserves prototype plus transform instancing for demo,
  validation, stress, center-cube, and CAD debug meshes.  It never expands a
  million-instance stress field into CPU triangles.
* `GridCommand` preserves the infinite-grid shader as a protocol command instead
  of materializing finite CAD line segments.
* Dynamic overlays (reference line, tiny-object impostors, focus marker, frustum
  wireframe, and grid-quad diagnostics) are collected through
  `entities::worldDraw()` each frame; they do not enter the immutable CAD cache.

`main.cpp` now routes example rendering through `submitMeshBatch()` and
`submitSceneDrawList()`.  Renderer calls remain only in these submission
helpers, frame lifecycle management, GPU-pick queues, and the CAD vector
batcher.  Environment settings such as the mesh headlight are not example
geometry and remain renderer state.  The former `drawTargetPoint()`,
`drawCube()`, and `drawMesh()` demo wrappers are removed.

Acceptance:

* Release build succeeds.
* Camera pan and orthographic-convergence regressions pass.
* Stress meshes retain instanced submission rather than CPU triangle expansion.

## Non-goals for this pass

* No `AcRxObject`, registry, RTTI, reactor, overrule, or deep-clone graph.
* No ECS component migration.
* No per-document worlds yet.
* No recursive block references or dimensions.
* No marker-specific picking.

These remain valuable follow-on phases, but they must not block the protocol and
cache foundation.

## Verification

1. Configure/build: `cmake --build build2022 --config Release` passed and produced
   `build2022/bin/Release/WINDOW.exe`.
2. Source migration: `appendVectorPrimitive()` now constructs `ViewportDraw`, sets
   subentity traits once, and calls `entities::worldDraw()`; manual post-tessellation
   trait loops are removed.
3. Full demo migration: static CAD, demo meshes, validation/stress meshes, the
   center cube, grid, reference line, tiny impostors, focus marker, and frustum
   diagnostics use the AcGi-lite/SceneDrawList submission path.
4. `python verify_camera_target.py Release 1` passed.  Its stop condition now
   waits for the scheduled pan target change instead of mistaking a depth-slab
   relog for the pan.
5. `python verify_ortho_convergence.py Release` passed.
6. Remaining optional diagnostics for manual runs use `GRID_PICK_AUDIT=1` and
   `GRID_CAD_DEMO=0`.
