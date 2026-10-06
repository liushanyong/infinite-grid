#ifndef ACGS_ORBIT_CAMERA_H
#define ACGS_ORBIT_CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <limits>
#include <optional>

// OpenCADStudio-style camera in Z-up convention.  Target/Rotation/Distance
// are authoritative; Position and the screen basis are derived so navigation
// cannot desynchronize.
class AcGsOrbitCamera
{
public:
  glm::dquat Rotation; // columns are right, up, eye direction
  glm::dvec3 Target;
  double Distance;

  // Derived, public only for the existing renderer call sites.
  glm::dvec3 Position;
  glm::dvec3 Front; // target - eye (double for VSG-style precision)
  glm::dvec3 Right;
  glm::dvec3 Up;
  // Turntable yaw axis.  Runtime-settable to any direction via setWorldUp();
  // the orbit code reads it on every horizontal drag.
  glm::dvec3 WorldUp;

  // Legacy Euler values kept for debug output and ViewCube code paths.
  float Yaw;
  float Pitch;

  float MovementSpeed;
  float MouseSensitivity;
  float Zoom; // vertical FOV, degrees

  std::optional<glm::dvec3> ModelMinimum;
  std::optional<glm::dvec3> ModelMaximum;
  double DepthHalfRange = 0.0;

