#ifndef ORBIT_CAMERA_H
#define ORBIT_CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

// TODO: split camera's -> Orbit Camera & FPS Camera & TPS Camera
class OrbitCamera
{
public:
  // Camera state is stored in world space, double precision.  Position
  // and Target can grow without bound (large open-world coordinates)
  // while the GPU only ever sees rebased + RTE floats via
  // getViewMatrix(rebaseOrigin).
  glm::dvec3 Position;
  glm::vec3  Front;   // unit direction, derived from Yaw/Pitch
  glm::vec3  Up;      // unit direction
  glm::vec3  Right;   // unit direction
  glm::vec3  WorldUp; // unit vector (default +Y)

  glm::dvec3 Target;

  // Euler Angles
  float Yaw;
  float Pitch;

  // Orbit Attributes
  // (No fixed Radius: the wheel dolly mutates Position directly, so
  //  the camera-to-target distance is always length(Position - Target).)


  // Basic Camera Options
  float MovementSpeed;
  float MouseSensitivity;
  float Zoom;

  // Constructor with default values
  OrbitCamera(
      glm::vec3 target = glm::vec3(0.0f),
      float radius = 10.0f,
      float yaw = -90.0f,
      float pitch = 0.0f)
  {
    Target = glm::dvec3(target);
    Yaw = yaw;
    Pitch = pitch;

    WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    MouseSensitivity = 0.1f;
    Zoom = 45.0f;
    MovementSpeed = 10.0f;

    // Seed Position from the requested initial distance.  After this
    // the dynamic distance (length(Position - Target)) is the single
    // source of truth -- the wheel dolly mutates Position directly,
    // and updateCameraVectors() below will pick up that distance
    // automatically.
    const double r0 = (double)radius;
    const double cosP0 = cos(glm::radians((double)Pitch));
    Position = Target + glm::dvec3(
        r0 * cosP0 * cos(glm::radians((double)Yaw)),
        r0 * sin(glm::radians((double)Pitch)),
        r0 * cosP0 * sin(glm::radians((double)Yaw)));

    updateCameraVectors();
  }

  // Returns the rebased view matrix for the Rebase + RTE pipeline.
  //
  //   * Rebase: the supplied rebaseOrigin is the chunk-aligned world anchor
  //     maintained by lib/coordinate/WorldRebase.h.
  //   * RTE:    the view matrix is built from (camera - rebase) and
  //     (target - rebase), so its rotation equals the original lookAt
  //     rotation (translation-invariant) but its translation column
  //     places the rebased camera at the view-space origin.
  //
  // Callers must also send objWorld - rebaseOrigin (rebase-relative)
  // for every object's uModelRelativePosition so the GPU pipeline is
  // mathematically equivalent to the no-rebase case.
  glm::mat4 getViewMatrix(const glm::dvec3 &rebaseOrigin) const
  {
    return glm::lookAt(glm::vec3(Position - rebaseOrigin),
                       glm::vec3(Target - rebaseOrigin),
                       Up);
  }

