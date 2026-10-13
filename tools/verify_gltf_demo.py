# Smoke test for the glTF demo: launches the app with GRID_GLTF, waits for
# the GRID_SCREENSHOT dump, kills the process, and checks that (a) the model
# imported, (b) the export wrote .glb + .gltf, (c) the screenshot has real
# content in the viewport area (not the empty background).
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXECUTABLE = ROOT / "build2022" / "bin" / "Release" / "WINDOW.exe"

# gltfDemoPlacement() = LARGE_COORDINATE_BASE_POINT + DETAIL_OFFSET
#                      + demoAnchor + (768, -1024, 0)
PLACEMENT = (10002304.0, -1024.0, 9998976.0)


def run_case(name, model, screenshot, extra_env=None, timeout_seconds=45):
    env = os.environ.copy()
    env.update({
        "WINDOW_RENDERER": "dx11",
        "GRID_GLTF": model,
        "GRID_LINE_DEBUG": "1",
        # Wireframe 2D (the startup default) draws no mesh faces; use a
        # filled style so imported geometry actually rasterizes.
        "GRID_START_STYLE": "4",
        "GRID_SCREENSHOT": str(screenshot),
        "GRID_CAMERA_START_TARGET": "%g,%g,%g,700" % PLACEMENT,
    })
    if extra_env:
        env.update(extra_env)

    print(f"[{name}] launching {model}")
    process = subprocess.Popen(
        [str(EXECUTABLE)],
        cwd=str(ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    imported = False
    skipped_log = []
    deadline = time.time() + timeout_seconds
    try:
        for line in process.stdout:
            line = line.strip()
            if "GRID_GLTF" in line:
                print(f"  {line}")
                if line.startswith("GRID_GLTF: imported"):
                    imported = True
                if "no triangle geometry" in line or "parse failed" in line:
                    imported = False
                if "skipped" in line:
                    skipped_log.append(line)
            if screenshot.exists() and time.time() > deadline - timeout_seconds + 8:
                break
            if time.time() > deadline:
                break
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
    return imported


def image_bright_ratio(path, sample_step=16, bright_threshold=8):
    # bgfx requestScreenShot writes a BMP (despite the file name); support
    # both BMP and PNG minimally.
    data = path.read_bytes()
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return png_bright_ratio(data, sample_step, bright_threshold)
    assert data[:2] == b"BM", "screenshot is neither PNG nor BMP"
    import struct
    bits_offset = struct.unpack("<I", data[10:14])[0]
    width, height = struct.unpack("<ii", data[18:26])
    bpp = struct.unpack("<H", data[28:30])[0]
    assert bpp == 32, f"unexpected bpp {bpp}"
    stride = width * 4
    bright = 0
    sampled = 0
    for y in range(0, height, sample_step):
        # BMP rows are bottom-up.
        row_offset = bits_offset + (height - 1 - y) * stride
        for x in range(0, width, sample_step):
            offset = row_offset + x * 4
            value = data[offset + 2]  # red channel is enough
            sampled += 1
            if value >= bright_threshold:
                bright += 1
    ratio = bright / max(sampled, 1)
    print(f"  screenshot {width}x{height}: bright-sample ratio={ratio:.3f}")
    return ratio


def png_bright_ratio(data, sample_step, bright_threshold):
    import struct
    import zlib
    position = 8
    width = height = None
    color_type = None
    idat = b""
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        chunk_type = data[position + 4:position + 8]
        chunk = data[position + 8:position + 8 + length]
        if chunk_type == b"IHDR":
            width, height, _, color_type = struct.unpack(">IIBB", chunk[:10])
        elif chunk_type == b"IDAT":
            idat += chunk
        elif chunk_type == b"IEND":
            break
        position += 12 + length
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color_type]
    stride = width * channels
    bright = 0
    sampled = 0
    for y in range(0, height, sample_step):
        row_offset = y * (stride + 1)
        filter_type = raw[row_offset]
        row = raw[row_offset + 1:row_offset + 1 + stride]
        if filter_type in (1, 3, 4):
            continue
        for x in range(0, width, sample_step):
            offset = x * channels
            sampled += 1
            if row[offset] >= bright_threshold:
                bright += 1
    ratio = bright / max(sampled, 1)
    print(f"  screenshot {width}x{height}: bright-sample ratio={ratio:.3f}")
    return ratio


def main():
    shot_dir = ROOT / "build2022"
    duck_shot = shot_dir / "gltf_smoke_duck.png"
    if duck_shot.exists():
        duck_shot.unlink()

    imported = run_case("duck", "duck.glb", duck_shot,
                        {"GRID_GLTF_EXPORT": "1"})
    assert imported, "duck.glb did not import"

    export_glb = ROOT / "resources" / "models" / "export" / "duck_export.glb"
    export_gltf = ROOT / "resources" / "models" / "export" / "duck_export.gltf"
    assert export_glb.exists(), f"missing export: {export_glb}"
    assert export_gltf.exists(), f"missing export: {export_gltf}"
    assert export_glb.stat().st_size > 10000, "glb export suspiciously small"
    print(f"  export ok: glb={export_glb.stat().st_size} bytes, "
          f"gltf={export_gltf.stat().st_size} bytes")

    assert duck_shot.exists(), "screenshot missing"
    assert image_bright_ratio(duck_shot) > 0.02, "screenshot looks empty"

    # Round-trip: render the exported .glb back.
    roundtrip_shot = shot_dir / "gltf_smoke_roundtrip.png"
    if roundtrip_shot.exists():
        roundtrip_shot.unlink()
    imported = run_case("roundtrip", "export/duck_export.glb", roundtrip_shot)
    assert imported, "exported .glb did not re-import"
    assert roundtrip_shot.exists(), "roundtrip screenshot missing"
    assert image_bright_ratio(roundtrip_shot) > 0.02, "roundtrip looks empty"

    # Embedded-texture path: duck_textured.glb is generated on demand by
    # make_textured_glb.py (duck geometry + a red/black checkerboard as a
    # bufferView image) and removed afterwards, so the models directory
    # stays clean and the default startup import keeps falling back to
    # duck.glb with the glossy red stand-in material.
    textured_model = ROOT / "resources" / "models" / "duck_textured.glb"
    generator = ROOT / "tools" / "make_textured_glb.py"
    import subprocess as _sp
    gen = _sp.run([sys.executable, str(generator)], capture_output=True,
                  text=True)
    assert gen.returncode == 0 and textured_model.exists(), \
        f"make_textured_glb failed: {gen.stderr}"
    textured_shot = shot_dir / "gltf_smoke_textured.png"
    if textured_shot.exists():
        textured_shot.unlink()
    imported = run_case("textured", "duck_textured.glb", textured_shot)
    ok = imported and textured_shot.exists()
    red = image_red_ratio(textured_shot) if ok else 0.0
    textured_model.unlink()
    assert ok, "duck_textured.glb did not import"
    # An untextured import renders gray/white (ratio ~0); the checkerboard
    # keeps a clear red-dominant pixel population even with dark cells and
    # shading eating into it at close camera range.
    assert red > 0.003, f"textured model shows no albedo (red ratio {red:.4f})"

    # The default startup model (duck.glb + sphere_0_0 stand-in, the
    # CADplatformer red duck) must show the same red albedo.
    default_shot = shot_dir / "gltf_smoke_default.png"
    if default_shot.exists():
        default_shot.unlink()
    imported = run_case("default-duck", "duck.glb", default_shot)
    assert imported, "duck.glb did not import"
    assert default_shot.exists(), "default duck screenshot missing"
    red = image_red_ratio(default_shot)
    # The stand-in (sphere_0_0_bcr) is dark red; under demo lighting most
    # red-dominant pixels land at r=64..95, so the brightness floor is low
    # and dominance does the discrimination.
    assert red > 0.05, f"stand-in red material missing (red ratio {red:.4f})"

    print("GLTF DEMO SMOKE PASSED")


def image_red_ratio(path, sample_step=4):
    # Shares the BMP/PNG plumbing of image_bright_ratio but counts samples
    # whose red channel clearly dominates, i.e. the red stand-in albedo.
    data = path.read_bytes()
    assert data[:2] == b"BM", "expected BMP screenshot"
    bits_offset = struct.unpack("<I", data[10:14])[0]
    width, height = struct.unpack("<ii", data[18:26])
    stride = width * 4
    red = 0
    sampled = 0
    for y in range(0, height, sample_step):
        row_offset = bits_offset + (height - 1 - y) * stride
        for x in range(0, width, sample_step):
            offset = row_offset + x * 4
            b, g, r = data[offset], data[offset + 1], data[offset + 2]
            sampled += 1
            if r > 45 and r > g * 1.8 and r > b * 1.8:
                red += 1
    ratio = red / max(sampled, 1)
    print(f"  red-sample ratio={ratio:.4f}")
    return ratio


if __name__ == "__main__":
    main()
