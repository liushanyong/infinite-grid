# UCS Icon + ViewCube (BGFX + ImGui)

This example reproduces the OpenCADStudio UCS tripod and ViewCube interaction using the repository's SDL3 + BGFX + Dear ImGui stack.

## Build

```cmd
cd examples/UcsViewCubeImGui
configure.cmd
build.cmd
run.cmd
```

The executable is written to `build/bin/UcsViewCubeImGui.exe`.

## Controls

- Drag in the empty viewport to orbit the camera.
- Hover the ViewCube: faces, edges and corners are CPU hit-tested and highlighted.
- Click a ViewCube region to snap the camera direction.
- Use Home/Left/Right for quick view changes.
- Hover/click the UCS tripod in the lower-left corner to select an axis.

## Structure

- `lib/cadui/UcsIcon.*` - ImGui DrawList UCS tripod.
- `lib/cadui/ViewCube.*` - 26-region navigation cube widget and CPU hit testing.
- `lib/cadui/ViewCubeBgfx.*` - optional BGFX offscreen cube renderer.
- `lib/shaders/viewcubeFace` - compiled BGFX shader headers used by the renderer.

The ViewCube widget deliberately keeps hit testing on the CPU. BGFX renders only the shaded cube body; ImGui DrawList draws labels, outlines, the compass ring and overlay UI. This avoids GPU readback while keeping the hover state responsive.
