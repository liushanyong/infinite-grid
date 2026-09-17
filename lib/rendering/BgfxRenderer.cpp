#include "rendering/BgfxRenderer.h"

#include <algorithm>
#include <array>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

namespace GridShaders
{
// Rebuild this translation unit whenever the embedded infinite-grid shader
// headers are regenerated.
#include "shaders/infiniteGrid/vertex.h"
#include "shaders/infiniteGrid/frag.h"
} // namespace GridShaders

namespace CubeShaders
{
#include "shaders/centerAnchor/vertex.h"
#include "shaders/centerAnchor/frag.h"
} // namespace CubeShaders

namespace LineShaders
{
#include "shaders/worldLine/vertex.h"
#include "shaders/worldLine/frag.h"
} // namespace LineShaders

namespace PointShaders
{
#include "shaders/targetPoint/vertex.h"
#include "shaders/targetPoint/frag.h"
} // namespace PointShaders

namespace rendering
{

namespace
{

struct CubeVertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

std::array<CubeVertex, 36> makeCubeVertices()
{
    const glm::vec3 px(1.0f, 0.0f, 0.0f);
    const glm::vec3 nx(-1.0f, 0.0f, 0.0f);
    const glm::vec3 py(0.0f, 1.0f, 0.0f);
    const glm::vec3 ny(0.0f, -1.0f, 0.0f);
    const glm::vec3 pz(0.0f, 0.0f, 1.0f);
    const glm::vec3 nz(0.0f, 0.0f, -1.0f);

    return {
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), pz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), pz},

        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nz},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), nz},

        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), px},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), px},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), px},

        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), nx},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), nx},

        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), py},
        CubeVertex{glm::vec3(-0.5f, 0.5f, 0.5f), py},
        CubeVertex{glm::vec3(0.5f, 0.5f, -0.5f), py},
        CubeVertex{glm::vec3(-0.5f, 0.5f, -0.5f), py},

        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), ny},
        CubeVertex{glm::vec3(-0.5f, -0.5f, -0.5f), ny},
        CubeVertex{glm::vec3(0.5f, -0.5f, 0.5f), ny},
        CubeVertex{glm::vec3(-0.5f, -0.5f, 0.5f), ny},
};
}

std::array<CubeVertex, 24> makeCubeEdgeVertices()
{
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 corners[8] = {
        glm::vec3(-0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f,  0.5f),
        glm::vec3( 0.5f,  0.5f,  0.5f),
        glm::vec3( 0.5f,  0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f, -0.5f),
    };

    return {
        CubeVertex{corners[0], up}, CubeVertex{corners[1], up},
        CubeVertex{corners[1], up}, CubeVertex{corners[2], up},
        CubeVertex{corners[2], up}, CubeVertex{corners[3], up},
        CubeVertex{corners[3], up}, CubeVertex{corners[0], up},
        CubeVertex{corners[4], up}, CubeVertex{corners[5], up},
        CubeVertex{corners[5], up}, CubeVertex{corners[6], up},
        CubeVertex{corners[6], up}, CubeVertex{corners[7], up},
        CubeVertex{corners[7], up}, CubeVertex{corners[4], up},
        CubeVertex{corners[0], up}, CubeVertex{corners[4], up},
        CubeVertex{corners[1], up}, CubeVertex{corners[5], up},
        CubeVertex{corners[2], up}, CubeVertex{corners[6], up},
        CubeVertex{corners[3], up}, CubeVertex{corners[7], up},
    };
}

bgfx::UniformHandle createUniformHandle(const char *name, bgfx::UniformType::Enum type)
{
    const bgfx::UniformHandle handle = bgfx::createUniform(name, type);
    if (!bgfx::isValid(handle))
        std::cerr << "Failed to create bgfx uniform: " << name << std::endl;
    return handle;
}

void destroyUniform(bgfx::UniformHandle &handle)
{
    if (bgfx::isValid(handle))
        bgfx::destroy(handle);
    handle = BGFX_INVALID_HANDLE;
}

