#ifndef ACGS_ORBIT_CAMERA_H
#define ACGS_ORBIT_CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
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
      float pitch = 0.0f);

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
  void orbitAroundPivot(float deltaX, float deltaY, const glm::dvec3 &pivot);

  void zoom(float delta)
  {
    Distance = std::max(0.001, Distance * (1.0 - 0.1 * (double)delta));
    syncDerivedState();
  }

  // Port of Camera::zoom_about_point.  The cursor offset is computed on
  // the target plane before and after changing Distance; the difference moves
  // Target so the world point under the cursor remains screen-stable.
  void zoomAboutPoint(float delta, double ndcX, double ndcY, double aspect);

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
  void setViewDirection(const glm::dvec3 &direction);

  void setTargetDistance(double newDistance)
  {
    Distance = std::max(0.001, newDistance);
    syncDerivedState();
  }

  // Autofocus support: place the orbit target at a view-ray depth while
  // keeping the eye fixed.  In perspective this leaves the rendered image
  // unchanged.  In orthographic, Distance also controls orthoSize, so Zoom
  // is compensated to preserve the current frame.
  void setTargetDepth(double depth, bool preserveOrthoFrame);

  // Re-anchor the turntable to an arbitrary up axis without changing where
  // the camera looks: the gaze direction is preserved and the roll is
  // re-levelled so the camera up is perpendicular to the new axis.  If the
  // axis is nearly parallel to the gaze, levelling is undefined; the axis is
  // recorded but the orientation is left untouched.
  void setWorldUp(const glm::dvec3 &newWorldUp);

  // Adaptive helper: pick the world axis most perpendicular to the current
  // gaze, which keeps the turntable horizon meaningful after large orbits.
  void setWorldUpAuto();

  // Port of Camera::set_projection_preserving_frame.  A projection switch is
  // not just a flag change when model bounds are available: solve for the
  // Distance that keeps the same largest screen-plane NDC displacement.
  void setProjectionPreservingFrame(bool currentOrtho, bool newOrtho,
                                    double aspect);

  // OpenCADStudio fit: frame the screen-plane extent; never use the 3D
  // diagonal as the zoom extent because a depth-only outlier becomes a dot.
  void fitToBounds(const glm::dvec3 &minimum, const glm::dvec3 &maximum,
                   double aspect);

  void fitDepthToBounds(const glm::dvec3 &minimum, const glm::dvec3 &maximum);

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
                           const glm::dvec3 &maximum) const;

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
  void syncDerivedState();
};

#endif
