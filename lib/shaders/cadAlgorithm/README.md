# CAD Algorithm Shader Demo

This shader family ports the main CADplatformer mesh/visual-style algorithms into this project and exposes them through one instanced demo shader.

## Ported algorithm groups

- Mesh instancing
- Unified mesh shading branches:
  - Realistic
  - Conceptual
  - Depth only
  - Grayscale
  - Shaded
  - Sketch
  - Wireframe
  - X-ray
- Common math/lighting helpers:
  - hash/noise/fbm
  - Lambert
  - Phong
  - Blinn-Phong
  - GGX/Cook-Torrance
  - Fresnel-Schlick
  - Smith geometry
  - conceptual lighting
  - wireframe lighting
  - X-ray effect
  - sketch hatch

## Demo switch

The demo is enabled by default. Optionally set the following environment variable before starting the application:

```powershell
$env:GRID_CAD_SHADER_DEMO = "1"   # optional; enabled by default
```

Press `K` to cycle the six consolidated render modes.  Flat/Gouraud variants were visually redundant in the current unified shader, so the public `RenderMode` now exposes Shaded, Shaded + Edges, and Depth Buffer.  Shader branches such as Realistic, Conceptual, Grayscale, Sketch, and X-ray remain available as material/effect algorithms, not as a second visual-style selector:

1. Wireframe 2D
2. Wireframe 3D
3. Hidden Line
4. Shaded
5. Shaded + Edges
6. Depth Buffer

The renderer now uses fixed bgfx views:

0. background / grid
1. hidden-line depth prepass
2. solid fill
3. mesh edges
4. CAD wires
5. overlay points/lines

The six scene views target one explicit MSAA framebuffer.  Only view 0 clears color and depth; later views inherit that depth buffer so Hidden Line can occlude edges with a depth-only prepass.  A final present view resolves the MSAA scene target to the backbuffer.

## Compile / build

```powershell
lib\compile_shaders_all.bat -Filter cadAlgorithm -Force
.build_ninja.cmd
```

## Run demo

```powershell
build_ninja\bin\WINDOW.exe
```

The large-coordinate stress scene and CAD vector-primitive demo load by default. `GRID_CAD_SHADER_DEMO=0` is still supported to disable the CAD shader demo.