glm::mat4 projectionForDirect3D(const glm::mat4 &projection)
{
    // Convert GL clip-space depth [-1, 1] into D3D clip-space depth [0, 1]:
    //   z' = 0.5*z + 0.5*w
    //   w' = w
    // In glm (column-major) mat[col][row], so the '0.5 * w' term that lands
    // on the z-row lives at column 3, row 2 (i.e. depthRange[3][2]).
    // Writing to depthRange[2][3] instead placed 0.5 into row-3/col-2, which
    // corrupted w on every draw -- distorting the perspective near/far
    // depth mapping and the target-point CPU clip-space division.
    glm::mat4 depthRange(1.0f);
    depthRange[2][2] = 0.5f;
    depthRange[3][2] = 0.5f;
    return depthRange * projection;
}

std::array<float, 4> packVec4(const glm::vec3 &value, float extra)
{
    return {value.x, value.y, value.z, extra};
}

} // namespace

const char *BgfxRenderer::name() const
{
    return "bgfx";
}

Uint32 BgfxRenderer::windowFlags() const
{
    return 0;
}

bool BgfxRenderer::configureSDL()
{
    return true;
}

bool BgfxRenderer::initialize(SDL_Window *window)
{
    m_window = window;

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    m_width = static_cast<uint16_t>(std::max(1, width));
    m_height = static_cast<uint16_t>(std::max(1, height));

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void *hwnd = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!hwnd)
    {
        std::cerr << "Failed to get Win32 HWND from SDL window." << std::endl;
        return false;
    }

    bgfx::Init init;
    init.type = bgfx::RendererType::Direct3D11;
    init.platformData.nwh = hwnd;
    init.resolution.width = m_width;
    init.resolution.height = m_height;
    init.resolution.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init))
    {
        std::cerr << "Failed to initialize bgfx D3D11 renderer." << std::endl;
        return false;
    }

    bgfx::setViewMode(0, bgfx::ViewMode::Sequential);
    m_initialized = true;
    if (!createRenderResources())
    {
        shutdown();
        return false;
    }

    std::cout << "bgfx D3D11 renderer initialized." << std::endl;
    return true;
}

void BgfxRenderer::shutdown()
{
    if (!m_initialized)
    {
        m_window = nullptr;
        return;
    }

    if (bgfx::isValid(m_gridProgram))
        bgfx::destroy(m_gridProgram);
    m_gridProgram = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_cubeProgram))
        bgfx::destroy(m_cubeProgram);
    m_cubeProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cubeBuffer))
        bgfx::destroy(m_cubeBuffer);
    m_cubeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_aabbBuffer))
        bgfx::destroy(m_aabbBuffer);
    m_aabbBuffer = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_lineProgram))
        bgfx::destroy(m_lineProgram);
    m_lineProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_lineBuffer))
        bgfx::destroy(m_lineBuffer);
    m_lineBuffer = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(m_pointProgram))
        bgfx::destroy(m_pointProgram);
    m_pointProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_pointBuffer))
        bgfx::destroy(m_pointBuffer);
    m_pointBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_gridBuffer))
        bgfx::destroy(m_gridBuffer);
    m_gridBuffer = BGFX_INVALID_HANDLE;

    destroyUniform(m_gridInvViewProj);
    destroyUniform(m_gridViewProj);
    destroyUniform(m_gridCamFront);
    destroyUniform(m_gridOrthoPlaneCenter);
    destroyUniform(m_gridOrthoRight);
    destroyUniform(m_gridOrthoUp);
    destroyUniform(m_gridOriginRelative);
    destroyUniform(m_gridPlaneNormal);
    destroyUniform(m_gridPlaneTangentU);
    destroyUniform(m_gridPlaneTangentV);
    destroyUniform(m_gridAxisColorU);
    destroyUniform(m_gridAxisColorV);
    destroyUniform(m_gridStartAxisOrigin);
    destroyUniform(m_gridStartAxisDirection);
    destroyUniform(m_gridStartAxisVisible);
    destroyUniform(m_gridStartAxisLine);
    destroyUniform(m_gridAxisOriginGridRelative);
    destroyUniform(m_gridAxisLineX);
    destroyUniform(m_gridAxisLineZ);
    destroyUniform(m_gridIsOrtho);
    destroyUniform(m_gridGroundRelativeY);
    destroyUniform(m_gridStep);
    destroyUniform(m_gridAxisVisible);
    destroyUniform(m_gridScreenHeight);
    destroyUniform(m_gridScreenWidth);
    destroyUniform(m_gridColorMajor);
    destroyUniform(m_gridColorMinor);
    destroyUniform(m_gridOpacity);
    destroyUniform(m_gridOrthoPlaneValid);
    destroyUniform(m_view);
    destroyUniform(m_projection);
    destroyUniform(m_cubeRelativePosition);
    destroyUniform(m_cubeOpacity);
    destroyUniform(m_cubeColor);
    destroyUniform(m_lineStart);
    destroyUniform(m_lineEnd);
    destroyUniform(m_pointPosition);
    destroyUniform(m_pointSize);
    destroyUniform(m_pointColor);

    bgfx::shutdown();
    m_initialized = false;
    m_window = nullptr;
}

