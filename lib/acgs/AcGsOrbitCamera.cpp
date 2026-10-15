#include "acgs/AcGsOrbitCamera.h"

AcGsOrbitCamera::AcGsOrbitCamera(glm::vec3 target, float radius, float yaw,
                                 float pitch)
{
  Target = glm::dvec3(target);
  Distance = (double)radius;
  Yaw = yaw;
  Pitch = pitch;
  WorldUp = glm::dvec3(0.0, 0.0, 1.0);
  MouseSensitivity = 0.1f;
  Zoom = 45.0f;
  MovementSpeed = 10.0f;

  constexpr double kPi = 3.14159265358979323846;
  const double yawRadians = glm::radians((double)yaw);
  const double pitchRadians = glm::radians((double)pitch);
  const glm::dquat qYaw =
      glm::angleAxis(yawRadians, glm::dvec3(0.0, 0.0, 1.0));
  const glm::dquat qPitch = glm::angleAxis(
      kPi * 0.5 - pitchRadians, glm::dvec3(1.0, 0.0, 0.0));
  Rotation = glm::normalize(qYaw * qPitch);
  syncDerivedState();
}

void AcGsOrbitCamera::orbitAroundPivot(float deltaX, float deltaY,
                                       const glm::dvec3 &pivot)
{
  if (std::abs(deltaX) < 1e-6f && std::abs(deltaY) < 1e-6f)
    return;

  constexpr double kOrbitSpeed = 0.005;
  const glm::dquat oldRotation = Rotation;
  const glm::dquat yawRotation =
      glm::angleAxis(-(double)deltaX * kOrbitSpeed, WorldUp);
  Rotation = glm::normalize(yawRotation * Rotation);

  const glm::dvec3 cameraRight =
      glm::normalize(Rotation * glm::dvec3(1.0, 0.0, 0.0));
  const glm::dquat pitchRotation =
      glm::angleAxis(-(double)deltaY * kOrbitSpeed, cameraRight);
  Rotation = glm::normalize(pitchRotation * Rotation);

  if (pivot != Target)
  {
    const glm::dquat deltaRotation = Rotation * glm::conjugate(oldRotation);
    Target = pivot + deltaRotation * (Target - pivot);
  }
  syncDerivedState();
}

void AcGsOrbitCamera::zoomAboutPoint(float delta, double ndcX, double ndcY,
                                     double aspect)
{
  const double oldOrthoSize = orthoSize();
  const glm::dvec3 before =
      Right * (ndcX * oldOrthoSize * aspect) +
      Up * (ndcY * oldOrthoSize);
  zoom(delta);
  const double newOrthoSize = orthoSize();
  const glm::dvec3 after =
      Right * (ndcX * newOrthoSize * aspect) +
      Up * (ndcY * newOrthoSize);
  Target += before - after;
  syncDerivedState();
}

void AcGsOrbitCamera::setViewDirection(const glm::dvec3 &direction)
{
  if (glm::length(direction) < 1e-12)
    return;
  constexpr double kPi = 3.14159265358979323846;
  const glm::dvec3 front = glm::normalize(direction);
  const double pitchRadians =
      std::asin(glm::clamp(-front.z, -1.0, 1.0));
  double yawRadians = 0.0;
  if (std::abs(front.x) > 1e-9 || std::abs(front.y) > 1e-9)
    yawRadians = std::atan2(-front.x, front.y);
  const glm::dquat qYaw =
      glm::angleAxis(yawRadians, glm::dvec3(0.0, 0.0, 1.0));
  const glm::dquat qPitch = glm::angleAxis(
      kPi * 0.5 - pitchRadians, glm::dvec3(1.0, 0.0, 0.0));
  Rotation = glm::normalize(qYaw * qPitch);
  syncDerivedState();
}

void AcGsOrbitCamera::setTargetDepth(double depth, bool preserveOrthoFrame)
{
  if (!std::isfinite(depth) || depth <= 0.0)
    return;
  depth = std::max(0.001, depth);
  const double oldDepth = Distance;
  const double oldOrthoSize = orthoSize();
  const glm::dvec3 eyeDirection =
      glm::normalize(Rotation * glm::dvec3(0.0, 0.0, 1.0));
  Target -= eyeDirection * (depth - oldDepth);
  Distance = depth;
  if (preserveOrthoFrame)
  {
    const double zoomDegrees =
        glm::degrees(2.0 * std::atan(oldOrthoSize / depth));
    if (std::isfinite(zoomDegrees))
      Zoom = glm::clamp((float)zoomDegrees, 0.01f, 179.0f);
  }
  syncDerivedState();
}

