# Infinite Grid Plane

A simple CAD viewport built using **SDL3** with pluggable **bgfx** and native **WebGPU** renderer backends, inspired by the viewport grids in **Unity**, **Unreal Engine**, and **Blender**.

## Features
- Infinite procedural grid rendering
- Orbit camera movement
- Renderer backend boundary extracted to `lib/rendering/RendererBackend.*` and served by bgfx and native WebGPU implementations
- Large-coordinate rendering validation with **Rebase + RTE** two-layer scheme:
  - **Rebase** (chunk-anchored world origin, see `lib/coordinate/WorldRebase.h`): once per frame the world origin snaps to the nearest 1e4-unit chunk; every GPU-bound coordinate is expressed relative to this anchor so its magnitude stays bounded to ~chunkSize/2 no matter how far the camera flies.
  - **RTE** (relative-to-eye): the view matrix is built as `lookAt(camera - rebase, target - rebase, up)` with translation included, and every object's `uModelRelativePosition = objectWorld - rebase`.  The two layers cancel in the per-vertex dot product, leaving the rendered geometry mathematically identical to the pre-rebase pipeline.
  - Double-precision camera position and world-space anchors (`glm::dvec3`) live on the CPU only; the GPU never sees a coordinate whose magnitude exceeds ~5e3.
  - Includes a persistent green origin-to-large-coordinate reference line for sanity-checking the rebase alignment.
- Basic FPS counter
- Lightweight and minimal dependencies

## Demo

<img width="1200" alt="Screenshot 2025-05-14 at 1 46 56 PM" src="https://github.com/user-attachments/assets/e0d565c6-1ea4-4a2e-b219-10d6a3150984" />
</br>
<img width="1200" alt="Screenshot 2025-05-14 at 1 55 16 PM" src="https://github.com/user-attachments/assets/624e6254-9dfc-40cd-a0a7-17d9f7dfa322" />

## Getting Started

### Prerequisites
- SDL3
- bgfx backend implementation
- wgpu-native runtime (`lib/wgpu`) for the WebGPU backend
- C++ Compiler (MSVC is used by the supplied Windows build)

### Project Structure
```
/lib           -> Source (.cpp) and header (.h) files
/lib/rendering -> Renderer backend interface, bgfx implementation, and native WebGPU implementation
main.cpp       -> Entry point
.gitignore     -> Git ignore rules
CMakeLists.txt -> CMake build configuration
README.md      -> Project documentation
```

### Renderer Status
- The application loop talks to the renderer interface in `lib/rendering/RendererBackend.h`.
- OpenGL fallback/selection has been removed from runtime and build scripts.
- `lib/rendering/BgfxRenderer.*` remains the default production backend.
- `lib/rendering/WebGpuRenderer.*` implements the scene render path with WGSL pipelines for meshes, line ribbons, fills, points, and the grid.
- Select the native WebGPU backend at runtime with `set WINDOW_RENDERER=webgpu` before starting `WINDOW.exe`.
- bgfx shader sources are now migrated under `lib/shaders/bgfx/*/{vertex.sc,frag.sc}` for grid/cube/worldLine/targetPoint passes.
- BgfxRenderer now loads and validates those bgfx shader source files during initialization before entering the render loop.

## Controls
- **Middle Mouse + Drag** — Pan the orbit target
- **Shift + Middle Mouse + Drag** — Orbit around the target
- **Mouse Scroll** — Zoom in/out
- **P** — Switch perspective/orthographic projection
- **L** — Switch between the origin demo and the large-coordinate validation scene; the green world reference line stays visible in both scenes

### Build Instructions
```bash
git clone https://github.com/tasvln/infinite-grid-plane.git
cd infinite-grid-plane
cmake -S . -B build2022
cmake --build build2022 --config Release
```
> Ensure SDL3 is properly installed and linked.
