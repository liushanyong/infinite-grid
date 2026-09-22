#include "ViewCubeBgfx.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "vertex.h"
#include "frag.h"

namespace cadui
{
    namespace
    {
        constexpr bgfx::ViewId kViewCubeView = 200;
        constexpr float kF = 0.80f;
        constexpr float kE = 1.00f;
        constexpr float kM = (kF + kE) * 0.5f;
        constexpr float kRingZ = -1.0f;
        constexpr float kRingR0 = 1.40f;
        constexpr float kRingR1 = 1.74f;

        struct CubeVertex
        {
            float position[3];
            float normal[3];
            uint32_t color;
            float region;
        };

        const bgfx::EmbeddedShader kEmbeddedShaders[] =
        {
            BGFX_EMBEDDED_SHADER(vertex),
            BGFX_EMBEDDED_SHADER(frag),
            BGFX_EMBEDDED_SHADER_END()
        };

        uint32_t packColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            return static_cast<uint32_t>(r) |
                   (static_cast<uint32_t>(g) << 8u) |
                   (static_cast<uint32_t>(b) << 16u) |
                   (static_cast<uint32_t>(a) << 24u);
        }

        glm::vec3 quadNormal(const std::array<glm::vec3, 4>& points)
        {
            const glm::vec3 center = (points[0] + points[1] + points[2] + points[3]) * 0.25f;
            glm::vec3 normal = glm::cross(points[1] - points[0], points[3] - points[0]);
            if (glm::dot(normal, center) < 0.0f)
            {
                normal = -normal;
            }
            return glm::normalize(normal);
        }

        glm::vec3 triangleNormal(const std::array<glm::vec3, 3>& points)
        {
            const glm::vec3 center = glm::normalize((points[0] + points[1] + points[2]) / 3.0f);
            glm::vec3 normal = glm::cross(points[1] - points[0], points[2] - points[0]);
            if (glm::dot(normal, center) < 0.0f)
            {
                normal = -normal;
            }
            return glm::normalize(normal);
        }

