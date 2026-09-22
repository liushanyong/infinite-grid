#include "rendering/BgfxRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

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
namespace MeshInstanceShaders
{
#include "shaders/centerAnchorInstance/vertex.h"
#include "shaders/centerAnchorInstance/frag.h"
} // namespace MeshInstanceShaders
namespace PointInstanceShaders
{
#include "shaders/targetPointInstance/vertex.h"
#include "shaders/targetPointInstance/frag.h"
} // namespace PointInstanceShaders

namespace CadAlgorithmShaders
{
#include "shaders/cadAlgorithm/vs_cad_instance.h"
#include "shaders/cadAlgorithm/fs_cad_styles.h"
} // namespace CadAlgorithmShaders

namespace PrimPolylineShaders
{
#include "shaders/cadPrimitives/polyline/vs_polyline.h"
#include "shaders/cadPrimitives/polyline/fs_polyline.h"
} // namespace PrimPolylineShaders

namespace PrimFilledShaders
{
#include "shaders/cadPrimitives/filled/vs_filled.h"
#include "shaders/cadPrimitives/filled/fs_filled.h"
} // namespace PrimFilledShaders

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
    // bgfx reports whether the backend expects NDC depth in [-1, 1].
    // OpenGL-style backends keep homogeneousDepth; D3D, Vulkan and Metal use
    // [0, 1], so remap z/w with z' = 0.5 * z + 0.5 * w.
    // In glm (column-major) mat[col][row], the '0.5 * w' term that lands on
    // the z-row lives at column 3, row 2 (i.e. depthRange[3][2]).
    const bgfx::Caps *caps = bgfx::getCaps();
    if (!caps || caps->homogeneousDepth)
        return projection;

    glm::mat4 depthRange(1.0f);
    depthRange[2][2] = 0.5f;
    depthRange[3][2] = 0.5f;
    return depthRange * projection;
}

float normalizedLogDepth(float viewDepth, const glm::vec4 &logDepth)
{
    if (logDepth.x < 0.5f)
        return viewDepth;

    const float depth = std::max(viewDepth, logDepth.y);
    const float numerator = std::log2(std::max(depth / logDepth.y, 1.0f));
    const float denominator =
        std::log2(std::max(logDepth.z / logDepth.y, 1.000001f));
    return std::clamp(numerator / denominator, 0.0f, 1.0f);
}

std::array<float, 4> packVec4(const glm::vec3 &value, float extra)
{
    return {value.x, value.y, value.z, extra};
}

// ---------------------------------------------------------------------------
// Procedural test meshes (unit-sized, flat normals, CCW front faces to
// match the cube buffer + BGFX_STATE_CULL_CW convention).
// ---------------------------------------------------------------------------
void appendTriangle(std::vector<float> &out, glm::vec3 a, glm::vec3 b,
                    glm::vec3 c, const glm::vec3 &outward)
{
    const glm::vec3 crossAB = glm::cross(b - a, c - a);
    if (glm::dot(crossAB, crossAB) < 1e-12f)
        return; // degenerate (sphere pole quads collapse to triangles)
    if (glm::dot(crossAB, outward) < 0.0f)
        std::swap(b, c);
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    for (const glm::vec3 *vertex : {&a, &b, &c})
    {
        out.push_back(vertex->x);
        out.push_back(vertex->y);
        out.push_back(vertex->z);
        out.push_back(normal.x);
        out.push_back(normal.y);
        out.push_back(normal.z);
    }
}

std::vector<float> makeSphereMesh()
{
    std::vector<float> out;
    constexpr int kStacks = 12;
    constexpr int kSlices = 16;
    constexpr float kRadius = 0.5f;
    out.reserve(static_cast<size_t>(kStacks * kSlices * 2) * 18);
    auto point = [](int stack, int slice) {
        const float v = glm::pi<float>() * static_cast<float>(stack) /
                        static_cast<float>(kStacks);
        const float u = 2.0f * glm::pi<float>() * static_cast<float>(slice) /
                        static_cast<float>(kSlices);
        return glm::vec3(kRadius * std::sin(v) * std::cos(u),
                         kRadius * std::cos(v),
                         kRadius * std::sin(v) * std::sin(u));
    };
    for (int stack = 0; stack < kStacks; ++stack)
        for (int slice = 0; slice < kSlices; ++slice)
        {
            const glm::vec3 a = point(stack, slice);
            const glm::vec3 b = point(stack + 1, slice);
            const glm::vec3 c = point(stack + 1, slice + 1);
            const glm::vec3 d = point(stack, slice + 1);
            appendTriangle(out, a, b, c, a);
            appendTriangle(out, a, c, d, a);
        }
    return out;
}

std::vector<float> makeConeMesh()
{
    std::vector<float> out;
    constexpr int kSegments = 16;
    constexpr float kRadius = 0.5f;
    constexpr float kHalfHeight = 0.5f;
    out.reserve(static_cast<size_t>(kSegments * 2) * 18);
    const glm::vec3 apex(0.0f, kHalfHeight, 0.0f);
    auto rim = [](int segment) {
        const float phi = 2.0f * glm::pi<float>() * static_cast<float>(segment) /
                          static_cast<float>(kSegments);
        return glm::vec3(kRadius * std::cos(phi), -kHalfHeight,
                         kRadius * std::sin(phi));
    };
    for (int segment = 0; segment < kSegments; ++segment)
    {
        const glm::vec3 p0 = rim(segment);
        const glm::vec3 p1 = rim(segment + 1);
        const glm::vec3 mid = 0.5f * (p0 + p1);
        appendTriangle(out, apex, p0, p1, glm::vec3(mid.x, 0.0f, mid.z));
        appendTriangle(out, glm::vec3(0.0f, -kHalfHeight, 0.0f), p0, p1,
                       glm::vec3(0.0f, -1.0f, 0.0f));
    }
    return out;
}

