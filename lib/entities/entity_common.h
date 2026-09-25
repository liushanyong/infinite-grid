#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace entities
{

struct EntityCommon
{
    std::uint32_t handle = 0;
    std::string name;
    std::string layer = "0";
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string lineType = "ByLayer";
    double lineWeight = 0.0;
    bool visible = true;
};

} // namespace entities