        void appendQuad(std::vector<CubeVertex>& vertices,
                        std::vector<uint16_t>& indices,
                        const std::array<glm::vec3, 4>& points,
                        int region)
        {
            const glm::vec3 normal = quadNormal(points);
            const uint32_t color = packColor(158, 194, 214, 255);
            const uint16_t base = static_cast<uint16_t>(vertices.size());
            for (const glm::vec3& point : points)
            {
                vertices.push_back(CubeVertex{
                    point.x, point.y, point.z,
                    normal.x, normal.y, normal.z,
                    color,
                    static_cast<float>(region) / 25.0f
                });
            }
            indices.insert(indices.end(), { base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                            base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3) });
        }

        void appendTriangle(std::vector<CubeVertex>& vertices,
                            std::vector<uint16_t>& indices,
                            const std::array<glm::vec3, 3>& points,
                            int region)
        {
            const glm::vec3 normal = triangleNormal(points);
            const uint32_t color = packColor(158, 194, 214, 255);
            const uint16_t base = static_cast<uint16_t>(vertices.size());
            for (const glm::vec3& point : points)
            {
                vertices.push_back(CubeVertex{
                    point.x, point.y, point.z,
                    normal.x, normal.y, normal.z,
                    color,
                    static_cast<float>(region) / 25.0f
                });
            }
            indices.insert(indices.end(), { base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2) });
        }

        void buildGeometry(std::vector<CubeVertex>& vertices, std::vector<uint16_t>& indices)
        {
            appendQuad(vertices, indices, {{ {-kF, -kF,  kE}, { kF, -kF,  kE}, { kF,  kF,  kE}, {-kF,  kF,  kE} }}, 0);
            appendQuad(vertices, indices, {{ {-kF,  kF, -kE}, { kF,  kF, -kE}, { kF, -kF, -kE}, {-kF, -kF, -kE} }}, 1);
            appendQuad(vertices, indices, {{ { kF, -kE, -kF}, {-kF, -kE, -kF}, {-kF, -kE,  kF}, { kF, -kE,  kF} }}, 2);
            appendQuad(vertices, indices, {{ {-kF,  kE, -kF}, { kF,  kE, -kF}, { kF,  kE,  kF}, {-kF,  kE,  kF} }}, 3);
            appendQuad(vertices, indices, {{ { kE,  kF, -kF}, { kE, -kF, -kF}, { kE, -kF,  kF}, { kE,  kF,  kF} }}, 4);
            appendQuad(vertices, indices, {{ {-kE, -kF, -kF}, {-kE,  kF, -kF}, {-kE,  kF,  kF}, {-kE, -kF,  kF} }}, 5);

            appendQuad(vertices, indices, {{ { kF, -kF,  kE}, {-kF, -kF,  kE}, {-kF, -kE,  kF}, { kF, -kE,  kF} }}, 6);
            appendQuad(vertices, indices, {{ {-kF,  kF,  kE}, { kF,  kF,  kE}, { kF,  kE,  kF}, {-kF,  kE,  kF} }}, 7);
            appendQuad(vertices, indices, {{ { kF,  kF,  kE}, { kF, -kF,  kE}, { kE, -kF,  kF}, { kE,  kF,  kF} }}, 8);
            appendQuad(vertices, indices, {{ {-kF, -kF,  kE}, {-kF,  kF,  kE}, {-kE,  kF,  kF}, {-kE, -kF,  kF} }}, 9);
            appendQuad(vertices, indices, {{ { kF, -kF, -kE}, {-kF, -kF, -kE}, {-kF, -kE, -kF}, { kF, -kE, -kF} }}, 10);
            appendQuad(vertices, indices, {{ {-kF,  kF, -kE}, { kF,  kF, -kE}, { kF,  kE, -kF}, {-kF,  kE, -kF} }}, 11);
            appendQuad(vertices, indices, {{ { kF,  kF, -kE}, { kF, -kF, -kE}, { kE, -kF, -kF}, { kE,  kF, -kF} }}, 12);
            appendQuad(vertices, indices, {{ {-kF, -kF, -kE}, {-kF,  kF, -kE}, {-kE,  kF, -kF}, {-kE, -kF, -kF} }}, 13);
            appendQuad(vertices, indices, {{ { kF, -kE, -kF}, { kF, -kE,  kF}, { kE, -kF,  kF}, { kE, -kF, -kF} }}, 14);
            appendQuad(vertices, indices, {{ {-kF, -kE,  kF}, {-kF, -kE, -kF}, {-kE, -kF, -kF}, {-kE, -kF,  kF} }}, 15);
            appendQuad(vertices, indices, {{ { kF,  kE,  kF}, { kF,  kE, -kF}, { kE,  kF, -kF}, { kE,  kF,  kF} }}, 16);
            appendQuad(vertices, indices, {{ {-kF,  kE,  kF}, {-kF,  kE, -kF}, {-kE,  kF, -kF}, {-kE,  kF,  kF} }}, 17);

            const std::array<std::array<glm::vec3, 3>, 8> corners = {{
                {{ { kF,  kF,  kE}, { kF,  kE,  kF}, { kE,  kF,  kF} }},
                {{ {-kF,  kF,  kE}, {-kF,  kE,  kF}, {-kE,  kF,  kF} }},
                {{ { kF,  kF, -kE}, { kF,  kE, -kF}, { kE,  kF, -kF} }},
                {{ {-kF,  kF, -kE}, {-kF,  kE, -kF}, {-kE,  kF, -kF} }},
                {{ { kF, -kF,  kE}, { kF, -kE,  kF}, { kE, -kF,  kF} }},
                {{ {-kF, -kF,  kE}, {-kF, -kE,  kF}, {-kE, -kF,  kF} }},
                {{ { kF, -kF, -kE}, { kF, -kE, -kF}, { kE, -kF, -kF} }},
                {{ {-kF, -kF, -kE}, {-kF, -kE, -kF}, {-kE, -kF, -kF} }}
            }};
            for (unsigned i = 0; i < 8; ++i)
            {
                appendTriangle(vertices, indices, corners[i], 18 + static_cast<int>(i));
            }
        }

        void buildRing(std::vector<CubeVertex>& vertices, std::vector<uint16_t>& indices)
        {
            constexpr unsigned segments = 64;
            for (unsigned s = 0; s < segments; ++s)
            {
                const float a0 = static_cast<float>(s) * 6.28318530718f / static_cast<float>(segments);
                const float a1 = static_cast<float>(s + 1) * 6.28318530718f / static_cast<float>(segments);
                const glm::vec3 points[4] = {
                    { std::cos(a0) * kRingR0, std::sin(a0) * kRingR0, kRingZ },
                    { std::cos(a1) * kRingR0, std::sin(a1) * kRingR0, kRingZ },
                    { std::cos(a1) * kRingR1, std::sin(a1) * kRingR1, kRingZ },
                    { std::cos(a0) * kRingR1, std::sin(a0) * kRingR1, kRingZ }
                };
                const uint16_t base = static_cast<uint16_t>(vertices.size());
                for (const glm::vec3& point : points)
                {
                    vertices.push_back(CubeVertex{
                        point.x, point.y, point.z,
                        0.0f, 0.0f, 1.0f,
                        packColor(158, 194, 214, 255),
                        -1.0f
                    });
                }
                indices.insert(indices.end(), { base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                                base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3) });
            }
        }

        glm::mat4 mat3ToMat4(const glm::mat3& rotation)
        {
            glm::mat4 result{ 1.0f };
            for (unsigned column = 0; column < 3; ++column)
            {
                for (unsigned row = 0; row < 3; ++row)
                {
                    result[column][row] = rotation[column][row];
                }
            }
            return result;
        }

        glm::mat4 depthRangeProjection(const glm::mat4& projection)
        {
            const bgfx::Caps* caps = bgfx::getCaps();
            if (!caps || caps->homogeneousDepth)
            {
                return projection;
            }
            glm::mat4 depthRange{ 1.0f };
            depthRange[2][2] = 0.5f;
            depthRange[3][2] = 0.5f;
            return depthRange * projection;
        }
    } // namespace

    struct ViewCubeBgfxRenderer::Impl
    {
        bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
        bgfx::VertexBufferHandle cubeVertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle cubeIndexBuffer = BGFX_INVALID_HANDLE;
        bgfx::VertexBufferHandle ringVertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ringIndexBuffer = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle viewUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle projectionUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle hoverUniform = BGFX_INVALID_HANDLE;
        bgfx::VertexLayout vertexLayout;
        unsigned width{ 0 };
        unsigned height{ 0 };
        bool created{ false };
    };

    ViewCubeBgfxRenderer::ViewCubeBgfxRenderer()
        : m_impl{ new Impl }
    {
    }

    ViewCubeBgfxRenderer::~ViewCubeBgfxRenderer()
    {
        destroy();
        delete m_impl;
    }

    bool ViewCubeBgfxRenderer::create()
    {
        if (m_impl->created)
        {
            return true;
        }

        const bgfx::RendererType::Enum rendererType = bgfx::getRendererType();
        bgfx::ShaderHandle vertexShader = bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "vertex");
        bgfx::ShaderHandle fragmentShader = bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "frag");
        if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader))
        {
            return false;
        }

        m_impl->program = bgfx::createProgram(vertexShader, fragmentShader, true);
        m_impl->viewUniform = bgfx::createUniform("uView", bgfx::UniformType::Mat4);
        m_impl->projectionUniform = bgfx::createUniform("uProj", bgfx::UniformType::Mat4);
        m_impl->hoverUniform = bgfx::createUniform("uHover", bgfx::UniformType::Vec4);
        m_impl->vertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
            .end();

        std::vector<CubeVertex> cubeVertices;
        std::vector<uint16_t> cubeIndices;
        buildGeometry(cubeVertices, cubeIndices);

        std::vector<CubeVertex> ringVertices;
        std::vector<uint16_t> ringIndices;
        buildRing(ringVertices, ringIndices);

        m_impl->cubeVertexBuffer = bgfx::createVertexBuffer(
            bgfx::copy(cubeVertices.data(), static_cast<uint32_t>(cubeVertices.size() * sizeof(CubeVertex))),
            m_impl->vertexLayout);
        m_impl->cubeIndexBuffer = bgfx::createIndexBuffer(
            bgfx::copy(cubeIndices.data(), static_cast<uint32_t>(cubeIndices.size() * sizeof(uint16_t))));
        m_impl->ringVertexBuffer = bgfx::createVertexBuffer(
            bgfx::copy(ringVertices.data(), static_cast<uint32_t>(ringVertices.size() * sizeof(CubeVertex))),
            m_impl->vertexLayout);
        m_impl->ringIndexBuffer = bgfx::createIndexBuffer(
            bgfx::copy(ringIndices.data(), static_cast<uint32_t>(ringIndices.size() * sizeof(uint16_t))));

        m_impl->created = bgfx::isValid(m_impl->program)
            && bgfx::isValid(m_impl->cubeVertexBuffer)
            && bgfx::isValid(m_impl->cubeIndexBuffer)
            && bgfx::isValid(m_impl->ringVertexBuffer)
            && bgfx::isValid(m_impl->ringIndexBuffer);
        return m_impl->created;
    }

    void ViewCubeBgfxRenderer::destroy()
    {
        if (!m_impl->created)
        {
            return;
        }

        if (bgfx::isValid(m_impl->frameBuffer))
        {
            bgfx::destroy(m_impl->frameBuffer);
        }
        if (bgfx::isValid(m_impl->cubeVertexBuffer))
        {
            bgfx::destroy(m_impl->cubeVertexBuffer);
        }
        if (bgfx::isValid(m_impl->cubeIndexBuffer))
        {
            bgfx::destroy(m_impl->cubeIndexBuffer);
        }
        if (bgfx::isValid(m_impl->ringVertexBuffer))
        {
            bgfx::destroy(m_impl->ringVertexBuffer);
        }
        if (bgfx::isValid(m_impl->ringIndexBuffer))
        {
            bgfx::destroy(m_impl->ringIndexBuffer);
        }
        if (bgfx::isValid(m_impl->viewUniform))
        {
            bgfx::destroy(m_impl->viewUniform);
        }
        if (bgfx::isValid(m_impl->projectionUniform))
        {
            bgfx::destroy(m_impl->projectionUniform);
        }
        if (bgfx::isValid(m_impl->hoverUniform))
        {
            bgfx::destroy(m_impl->hoverUniform);
        }
        if (bgfx::isValid(m_impl->program))
        {
            bgfx::destroy(m_impl->program);
        }

        m_impl->frameBuffer = BGFX_INVALID_HANDLE;
        m_impl->colorTexture = BGFX_INVALID_HANDLE;
        m_impl->cubeVertexBuffer = BGFX_INVALID_HANDLE;
        m_impl->cubeIndexBuffer = BGFX_INVALID_HANDLE;
        m_impl->ringVertexBuffer = BGFX_INVALID_HANDLE;
        m_impl->ringIndexBuffer = BGFX_INVALID_HANDLE;
        m_impl->viewUniform = BGFX_INVALID_HANDLE;
        m_impl->projectionUniform = BGFX_INVALID_HANDLE;
        m_impl->hoverUniform = BGFX_INVALID_HANDLE;
        m_impl->program = BGFX_INVALID_HANDLE;
        m_impl->width = 0;
        m_impl->height = 0;
        m_impl->created = false;
        m_textureId = nullptr;
    }

    bool ViewCubeBgfxRenderer::isValid() const
    {
        return m_impl && m_impl->created && bgfx::isValid(m_impl->frameBuffer);
    }

    ImTextureID ViewCubeBgfxRenderer::textureId() const
    {
        return m_textureId;
    }

    void ViewCubeBgfxRenderer::ensureFrameBufferSize(unsigned width, unsigned height)
    {
        if (m_impl->width == width && m_impl->height == height && bgfx::isValid(m_impl->frameBuffer))
        {
            return;
        }

        if (bgfx::isValid(m_impl->frameBuffer))
        {
            bgfx::destroy(m_impl->frameBuffer);
        }

        const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        m_impl->frameBuffer = bgfx::createFrameBuffer(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            bgfx::TextureFormat::BGRA8,
            flags);
        m_impl->colorTexture = bgfx::getTexture(m_impl->frameBuffer, 0);
        m_impl->width = width;
        m_impl->height = height;

        union TexturePayload
        {
            ImTextureID pointer;
            struct
            {
                bgfx::TextureHandle handle;
                uint8_t flags;
                uint8_t mip;
            } packed;
        } payload{};
        payload.packed.handle = m_impl->colorTexture;
        payload.packed.flags = 1;
        payload.packed.mip = 0;
        m_textureId = payload.pointer;
    }

    void ViewCubeBgfxRenderer::render(const ImVec2& size,
                                      const glm::mat3& cubeRotation,
                                      const glm::mat3& compassRotation,
                                      int hoveredRegionId)
    {
        if (!m_impl->created)
        {
            return;
        }

        const unsigned width = std::max(1u, static_cast<unsigned>(size.x));
        const unsigned height = std::max(1u, static_cast<unsigned>(size.y));
        ensureFrameBufferSize(width, height);
        if (!bgfx::isValid(m_impl->frameBuffer))
        {
            return;
        }

        bgfx::setViewName(kViewCubeView, "CadViewCube");
        bgfx::setViewClear(kViewCubeView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000u, 1.0f, 0);
        bgfx::setViewFrameBuffer(kViewCubeView, m_impl->frameBuffer);
        bgfx::setViewRect(kViewCubeView, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        const glm::mat4 projection = depthRangeProjection(glm::ortho(-1.85f, 1.85f, -1.85f, 1.85f, -10.0f, 10.0f));
        const float hover[4] = { hoveredRegionId >= 0 ? static_cast<float>(hoveredRegionId) : -1.0f,
                                 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_impl->projectionUniform, glm::value_ptr(projection));
        bgfx::setUniform(m_impl->hoverUniform, hover);
        const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                               BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;

        bgfx::setUniform(m_impl->viewUniform, glm::value_ptr(mat3ToMat4(compassRotation)));
        bgfx::setState(state);
        bgfx::setVertexBuffer(0, m_impl->ringVertexBuffer);
        bgfx::setIndexBuffer(m_impl->ringIndexBuffer);
        bgfx::submit(kViewCubeView, m_impl->program);

        bgfx::setUniform(m_impl->viewUniform, glm::value_ptr(mat3ToMat4(cubeRotation)));
        bgfx::setState(state);
        bgfx::setVertexBuffer(0, m_impl->cubeVertexBuffer);
        bgfx::setIndexBuffer(m_impl->cubeIndexBuffer);
        bgfx::submit(kViewCubeView, m_impl->program);
    }
} // namespace cadui