std::vector<float> makeTorusMesh()
{
    std::vector<float> out;
    constexpr int kMajor = 16;
    constexpr int kMinor = 8;
    constexpr float kMajorRadius = 0.325f;
    constexpr float kMinorRadius = 0.175f;
    out.reserve(static_cast<size_t>(kMajor * kMinor * 2) * 18);
    auto point = [](int major, int minor, glm::vec3 *normalOut) {
        const float u = 2.0f * glm::pi<float>() * static_cast<float>(major) /
                        static_cast<float>(kMajor);
        const float v = 2.0f * glm::pi<float>() * static_cast<float>(minor) /
                        static_cast<float>(kMinor);
        const float cu = std::cos(u);
        const float su = std::sin(u);
        const float cv = std::cos(v);
        const float sv = std::sin(v);
        if (normalOut)
            *normalOut = glm::vec3(cv * cu, sv, cv * su);
        return glm::vec3((kMajorRadius + kMinorRadius * cv) * cu,
                         kMinorRadius * sv,
                         (kMajorRadius + kMinorRadius * cv) * su);
    };
    for (int major = 0; major < kMajor; ++major)
        for (int minor = 0; minor < kMinor; ++minor)
        {
            glm::vec3 normalA;
            const glm::vec3 a = point(major, minor, &normalA);
            const glm::vec3 b = point(major + 1, minor, nullptr);
            const glm::vec3 c = point(major + 1, minor + 1, nullptr);
            const glm::vec3 d = point(major, minor + 1, nullptr);
            appendTriangle(out, a, b, c, normalA);
            appendTriangle(out, a, c, d, normalA);
        }
    return out;
}

bgfx::VertexBufferHandle createMeshBuffer(const std::vector<float> &vertices)
{
    if (vertices.empty())
        return BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(),
                   static_cast<uint32_t>(vertices.size() * sizeof(float))),
        layout);
}


bgfx::VertexBufferHandle createCadMeshBuffer(const std::vector<float> &vertices)
{
    if (vertices.empty() || vertices.size() % 6 != 0)
        return BGFX_INVALID_HANDLE;

    // The CAD algorithm shader uses a barycentric coordinate for wireframe and
    // unified shading.  Mesh generators emit triangle soup, so every three
    // vertices form one triangle and receive (1,0,0), (0,1,0), and (0,0,1).
    const size_t vertexCount = vertices.size() / 6;
    std::vector<float> cadVertices;
    cadVertices.reserve(vertexCount * 9);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        const size_t source = i * 6;
        cadVertices.insert(cadVertices.end(), vertices.begin() + source,
                           vertices.begin() + source + 6);
        switch (i % 3)
        {
        case 0:
            cadVertices.insert(cadVertices.end(), {1.0f, 0.0f, 0.0f});
            break;
        case 1:
            cadVertices.insert(cadVertices.end(), {0.0f, 1.0f, 0.0f});
            break;
        default:
            cadVertices.insert(cadVertices.end(), {0.0f, 0.0f, 1.0f});
            break;
        }
    }

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 3, bgfx::AttribType::Float)
        .end();
    return bgfx::createVertexBuffer(
        bgfx::copy(cadVertices.data(),
                   static_cast<uint32_t>(cadVertices.size() * sizeof(float))),
        layout);
}

bgfx::VertexBufferHandle createCadCubeBuffer(const std::array<CubeVertex, 36> &vertices)
{
    std::vector<float> source;
    source.reserve(vertices.size() * 6);
    for (const CubeVertex &vertex : vertices)
    {
        source.push_back(vertex.position.x);
        source.push_back(vertex.position.y);
        source.push_back(vertex.position.z);
        source.push_back(vertex.normal.x);
        source.push_back(vertex.normal.y);
        source.push_back(vertex.normal.z);
    }
    return createCadMeshBuffer(source);
}

// Shader headers contain one binary per supported API.  bgfx validates the
// binary type at shader creation, so select the array using the renderer that
// bgfx actually initialized (which may differ from the requested API only if
// a future fallback path is added).
struct ShaderBinary
{
    const uint8_t *data;
    uint32_t size;
};

template <typename T>
ShaderBinary selectShaderBinary(
    bgfx::RendererType::Enum renderer,
    const T *d3d11, size_t d3d11Size,
    const T *d3d12, size_t d3d12Size,
    const T *glsl, size_t glslSize,
    const T *spirv, size_t spirvSize)
{
    switch (renderer)
    {
    case bgfx::RendererType::Direct3D12:
        return {d3d12, static_cast<uint32_t>(d3d12Size)};
    case bgfx::RendererType::OpenGL:
    case bgfx::RendererType::OpenGLES:
        return {glsl, static_cast<uint32_t>(glslSize)};
    case bgfx::RendererType::Vulkan:
        return {spirv, static_cast<uint32_t>(spirvSize)};
    case bgfx::RendererType::Direct3D11:
    default:
        return {d3d11, static_cast<uint32_t>(d3d11Size)};
    }
}

#define SELECT_SHADER_BINARY(NAMESPACE, NAME)                                  \
    selectShaderBinary(                                                        \
        bgfx::getRendererType(),                                               \
        NAMESPACE::NAME##_dx11, sizeof(NAMESPACE::NAME##_dx11),                \
        NAMESPACE::NAME##_dx12, sizeof(NAMESPACE::NAME##_dx12),                \
        NAMESPACE::NAME##_glsl, sizeof(NAMESPACE::NAME##_glsl),                \
        NAMESPACE::NAME##_spv, sizeof(NAMESPACE::NAME##_spv))

} // namespace


