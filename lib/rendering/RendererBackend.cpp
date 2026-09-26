#include "rendering/RendererBackend.h"
#include "rendering/BgfxRenderer.h"

namespace rendering
{

std::unique_ptr<RendererBackend> createRenderer(BackendType type, GraphicsApi api)
{
    switch (type)
    {
    case BackendType::WebGpuMigration:
        return std::make_unique<BgfxRenderer>(GraphicsApi::WebGPU);
    case BackendType::Bgfx:
    default:
        return std::make_unique<BgfxRenderer>(api);
    }
}

} // namespace rendering