void BgfxRenderer::beginFrame(const glm::vec4 &clearColor)
{
    if (!m_initialized)
        return;

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    width = std::max(1, width);
    height = std::max(1, height);
    if (width != m_width || height != m_height)
    {
        bgfx::reset(static_cast<uint16_t>(width),
                    static_cast<uint16_t>(height),
                    BGFX_RESET_VSYNC);
        m_width = static_cast<uint16_t>(width);
        m_height = static_cast<uint16_t>(height);
    }

    const auto channel = [](float value) {
        return uint32_t(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    };
    const uint32_t rgba = channel(clearColor.r) << 24 |
                          channel(clearColor.g) << 16 |
                          channel(clearColor.b) << 8 |
                          channel(clearColor.a);

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, m_width, m_height);
    bgfx::touch(0);
}

void BgfxRenderer::endFrame()
{
    if (m_initialized)
        bgfx::frame();
}

void BgfxRenderer::present()
{
    // bgfx::frame() presents in endFrame(); SDL3 remains the window owner.
}

void BgfxRenderer::drawGrid(const GridRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_gridProgram))
        return;

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LEQUAL |
                   BGFX_STATE_BLEND_ALPHA);
    bgfx::setVertexBuffer(0, m_gridBuffer);
    bgfx::setUniform(m_gridInvViewProj, glm::value_ptr(data.invViewProj));
    const glm::mat4 depthViewProj =
        projectionForDirect3D(data.projection) * data.view;
    bgfx::setUniform(m_gridViewProj, glm::value_ptr(depthViewProj));
    bgfx::setUniform(m_gridCamFront, packVec4(data.camFront, 0.0f).data());
    bgfx::setUniform(m_gridOrthoPlaneCenter,
                     packVec4(data.orthoPlaneCenter, 0.0f).data());
    bgfx::setUniform(m_gridOrthoRight,
                     packVec4(data.orthoRight, 0.0f).data());
    bgfx::setUniform(m_gridOrthoUp,
                     packVec4(data.orthoUp, 0.0f).data());
    bgfx::setUniform(m_gridOriginRelative,
                     packVec4(data.planeOriginRelative, data.plane).data());
    bgfx::setUniform(m_gridPlaneNormal,
                     packVec4(data.planeNormal, 0.0f).data());
    bgfx::setUniform(m_gridPlaneTangentU,
                     packVec4(data.planeTangentU, 0.0f).data());
    bgfx::setUniform(m_gridPlaneTangentV,
                     packVec4(data.planeTangentV, 0.0f).data());
    bgfx::setUniform(m_gridAxisColorU,
                     packVec4(data.axisColorU, 0.0f).data());
    bgfx::setUniform(m_gridAxisColorV,
                     packVec4(data.axisColorV, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisOrigin,
                     packVec4(data.startAxisOrigin, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisDirection,
                     packVec4(data.startAxisDirection, 0.0f).data());
    bgfx::setUniform(m_gridStartAxisVisible,
                     glm::value_ptr(glm::vec4(data.startAxisVisible, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridStartAxisLine,
                     packVec4(data.startAxisLine, 0.0f).data());
    bgfx::setUniform(
        m_gridAxisOriginGridRelative,
        packVec4(glm::vec3(data.axisOriginGridRelative, 0.0f), 0.0f).data());
    bgfx::setUniform(m_gridAxisLineX, packVec4(data.axisLineX, 0.0f).data());
    bgfx::setUniform(m_gridAxisLineZ, packVec4(data.axisLineZ, 0.0f).data());
    bgfx::setUniform(m_gridIsOrtho,
                     glm::value_ptr(glm::vec4(data.isOrtho, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridGroundRelativeY,
                     glm::value_ptr(glm::vec4(data.groundRelativeY, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridStep,
                     glm::value_ptr(glm::vec4(data.step, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(
        m_gridAxisVisible,
        packVec4(glm::vec3(data.axisVisible, 0.0f), 0.0f).data());
    bgfx::setUniform(
        m_gridScreenHeight,
        glm::value_ptr(glm::vec4(data.screenHeight, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(
        m_gridScreenWidth,
        glm::value_ptr(glm::vec4(data.screenWidth, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridColorMajor,
                     packVec4(data.gridColorMajor, 0.0f).data());
    bgfx::setUniform(m_gridColorMinor,
                     packVec4(data.gridColorMinor, 0.0f).data());
    bgfx::setUniform(m_gridOpacity,
                     glm::value_ptr(glm::vec4(data.gridOpacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridOrthoPlaneValid,
                     glm::value_ptr(glm::vec4(data.orthoPlaneValid, 0.0f, 0.0f, 0.0f)));
    bgfx::submit(0, m_gridProgram);
}

void BgfxRenderer::drawCube(const CubeRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_cubeProgram))
        return;

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    // Keep clockwise back-face culling: projectionForDirect3D only remaps Z,
    // so screen-space winding is preserved and CULL_CW is required.
    // Using CULL_CCW would strip the front faces of every translucent cube
    // and break the intended painter-style depth ordering.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                   BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_CULL_CW |
                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
    bgfx::setTransform(glm::value_ptr(data.model));
    bgfx::setVertexBuffer(0, m_cubeBuffer);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_cubeRelativePosition,
                     glm::value_ptr(glm::vec4(data.modelRelativePosition, 1.0f)));
    bgfx::setUniform(m_cubeOpacity,
                     glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_cubeColor,
                     glm::value_ptr(glm::vec4(data.objectColor, 1.0f)));
    bgfx::submit(0, m_cubeProgram);
}

void BgfxRenderer::drawAabb(const AabbRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_cubeProgram) ||
        !bgfx::isValid(m_aabbBuffer))
        return;

    const glm::vec3 size = glm::max(data.relativeMax - data.relativeMin,
                                    glm::vec3(1e-5f));
    const glm::vec3 center = (data.relativeMax + data.relativeMin) * 0.5f;
    const glm::mat4 model = glm::scale(glm::mat4(1.0f), size);
    const glm::mat4 projection = projectionForDirect3D(data.projection);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LEQUAL |
                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_PT_LINES |
                   BGFX_STATE_LINEAA | BGFX_STATE_MSAA);
    bgfx::setTransform(glm::value_ptr(model));
    bgfx::setVertexBuffer(0, m_aabbBuffer);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_cubeRelativePosition,
                     glm::value_ptr(glm::vec4(center, 1.0f)));
    bgfx::setUniform(m_cubeOpacity,
                     glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_cubeColor,
                     glm::value_ptr(glm::vec4(data.color, 1.0f)));
    bgfx::submit(0, m_cubeProgram);
}

void BgfxRenderer::drawWorldLine(const WorldLineRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_lineProgram))
        return;

    const std::array<float, 2> vertices{0.0f, 1.0f};
    bgfx::update(m_lineBuffer, 0, bgfx::copy(vertices.data(), sizeof(vertices)));
    const glm::mat4 projection = projectionForDirect3D(data.projection);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                   BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA |
                   BGFX_STATE_PT_LINES | BGFX_STATE_LINEAA);
    bgfx::setVertexBuffer(0, m_lineBuffer);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_lineStart,
                     glm::value_ptr(glm::vec4(data.relativeStart, 1.0f)));
    bgfx::setUniform(m_lineEnd,
                     glm::value_ptr(glm::vec4(data.relativeEnd, 1.0f)));
    bgfx::submit(0, m_lineProgram);
}

void BgfxRenderer::drawTargetPoint(const TargetPointRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_pointProgram))
        return;

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const glm::vec4 clip = projection * data.view *
                           glm::vec4(data.relativePosition, 1.0f);
    if (!(clip.w > 0.0f))
        return;

    const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
    // The target is a screen-space quad standing in for GL_POINTS. D3D's
    // 24-bit depth buffer quantizes coincident reference depths at this far
    // plane. Bias the perspective target forward just enough to restore GL's
    // coincident ordering without passing the cube's front surface. Ortho
    // depth is linear and must match the reference exactly.
    const float depthBias = data.isOrtho > 0.5f ? 0.0f : -0.0001f;
    const glm::vec4 position(
        ndc,
        clip.z / clip.w + depthBias,
        1.0f);
    const glm::vec4 pointSize(data.pointSize * 2.0f / float(m_width),
                              data.pointSize * 2.0f / float(m_height),
                              0.0f, 0.0f);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_WRITE_Z |
                   BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA);
    bgfx::setVertexBuffer(0, m_pointBuffer);
    bgfx::setUniform(m_pointPosition, glm::value_ptr(position));
    bgfx::setUniform(m_pointSize, glm::value_ptr(pointSize));
    bgfx::setUniform(m_pointColor,
                     glm::value_ptr(glm::vec4(data.color, 1.0f)));
    bgfx::submit(0, m_pointProgram);
}

bool BgfxRenderer::createRenderResources()
{
    const auto createShader = [](const uint8_t *data, uint32_t size,
                                 const char *name) {
        const bgfx::ShaderHandle handle = bgfx::createShader(bgfx::copy(data, size));
        if (bgfx::isValid(handle))
            bgfx::setName(handle, name);
        return handle;
    };

    const bgfx::ShaderHandle gridVertex = createShader(
        GridShaders::vertex_dx11, sizeof(GridShaders::vertex_dx11), "grid_vs");
    const bgfx::ShaderHandle gridFragment = createShader(
        GridShaders::frag_dx11, sizeof(GridShaders::frag_dx11), "grid_fs");
    m_gridProgram = bgfx::createProgram(gridVertex, gridFragment, true);

    const bgfx::ShaderHandle cubeVertex = createShader(
        CubeShaders::vertex_dx11, sizeof(CubeShaders::vertex_dx11), "cube_vs");
    const bgfx::ShaderHandle cubeFragment = createShader(
        CubeShaders::frag_dx11, sizeof(CubeShaders::frag_dx11), "cube_fs");
    m_cubeProgram = bgfx::createProgram(cubeVertex, cubeFragment, true);

    const bgfx::ShaderHandle lineVertex = createShader(
        LineShaders::vertex_dx11, sizeof(LineShaders::vertex_dx11), "line_vs");
    const bgfx::ShaderHandle lineFragment = createShader(
        LineShaders::frag_dx11, sizeof(LineShaders::frag_dx11), "line_fs");
    m_lineProgram = bgfx::createProgram(lineVertex, lineFragment, true);

    const bgfx::ShaderHandle pointVertex = createShader(
        PointShaders::vertex_dx11, sizeof(PointShaders::vertex_dx11), "point_vs");
    const bgfx::ShaderHandle pointFragment = createShader(
        PointShaders::frag_dx11, sizeof(PointShaders::frag_dx11), "point_fs");
    m_pointProgram = bgfx::createProgram(pointVertex, pointFragment, true);

    bool ready = bgfx::isValid(m_gridProgram) && bgfx::isValid(m_cubeProgram) &&
                 bgfx::isValid(m_lineProgram) && bgfx::isValid(m_pointProgram);
    if (!ready)
        std::cerr << "Failed to create one or more bgfx shader programs." << std::endl;

    if (ready)
    {
        m_gridInvViewProj = createUniformHandle("uInvViewProj", bgfx::UniformType::Mat4);
        m_gridViewProj = createUniformHandle("uViewProj", bgfx::UniformType::Mat4);
        m_gridCamFront = createUniformHandle("uCamFront", bgfx::UniformType::Vec4);
        m_gridOrthoPlaneCenter = createUniformHandle("uOrthoPlaneCenter", bgfx::UniformType::Vec4);
        m_gridOrthoRight = createUniformHandle("uOrthoRight", bgfx::UniformType::Vec4);
        m_gridOrthoUp = createUniformHandle("uOrthoUp", bgfx::UniformType::Vec4);
        m_gridOriginRelative = createUniformHandle("uOriginRelative", bgfx::UniformType::Vec4);
        m_gridPlaneNormal = createUniformHandle("uPlaneNormal", bgfx::UniformType::Vec4);
        m_gridPlaneTangentU = createUniformHandle("uPlaneTangentU", bgfx::UniformType::Vec4);
        m_gridPlaneTangentV = createUniformHandle("uPlaneTangentV", bgfx::UniformType::Vec4);
        m_gridAxisColorU = createUniformHandle("uAxisColorU", bgfx::UniformType::Vec4);
        m_gridAxisColorV = createUniformHandle("uAxisColorV", bgfx::UniformType::Vec4);
        m_gridStartAxisOrigin = createUniformHandle("uStartAxisOrigin", bgfx::UniformType::Vec4);
        m_gridStartAxisDirection = createUniformHandle("uStartAxisDirection", bgfx::UniformType::Vec4);
        m_gridStartAxisVisible = createUniformHandle("uStartAxisVisible", bgfx::UniformType::Vec4);
        m_gridStartAxisLine = createUniformHandle("uStartAxisLine", bgfx::UniformType::Vec4);
        m_gridAxisOriginGridRelative = createUniformHandle(
            "uAxisOriginGridRelative", bgfx::UniformType::Vec4);
        m_gridAxisLineX = createUniformHandle("uAxisLineX", bgfx::UniformType::Vec4);
        m_gridAxisLineZ = createUniformHandle("uAxisLineZ", bgfx::UniformType::Vec4);
        m_gridIsOrtho = createUniformHandle("uIsOrtho", bgfx::UniformType::Vec4);
        m_gridGroundRelativeY = createUniformHandle("uGroundRelativeY", bgfx::UniformType::Vec4);
        m_gridStep = createUniformHandle("uStep", bgfx::UniformType::Vec4);
        m_gridAxisVisible = createUniformHandle("uAxisVisible", bgfx::UniformType::Vec4);
        m_gridScreenHeight = createUniformHandle("uScreenHeight", bgfx::UniformType::Vec4);
        m_gridScreenWidth = createUniformHandle("uScreenWidth", bgfx::UniformType::Vec4);
        m_gridColorMajor = createUniformHandle("uGridColorMajor", bgfx::UniformType::Vec4);
        m_gridColorMinor = createUniformHandle("uGridColorMinor", bgfx::UniformType::Vec4);
        m_gridOpacity = createUniformHandle("uGridOpacity", bgfx::UniformType::Vec4);
        m_gridOrthoPlaneValid = createUniformHandle("uOrthoPlaneValid", bgfx::UniformType::Vec4);
        m_view = createUniformHandle("uView", bgfx::UniformType::Mat4);
        m_projection = createUniformHandle("projection", bgfx::UniformType::Mat4);
        m_cubeRelativePosition = createUniformHandle("uModelRelativePosition", bgfx::UniformType::Vec4);
        m_cubeOpacity = createUniformHandle("uCubeOpacity", bgfx::UniformType::Vec4);
        m_cubeColor = createUniformHandle("uObjectColor", bgfx::UniformType::Vec4);
        m_lineStart = createUniformHandle("uRelativeStart", bgfx::UniformType::Vec4);
        m_lineEnd = createUniformHandle("uRelativeEnd", bgfx::UniformType::Vec4);
        m_pointPosition = createUniformHandle("uRelativePosition", bgfx::UniformType::Vec4);
        m_pointSize = createUniformHandle("uPointSize", bgfx::UniformType::Vec4);
        m_pointColor = createUniformHandle("uColor", bgfx::UniformType::Vec4);

        ready = bgfx::isValid(m_gridInvViewProj) &&
                bgfx::isValid(m_gridViewProj) &&
                bgfx::isValid(m_gridCamFront) &&
                bgfx::isValid(m_gridOrthoPlaneCenter) &&
                bgfx::isValid(m_gridOrthoRight) &&
                bgfx::isValid(m_gridOrthoUp) &&
                bgfx::isValid(m_gridOriginRelative) &&
                bgfx::isValid(m_gridPlaneNormal) &&
                bgfx::isValid(m_gridPlaneTangentU) &&
                bgfx::isValid(m_gridPlaneTangentV) &&
                bgfx::isValid(m_gridAxisColorU) &&
                bgfx::isValid(m_gridAxisColorV) &&
                bgfx::isValid(m_gridStartAxisOrigin) &&
                bgfx::isValid(m_gridStartAxisDirection) &&
                bgfx::isValid(m_gridStartAxisVisible) &&
                bgfx::isValid(m_gridStartAxisLine) &&
                bgfx::isValid(m_gridAxisOriginGridRelative) &&
                bgfx::isValid(m_gridAxisLineX) &&
                bgfx::isValid(m_gridAxisLineZ) &&
                bgfx::isValid(m_gridIsOrtho) &&
                bgfx::isValid(m_gridGroundRelativeY) &&
                bgfx::isValid(m_gridStep) &&
                bgfx::isValid(m_gridAxisVisible) &&
                bgfx::isValid(m_gridScreenHeight) &&
                bgfx::isValid(m_gridScreenWidth) &&
                bgfx::isValid(m_gridColorMajor) &&
                bgfx::isValid(m_gridColorMinor) &&
                bgfx::isValid(m_gridOpacity) &&
                bgfx::isValid(m_gridOrthoPlaneValid) &&
                bgfx::isValid(m_view) && bgfx::isValid(m_projection) &&
                bgfx::isValid(m_cubeRelativePosition) &&
                bgfx::isValid(m_cubeOpacity) && bgfx::isValid(m_cubeColor) &&
                bgfx::isValid(m_lineStart) && bgfx::isValid(m_lineEnd) &&
                bgfx::isValid(m_pointPosition) && bgfx::isValid(m_pointSize) &&
                bgfx::isValid(m_pointColor);
    }

    if (ready)
    {
        bgfx::VertexLayout gridLayout;
        gridLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
        const std::array<float, 6> fullscreenTriangle{-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
        m_gridBuffer = bgfx::createVertexBuffer(
            bgfx::copy(fullscreenTriangle.data(), sizeof(fullscreenTriangle)), gridLayout);

        bgfx::VertexLayout cubeLayout;
        cubeLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .end();
        const std::array<CubeVertex, 36> cubeVertices = makeCubeVertices();
        m_cubeBuffer = bgfx::createVertexBuffer(
            bgfx::copy(cubeVertices.data(), sizeof(cubeVertices)), cubeLayout);
        const std::array<CubeVertex, 24> aabbVertices = makeCubeEdgeVertices();
        m_aabbBuffer = bgfx::createVertexBuffer(
            bgfx::copy(aabbVertices.data(), sizeof(aabbVertices)), cubeLayout);

        bgfx::VertexLayout lineLayout;
        lineLayout.begin()
            .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
            .end();
        m_lineBuffer = bgfx::createDynamicVertexBuffer(2, lineLayout);

        bgfx::VertexLayout pointLayout;
        pointLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
        const std::array<float, 6> pointDisc{-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
        m_pointBuffer = bgfx::createVertexBuffer(
            bgfx::copy(pointDisc.data(), sizeof(pointDisc)), pointLayout);

        ready = bgfx::isValid(m_gridBuffer) && bgfx::isValid(m_cubeBuffer) &&
                bgfx::isValid(m_aabbBuffer) &&
                bgfx::isValid(m_lineBuffer) && bgfx::isValid(m_pointBuffer);
        if (!ready)
            std::cerr << "Failed to create bgfx vertex buffers." << std::endl;
    }

    return ready;
}

} // namespace rendering
