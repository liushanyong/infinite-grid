#ifndef COORDINATE_WORLD_REBASE_H
#define COORDINATE_WORLD_REBASE_H

#include <glm/glm.hpp>
#include <cmath>

// WorldRebase -- the Rebase layer of the Rebase + RTE two-layer scheme.
//
//   * Rebase  (this struct) -- the GPU pipeline always observes the world
//     as if its origin were this chunk-aligned point.  When the camera
//     flies past a chunk boundary, the rebase origin jumps by exactly
//     one chunk and every world coordinate simply re-anchors, so the
//     visible scene stays continuous and uniform values stay bounded
//     to ~chunkSize/2.
//
//   * RTE     (relative-to-eye) -- the view matrix is computed as
//     lookAt(cameraPos - rebase, target - rebase, up).  Combined with
//     per-object uModelRelativePosition = objectWorld - rebase, the GPU
//     never sees a coordinate whose magnitude exceeds chunkSize/2.
//
// Why both layers are required:
//   - Rebase alone: the camera absolute world position still grows; an
//     object at world (1e9, 0, 0) sent verbatim to the GPU quantizes its
//     vertex positions by ~16 m on a float32 pipeline.
//   - RTE alone (no chunk anchor): every object uniform is obj - camera,
//     which drifts continuously as the camera moves.  For static objects
//     the upload is wasted bandwidth, and CPU-side round(x/step)*step at
//     the grid anchor still has to subtract a 1e9-magnitude camera
//     position before the GPU sees anything.
//
// The view matrix rotation (basis vectors right/up/forward) is independent
// of the rebase origin because lookAt only depends on the direction
// (target - camera), which is invariant under translation.  This is the
// property that makes the rebased pipeline mathematically equivalent
// to the non-rebased one.

class WorldRebase
{
public:
  // Default chunk = 1e4 world units (~10 km).  At chunk/2 = 5e3, float32
  // ULP is ~4e-4 -- well below the 1 m major grid step, so precision
  // inside the rebased coordinate frame is sub-meter everywhere.
  static constexpr double kDefaultChunkSize = 1.0e4;

  explicit WorldRebase(double chunkSize = kDefaultChunkSize)
      : m_ChunkSize(chunkSize), m_Origin(0.0, 0.0, 0.0)
  {
  }

  double chunkSize() const { return m_ChunkSize; }
  const glm::dvec3 &origin() const { return m_Origin; }

  // Recompute the chunk anchor for the current camera position.  The
  // origin is always a multiple of chunkSize on every axis, so any
  // future uniform (objWorld - origin) is bounded to chunkSize/2 in
  // magnitude no matter how far the camera flies.
  //
  // Returns true if the origin moved compared with the previous frame --
  // callers can use this signal to flush dependent caches (e.g. the
  // grid step anchor), although strictly speaking no manual update is
  // required: the math is self-consistent across chunk boundaries.
  bool update(const glm::dvec3 &cameraWorldPosition)
  {
    const glm::dvec3 snapped(
        std::round(cameraWorldPosition.x / m_ChunkSize) * m_ChunkSize,
        std::round(cameraWorldPosition.y / m_ChunkSize) * m_ChunkSize,
        std::round(cameraWorldPosition.z / m_ChunkSize) * m_ChunkSize);
    const bool changed = (snapped != m_Origin);
    m_Origin = snapped;
    return changed;
  }

  // Convert a CPU-side double world coordinate into the GPU-bound float
  // that lives in the rebased frame.  Always go through this method
  // before uploading anything that participates in shader math.
  glm::vec3 localize(const glm::dvec3 &world) const
  {
    return glm::vec3(world - m_Origin);
  }

  // XZ-plane variant for ground-plane math that does not need Y.
  glm::vec2 localizeXZ(const glm::dvec3 &world) const
  {
    return glm::vec2(glm::dvec2(world.x, world.z) -
                     glm::dvec2(m_Origin.x, m_Origin.z));
  }

  const glm::dvec3 &localizeOrigin() const { return m_Origin; }

private:
  double     m_ChunkSize;
  glm::dvec3 m_Origin;
};

#endif  // COORDINATE_WORLD_REBASE_H
