# CAD Algorithm Shader Demo

This shader family ports the main CADplatformer mesh/visual-style algorithms into this project and exposes them through one instanced demo shader.

## Ported algorithm groups

- Mesh instancing
- Unified mesh shading
- Visual styles:
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

The demo is enabled by default. Optionally set the following environment variables before starting the application:

```powershell
$env:GRID_CAD_SHADER_DEMO = "1"   # optional; enabled by default
$env:GRID_CAD_STYLE = "0"   # 0..7
```

Style index:

0. Realistic
1. Conceptual
2. Depth only
3. Grayscale
4. Shaded
5. Sketch
6. Wireframe
7. X-ray

By default, the renderer replaces the normal mesh-instancing pass with the CAD algorithm demo shader and keeps the large-coordinate scene and camera behavior unchanged. Set `GRID_CAD_SHADER_DEMO=0` to restore the normal path.

Press `K` while the demo is enabled to cycle through the eight visual styles without restarting the application.

## Compile / build

```powershell
lib\compile_shaders_all.bat -Filter cadAlgorithm -Force
.build_ninja.cmd
```

## Run demo

```powershell
$env:GRID_CAD_STYLE = "0"
$env:GRID_CAMERA_LARGE = "1"
$env:GRID_STRESS_COUNT = "1"
# Set GRID_CAD_SHADER_DEMO=0 to disable the demo
build_ninja\bin\WINDOW.exe
```
