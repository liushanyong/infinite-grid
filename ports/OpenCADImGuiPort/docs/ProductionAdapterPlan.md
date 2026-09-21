# OpenCADStudio Production Adapter Plan

## Purpose

`DocumentAppController` proves the UI behavior against a local document model.
The next migration step is to replace it with a production adapter without
changing ImGui panels.

The immutable UI boundary is:

```
ImGui -> UiAction -> IAppController
IAppController -> AppSnapshot -> ImGui
```

## Adapter requirements

1. Implement `IAppController` in the production adapter.
2. Forward `UiAction` to OpenCADStudio commands.
3. Map OpenCADStudio document state to `AppSnapshot` every frame or on change.
4. Do not expose ImGui, SDL, or bgfx types through the adapter.
5. Keep expensive operations out of the UI thread when OpenCADStudio commands
   are not synchronous.

## Action mapping

| UiAction | Production adapter behavior |
|---|---|
| `RunCommand` | Forward to the CAD command processor and add UI command history. |
| `CreateObject` | Create line/rectangle/circle through the document factory. |
| `UpdateObject` | Apply geometry edits through undoable document transactions. |
| `DeleteObject` | Delete through the production selection/delete command. |
| `AssignObjectLayer` | Move one entity or the whole selection set between layers using the production API. |
| `SelectLayer` / `ToggleLayer*` | Map to layer manager state. |
| `SelectObject` | Map to production selection set; preserve `SelectionModifier::Replace` / `Toggle`. |
| `SetSelection` | Apply a complete selection set, including viewport box-selection results. |
| `SetTool` / `ViewportClick` | Map to active tool / drawing command state machine. |
| `OpenFile` / `Save` | Use production file services and progress/status callbacks. |
| `Undo` / `Redo` | Use production undo/redo stack; do not duplicate history. |
| `ToggleGrid` / `ToggleSnap` / `ToggleOrtho` | Map to viewport/modeling aids state. |
| `FocusViewport` / `Exit` | Forward to viewport/application controller. |

## Snapshot mapping

`AppSnapshot` should be populated from the production document as follows:

- `objects`: lightweight render/selection descriptors, not full B-Rep data.
- `layers`: layer name, visibility, lock state and current flag.
- `selectedObjectId`: primary selected entity.
- `selectedObjectIds`: production selection set; primary selection remains last/current entity.
- `activeTool`: production command or tool state.
- `hasPendingDrawPoint`: active drawing command state.
- `canUndo` / `canRedo`: production undo stack status.
- `statusText`: command prompt or last command result.
- `gridEnabled`, `snapEnabled`, `orthoEnabled`: modeling aids state.

## Implementation steps

1. Inventory OpenCADStudio document, layer, selection, undo, command and file
   APIs.
2. Define `OpenCADStudioAppController` in the port's `src/app` directory.
3. Implement read-only snapshot mapping first.
4. Implement selection, layer visibility, and viewport commands.
5. Implement creation/edit/delete through production undo transactions.
6. Implement file open/save and error reporting through `statusText`.
7. Add adapter tests with fake production documents before live integration.
8. Swap `DocumentAppController` for `OpenCADStudioAppController` in `main.cpp`.
9. Run build and tests; validate against the original UI feature checklist.

## Definition of done

- No ImGui headers are included by the production adapter.
- All ImGui mutations go through `UiAction`.
- Document state shown in UI comes from production state, not duplicated state.
- Undo/redo is owned by the production application.
- Locked layers block production mutations.
- Open/save, selection, layers, drawing tools, properties and command line are
  connected.
- Existing port tests continue to pass.
