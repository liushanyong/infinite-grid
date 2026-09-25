#include "ViewCubeBgfx.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "vertex.h"
#include "frag.h"
#include "vs_line.h"
#include "fs_line.h"
#include "text_vertex.h"
#include "text_frag.h"

namespace cadui
{
    namespace
    {
        constexpr bgfx::ViewId kViewCubeView = 200;
        constexpr uint32_t kMaxLabelVertices = 2048;
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

        const bgfx::EmbeddedShader kEmbeddedTextShaders[] =
        {
            BGFX_EMBEDDED_SHADER(text_vertex),
            BGFX_EMBEDDED_SHADER(text_frag),
            BGFX_EMBEDDED_SHADER_END()
        };

        const bgfx::EmbeddedShader kEmbeddedLineShaders[] =
        {
            BGFX_EMBEDDED_SHADER(vs_line),
            BGFX_EMBEDDED_SHADER(fs_line),
            BGFX_EMBEDDED_SHADER_END()
        };

        struct TextVertex
        {
            float position[2];
            float uv[2];
            uint32_t color;
            float eyeZ;
        };

        struct LineVertex
        {
            float position[3];
        };

        struct LineEdge
        {
            glm::vec3 a;
            glm::vec3 b;
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

            // Corner order must match regionLabelById(), snapDirection() and the
            // OpenCADStudio region_centroids() order:
            // TFR, TFL, TBR, TBL, BFR, BFL, BBR, BBL.
            const std::array<std::array<glm::vec3, 3>, 8> corners = {{
                {{ { kF, -kF,  kE}, { kF, -kE,  kF}, { kE, -kF,  kF} }},
                {{ {-kF, -kF,  kE}, {-kF, -kE,  kF}, {-kE, -kF,  kF} }},
                {{ { kF,  kF,  kE}, { kF,  kE,  kF}, { kE,  kF,  kF} }},
                {{ {-kF,  kF,  kE}, {-kF,  kE,  kF}, {-kE,  kF,  kF} }},
                {{ { kF, -kF, -kE}, { kF, -kE, -kF}, { kE, -kF, -kF} }},
                {{ {-kF, -kF, -kE}, {-kF, -kE, -kF}, {-kE, -kF, -kF} }},
                {{ { kF,  kF, -kE}, { kF,  kE, -kF}, { kE,  kF, -kF} }},
                {{ {-kF,  kF, -kE}, {-kF,  kE, -kF}, {-kE,  kF, -kF} }}
            }};
            for (unsigned i = 0; i < 8; ++i)
            {
                appendTriangle(vertices, indices, corners[i], 18 + static_cast<int>(i));
            }
        }

        void buildRing(std::vector<CubeVertex>& vertices, std::vector<uint16_t>& indices)
        {
            // Draw the ring analytically in the fragment shader.  The vertex
            // normals carry local XY coordinates, so the shader can compute a
            // smooth radial mask without any triangular tessellation edges.
            const float extent = 1.85f;
            const glm::vec3 points[4] = {
                { -extent, -extent, kRingZ },
                {  extent, -extent, kRingZ },
                {  extent,  extent, kRingZ },
                { -extent,  extent, kRingZ }
            };
            const glm::vec3 localCoords[4] = {
                { -extent, -extent, 0.0f },
                {  extent, -extent, 0.0f },
                {  extent,  extent, 0.0f },
                { -extent,  extent, 0.0f }
            };
            const uint16_t base = static_cast<uint16_t>(vertices.size());
            for (unsigned i = 0; i < 4; ++i)
            {
                vertices.push_back(CubeVertex{
                    points[i].x, points[i].y, points[i].z,
                    localCoords[i].x, localCoords[i].y, localCoords[i].z,
                    packColor(158, 194, 214, 255),
                    -1.0f
                });
            }
            indices.insert(indices.end(), { base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                            base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3) });
        }

