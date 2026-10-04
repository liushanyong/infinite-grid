# resources/

Bundled assets that ship with the project so the demo and tests work
out of the box without external downloads.

## Layout

```
resources/
├── README.md          # this file
├── fonts/             # TTF / OTF for the SDF text renderer
│   └── NotoSansLatin.ttf
├── dwg/               # sample DWG files for `GRID_DWG=...` testing
│   └── page-setups-metric.dwg
├── textures/          # default textures for renderer subsystems
│   └── grid-uv.png    # 256x256 procedural checkerboard for the grid UV
└── shaders/           # rendered shader source (mirrors lib/shaders/)
    └── README.md
```

## Path lookup (`lib/util/resource_path.h`)

`resource_path()` resolves a relative path under `resources/` in this
order, returning the first match:

1. `<GRID_RESOURCE_DIR>/<relative>` — environment override (CI, install
   prefix, custom data directories);
2. `<exe>/resources/<relative>` — running from a build tree with the
   resources copied next to the executable;
3. `<exe>/../resources/<relative>` — the canonical install layout
   (binary in `bin/`, resources one level up);
4. `<exe>/../../resources/<relative>` — typical build tree (cmake puts
   the binary in `bin/<config>/`);
5. `<cwd>/resources/<relative>` — running the binary in-tree.

If none matches the function returns the path under step 5 unchanged so
the caller can surface a clear "missing resource" error.

## Adding new assets

- Keep binary assets under **10 MB** per file; if you need larger
  assets, prefer runtime download and document the URL here.
- Use SPDX-compatible licenses whenever possible (OFL for fonts, CC0 or
  CC-BY for textures and 3D models). Note the license and origin in the
  file name where possible (`MyModel-CC0.glb`).
- Place 3D models / fonts in the most specific subdirectory; do not nest
  deep — the lookup above is a flat prefix match.