BgfxRenderer::BgfxRenderer(GraphicsApi api)
    : m_api(api)
{
}

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
    switch (m_api)
    {
    case GraphicsApi::Direct3D11:
        init.type = bgfx::RendererType::Direct3D11;
        break;
    case GraphicsApi::Direct3D12:
        init.type = bgfx::RendererType::Direct3D12;
        break;
    case GraphicsApi::OpenGL:
        init.type = bgfx::RendererType::OpenGL;
        break;
    case GraphicsApi::Vulkan:
        init.type = bgfx::RendererType::Vulkan;
        break;
    case GraphicsApi::Auto:
    default:
        init.type = bgfx::RendererType::Direct3D11;
        break;
    }
    init.platformData.nwh = hwnd;
    init.resolution.width = m_width;
    init.resolution.height = m_height;
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4;
    init.debug = std::getenv("GRID_GPU_DEBUG") != nullptr;

    if (!bgfx::init(init))
    {
        std::cerr << "Failed to initialize bgfx D3D11 renderer." << std::endl;
        return false;
    }

    bgfx::setViewMode(0, bgfx::ViewMode::Sequential);
    // dbgTextPrintf output is only drawn when this debug flag is set
    // (see the "else if (m_debug & BGFX_DEBUG_TEXT)" branch of the
    // backend submit); enable it so the FPS overlay is visible.
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    m_initialized = true;
    if (!createRenderResources())
    {
        shutdown();
        return false;
    }

    std::cout << "bgfx " << bgfx::getRendererName(bgfx::getRendererType())
              << " renderer initialized." << std::endl;
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
    if (bgfx::isValid(m_meshInstanceProgram))
        bgfx::destroy(m_meshInstanceProgram);
    if (bgfx::isValid(m_pointInstanceProgram))
        bgfx::destroy(m_pointInstanceProgram);
    if (bgfx::isValid(m_cadAlgorithmProgram))
        bgfx::destroy(m_cadAlgorithmProgram);
    if (bgfx::isValid(m_polylineProgram))
        bgfx::destroy(m_polylineProgram);
    if (bgfx::isValid(m_fillProgram))
        bgfx::destroy(m_fillProgram);
    m_cubeProgram = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cubeBuffer))
        bgfx::destroy(m_cubeBuffer);
    if (bgfx::isValid(m_cadCubeBuffer))
        bgfx::destroy(m_cadCubeBuffer);
    if (bgfx::isValid(m_cadSphereBuffer))
        bgfx::destroy(m_cadSphereBuffer);
    m_cadSphereBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cadConeBuffer))
        bgfx::destroy(m_cadConeBuffer);
    m_cadConeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_cadTorusBuffer))
        bgfx::destroy(m_cadTorusBuffer);
    m_cadTorusBuffer = BGFX_INVALID_HANDLE;
    m_cubeBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_sphereBuffer))
        bgfx::destroy(m_sphereBuffer);
    m_sphereBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_coneBuffer))
        bgfx::destroy(m_coneBuffer);
    m_coneBuffer = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(m_torusBuffer))
        bgfx::destroy(m_torusBuffer);
    m_torusBuffer = BGFX_INVALID_HANDLE;
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

    destroyUniform(m_cadView);
    destroyUniform(m_cadProjection);
    destroyUniform(m_cadCameraPos);
    destroyUniform(m_cadBaseColor);
    destroyUniform(m_cadLightDir);
    destroyUniform(m_cadStyleParams);
    destroyUniform(m_cadWireframeColor);
    destroyUniform(m_cadStrokeParams);
    destroyUniform(m_primParams);
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
    destroyUniform(m_logDepth);
    destroyUniform(m_view);
    destroyUniform(m_projection);
    destroyUniform(m_cubeRelativePosition);
    destroyUniform(m_cubeOpacity);
    destroyUniform(m_cubeColor);
    destroyUniform(m_lineStart);
    destroyUniform(m_lineEnd);
    destroyUniform(m_lineColor);
    destroyUniform(m_lineWidth);
    destroyUniform(m_lineDepthBias);
    destroyUniform(m_pointPosition);
    destroyUniform(m_pointSize);
    destroyUniform(m_pointColor);

    bgfx::shutdown();
    m_initialized = false;
    m_window = nullptr;
}