        // Extracts crease/boundary edges exactly like OpenCADStudio.  We only
        // draw the cube's boundary edges here; the compass ring stays analytic
        // so it does not expose 64 triangular hard edges.
        std::vector<LineEdge> extractBoundaryEdges(
            const std::vector<CubeVertex>& vertices,
            const std::vector<uint16_t>& indices)
        {
            struct EdgeKey
            {
                int region;
                uint16_t a;
                uint16_t b;
                bool operator<(const EdgeKey& other) const
                {
                    if (region != other.region) return region < other.region;
                    if (a != other.a) return a < other.a;
                    return b < other.b;
                }
            };

            std::map<EdgeKey, std::pair<uint32_t, LineEdge>> edges;
            for (size_t offset = 0; offset + 2 < indices.size(); offset += 3)
            {
                const uint16_t triangle[3] = {
                    indices[offset], indices[offset + 1], indices[offset + 2]
                };
                const int region = static_cast<int>(std::lround(
                    vertices[triangle[0]].region * 25.0f));
                for (unsigned edge = 0; edge < 3; ++edge)
                {
                    const uint16_t ia = triangle[edge];
                    const uint16_t ib = triangle[(edge + 1) % 3];
                    const uint16_t keyA = std::min(ia, ib);
                    const uint16_t keyB = std::max(ia, ib);
                    auto& item = edges[EdgeKey{ region, keyA, keyB }];
                    if (item.first == 0)
                    {
                        const CubeVertex& va = vertices[ia];
                        const CubeVertex& vb = vertices[ib];
                        item.second = LineEdge{
                            { va.position[0], va.position[1], va.position[2] },
                            { vb.position[0], vb.position[1], vb.position[2] }
                        };
                    }
                    ++item.first;
                }
            }

            std::vector<LineEdge> result;
            result.reserve(edges.size());
            for (const auto& entry : edges)
            {
                if (entry.second.first == 1)
                {
                    result.push_back(entry.second.second);
                }
            }
            return result;
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
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle viewUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle projectionUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle hoverUniform = BGFX_INVALID_HANDLE;
        bgfx::ProgramHandle lineProgram = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle lineVertexBuffer = BGFX_INVALID_HANDLE;
        uint32_t lineVertexCapacity = 0;
        std::vector<LineEdge> cubeEdges;
        bgfx::VertexLayout lineVertexLayout;
        bgfx::ProgramHandle textProgram = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle textSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle textScreenUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle textDepthUniform = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle fontTexture = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle labelVertexBuffer = BGFX_INVALID_HANDLE;
        uint32_t labelVertexCapacity = 0;
        bgfx::VertexLayout vertexLayout;
        bgfx::VertexLayout textVertexLayout;
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
        bgfx::ShaderHandle textVertexShader = bgfx::createEmbeddedShader(kEmbeddedTextShaders, rendererType, "text_vertex");
        bgfx::ShaderHandle textFragmentShader = bgfx::createEmbeddedShader(kEmbeddedTextShaders, rendererType, "text_frag");
        if (bgfx::isValid(textVertexShader) && bgfx::isValid(textFragmentShader))
        {
            m_impl->textProgram = bgfx::createProgram(textVertexShader, textFragmentShader, true);
        }
        bgfx::ShaderHandle lineVertexShader = bgfx::createEmbeddedShader(kEmbeddedLineShaders, rendererType, "vs_line");
        bgfx::ShaderHandle lineFragmentShader = bgfx::createEmbeddedShader(kEmbeddedLineShaders, rendererType, "fs_line");
        if (bgfx::isValid(lineVertexShader) && bgfx::isValid(lineFragmentShader))
        {
            m_impl->lineProgram = bgfx::createProgram(lineVertexShader, lineFragmentShader, true);
        }
        m_impl->viewUniform = bgfx::createUniform("uView", bgfx::UniformType::Mat4);
        m_impl->projectionUniform = bgfx::createUniform("uProj", bgfx::UniformType::Mat4);
        m_impl->hoverUniform = bgfx::createUniform("uHover", bgfx::UniformType::Vec4);
        m_impl->textSampler = bgfx::createUniform("s_font", bgfx::UniformType::Sampler);
        m_impl->textScreenUniform = bgfx::createUniform("uScreen", bgfx::UniformType::Vec4);
        m_impl->textDepthUniform = bgfx::createUniform("uDepth", bgfx::UniformType::Vec4);
        m_impl->vertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
            .end();
        m_impl->textVertexLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
            .end();
        m_impl->lineVertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .end();

        std::vector<CubeVertex> cubeVertices;
        std::vector<uint16_t> cubeIndices;
        buildGeometry(cubeVertices, cubeIndices);
        m_impl->cubeEdges = extractBoundaryEdges(cubeVertices, cubeIndices);

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
        m_impl->lineVertexCapacity = std::max<uint32_t>(
            static_cast<uint32_t>(m_impl->cubeEdges.size() * 6), 8);
        m_impl->lineVertexBuffer = bgfx::createDynamicVertexBuffer(
            m_impl->lineVertexCapacity, m_impl->lineVertexLayout);

        // Create the render target immediately.  The widget checks isValid()
        // before calling render(), so lazy first-frame allocation would keep
        // ViewCube on the non-depth-tested ImGui fallback forever.
        ensureFrameBufferSize(1, 1);

        m_impl->created = bgfx::isValid(m_impl->program)
            && bgfx::isValid(m_impl->textProgram)
            && bgfx::isValid(m_impl->lineProgram)
            && bgfx::isValid(m_impl->lineVertexBuffer)
            && bgfx::isValid(m_impl->textSampler)
            && bgfx::isValid(m_impl->textScreenUniform)
            && bgfx::isValid(m_impl->textDepthUniform)
            && bgfx::isValid(m_impl->cubeVertexBuffer)
            && bgfx::isValid(m_impl->cubeIndexBuffer)
            && bgfx::isValid(m_impl->ringVertexBuffer)
            && bgfx::isValid(m_impl->ringIndexBuffer)
            && bgfx::isValid(m_impl->frameBuffer);
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
        if (bgfx::isValid(m_impl->labelVertexBuffer))
        {
            bgfx::destroy(m_impl->labelVertexBuffer);
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
        if (bgfx::isValid(m_impl->lineVertexBuffer))
        {
            bgfx::destroy(m_impl->lineVertexBuffer);
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
        if (bgfx::isValid(m_impl->lineProgram))
        {
            bgfx::destroy(m_impl->lineProgram);
        }
        if (bgfx::isValid(m_impl->textSampler))
        {
            bgfx::destroy(m_impl->textSampler);
        }
        if (bgfx::isValid(m_impl->textScreenUniform))
        {
            bgfx::destroy(m_impl->textScreenUniform);
        }
        if (bgfx::isValid(m_impl->textDepthUniform))
        {
            bgfx::destroy(m_impl->textDepthUniform);
        }
        if (bgfx::isValid(m_impl->textProgram))
        {
            bgfx::destroy(m_impl->textProgram);
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
        m_impl->lineVertexBuffer = BGFX_INVALID_HANDLE;
        m_impl->lineVertexCapacity = 0;
        m_impl->cubeEdges.clear();
        m_impl->viewUniform = BGFX_INVALID_HANDLE;
        m_impl->projectionUniform = BGFX_INVALID_HANDLE;
        m_impl->hoverUniform = BGFX_INVALID_HANDLE;
        m_impl->lineProgram = BGFX_INVALID_HANDLE;
        m_impl->textSampler = BGFX_INVALID_HANDLE;
        m_impl->textScreenUniform = BGFX_INVALID_HANDLE;
        m_impl->textDepthUniform = BGFX_INVALID_HANDLE;
        m_impl->fontTexture = BGFX_INVALID_HANDLE;
        m_impl->labelVertexBuffer = BGFX_INVALID_HANDLE;
        m_impl->labelVertexCapacity = 0;
        m_impl->textProgram = BGFX_INVALID_HANDLE;
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

    void ViewCubeBgfxRenderer::setFontTexture(bgfx::TextureHandle texture)
    {
        m_impl->fontTexture = texture;
    }

    bool ViewCubeBgfxRenderer::canRenderText() const
    {
        return m_impl && m_impl->created && bgfx::isValid(m_impl->textProgram)
            && bgfx::isValid(m_impl->textSampler)
            && bgfx::isValid(m_impl->textScreenUniform)
            && bgfx::isValid(m_impl->textDepthUniform)
            && bgfx::isValid(m_impl->fontTexture);
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

        const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        const uint64_t depthFlags = BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY;
        m_impl->colorTexture = bgfx::createTexture2D(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            false,
            1,
            bgfx::TextureFormat::BGRA8,
            colorFlags);
        m_impl->depthTexture = bgfx::createTexture2D(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            false,
            1,
            bgfx::TextureFormat::D24S8,
            depthFlags);

        if (bgfx::isValid(m_impl->colorTexture) && bgfx::isValid(m_impl->depthTexture))
        {
            const bgfx::TextureHandle attachments[2] = { m_impl->colorTexture, m_impl->depthTexture };
            m_impl->frameBuffer = bgfx::createFrameBuffer(2, attachments, true);
        }
        else
        {
            m_impl->frameBuffer = BGFX_INVALID_HANDLE;
            if (bgfx::isValid(m_impl->colorTexture))
            {
                bgfx::destroy(m_impl->colorTexture);
                m_impl->colorTexture = BGFX_INVALID_HANDLE;
            }
            if (bgfx::isValid(m_impl->depthTexture))
            {
                bgfx::destroy(m_impl->depthTexture);
                m_impl->depthTexture = BGFX_INVALID_HANDLE;
            }
        }

        if (bgfx::isValid(m_impl->frameBuffer))
        {
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
            payload.packed.flags = 3; // bit0: blendable image, bit1: premultiplied
            payload.packed.mip = 0;
            m_textureId = payload.pointer;
        }
        else
        {
            m_impl->width = 0;
            m_impl->height = 0;
            m_textureId = nullptr;
        }
    }

    void ViewCubeBgfxRenderer::render(const ImVec2& size,
                                      const glm::mat3& cubeRotation,
                                      const glm::mat3& compassRotation,
                                      int hoveredRegionId,
                                      const std::vector<ViewCubeLabelVertex>& labelVertices)
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
        bgfx::setViewMode(kViewCubeView, bgfx::ViewMode::Sequential);
        bgfx::setViewClear(kViewCubeView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000u, 1.0f, 0);
        bgfx::setViewFrameBuffer(kViewCubeView, m_impl->frameBuffer);
        bgfx::setViewRect(kViewCubeView, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        // OpenCADStudio uses an extremely wide orthographic depth range.  This
        // also gives the depth-tested labels the exact same clip depth as the
        // reference text pass.
        const glm::mat4 projection = depthRangeProjection(
            glm::ortho(-1.85f, 1.85f, -1.85f, 1.85f, -2000.0f, 2000.0f));
        const float hover[4] = { hoveredRegionId >= 0 ? static_cast<float>(hoveredRegionId) / 25.0f : -1.0f,
                                 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_impl->projectionUniform, glm::value_ptr(projection));
        bgfx::setUniform(m_impl->hoverUniform, hover);
        const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                               BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
        // The ring is decorative and must never be lost to equal-depth failures
        // against the cube's far surface.  Draw it first without depth writes;
        // the cube then occludes it, and text remains depth-tested against the cube.
        // Premultiplied alpha: this render target is composited again by ImGui,
        // so the private framebuffer must store premultiplied color/coverage.
        const uint64_t ringState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                                   BGFX_STATE_BLEND_NORMAL | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA;

        // OpenCADStudio draws the cube first and the compass ring second.  Keep
        // the same order before the depth-tested text pass.
        bgfx::setUniform(m_impl->viewUniform, glm::value_ptr(mat3ToMat4(cubeRotation)));
        bgfx::setState(state);
        bgfx::setVertexBuffer(0, m_impl->cubeVertexBuffer);
        bgfx::setIndexBuffer(m_impl->cubeIndexBuffer);
        bgfx::submit(kViewCubeView, m_impl->program);

        bgfx::setUniform(m_impl->viewUniform, glm::value_ptr(mat3ToMat4(compassRotation)));
        bgfx::setState(ringState);
        bgfx::setVertexBuffer(0, m_impl->ringVertexBuffer);
        bgfx::setIndexBuffer(m_impl->ringIndexBuffer);
        bgfx::submit(kViewCubeView, m_impl->program);

        if (bgfx::isValid(m_impl->lineProgram) && bgfx::isValid(m_impl->lineVertexBuffer))
        {
            // bgfx is triangle-list based, so reference line primitives are
            // represented as thin camera-facing quads.  Build them after rotation
            // so the thickness is constant on screen and the depth test remains
            // identical to the cube/ring depth buffer.
            std::vector<LineVertex> lineVertices;
            lineVertices.reserve(m_impl->cubeEdges.size() * 4);
            constexpr float lineWidth = 0.021f;
            constexpr float depthBias = 0.005f;
            for (const LineEdge& edge : m_impl->cubeEdges)
            {
                const glm::vec3 a = cubeRotation * edge.a + glm::vec3{ 0.0f, 0.0f, depthBias };
                const glm::vec3 b = cubeRotation * edge.b + glm::vec3{ 0.0f, 0.0f, depthBias };
                const glm::vec2 direction{ b.x - a.x, b.y - a.y };
                const float length = glm::length(direction);
                if (length < 0.0001f)
                {
                    continue;
                }
                const glm::vec2 normal = glm::normalize(glm::vec2{ -direction.y, direction.x }) * lineWidth;
                const LineVertex quad[4] = {
                    { a.x + normal.x, a.y + normal.y, a.z },
                    { b.x + normal.x, b.y + normal.y, b.z },
                    { b.x - normal.x, b.y - normal.y, b.z },
                    { a.x - normal.x, a.y - normal.y, a.z }
                };
                constexpr uint32_t order[6] = { 0, 1, 2, 0, 2, 3 };
                for (uint32_t index : order)
                {
                    lineVertices.push_back(quad[index]);
                }
            }

            if (!lineVertices.empty() && lineVertices.size() <= m_impl->lineVertexCapacity)
            {
                bgfx::update(m_impl->lineVertexBuffer, 0, bgfx::copy(
                    lineVertices.data(),
                    static_cast<uint32_t>(lineVertices.size() * sizeof(LineVertex))));
                bgfx::setUniform(m_impl->projectionUniform, glm::value_ptr(projection));
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                               BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA);
                bgfx::setVertexBuffer(0, m_impl->lineVertexBuffer, 0,
                                      static_cast<uint32_t>(lineVertices.size()));
                bgfx::submit(kViewCubeView, m_impl->lineProgram);
            }
        }

        if (!canRenderText() || labelVertices.empty())
        {
            return;
        }

        const uint32_t vertexCount = std::min<uint32_t>(
            static_cast<uint32_t>(labelVertices.size()), kMaxLabelVertices);
        if (m_impl->labelVertexCapacity < vertexCount)
        {
            if (bgfx::isValid(m_impl->labelVertexBuffer))
            {
                bgfx::destroy(m_impl->labelVertexBuffer);
            }
            m_impl->labelVertexCapacity = std::max<uint32_t>(vertexCount, 256);
            m_impl->labelVertexBuffer = bgfx::createDynamicVertexBuffer(
                m_impl->labelVertexCapacity, m_impl->textVertexLayout);
        }
        if (!bgfx::isValid(m_impl->labelVertexBuffer))
        {
            m_impl->labelVertexCapacity = 0;
            return;
        }

        std::vector<TextVertex> vertices;
        vertices.reserve(vertexCount);
        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            const ViewCubeLabelVertex& source = labelVertices[i];
            vertices.push_back(TextVertex{
                source.position.x, source.position.y,
                source.uv.x, source.uv.y,
                source.color,
                source.depth
            });
        }
        bgfx::update(m_impl->labelVertexBuffer, 0, bgfx::copy(
            vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(TextVertex))));

        const float screenWidth = std::max(1.0f, size.x);
        const float screenHeight = std::max(1.0f, size.y);
        const float screen[4] = { screenWidth, screenHeight, 1.0f / screenWidth, 1.0f / screenHeight };
        const bgfx::Caps* caps = bgfx::getCaps();
        float depth[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const float radius = size.y * 0.5f;
        if (caps && caps->homogeneousDepth)
        {
            depth[0] = -radius / 2000.0f;
        }
        else
        {
            depth[0] = -radius / 2000.0f;
            depth[1] = 0.5f;
        }
        bgfx::setUniform(m_impl->textScreenUniform, screen);
        bgfx::setUniform(m_impl->textDepthUniform, depth);
        bgfx::setTexture(0, m_impl->textSampler, m_impl->fontTexture);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                       | BGFX_STATE_BLEND_NORMAL | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA);
        bgfx::setVertexBuffer(0, m_impl->labelVertexBuffer, 0, vertexCount);
        bgfx::submit(kViewCubeView, m_impl->textProgram);
    }
} // namespace cadui