  void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true)
  {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // Constrain pitch to prevent flip
    if (constrainPitch)
    {
      if (Pitch > 89.0f)
        Pitch = 89.0f;
      if (Pitch < -89.0f)
        Pitch = -89.0f;
    }

    updateCameraVectors();
  }

  void processMouseScroll(float yoffset)
  {
    // Dolly the camera along its view direction.  Scrolling up
    // (positive yoffset) drives the
    // camera toward the target, scrolling down pulls it back.  Step
    // size is 10%% of the current target distance per wheel notch so
    // the rate is the same as the ortho path regardless of how zoomed
    // out the camera is.
    if (yoffset == 0.0f)
      return;
    const glm::dvec3 front = glm::normalize(glm::dvec3(Target) - Position);
    const double dist = glm::length(glm::dvec3(Target) - Position);
    if (dist < 1e-6)
      return;

    Position += front * (dist * 0.1 * (double)yoffset);

    // Keep close-up usable while allowing the camera to fit the full
    // 1e7-coordinate validation scene.  The hard limit only guards against
    // non-finite or runaway dolly states.
    const double newDist = glm::length(glm::dvec3(Target) - Position);
    if (newDist < 1.0)
      Position = Target - front * 1.0;
    else if (newDist > 1.0e9)
      Position = Target - front * 1.0e9;

    // Recompute the basis vectors from the new Position.  We do NOT
    // call updateCameraVectors() here because that would snap
    // Position back onto the spherical surface defined by
    // Yaw / Pitch and undo the dolly.
    Front = glm::normalize(glm::vec3(Target - Position));
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
  }

  void processMousePan(float xoffset, float yoffset, float worldPerPixel,
                       bool panOnGroundPlane = false,
                       const glm::dvec3 &panPlaneTangentU = glm::dvec3(1.0, 0.0, 0.0),
                       const glm::dvec3 &panPlaneTangentV = glm::dvec3(0.0, 0.0, 1.0))
  {
    const double screenX = -xoffset * worldPerPixel;
    const double screenY = yoffset * worldPerPixel;
    glm::dvec3 delta;

    if (panOnGroundPlane)
    {
      // Build the basis in double precision from the actual eye/target ray.
      // The stored float basis is still used for rendering, but panning with
      // the double basis avoids accumulating quantized world-space steps.
      const glm::dvec3 front = glm::normalize(Target - Position);
      const glm::dvec3 right =
          glm::normalize(glm::cross(front, glm::dvec3(WorldUp)));
      const glm::dvec3 up = glm::normalize(glm::cross(right, front));

      // Find the in-plane translation whose camera-space projection matches
      // the requested screen translation.  Solve for the tangent-plane
      // coefficients of: right * screenX + up * screenY.
      const double a11 = glm::dot(right, panPlaneTangentU);
      const double a12 = glm::dot(right, panPlaneTangentV);
      const double a21 = glm::dot(up, panPlaneTangentU);
      const double a22 = glm::dot(up, panPlaneTangentV);
      const double determinant = a11 * a22 - a12 * a21;
      constexpr double kMinDeterminant = 1.0e-2;

      if (std::abs(determinant) >= kMinDeterminant)
      {
        const double tangentUCoefficient =
            (screenX * a22 - a12 * screenY) / determinant;
        const double tangentVCoefficient =
            (a11 * screenY - a21 * screenX) / determinant;
        delta = panPlaneTangentU * tangentUCoefficient +
                panPlaneTangentV * tangentVCoefficient;
      }
      else
      {
        // The active plane is edge-on in screen space; fall back to the
        // camera plane rather than dividing by a numerically unstable matrix.
        delta = right * screenX + up * screenY;
      }
    }
    else
    {
      delta = glm::dvec3(Right) * screenX + glm::dvec3(Up) * screenY;
    }

    Target += delta;
    Position += delta;
  }

  void setTarget(glm::vec3 newTarget)
  {
    Target = newTarget;
    updateCameraVectors();
  }

  void setTarget(glm::dvec3 newTarget)
  {
    Target = newTarget;
    updateCameraVectors();
  }

  // Reposition the camera so it orbits newTarget at exactly newDistance,
  // preserving the current Yaw/Pitch.  Used by the L-key reset (and
  // anywhere else we want a clean snap).
  void setOrbit(const glm::dvec3 &newTarget, double newDistance)
  {
    Target = newTarget;
    const double cosP = cos(glm::radians((double)Pitch));
    Position = Target + glm::dvec3(
        newDistance * cosP * cos(glm::radians((double)Yaw)),
        newDistance * sin(glm::radians((double)Pitch)),
        newDistance * cosP * sin(glm::radians((double)Yaw)));
    Front = glm::normalize(glm::vec3(Target - Position));
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
  }

  // Dolly along the actual eye->target direction.  Projection switching must
  // not reuse Yaw/Pitch because wheel dolly and viewport conversions can make
  // those Euler angles stale while Front remains the intended view direction.
  void setTargetDistance(double newDistance)
  {
    const glm::dvec3 front = glm::normalize(Target - Position);
    newDistance = glm::clamp(newDistance, 1.0, 1.0e9);

    Position = Target - front * newDistance;
    Front = glm::normalize(glm::vec3(front));
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));

    // Keep orbit angles synchronized with the moved position so the next
    // mouse orbit continues from the converted view instead of snapping back.
    Yaw = glm::degrees(std::atan2((double)-Front.z, (double)-Front.x));
    Pitch = glm::degrees(std::asin(glm::clamp((double)-Front.y, -1.0, 1.0)));
  }

private:
  void updateCameraVectors()
  {
    // Orbit spherical coordinates to Cartesian.  We use the *current*
    // camera-to-target distance instead of any stored value, so a
    // wheel-dollied camera stays at its dolly distance even after the
    // user starts dragging to rotate.
    const double distance = glm::length(Position - Target);
    const double cosP = cos(glm::radians((double)Pitch));
    const double x = distance * cosP * cos(glm::radians((double)Yaw));
    const double y = distance * sin(glm::radians((double)Pitch));
    const double z = distance * cosP * sin(glm::radians((double)Yaw));

    Position = Target + glm::dvec3(x, y, z);

    Front = glm::normalize(glm::vec3(Target - Position));
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
  }
};

#endif