bool BgfxRenderer::beginFrame(const glm::vec4 &clearColor)
{
    if (!m_initialized)
        return false;

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    width = std::max(1, width);
    height = std::max(1, height);
    if (width != m_width || height != m_height)
    {
        // Rendering now targets the MSAA backbuffer directly; there is no
        // offscreen scene frame buffer to recreate on resize.
        bgfx::reset(static_cast<uint16_t>(width),
                    static_cast<uint16_t>(height),
                    BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
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
    return true;
}


void BgfxRenderer::endFrame()
{
    if (!m_initialized)
        return;


    // FPS overlay: bgfx debug text renders as the last step of the
    // frame.
    ++m_frameCount;
    const uint32_t now = SDL_GetTicks();
    if (m_fpsLastTick == 0)
        m_fpsLastTick = now;
    const uint32_t elapsed = now - m_fpsLastTick;
    if (elapsed >= 500)
    {
        m_fps = static_cast<float>(m_frameCount) * 1000.0f /
                static_cast<float>(elapsed);
        m_frameCount = 0;
        m_fpsLastTick = now;
    }
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(1, 1, 0x0f, "FPS: %.1f (%.1f ms)",
                        m_fps, m_fps > 0.0f ? 1000.0f / m_fps : 0.0f);

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
    // The grid is translucent, but its line/axis pixels must occlude
    // geometry behind the grid plane. Fragments with zero alpha are already
    // discarded, so only visible line pixels write depth; the gaps between
    // lines stay transparent for later passes.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LEQUAL |
                   BGFX_STATE_BLEND_ALPHA |
                   BGFX_STATE_WRITE_Z);
    bgfx::setVertexBuffer(0, m_pointBuffer); // Grid and target-point share the same fullscreen-triangle layout/data.
    bgfx::setUniform(m_gridInvViewProj, glm::value_ptr(data.invViewProj));
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
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
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
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
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_CULL_CW |
                           BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
    bgfx::setState(data.opacity >= 0.999f ? state | BGFX_STATE_WRITE_Z : state);
    bgfx::setTransform(glm::value_ptr(data.model));
    bgfx::VertexBufferHandle meshBuffer = m_cubeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere:
        meshBuffer = m_sphereBuffer;
        break;
    case rendering::MeshType::Cone:
        meshBuffer = m_coneBuffer;
        break;
    case rendering::MeshType::Torus:
        meshBuffer = m_torusBuffer;
        break;
    case rendering::MeshType::Cube:
        break;
    }
    bgfx::setVertexBuffer(0, meshBuffer);
    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_cubeRelativePosition,
                     glm::value_ptr(glm::vec4(data.modelRelativePosition, 1.0f)));
    bgfx::setUniform(m_cubeOpacity,
                     glm::value_ptr(glm::vec4(data.opacity, 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_cubeColor,
                     glm::value_ptr(glm::vec4(data.objectColor, 1.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    bgfx::submit(0, m_cubeProgram);
}


void BgfxRenderer::drawMeshInstances(const MeshInstancesRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_meshInstanceProgram) ||
        !data.instances || data.instanceCount == 0)
        return;

    bgfx::VertexBufferHandle meshBuffer = m_cubeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere: meshBuffer = m_sphereBuffer; break;
    case rendering::MeshType::Cone:   meshBuffer = m_coneBuffer;   break;
    case rendering::MeshType::Torus:  meshBuffer = m_torusBuffer;  break;
    case rendering::MeshType::Cube:   break;
    }

    constexpr uint16_t kStride = 64;
    static_assert(sizeof(MeshInstance) == kStride,
                  "MeshInstance must match the GPU instance stride");
    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_LEQUAL |
                           BGFX_STATE_CULL_CW | BGFX_STATE_MSAA |
                           (data.opaque ? BGFX_STATE_WRITE_Z :
                                          BGFX_STATE_BLEND_ALPHA);

    bgfx::setUniform(m_view, glm::value_ptr(data.view));
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));

    uint32_t first = 0;
    while (first < data.instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            data.instanceCount - first, kStride);
        if (available == 0)
            break;
        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, available, kStride);
        auto *gpu = reinterpret_cast<MeshInstance *>(idb.data);
        std::memcpy(gpu, data.instances + first,
                    sizeof(MeshInstance) * idb.num);

        bgfx::setState(state);
        bgfx::setVertexBuffer(0, meshBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::submit(0, m_meshInstanceProgram);
        first += idb.num;
    }
}

void BgfxRenderer::drawTargetPointInstances(
    const TargetPointInstancesRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_pointInstanceProgram) ||
        !data.instances || data.instanceCount == 0)
        return;
    constexpr uint16_t kStride = 48;
    struct PointInstanceGpu
    {
        glm::vec4 positionDepth;
        glm::vec4 screenSize;
        glm::vec4 colorOpacity;
    };
    static_assert(sizeof(PointInstanceGpu) == kStride,
                  "Point instance must match the GPU instance stride");

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const glm::vec2 pixelSizeNdc(
        data.pointSize * 2.0f / float(m_width),
        data.pointSize * 2.0f / float(m_height));
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_LEQUAL |
                           BGFX_STATE_BLEND_ALPHA;

    uint32_t first = 0;
    while (first < data.instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            data.instanceCount - first, kStride);
        if (available == 0)
            break;
        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, available, kStride);
        auto *gpu = reinterpret_cast<PointInstanceGpu *>(idb.data);

        uint32_t written = 0;
        for (uint32_t i = 0; i < idb.num; ++i)
        {
            const TargetPointInstance &input = data.instances[first + i];
            const glm::vec4 clip = projection * data.view *
                                   glm::vec4(input.relativePosition, 1.0f);
            if (!(clip.w > 0.0f))
                continue;
            const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
            const glm::vec4 viewPosition = data.view *
                glm::vec4(input.relativePosition, 1.0f);
            float depth = clip.z / clip.w;
            if (data.isOrtho <= 0.5f)
            {
                const float viewDepth = -viewPosition.z;
                const float biasWorld =
                    std::max(0.5f * data.pixelSizeWorld, 0.01f);
                depth = normalizedLogDepth(
                    std::max(viewDepth - biasWorld, data.logDepth.y),
                    data.logDepth);
            }

            gpu[written].positionDepth = glm::vec4(ndc, depth, 1.0f);
            gpu[written].screenSize = glm::vec4(pixelSizeNdc, 0.0f, 0.0f);
            gpu[written].colorOpacity = glm::vec4(input.color, 1.0f);
            ++written;
        }

        if (written == 0)
        {
            first += idb.num;
            continue;
        }
        idb.num = written;
        idb.size = uint32_t(written) * kStride;

        bgfx::setState(state);
        bgfx::setVertexBuffer(0, m_pointBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::submit(0, m_pointInstanceProgram);
        first += idb.num;
    }
}

