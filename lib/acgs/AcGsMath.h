#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace acgs::math
{

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using DVec2 = glm::dvec2;
using DVec3 = glm::dvec3;
using DVec4 = glm::dvec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using DMat3 = glm::dmat3;
using DMat4 = glm::dmat4;
using DQuat = glm::dquat;

using glm::abs;
using glm::clamp;
using glm::cross;
using glm::distance;
using glm::dot;
using glm::inverse;
using glm::length;
using glm::max;
using glm::min;
using glm::normalize;
using glm::ortho;
using glm::perspective;
using glm::pi;
using glm::radians;
using glm::sin;
using glm::translate;
using glm::two_pi;
using glm::value_ptr;

} // namespace acgs::math
