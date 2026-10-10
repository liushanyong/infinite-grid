#include "acgs/AcGsSelectionManager.h"
#include "acgs/AcGsManager.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rendering/RendererBackend.h"

namespace acgs
{

glm::vec4 encodeGpuPickId(std::uint32_t id)
{
    return {
        float((id >> 16) & 0xff) / 255.0f,
        float((id >> 8) & 0xff) / 255.0f,
        float(id & 0xff) / 255.0f,
        float((id >> 24) & 0xff) / 255.0f};
}

AcGsSelectionManager &AcGsSelectionManager::instance()
{
    static AcGsSelectionManager manager;
    return manager;
}

std::uint32_t AcGsSelectionManager::registerEntity(
    const AcGsPickEntity &entity)
{
    // Identity-stable: re-registering the same entity returns the same
    // id (the queue path runs every frame; ids must not churn).
    if (const auto found = idByEntity_.find(entity);
        found != idByEntity_.end())
    {
        return found->second;
    }
    // Ids 0, 1 and 0xffffffff are reserved (0/0xffffffff are the "no pick"
    // encodings, 1 is the fixed center-cube id).
    for (std::uint32_t count = 0; count < 0xfffffffcu; ++count)
    {
        const std::uint32_t id = nextEntityId_;
        nextEntityId_ =
            nextEntityId_ >= 0xfffffffeu ? 2 : nextEntityId_ + 1;
        if (id == 0 || id == 0xffffffffu || id == 1)
            continue;

        auto [existing, inserted] = registry_.emplace(id, entity);
        if (inserted || existing->second == entity)
        {
            idByEntity_.emplace(entity, id);
            return id;
        }
    }
    return 0;
}

const AcGsPickEntity *AcGsSelectionManager::find(std::uint32_t id) const
{
    const auto found = registry_.find(id);
    return found != registry_.end() ? &found->second : nullptr;
}

std::uint32_t AcGsSelectionManager::findIdFor(
    const AcGsPickEntity &entity) const
{
    const auto found = idByEntity_.find(entity);
    return found != idByEntity_.end() ? found->second : 0;
}

bool AcGsSelectionManager::pickEnabled()
{
    const char *value = std::getenv("GRID_GPU_PICK");
    return value == nullptr || std::strcmp(value, "0") != 0;
}

void AcGsSelectionManager::queueSoupChunks(
    const std::vector<rendering::FillVertex> &vertices,
    const glm::mat4 &view, const glm::mat4 &pickProjection,
    const glm::vec4 &logDepth, std::uint32_t objectId, bool active,
    std::uint8_t occlusionRank) const
{
    if (!active || objectId == 0 || vertices.size() < 3)
        return;

    constexpr std::size_t kMaxPickChunkVertices = 3 * 21000;
    for (std::size_t first = 0; first < vertices.size();
         first += kMaxPickChunkVertices)
    {
        const std::size_t count =
            std::min(kMaxPickChunkVertices, vertices.size() - first);
        acgsGetManager()->device()->queueGpuTrianglePick(
            0, vertices.data() + first,
                                     std::uint32_t(count), view,
                                     pickProjection, logDepth, objectId,
                                     occlusionRank);
    }
}

std::uint64_t AcGsSelectionManager::hashSceneBytes(std::uint64_t hash,
                                                   const void *data,
                                                   std::size_t size)
{
    const unsigned char *bytes =
        static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

} // namespace acgs