void AcGsOrbitCamera::setWorldUp(const glm::dvec3 &newWorldUp)
{
  if (!std::isfinite(newWorldUp.x) || !std::isfinite(newWorldUp.y) ||
      !std::isfinite(newWorldUp.z) || glm::length(newWorldUp) < 1e-12)
    return;

  const glm::dvec3 axis = glm::normalize(newWorldUp);
  WorldUp = glm::vec3(axis);
  const glm::dvec3 eyeDirection =
      glm::normalize(Rotation * glm::dvec3(0.0, 0.0, 1.0));
  const double alignment = glm::dot(axis, eyeDirection);
  if (std::abs(alignment) > 1.0 - 1e-6)
  {
    syncDerivedState();
    return;
  }

  glm::dvec3 up = glm::normalize(axis - eyeDirection * alignment);
  const glm::dvec3 right = glm::normalize(glm::cross(up, eyeDirection));
  up = glm::cross(eyeDirection, right);
  Rotation =
      glm::normalize(glm::dquat(glm::dmat3(right, up, eyeDirection)));
  syncDerivedState();
}

void AcGsOrbitCamera::setWorldUpAuto()
{
  const glm::dvec3 eyeDirection =
      glm::normalize(Rotation * glm::dvec3(0.0, 0.0, 1.0));
  static const glm::dvec3 kAxes[3] = {
      glm::dvec3(1.0, 0.0, 0.0),
      glm::dvec3(0.0, 1.0, 0.0),
      glm::dvec3(0.0, 0.0, 1.0)};
  int bestAxis = 2;
  double bestAlignment = 2.0;
  for (int axis = 0; axis < 3; ++axis)
  {
    const double alignment = std::abs(glm::dot(kAxes[axis], eyeDirection));
    if (alignment < bestAlignment)
    {
      bestAlignment = alignment;
      bestAxis = axis;
    }
  }
  setWorldUp(kAxes[bestAxis]);
}

void AcGsOrbitCamera::setProjectionPreservingFrame(bool currentOrtho,
                                                   bool newOrtho,
                                                   double aspect)
{
  if (!ModelMinimum || !ModelMaximum)
    return;
  aspect = std::max(0.01, aspect);
  const double tanHalfFov = std::tan(glm::radians((double)Zoom) * 0.5);
  if (!std::isfinite(tanHalfFov) || tanHalfFov <= 1e-6)
    return;

  const auto boundsCorner = [&](int bits) {
    return glm::dvec3(
        (bits & 1) ? ModelMaximum->x : ModelMinimum->x,
        (bits & 2) ? ModelMaximum->y : ModelMinimum->y,
        (bits & 4) ? ModelMaximum->z : ModelMinimum->z);
  };
  double maxRadius = 0.0;
  double minimumZ = std::numeric_limits<double>::infinity();
  double maximumZ = -std::numeric_limits<double>::infinity();
  for (int corner = 0; corner < 8; ++corner)
  {
    const glm::dvec3 local =
        glm::conjugate(Rotation) * (boundsCorner(corner) - Target);
    maxRadius = std::max(maxRadius,
                         std::max(std::abs(local.y), std::abs(local.x) / aspect));
    minimumZ = std::min(minimumZ, local.z);
    maximumZ = std::max(maximumZ, local.z);
  }
  const double depthMargin =
      std::max(0.001, std::abs(maximumZ - minimumZ) * 0.001);
  const double nearSafe = maximumZ > 0.0
      ? (maximumZ / (1.0 - 0.001)) * 1.001
      : 0.001;
  const double farSafe = minimumZ < 0.0
      ? (-minimumZ / (1000.0 - 1.0)) * 1.001
      : 0.001;
  const double depthSafeDistance =
      std::max({maximumZ + depthMargin, nearSafe, farSafe, 0.001});

  if (!std::isfinite(maxRadius) || maxRadius <= 1e-6)
  {
    if (!newOrtho && std::isfinite(depthSafeDistance))
      Distance = std::max(Distance, depthSafeDistance);
    syncDerivedState();
    return;
  }

  double oldExtent;
  if (currentOrtho)
  {
    oldExtent = maxRadius / std::max(1e-6, Distance * tanHalfFov);
  }
  else
  {
    const double near = std::max(1e-6, Distance * 0.001);
    oldExtent = 0.0;
    bool allInFront = true;
    for (int corner = 0; corner < 8; ++corner)
    {
      const glm::dvec3 local =
          glm::conjugate(Rotation) * (boundsCorner(corner) - Target);
      const double radius =
          std::max(std::abs(local.y), std::abs(local.x) / aspect);
      const double depth = Distance - local.z;
      if (depth <= near)
      {
        allInFront = false;
        break;
      }
      oldExtent = std::max(oldExtent, radius / (depth * tanHalfFov));
    }
    if (!allInFront)
      oldExtent = maxRadius / (std::max(0.001, Distance) * tanHalfFov);
  }

  if (!std::isfinite(oldExtent) || oldExtent <= 1e-6)
  {
    if (!newOrtho && std::isfinite(depthSafeDistance))
      Distance = std::max(Distance, depthSafeDistance);
    syncDerivedState();
    return;
  }
  if (newOrtho)
  {
    const double newDistance = maxRadius / (oldExtent * tanHalfFov);
    if (std::isfinite(newDistance))
      Distance = std::max(0.001, newDistance);
  }
  else
  {
    const double extentAtDepth = oldExtent * tanHalfFov;
    double newDistance = -std::numeric_limits<double>::infinity();
    for (int corner = 0; corner < 8; ++corner)
    {
      const glm::dvec3 local =
          glm::conjugate(Rotation) * (boundsCorner(corner) - Target);
      const double radius =
          std::max(std::abs(local.y), std::abs(local.x) / aspect);
      newDistance = std::max(newDistance, local.z + radius / extentAtDepth);
    }
    newDistance = std::max(newDistance, depthSafeDistance);
    if (std::isfinite(newDistance))
      Distance = std::max(0.001, newDistance);
  }
  syncDerivedState();
}

