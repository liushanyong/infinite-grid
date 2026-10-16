# cadui

A small Dear ImGui-based CAD overlay widget library.

Current widgets:

- `cadui::drawGripOverlay` - clipped CAD grip markers, control polygons, and move gizmos; `projectGripMarkers` handles world-to-screen placement and gizmo screen axes.
- `cadui::UcsIconWidget` - projected UCS tripod with axis hit testing and grips.
- `cadui::ViewCubeWidget` - navigation cube with 26 selectable regions.
- `cadui::ViewCubeBgfxRenderer` - optional offscreen BGFX renderer for the ViewCube body.
- `cadui::OverlayWidgets` - shared lifecycle, hit routing, and frame rendering for the ViewCube and UCS icon overlays.

The library is reusable from any SDL3/BGFX/ImGui application. Build the `examples/UcsViewCubeImGui` target for a complete example.

The `ViewCubeWidget` and `UcsIconWidget` are reusable UI components with no AcGi, AcGs, or AcDb dependency. The main `WINDOW` application coordinates their overlays through `cadui::OverlayWidgets`, which applies camera actions through the active `AcGsView`.
