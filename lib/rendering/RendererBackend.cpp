#include "rendering/RendererBackend.h"
#include "rendering/BgfxRenderer.h"

namespace rendering
{

std::unique_ptr<RendererBackend> createRenderer(BackendType type)
{
    switch (type)
    {
    case BackendType::Bgfx:
    default:
        return std::make_unique<BgfxRenderer>();
    }
}

} // namespace rendering