void AcGsOrbitCamera::fitToBounds(const glm::dvec3 &minimum,
                                  const glm::dvec3 &maximum, double aspect)
{
  Target = (minimum + maximum) * 0.5;
  syncDerivedState();
  aspect = std::max(0.01, aspect);
  double halfHeight = 1e-6;
  for (int corner = 0; corner < 8; ++corner)
  {
    const glm::dvec3 boundsCorner(
        (corner & 1) ? maximum.x : minimum.x,
        (corner & 2) ? maximum.y : minimum.y,
        (corner & 4) ? maximum.z : minimum.z);
    const glm::dvec3 local =
        glm::conjugate(Rotation) * (boundsCorner - Target);
    halfHeight = std::max(
        {halfHeight, std::abs(local.y), std::abs(local.x) / aspect});
  }
  const double tanHalfFov = std::tan(glm::radians((double)Zoom) * 0.5);
  Distance = std::max(0.001, halfHeight / tanHalfFov * 1.1);
  fitDepthToBounds(minimum, maximum);
  syncDerivedState();
}

void AcGsOrbitCamera::fitDepthToBounds(const glm::dvec3 &minimum,
                                       const glm::dvec3 &maximum)
{
  ModelMinimum = minimum;
  ModelMaximum = maximum;
  DepthHalfRange = depthExtentInView(minimum, maximum);
}

double AcGsOrbitCamera::depthExtentInView(const glm::dvec3 &minimum,
                                          const glm::dvec3 &maximum) const
{
  double depthRadius = 0.0;
  for (int corner = 0; corner < 8; ++corner)
  {
    const glm::dvec3 boundsCorner(
        (corner & 1) ? maximum.x : minimum.x,
        (corner & 2) ? maximum.y : minimum.y,
        (corner & 4) ? maximum.z : minimum.z);
    const glm::dvec3 local =
        glm::conjugate(Rotation) * (boundsCorner - Target);
    depthRadius = std::max(depthRadius, std::abs(local.z));
  }
  const double diagonal = glm::length(maximum - minimum);
  return std::max(10.0, depthRadius * 1.5 + diagonal * 0.25);
}

void AcGsOrbitCamera::syncDerivedState()
{
  const glm::dvec3 eyeDirection =
      glm::normalize(Rotation * glm::dvec3(0.0, 0.0, 1.0));
  Position = Target + eyeDirection * Distance;
  Front = -eyeDirection;
  Right = Rotation * glm::dvec3(1.0, 0.0, 0.0);
  Up = Rotation * glm::dvec3(0.0, 1.0, 0.0);
  Pitch = glm::degrees(glm::asin(glm::clamp(eyeDirection.z, -1.0, 1.0)));
  if (std::abs(eyeDirection.x) < 1e-6 &&
      std::abs(eyeDirection.y) < 1e-6)
    Yaw = 0.0f;
  else
    Yaw = glm::degrees(std::atan2(eyeDirection.x, -eyeDirection.y));
}