  AcGsOrbitCamera(
      glm::vec3 target = glm::vec3(0.0f),
      float radius = 10.0f,
      float yaw = -90.0f,
      float pitch = 0.0f)
  {
    Target = glm::dvec3(target);
    Distance = (double)radius;
    Yaw = yaw;
    Pitch = pitch;
    WorldUp = glm::dvec3(0.0, 0.0, 1.0);
    MouseSensitivity = 0.1f;
    Zoom = 45.0f;
    MovementSpeed = 10.0f;

    // Port of OpenCADStudio yaw_pitch_to_quat (Z-up, roll=0):
    //   yaw = 0, pitch = pi/2 -> top view (camera above +Z looking down)
    //   yaw = 0, pitch = 0    -> camera on -Y looking toward +Y
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

  double orthoSize() const
  {
    return Distance * std::tan(glm::radians((double)Zoom) * 0.5);
  }

  glm::mat4 getViewMatrix(const glm::dvec3 &rebaseOrigin) const
  {
    // VSG "double all the way": compute the lookAt matrix entirely in
    // double precision, converting to float only at the final return.
    // The basis vectors (Front, Up) are already glm::dvec3.
    const glm::dvec3 eyeRelative = Position - rebaseOrigin;
    const glm::dmat4 viewDouble = glm::lookAt(
        eyeRelative,
        eyeRelative + Front,
        Up);
    return glm::mat4(viewDouble);
  }

  // Strict RTE view basis: the GPU matrix contains rotation only.  Large
  // eye/target translations never enter the f32 view matrix; positions are
  // supplied relative to the eye (or split into high/low and subtracted in
  // shader).  This matches OpenCADStudio's view_proj_rte invariant.
  glm::mat4 getViewRotationMatrix() const
  {
    const glm::dvec3 forward = -glm::normalize(
        Rotation * glm::dvec3(0.0, 0.0, 1.0));
    const glm::dvec3 up = glm::normalize(
        Rotation * glm::dvec3(0.0, 1.0, 0.0));

    // Build the basis exactly like the reference implementation: look from
    // the origin toward the eye direction and then force the translation
    // column to zero.  This is guaranteed to match the old lookAt basis.
    glm::dmat4 viewDouble = glm::lookAt(
        glm::dvec3(0.0), forward, up);
    viewDouble[3] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return glm::mat4(viewDouble);
  }

  // Port of Camera::orbit (Z-up turntable).  Horizontal drag rotates around
  // world +Z; vertical drag rotates around the camera's current right axis.
  void orbitAroundPivot(float deltaX, float deltaY, const glm::dvec3 &pivot)
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

  void zoom(float delta)
  {
    Distance = std::max(0.001, Distance * (1.0 - 0.1 * (double)delta));
    syncDerivedState();
  }

  // Port of Camera::zoom_about_point.  The cursor offset is computed on
  // the target plane before and after changing Distance; the difference moves
  // Target so the world point under the cursor remains screen-stable.
  void zoomAboutPoint(float delta, double ndcX, double ndcY, double aspect)
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

  // Port of Camera::pan_screen.
  void panScreen(float deltaX, float deltaY, float viewportHeight)
  {
    if (viewportHeight <= 0.0f)
      return;

    const double worldPerPixel = (2.0 * orthoSize()) / (double)viewportHeight;
    const glm::dvec3 &cameraRight = Right;
    const glm::dvec3 &cameraUp = Up;
    Target -= cameraRight * ((double)deltaX * worldPerPixel);
    Target += cameraUp * ((double)deltaY * worldPerPixel);
    syncDerivedState();
  }

  void setTarget(const glm::dvec3 &newTarget)
  {
    Target = newTarget;
    syncDerivedState();
  }

  void setOrbit(const glm::dvec3 &newTarget, double newDistance)
  {
    Target = newTarget;
    Distance = std::max(0.001, newDistance);
    syncDerivedState();
  }

  // ObjectARX: AcDbViewportTableRecord::setViewDirection — rebuild the
  // turntable rotation from a gaze direction (camera -> target), keeping
  // the roll level with WorldUp.  The turntable stores the camera's eye
  // direction (the gaze's negation), so the elevation convention flips.
  void setViewDirection(const glm::dvec3 &direction)
  {
    if (glm::length(direction) < 1e-12)
      return;
    constexpr double kPi = 3.14159265358979323846;
    const glm::dvec3 front = glm::normalize(direction);
    const double pitchRadians = std::asin(
        glm::clamp(-front.z, -1.0, 1.0));
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

  void setTargetDistance(double newDistance)
  {
    Distance = std::max(0.001, newDistance);
    syncDerivedState();
  }

  // Autofocus support: place the orbit target at a view-ray depth while
  // keeping the eye fixed.  In perspective this leaves the rendered image
  // unchanged.  In orthographic, Distance also controls orthoSize, so Zoom
  // is compensated to preserve the current frame.
  void setTargetDepth(double depth, bool preserveOrthoFrame)
  {
    if (!std::isfinite(depth) || depth <= 0.0)
      return;

    depth = std::max(0.001, depth);
    const double oldDepth = Distance;
    const double oldOrthoSize = orthoSize();
    const glm::dvec3 eyeDirection = glm::normalize(
        Rotation * glm::dvec3(0.0, 0.0, 1.0));

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

  // Re-anchor the turntable to an arbitrary up axis without changing where
  // the camera looks: the gaze direction is preserved and the roll is
  // re-levelled so the camera up is perpendicular to the new axis.  If the
  // axis is nearly parallel to the gaze, levelling is undefined; the axis is
  // recorded but the orientation is left untouched.
  void setWorldUp(const glm::dvec3 &newWorldUp)
  {
    if (!std::isfinite(newWorldUp.x) || !std::isfinite(newWorldUp.y) ||
        !std::isfinite(newWorldUp.z))
      return;
    if (glm::length(newWorldUp) < 1e-12)
      return;

    const glm::dvec3 axis = glm::normalize(newWorldUp);
    WorldUp = glm::vec3(axis);

    const glm::dvec3 eyeDirection = glm::normalize(
        Rotation * glm::dvec3(0.0, 0.0, 1.0));
    const double alignment = glm::dot(axis, eyeDirection);
    if (std::abs(alignment) > 1.0 - 1e-6)
    {
      syncDerivedState();
      return;
    }

    glm::dvec3 up =
        glm::normalize(axis - eyeDirection * alignment);
    const glm::dvec3 right =
        glm::normalize(glm::cross(up, eyeDirection));
    up = glm::cross(eyeDirection, right);
    Rotation =
        glm::normalize(glm::dquat(glm::dmat3(right, up, eyeDirection)));
    syncDerivedState();
  }

  // Adaptive helper: pick the world axis most perpendicular to the current
  // gaze, which keeps the turntable horizon meaningful after large orbits.
  void setWorldUpAuto()
  {
    const glm::dvec3 eyeDirection = glm::normalize(
        Rotation * glm::dvec3(0.0, 0.0, 1.0));
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

  // Port of Camera::set_projection_preserving_frame.  A projection switch is
  // not just a flag change when model bounds are available: solve for the
  // Distance that keeps the same largest screen-plane NDC displacement.
  void setProjectionPreservingFrame(bool currentOrtho, bool newOrtho,
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
      const glm::dvec3 local = glm::conjugate(Rotation) *
                               (boundsCorner(corner) - Target);
      maxRadius = std::max(maxRadius,
                           std::max(std::abs(local.y),
                                    std::abs(local.x) / aspect));
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
    const double depthSafeDistance = std::max(
        {maximumZ + depthMargin, nearSafe, farSafe, 0.001});

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
      oldExtent = maxRadius /
                  std::max(1e-6, Distance * tanHalfFov);
    }
    else
    {
      const double near = std::max(1e-6, Distance * 0.001);
      oldExtent = 0.0;
      bool allInFront = true;
      for (int corner = 0; corner < 8; ++corner)
      {
        const glm::dvec3 local = glm::conjugate(Rotation) *
                                 (boundsCorner(corner) - Target);
        const double radius = std::max(
            std::abs(local.y), std::abs(local.x) / aspect);
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
        const glm::dvec3 local = glm::conjugate(Rotation) *
                                 (boundsCorner(corner) - Target);
        const double radius = std::max(
            std::abs(local.y), std::abs(local.x) / aspect);
        newDistance = std::max(newDistance, local.z + radius / extentAtDepth);
      }

      newDistance = std::max(newDistance, depthSafeDistance);
      if (std::isfinite(newDistance))
        Distance = std::max(0.001, newDistance);
    }

    syncDerivedState();
  }

  // OpenCADStudio fit: frame the screen-plane extent; never use the 3D
  // diagonal as the zoom extent because a depth-only outlier becomes a dot.
  void fitToBounds(const glm::dvec3 &minimum, const glm::dvec3 &maximum,
                   double aspect)
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
      const glm::dvec3 local = glm::conjugate(Rotation) *
                               (boundsCorner - Target);
      halfHeight = std::max(
          {halfHeight, std::abs(local.y), std::abs(local.x) / aspect});
    }

    const double tanHalfFov =
        std::tan(glm::radians((double)Zoom) * 0.5);
    Distance = std::max(0.001, halfHeight / tanHalfFov * 1.1);
    fitDepthToBounds(minimum, maximum);
    syncDerivedState();
  }

  void fitDepthToBounds(const glm::dvec3 &minimum, const glm::dvec3 &maximum)
  {
    ModelMinimum = minimum;
    ModelMaximum = maximum;
    DepthHalfRange = depthExtentInView(minimum, maximum);
  }

  // Switching between independent scenes invalidates the previous fit's
  // model bounds.  Keep them explicit so a stale 1e7-world box cannot remain
  // attached to a camera that has just been reset to the origin.
  void clearDepthBounds()
  {
    ModelMinimum.reset();
    ModelMaximum.reset();
    DepthHalfRange = 0.0;
  }

  double depthExtentInView(const glm::dvec3 &minimum,
                           const glm::dvec3 &maximum) const
  {
    double depthRadius = 0.0;
    for (int corner = 0; corner < 8; ++corner)
    {
      const glm::dvec3 boundsCorner(
          (corner & 1) ? maximum.x : minimum.x,
          (corner & 2) ? maximum.y : minimum.y,
          (corner & 4) ? maximum.z : minimum.z);
      const glm::dvec3 local = glm::conjugate(Rotation) *
                               (boundsCorner - Target);
      depthRadius = std::max(depthRadius, std::abs(local.z));
    }

    const double diagonal = glm::length(maximum - minimum);
    return std::max(10.0, depthRadius * 1.5 + diagonal * 0.25);
  }

  // Re-derive depth from live orientation, matching Camera::ortho_depth_range.
  double orthoDepthRadius() const
  {
    if (ModelMinimum && ModelMaximum)
      return depthExtentInView(*ModelMinimum, *ModelMaximum);
    if (DepthHalfRange > 0.0)
      return std::max(10.0, DepthHalfRange);
    return std::max(10.0, Distance * 1000.0);
  }

private:
  void syncDerivedState()
  {
    const glm::dvec3 eyeDirection = glm::normalize(
        Rotation * glm::dvec3(0.0, 0.0, 1.0));
    Position = Target + eyeDirection * Distance;
    // Keep basis vectors in double precision (VSG "double all the way").
    Front = -eyeDirection;
    Right = Rotation * glm::dvec3(1.0, 0.0, 0.0);
    Up = Rotation * glm::dvec3(0.0, 1.0, 0.0);

    // OpenCAD sync_yaw_pitch (Z-up).
    Pitch = glm::degrees(std::asin(glm::clamp(eyeDirection.z, -1.0, 1.0)));
    if (std::abs(eyeDirection.x) < 1e-6 && std::abs(eyeDirection.y) < 1e-6)
      Yaw = 0.0f;
    else
      Yaw = glm::degrees(std::atan2(eyeDirection.x, -eyeDirection.y));
  }
};

#endif