void BgfxRenderer::drawCadAlgorithmDemo(const CadAlgorithmDemoRenderData &data)
{
    bgfx::VertexBufferHandle meshBuffer = m_cadCubeBuffer;
    switch (data.mesh)
    {
    case rendering::MeshType::Sphere: meshBuffer = m_cadSphereBuffer; break;
    case rendering::MeshType::Cone:   meshBuffer = m_cadConeBuffer;   break;
    case rendering::MeshType::Torus:  meshBuffer = m_cadTorusBuffer;  break;
    case rendering::MeshType::Cube:   break;
    }

    if (!m_initialized || !bgfx::isValid(m_cadAlgorithmProgram) ||
        !bgfx::isValid(meshBuffer) || !data.instances ||
        data.instanceCount == 0)
        return;

    constexpr uint16_t kStride = 64;
    static_assert(sizeof(MeshInstance) == kStride,
                  "MeshInstance must match the CAD algorithm demo stride");

    const glm::mat4 projection = projectionForDirect3D(data.projection);
    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_DEPTH_TEST_LEQUAL |
                           BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

    bgfx::setUniform(m_cadView, glm::value_ptr(data.view));
    bgfx::setUniform(m_cadProjection, glm::value_ptr(projection));
    bgfx::setUniform(m_cadCameraPos, glm::value_ptr(glm::vec4(data.cameraPos, 1.0f)));
    bgfx::setUniform(m_cadBaseColor, glm::value_ptr(glm::vec4(data.baseColor, 1.0f)));
    bgfx::setUniform(m_cadLightDir, glm::value_ptr(glm::vec4(glm::normalize(data.lightDir), 0.0f)));
    bgfx::setUniform(m_cadStyleParams, glm::value_ptr(glm::vec4(static_cast<float>(data.style), data.metallic, data.roughness, data.transparency)));
    bgfx::setUniform(m_cadWireframeColor, glm::value_ptr(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)));
    bgfx::setUniform(m_cadStrokeParams, glm::value_ptr(glm::vec4(data.strokeWidth, data.strokeDensity, 0.0f, 0.0f)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));

    uint32_t first = 0;
    while (first < data.instanceCount)
    {
        const uint32_t available = bgfx::getAvailInstanceDataBuffer(
            data.instanceCount - first, kStride);
        if (available == 0)
            break;
        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, available, kStride);
        auto *gpu = reinterpret_cast<MeshInstance *>(idb.data);
        std::memcpy(gpu, data.instances + first,
                    sizeof(MeshInstance) * idb.num);

        bgfx::setState(state);
        bgfx::setVertexBuffer(0, meshBuffer);
        bgfx::setInstanceDataBuffer(&idb);
        bgfx::submit(0, m_cadAlgorithmProgram);
        first += idb.num;
    }
}

void BgfxRenderer::drawPolylines(const PolylineRenderData& data) {
    static bool dbg = true;
    if (dbg) { dbg = false;
        printf("[polyline] valid=%d count=%u\n", bgfx::isValid(m_polylineProgram), data.vertexCount);
    }
    if (!bgfx::isValid(m_polylineProgram) || data.vertexCount < 2 || !data.vertices) return;
    if (data.vertexCount > 65535) return; 
 
    bgfx::TransientVertexBuffer tvb;
    const uint32_t vertSize = sizeof(PrimVertex); 
    const uint32_t totalBytes = data.vertexCount * vertSize; 
    if (data.vertexCount > bgfx::getAvailTransientVertexBuffer(data.vertexCount, m_polylineLayout)) return; 
    bgfx::allocTransientVertexBuffer(&tvb, data.vertexCount, m_polylineLayout); 
    memcpy(tvb.data, data.vertices, totalBytes);
 
    glm::mat4 identity = glm::mat4(1.0f); 
    bgfx::setTransform(glm::value_ptr(identity)); 
    bgfx::setVertexBuffer(0, &tvb); 
    bgfx::setUniform(m_view, glm::value_ptr(data.view)); 
    const glm::mat4 proj = projectionForDirect3D(data.projection);
    bgfx::setUniform(m_projection, glm::value_ptr(proj));
    float logDepth[4] = { data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w }; 
    bgfx::setUniform(m_logDepth, logDepth); 
    float prim[4] = { 0.15f, data.edgeSoftness, 0.0f, 0.0f }; 
    bgfx::setUniform(m_primParams, prim);
 
    constexpr uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A 
        | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA; 
    bgfx::setState(state); 
    bgfx::submit(0, m_polylineProgram); 
}
 
void BgfxRenderer::drawFilledTriangles(const FilledTrianglesRenderData& data) {
    static bool dbg = true;
    if (dbg) { dbg = false;
        printf("[filled] valid=%d count=%u\n", bgfx::isValid(m_fillProgram), data.vertexCount);
    } 
    if (!bgfx::isValid(m_fillProgram) || data.vertexCount < 3 || !data.vertices) return; 
    if (data.vertexCount > 65535) return;
 
    bgfx::TransientVertexBuffer tvb; 
    const uint32_t vertSize = sizeof(FillVertex); 
    const uint32_t totalBytes = data.vertexCount * vertSize; 
    if (data.vertexCount > bgfx::getAvailTransientVertexBuffer(data.vertexCount, m_fillLayout)) return; 
    bgfx::allocTransientVertexBuffer(&tvb, data.vertexCount, m_fillLayout); 
    memcpy(tvb.data, data.vertices, totalBytes);
 
    glm::mat4 identity = glm::mat4(1.0f); 
    bgfx::setTransform(glm::value_ptr(identity)); 
    bgfx::setVertexBuffer(0, &tvb); 
    bgfx::setUniform(m_view, glm::value_ptr(data.view)); 
    const glm::mat4 proj = projectionForDirect3D(data.projection);
    bgfx::setUniform(m_projection, glm::value_ptr(proj)); 
    float logDepth[4] = { data.logDepth.x, data.logDepth.y, data.logDepth.z, data.logDepth.w }; 
    bgfx::setUniform(m_logDepth, logDepth);
 
    constexpr uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z 
        | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA; 
    bgfx::setState(state); 
    bgfx::submit(0, m_fillProgram); 
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
                   BGFX_STATE_DEPTH_TEST_LEQUAL |
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
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    bgfx::submit(0, m_cubeProgram);
}

