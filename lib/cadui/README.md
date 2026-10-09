# cadui

A small Dear ImGui-based CAD overlay widget library.

Current widgets:

- `cadui::UcsIconWidget` - projected UCS tripod with axis hit testing and grips.
- `cadui::ViewCubeWidget` - navigation cube with 26 selectable regions.
- `cadui::ViewCubeBgfxRenderer` - optional offscreen BGFX renderer for the ViewCube body.

The library is reusable from any SDL3/BGFX/ImGui application. Build the `examples/UcsViewCubeImGui` target for a complete example.

The main `WINDOW` application displays the ViewCube as a BGFX-backed ImGui camera overlay when its renderer is BGFX-based. The widget reports navigation actions and has no dependency on AcGi, AcGs, or AcDb.
