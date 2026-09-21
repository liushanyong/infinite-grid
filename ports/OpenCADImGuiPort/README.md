# OpenCADStudio ImGui Port

This is a separate sandbox project used to prototype the native ImGui UI shell for
OpenCADStudio. It is intentionally independent from the root `WINDOW` target and
does not modify any existing build file.

It currently provides:

* SDL3 windowing and bgfx rendering
* Dear ImGui with docking enabled
* A bgfx-backed ImGui renderer
* Dockspace, menu bar, toolbar, viewport, properties, command line, layers,
  outline and status bar
* A document model with layers and 2D scene objects
* Interactive line, rectangle and circle drawing
* Screen-space object picking
* Multi-object selection with Ctrl toggle, Shift+drag box selection, Select All, Invert Selection, batch delete and batch layer assignment
* Locked-layer protection and per-layer object counts
* Undo/redo for document mutations
* Layer creation, object layer assignment and lock protection
* Command-line history, repeat-run, parameterized geometry commands and `save`/`open` file paths
* Document save/load with a compact portable state format
* Document panel with explicit file path Save/Open controls
* Grid snapping now applies to viewport drawing clicks and cursor coordinates
* Keyboard shortcuts for common editing operations

## Build

From the repository root:

```bash
cmake -S ports/OpenCADImGuiPort -B ports/OpenCADImGuiPort/build
cmake --build ports/OpenCADImGuiPort/build --config Release
```

The executable will be created at:

```
ports/OpenCADImGuiPort/build/bin/OpenCADImGuiPort
```

## Test

```bash
cmake --build ports/OpenCADImGuiPort/build
ctest --test-dir ports/OpenCADImGuiPort/build --output-on-failure
```

## Goal

This project is the migration target for the `BGFX + SDL + ImGui` native UI
route. The existing iced-based UI remains untouched until the ImGui shell is
ready to replace it.

## Application boundary

The ImGui layer only communicates through `IAppController`. The current
implementation is `DocumentAppController`, which delegates document state to
`DocumentStore`. This is the integration seam for the production CAD
application:

```
ImGui panels/actions
        |
        v
IAppController (UiAction / AppSnapshot)
        |
        v
DocumentAppController
        |
        v
DocumentStore  ==>  future OpenCADStudio document/application adapter
```

The next real-application step is to implement a controller that forwards
`UiAction` calls to OpenCADStudio's document and command APIs, then maps their
state into `AppSnapshot`. No ImGui panel code should need to change for that
integration.

## Current structure

```
src/
  app/                 UI actions, document state, application adapter
    AppSnapshot.hpp
    UiAction.hpp
    IAppController.hpp
    DocumentStore.hpp/cpp
    DocumentAppController.hpp/cpp
  platform/            input routing between ImGui and the CAD viewport
    InputRouter.hpp/cpp
  ui/                  ImGui UI shell
    UiHost.hpp/cpp
    ViewportHost.hpp
    CadViewportHost.hpp/cpp
    GridViewportHost.hpp/cpp
    ViewportPanel.hpp/cpp
    ToolbarPanel.hpp/cpp
    PropertiesPanel.hpp/cpp
    CommandLinePanel.hpp/cpp
    LayersPanel.hpp/cpp
    OutlinePanel.hpp/cpp
    DocumentPanel.hpp/cpp
    StatusBar.hpp/cpp
  backend/             bgfx-backed Dear ImGui renderer
docs/
  ProductionAdapterPlan.md
tests/
  DocumentAppControllerTests.cpp
```

## Sprint progress

* Sprint 0: SDL3 + bgfx + ImGui shell — done.
* Sprint 1: UiHost, UiAction, InputRouter, app controller layer — done.
* Sprint 2: docked panels, command line, grid viewport, layers, status bar — done.
* Sprint 3: document objects, selection, viewport picking, locked-layer protection, command history — done.
* Sprint 4: object outline, global shortcuts, cursor world readout, interactive drawing tools — done.
* Sprint 5: `DocumentStore` + `DocumentAppController`, adapter tests, undo/redo — done.
* Sprint 6: object layer assignment and production adapter integration plan — done.
* Sprint 7: document persistence, file save/load, Document panel and serialization tests — done.
* Sprint 8: snapped viewport drawing clicks, cursor coordinates and layer creation — done.
* Sprint 9: multi-object selection, box selection, Select All / Invert, batch operations and selection serialization — done.
* Next: implement `OpenCADStudioAppController` over `IAppController`, then bind geometry and selection to the real document model.
