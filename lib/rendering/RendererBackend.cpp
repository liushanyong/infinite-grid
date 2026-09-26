#include "rendering/RendererBackend.h"
#include "rendering/BgfxRenderer.h"
#include "rendering/WebGpuRenderer.h"

#include <iostream>

namespace rendering
{

std::unique_ptr<RendererBackend> createRenderer(BackendType type, GraphicsApi api)
{
    switch (type)
    {
    case BackendType::WebGpu:
        if (webGpuRuntimeAvailable())
            return std::make_unique<WebGpuRenderer>();

        std::cerr << "wgpu-native runtime was not found; "
                     "using the D3D12 WebGPU migration backend."
                  << std::endl;
        return std::make_unique<BgfxRenderer>(GraphicsApi::Direct3D12);
    case BackendType::WebGpuMigration:
        return std::make_unique<BgfxRenderer>(GraphicsApi::WebGPU);
    case BackendType::Bgfx:
    default:
        return std::make_unique<BgfxRenderer>(api);
    }
}

} // namespace rendering
