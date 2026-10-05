#pragma once

#include <glm/glm.hpp>

#include <string>

#include "acdb/AcDbRenderClass.h"

namespace acdb
{

// Field names and defaults follow vsg::AcDbMaterial.  The first implementation
// packs metallic/roughness/emissive/alpha into mesh instance data, while these
// fields remain the canonical authoring representation.
struct AcDbMaterial
{
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 diffuseFactor{0.9f, 0.9f, 0.9f, 1.0f};
    glm::vec4 specularFactor{0.2f, 0.2f, 0.2f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float alphaMask = 1.0f;
    float alphaMaskCutoff = 0.5f;

    std::string diffuseTexture;
    RenderClass renderClass = RenderClass::Realistic;
};

} // namespace acdb
