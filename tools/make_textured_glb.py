# Builds resources/models/duck_textured.glb: the duck geometry plus a
# PBR metallic-roughness material whose base-color texture is a PNG embedded
# as a bufferView image.  This exercises the full glTF texture import path
# (embedded image -> tinygltf as-is loader -> bimg decode -> renderer slot).
# The texture is a red/black checkerboard generated inline so a working
# albedo bind is unmistakable in a screenshot.
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def checker_png(size=256, cells=8):
    # Minimal PNG writer: RGB8, no filtering.
    rows = bytearray()
    half = size // cells
    for y in range(size):
        rows.append(0)  # filter: none
        for x in range(size):
            red = ((x // half) + (y // half)) % 2 == 0
            rows += bytes((255 if red else 30, 25 if red else 25, 25))
    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(
            ">I", zlib.crc32(body))
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b""))


def main():
    source = ROOT / "resources" / "models" / "duck.glb"
    output = ROOT / "resources" / "models" / "duck_textured.glb"

    data = source.read_bytes()
    json_len, = struct.unpack("<I", data[12:16])
    gltf = json.loads(data[20:20 + json_len])
    # skip the 8-byte BIN chunk header (length + type) after the JSON chunk
    bin_chunk = data[28 + json_len:]
    # pad the existing bin to 4 bytes for the image bufferView offset
    while len(bin_chunk) % 4 != 0:
        bin_chunk += b"\x00"

    png = checker_png()
    image_view = len(gltf["bufferViews"])
    gltf["bufferViews"].append({
        "buffer": 0,
        "byteOffset": len(bin_chunk),
        "byteLength": len(png),
    })
    bin_chunk += png
    # the declared buffer length must cover the appended image bytes, or
    # loaders normalize buffer.data down and the image reads as garbage
    gltf["buffers"][0]["byteLength"] = len(bin_chunk)

    image_index = len(gltf.get("images", []))
    gltf.setdefault("images", []).append({
        "bufferView": image_view,
        "mimeType": "image/png",
        "name": "grid-uv",
    })
    texture_index = len(gltf.get("textures", []))
    gltf.setdefault("textures", []).append({"source": image_index})
    material_index = len(gltf.get("materials", []))
    gltf.setdefault("materials", []).append({
        "name": "grid-uv-pbr",
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
            "baseColorTexture": {"index": texture_index},
            "metallicFactor": 0.1,
            "roughnessFactor": 0.7,
        },
    })
    for mesh in gltf["meshes"]:
        for primitive in mesh["primitives"]:
            primitive["material"] = material_index

    out_json = json.dumps(gltf).encode("utf-8")
    while len(out_json) % 4 != 0:
        out_json += b" "
    out_bin = bin_chunk
    total = 12 + 8 + len(out_json) + 8 + len(out_bin)
    glb = b"glTF" + struct.pack("<II", 2, total)
    glb += struct.pack("<I", len(out_json)) + b"JSON" + out_json
    glb += struct.pack("<I", len(out_bin)) + b"BIN\x00" + out_bin
    output.write_bytes(glb)
    print(f"wrote {output} ({len(glb)} bytes, image {len(png)} bytes)")


if __name__ == "__main__":
    sys.exit(main())
