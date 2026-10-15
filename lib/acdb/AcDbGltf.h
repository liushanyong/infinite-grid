#pragma once

#include "AcDbPort.h"

// glTF 2.0 bridge (lib/acdb layer).  Mirrors AcDbDwgBridge: one file format
// in, acdb entities out -- plus the reverse direction.  Import decodes a
// .glb / .gltf document into one AcDbPolyFaceMesh per primitive (node
// transforms baked, glTF's Y-up converted to this project's Z-up) with the
// PBR metallic-roughness factors mapped onto AcDbMaterial.  Export
// serializes mesh entities back through tinygltf::WriteGltfSceneToFile.
//
// Textures stay encoded bytes on purpose: the acdb layer must not depend on
// an image decoder, so the host hands the bytes to the renderer (bimg) and
// receives a texture slot index back.  Embedded (bufferView / data-URI)
// images and external file images both end up in AcDbGltfTexture::bytes.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "acdb/AcDbPolyFaceMesh.h"

namespace acdb
{

// One referenced glTF image, kept as the encoded file bytes (PNG/JPG/...).
// |key| is the stable name stored in AcDbMaterial::diffuseTexture so a
// session-level cache can map it to a renderer texture slot.
struct ACDB_PORT AcDbGltfTexture
{
    std::string key;
    std::string name; // image name / uri as authored, for diagnostics
    std::vector<std::uint8_t> bytes;
};

struct ACDB_PORT AcDbGltfImport
{
    std::vector<AcDbPolyFaceMesh> meshes;
    std::vector<AcDbGltfTexture> textures;

    // World-space bounds of the baked geometry (Z-up).
    glm::dvec3 minimum{0.0};
    glm::dvec3 maximum{0.0};

    // Primitives present in the file but not imported (non-triangle modes,
    // sparse accessors, unsupported extensions) with their reason strings.
    std::vector<std::string> skipped;
};

// Decodes |path| (.glb binary or .gltf JSON).  Returns false and fills
// |error| when the file cannot be read or parsed; a successfully parsed
// document may still carry skipped primitives (report via |out.skipped|).
bool importGltf(const std::filesystem::path &path, AcDbGltfImport &out,
                std::string *error = nullptr);

struct ACDB_PORT AcDbGltfExportOptions
{
    bool binary = true;      // .glb container when true, JSON .gltf otherwise
    bool embedBuffers = true; // base64-embedded buffer in .gltf, binary chunk in .glb
};

// Serializes |meshes| (positions/normals/uvs/indices + AcDbMaterial factors)
// into a single-node-per-mesh glTF document.  Material texture references
// are not exported: AcDbMaterial::diffuseTexture carries a session texture
// key, not a portable file path, so exported materials carry factors only.
bool exportGltf(const std::vector<const AcDbPolyFaceMesh *> &meshes,
                const std::filesystem::path &path,
                const AcDbGltfExportOptions &options,
                std::string *error = nullptr);

} // namespace acdb
