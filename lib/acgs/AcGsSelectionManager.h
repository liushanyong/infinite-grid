#pragma once

// AcGsSelectionManager — GPU-assisted selection bookkeeping, modeled on the
// selection internals ObjectARX keeps inside AcGiManager: an entity-id
// registry shared between the visible pass and the GPU ID pass, the
// 1x1-pick request lifecycle, and the bounded-chunk triangle-soup queue.
//
// Entity identity is type-erased (kind tag + opaque pointers): the
// application maps its own entity records to and from AcGsPickEntity, so
// the manager never depends on demo-level scene types.

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rendering
{
class RendererBackend;
struct FillVertex;
}

namespace acgs
{

// Type-erased entity reference.  |kind| is an application-defined
// discriminator; the three pointers carry whatever the application needs to
// resolve the entity later (mesh record, tessellation range, curve command
// — any may be null).
struct AcGsPickEntity
{
    std::uint32_t kind = 0;
    const void *mesh = nullptr;
    const void *range = nullptr;
    const void *curve = nullptr;
    const void *text = nullptr;

    bool operator==(const AcGsPickEntity &other) const = default;
};

// Encode an entity id into the RGBA color the GPU ID pass writes.
glm::vec4 encodeGpuPickId(std::uint32_t id);

// Identity hash for the reverse entity->id lookup: the queue path
// re-registers the same scene entities every frame, and ids must stay
// stable across frames (the selection-outline uniform and the unified
// pixel read resolve against the standing registry).
struct AcGsPickEntityHash
{
    std::size_t operator()(const AcGsPickEntity &entity) const
    {
        std::size_t hash = 0xcbf29ce484222325ull;
        auto mix = [&hash](std::uintptr_t value)
        {
            hash ^= value + 0x9e3779b97f4a7c15ull +
                    (hash << 6) + (hash >> 2);
        };
        mix(entity.kind);
        mix(reinterpret_cast<std::uintptr_t>(entity.mesh));
        mix(reinterpret_cast<std::uintptr_t>(entity.range));
        mix(reinterpret_cast<std::uintptr_t>(entity.curve));
        mix(reinterpret_cast<std::uintptr_t>(entity.text));
        return hash;
    }
};

class AcGsSelectionManager
{
public:
    static AcGsSelectionManager &instance();

    // Camera basis captured when a pick request is issued, so the CPU
    // fallback raycast can reuse the exact frustum of the request.
    struct CameraBasis
    {
        glm::dvec3 position{0.0};
        glm::dvec3 front{0.0, 0.0, -1.0};
        glm::dvec3 right{1.0, 0.0, 0.0};
        glm::dvec3 up{0.0, 1.0, 0.0};
        bool ortho = false;
        double orthoHalfHeight = 0.0;
    };

    // 1x1-pick request lifecycle state.
    struct FocusState
    {
        std::optional<CameraBasis> camera;
        double ndcX = 0.0;
        double ndcY = 0.0;
        std::uint32_t requestToken = 0;
        std::uint32_t pendingFrames = 0;
        bool pendingNdc = false;
        bool waitingResult = false;
    };

    // ---- entity id registry ----

    // Registers |entity| and returns a stable pick id (0 on exhaustion).
    // Re-registering an equal entity returns the same id.
    std::uint32_t registerEntity(const AcGsPickEntity &entity);
    const AcGsPickEntity *find(std::uint32_t id) const;
    std::uint32_t findIdFor(const AcGsPickEntity &entity) const;
    // The full-scene ID pass rebuilds the registry every request frame.
    void clearRegistry() { registry_.clear(); idByEntity_.clear(); }
    std::size_t registrySize() const { return registry_.size(); }
    std::uint32_t nextIdCounter() const { return nextEntityId_; }
    template <typename Fn> void forEach(Fn &&fn) const
    {
        for (const auto &[id, entity] : registry_)
            fn(id, entity);
    }

    // ---- pick request lifecycle ----

    // GPU rough-picking is the default; GRID_GPU_PICK=0 selects the CPU path.
    static bool pickEnabled();

    FocusState &focus() { return focus_; }
    const FocusState &focus() const { return focus_; }
    bool focusWaiting() const
    {
        return pickEnabled() && focus_.waitingResult;
    }

    // ---- triangle soup queueing ----

    // Submits |vertices| to the backend's GPU triangle-pick queue in
    // bounded chunks (the queue's per-request triangle capacity); no-op
    // unless |active| and the soup has at least one triangle.
    void queueSoupChunks(const std::vector<rendering::FillVertex> &vertices,
                         const glm::mat4 &view,
                         const glm::mat4 &pickProjection,
                         const glm::vec4 &logDepth, std::uint32_t objectId,
                         bool active) const;

    // FNV-1a over raw bytes; used to detect when the full-scene GPU ID
    // buffer must be re-rendered for the selection outline overlay.
    static std::uint64_t hashSceneBytes(std::uint64_t hash, const void *data,
                                        std::size_t size);

private:
    AcGsSelectionManager() = default;

    std::unordered_map<std::uint32_t, AcGsPickEntity> registry_;
    std::unordered_map<AcGsPickEntity, std::uint32_t,
                      AcGsPickEntityHash>
        idByEntity_;
    std::uint32_t nextEntityId_ = 2;
    FocusState focus_;
};

} // namespace acgs