void BgfxRenderer::drawWorldLine(const WorldLineRenderData &data)
{
    if (!m_initialized || !bgfx::isValid(m_lineProgram))
        return;

    // Ribbon quad: two triangles with (t, side) attributes.  The vertex
    // shader expands the segment by lineWidth pixels in NDC, so D3D11 gets
    // wide, MSAA-antialiased lines instead of unsupported 1px line prims.
    const std::array<float, 12> vertices{
        0.0f, -1.0f,  0.0f, 1.0f,  1.0f, -1.0f,
        1.0f, -1.0f,  0.0f, 1.0f,  1.0f,  1.0f,
    };
    bgfx::update(m_lineBuffer, 0, bgfx::copy(vertices.data(), sizeof(vertices)));
    const glm::mat4 projection = projectionForDirect3D(data.projection);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA |
                   BGFX_STATE_MSAA);
    bgfx::setVertexBuffer(0, m_lineBuffer);
    // The reference line can be coplanar with the analytic grid.  Offset
    // only fragment depth (not ribbon geometry): 1% camera distance is safe
    // in log-depth perspective, and the same amount as a normalized ortho
    // offset prevents one-ULP depth comparisons from hiding the ribbon.
    const float cameraDepth = std::abs(data.viewStart.z);
    const float depthBias = std::max(0.01f * cameraDepth, 1.0e-4f);
    const float normalizedBias = std::clamp(
        depthBias / std::max(data.logDepth.z - data.logDepth.y, 1.0f),
        0.0f, 0.1f);
    bgfx::setUniform(m_projection, glm::value_ptr(projection));
    bgfx::setUniform(m_lineStart,
                     glm::value_ptr(glm::vec4(data.viewStart, 1.0f)));
    bgfx::setUniform(m_lineEnd,
                     glm::value_ptr(glm::vec4(data.viewEnd, 1.0f)));
    bgfx::setUniform(m_lineWidth,
                     glm::value_ptr(glm::vec4(data.lineWidth, 0.0f,
                                              0.0f, 0.0f)));
    bgfx::setUniform(m_lineDepthBias,
                     glm::value_ptr(glm::vec4(depthBias, normalizedBias,
                                              0.0f, 0.0f)));
    bgfx::setUniform(m_gridScreenWidth,
                     glm::value_ptr(glm::vec4(static_cast<float>(m_width), 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_gridScreenHeight,
                     glm::value_ptr(glm::vec4(static_cast<float>(m_height), 0.0f, 0.0f, 0.0f)));
    bgfx::setUniform(m_lineColor,
                     glm::value_ptr(glm::vec4(data.color, data.opacity)));
    bgfx::setUniform(m_logDepth, glm::value_ptr(data.logDepth));
    if (std::getenv("GRID_CAMERA_DEBUG"))
    {
        static int debugCalls = 0;
        if (debugCalls < 8)
        {
            ++debugCalls;
            const glm::vec4 clip0 = projection * glm::vec4(data.viewStart, 1.0f);
            const glm::vec4 clip1 = projection * glm::vec4(data.viewEnd, 1.0f);
            std::cout << std::scientific << std::setprecision(6)
                      << "[WLINE] start=" << data.viewStart.x << "," << data.viewStart.y << "," << data.viewStart.z
                      << " end=" << data.viewEnd.x << "," << data.viewEnd.y << "," << data.viewEnd.z
                      << " clip0=(" << clip0.x << "," << clip0.y << "," << clip0.z << "," << clip0.w << ")"
                      << " ndc0=(" << (clip0.w ? clip0.x / clip0.w : 99.0f) << "," << (clip0.w ? clip0.y / clip0.w : 99.0f) << ")"
                      << " ndc1=(" << (clip1.w ? clip1.x / clip1.w : 99.0f) << "," << (clip1.w ? clip1.y / clip1.w : 99.0f) << ")\n";
        }
    }
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
    // The target is a screen-space quad standing in for GL_POINTS. The
    // perspective bias is applied in world units (half a pixel at the
    // point's depth), so it stays sub-pixel at every distance instead of
    // growing with the log-depth mapping at large world coordinates. Ortho
    // depth is linear and must match the reference exactly.
    const glm::vec4 viewPosition =
        data.view * glm::vec4(data.relativePosition, 1.0f);
    float depth = clip.z / clip.w;
    if (data.isOrtho <= 0.5f)
    {
        const float viewDepth = -viewPosition.z;
        const float biasWorld =
            std::max(0.5f * data.pixelSizeWorld, 0.01f);
        depth = normalizedLogDepth(
            std::max(viewDepth - biasWorld, data.logDepth.y), data.logDepth);
    }
    const glm::vec4 position(
        ndc,
        depth,
        1.0f);
    const glm::vec4 pointSize(data.pointSize * 2.0f / float(m_width),
                              data.pointSize * 2.0f / float(m_height),
                              0.0f, 0.0f);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
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

    const auto gridVertexBinary = SELECT_SHADER_BINARY(GridShaders, vertex);
    const auto gridFragmentBinary = SELECT_SHADER_BINARY(GridShaders, frag);
    const bgfx::ShaderHandle gridVertex = createShader(
        gridVertexBinary.data, gridVertexBinary.size, "grid_vs");
    const bgfx::ShaderHandle gridFragment = createShader(
        gridFragmentBinary.data, gridFragmentBinary.size, "grid_fs");
    m_gridProgram = bgfx::createProgram(gridVertex, gridFragment, true);

    const auto cubeVertexBinary = SELECT_SHADER_BINARY(CubeShaders, vertex);
    const auto cubeFragmentBinary = SELECT_SHADER_BINARY(CubeShaders, frag);
    const bgfx::ShaderHandle cubeVertex = createShader(
        cubeVertexBinary.data, cubeVertexBinary.size, "cube_vs");
    const bgfx::ShaderHandle cubeFragment = createShader(
        cubeFragmentBinary.data, cubeFragmentBinary.size, "cube_fs");
    m_cubeProgram = bgfx::createProgram(cubeVertex, cubeFragment, true);

    const auto meshInstanceVertexBinary =
        SELECT_SHADER_BINARY(MeshInstanceShaders, vertex);
    const auto meshInstanceFragmentBinary =
        SELECT_SHADER_BINARY(MeshInstanceShaders, frag);
    const bgfx::ShaderHandle meshInstanceVertex = createShader(
        meshInstanceVertexBinary.data, meshInstanceVertexBinary.size,
        "mesh_instance_vs");
    const bgfx::ShaderHandle meshInstanceFragment = createShader(
        meshInstanceFragmentBinary.data, meshInstanceFragmentBinary.size,
        "mesh_instance_fs");
    m_meshInstanceProgram = bgfx::createProgram(
        meshInstanceVertex, meshInstanceFragment, true);

    const auto lineVertexBinary = SELECT_SHADER_BINARY(LineShaders, vertex);
    const auto lineFragmentBinary = SELECT_SHADER_BINARY(LineShaders, frag);
    const bgfx::ShaderHandle lineVertex = createShader(
        lineVertexBinary.data, lineVertexBinary.size, "line_vs");
    const bgfx::ShaderHandle lineFragment = createShader(
        lineFragmentBinary.data, lineFragmentBinary.size, "line_fs");
    m_lineProgram = bgfx::createProgram(lineVertex, lineFragment, true);

    const auto pointVertexBinary = SELECT_SHADER_BINARY(PointShaders, vertex);
    const auto pointFragmentBinary = SELECT_SHADER_BINARY(PointShaders, frag);
    const bgfx::ShaderHandle pointVertex = createShader(
        pointVertexBinary.data, pointVertexBinary.size, "point_vs");
    const bgfx::ShaderHandle pointFragment = createShader(
        pointFragmentBinary.data, pointFragmentBinary.size, "point_fs");
    m_pointProgram = bgfx::createProgram(pointVertex, pointFragment, true);

    const auto pointInstanceVertexBinary =
        SELECT_SHADER_BINARY(PointInstanceShaders, vertex);
    const auto pointInstanceFragmentBinary =
        SELECT_SHADER_BINARY(PointInstanceShaders, frag);
    const bgfx::ShaderHandle pointInstanceVertex = createShader(
        pointInstanceVertexBinary.data, pointInstanceVertexBinary.size,
        "point_instance_vs");
    const bgfx::ShaderHandle pointInstanceFragment = createShader(
        pointInstanceFragmentBinary.data, pointInstanceFragmentBinary.size,
        "point_instance_fs");
    m_pointInstanceProgram = bgfx::createProgram(
        pointInstanceVertex, pointInstanceFragment, true);

    const auto cadAlgorithmVertexBinary =
        SELECT_SHADER_BINARY(CadAlgorithmShaders, vs_cad_instance);
    const auto cadAlgorithmFragmentBinary =
        SELECT_SHADER_BINARY(CadAlgorithmShaders, fs_cad_styles);
    const bgfx::ShaderHandle cadAlgorithmVertex = createShader(
        cadAlgorithmVertexBinary.data, cadAlgorithmVertexBinary.size,
        "cad_algorithm_vs");
    const bgfx::ShaderHandle cadAlgorithmFragment = createShader(
        cadAlgorithmFragmentBinary.data, cadAlgorithmFragmentBinary.size,
        "cad_algorithm_fs");
    m_cadAlgorithmProgram = bgfx::createProgram(
        cadAlgorithmVertex, cadAlgorithmFragment, true);

    const auto polylineVertexBinary =
        SELECT_SHADER_BINARY(PrimPolylineShaders, vs_polyline);
    const auto polylineFragmentBinary =
        SELECT_SHADER_BINARY(PrimPolylineShaders, fs_polyline);
    const bgfx::ShaderHandle polylineVertex = createShader(
        polylineVertexBinary.data, polylineVertexBinary.size,
        "prim_polyline_vs");
    const bgfx::ShaderHandle polylineFragment = createShader(
        polylineFragmentBinary.data, polylineFragmentBinary.size,
        "prim_polyline_fs");
    m_polylineProgram = bgfx::createProgram(polylineVertex, polylineFragment, true);

    const auto fillVertexBinary =
        SELECT_SHADER_BINARY(PrimFilledShaders, vs_filled);
    const auto fillFragmentBinary =
        SELECT_SHADER_BINARY(PrimFilledShaders, fs_filled);
    const bgfx::ShaderHandle fillVertex = createShader(
        fillVertexBinary.data, fillVertexBinary.size, "prim_fill_vs");
    const bgfx::ShaderHandle fillFragment = createShader(
        fillFragmentBinary.data, fillFragmentBinary.size, "prim_fill_fs");
    m_fillProgram = bgfx::createProgram(fillVertex, fillFragment, true);
    m_polylineLayout.begin() 
    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float) 
    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float) 
    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) 
    .end(); 
m_fillLayout.begin() 
    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float) 
    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float) 
    .end();


    bool ready = bgfx::isValid(m_gridProgram) &&
                 bgfx::isValid(m_meshInstanceProgram) &&
                 bgfx::isValid(m_pointInstanceProgram) &&
                 bgfx::isValid(m_cadAlgorithmProgram) &&
                 bgfx::isValid(m_polylineProgram) &&
                 bgfx::isValid(m_fillProgram) &&
                 bgfx::isValid(m_cubeProgram) &&
                 bgfx::isValid(m_lineProgram) &&
                 bgfx::isValid(m_pointProgram);
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
        m_logDepth = createUniformHandle("uLogDepth", bgfx::UniformType::Vec4);
        m_view = createUniformHandle("uView", bgfx::UniformType::Mat4);
        m_projection = createUniformHandle("projection", bgfx::UniformType::Mat4);
        m_cubeRelativePosition = createUniformHandle("uModelRelativePosition", bgfx::UniformType::Vec4);
        m_cubeOpacity = createUniformHandle("uCubeOpacity", bgfx::UniformType::Vec4);
        m_cubeColor = createUniformHandle("uObjectColor", bgfx::UniformType::Vec4);
        m_lineStart = createUniformHandle("uViewStart", bgfx::UniformType::Vec4);
        m_lineEnd = createUniformHandle("uViewEnd", bgfx::UniformType::Vec4);
        m_lineColor = createUniformHandle("uColor", bgfx::UniformType::Vec4);
        m_lineWidth = createUniformHandle("uLineWidth", bgfx::UniformType::Vec4);
        m_lineDepthBias = createUniformHandle("uDepthBias", bgfx::UniformType::Vec4);
        m_pointPosition = createUniformHandle("uRelativePosition", bgfx::UniformType::Vec4);
        m_pointSize = createUniformHandle("uPointSize", bgfx::UniformType::Vec4);
        m_pointColor = createUniformHandle("uColor", bgfx::UniformType::Vec4);
        // Uniform names must exactly match the names declared in the CAD
        // shader sources; bgfx binds program uniforms by name.
        m_cadView = createUniformHandle("u_cadView", bgfx::UniformType::Mat4);
        m_cadProjection = createUniformHandle("u_cadProjection", bgfx::UniformType::Mat4);
        m_cadCameraPos = createUniformHandle("u_cameraPos", bgfx::UniformType::Vec4);
        m_cadBaseColor = createUniformHandle("u_baseColor", bgfx::UniformType::Vec4);
        m_cadLightDir = createUniformHandle("u_lightDir", bgfx::UniformType::Vec4);
        m_cadStyleParams = createUniformHandle("u_styleParams", bgfx::UniformType::Vec4);
        m_cadWireframeColor = createUniformHandle("u_wireframeColor", bgfx::UniformType::Vec4);
        m_cadStrokeParams = createUniformHandle("u_strokeParams", bgfx::UniformType::Vec4);
        m_primParams = createUniformHandle("uPrimParams", bgfx::UniformType::Vec4);

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
                bgfx::isValid(m_logDepth) &&
                bgfx::isValid(m_view) && bgfx::isValid(m_projection) &&
                bgfx::isValid(m_cubeRelativePosition) &&
                bgfx::isValid(m_cubeOpacity) && bgfx::isValid(m_cubeColor) &&
                bgfx::isValid(m_lineStart) && bgfx::isValid(m_lineEnd) &&
                 bgfx::isValid(m_lineDepthBias) &&
                bgfx::isValid(m_lineColor) &&
                bgfx::isValid(m_lineWidth) &&
                bgfx::isValid(m_pointPosition) && bgfx::isValid(m_pointSize) &&
                bgfx::isValid(m_pointColor) &&
                bgfx::isValid(m_cadView) &&
                bgfx::isValid(m_cadProjection) &&
                bgfx::isValid(m_cadCameraPos) &&
                bgfx::isValid(m_cadBaseColor) &&
                bgfx::isValid(m_cadLightDir) &&
                bgfx::isValid(m_cadStyleParams) &&
                bgfx::isValid(m_cadWireframeColor) &&
                bgfx::isValid(m_cadStrokeParams) &&
                bgfx::isValid(m_primParams);
    }

    if (ready)
    {


        bgfx::VertexLayout cubeLayout;
        cubeLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .end();
        const std::array<CubeVertex, 36> cubeVertices = makeCubeVertices();
        m_sphereBuffer = createMeshBuffer(makeSphereMesh());
        m_coneBuffer = createMeshBuffer(makeConeMesh());
        m_torusBuffer = createMeshBuffer(makeTorusMesh());
        m_cubeBuffer = bgfx::createVertexBuffer(
            bgfx::copy(cubeVertices.data(), sizeof(cubeVertices)), cubeLayout);
        m_cadCubeBuffer = createCadCubeBuffer(cubeVertices);
        m_cadSphereBuffer = createCadMeshBuffer(makeSphereMesh());
        m_cadConeBuffer = createCadMeshBuffer(makeConeMesh());
        m_cadTorusBuffer = createCadMeshBuffer(makeTorusMesh());
        const std::array<CubeVertex, 24> aabbVertices = makeCubeEdgeVertices();
        m_aabbBuffer = bgfx::createVertexBuffer(
            bgfx::copy(aabbVertices.data(), sizeof(aabbVertices)), cubeLayout);

        bgfx::VertexLayout lineLayout;
        lineLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
        // Two triangles per ribbon quad: (t, side) = (0,-1) (0,+1) (1,-1)
        // and (1,-1) (0,+1) (1,+1).
        m_lineBuffer = bgfx::createDynamicVertexBuffer(6, lineLayout);

        bgfx::VertexLayout pointLayout;
        pointLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
        const std::array<float, 6> pointDisc{-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
        m_pointBuffer = bgfx::createVertexBuffer(
            bgfx::copy(pointDisc.data(), sizeof(pointDisc)), pointLayout);




        ready = bgfx::isValid(m_cubeBuffer) &&
                bgfx::isValid(m_cadCubeBuffer) &&
                bgfx::isValid(m_cadSphereBuffer) &&
                bgfx::isValid(m_cadConeBuffer) &&
                bgfx::isValid(m_cadTorusBuffer) &&
                bgfx::isValid(m_aabbBuffer) &&
                bgfx::isValid(m_lineBuffer) && bgfx::isValid(m_pointBuffer);
        if (!ready)
            std::cerr << "Failed to create bgfx vertex buffers." << std::endl;
    }

    return ready;
}

} // namespace rendering
