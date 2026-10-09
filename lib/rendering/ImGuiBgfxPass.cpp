#include "rendering/ImGuiBgfxPass.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include <bgfx/embedded_shader.h>

#include <imgui.h>

#include "imgui_shaders/imgui_pass.fragment.h"
#include "imgui_shaders/imgui_pass.vertex.h"

namespace rendering
{

namespace
{

const bgfx::EmbeddedShader kImGuiShaders[] = {
    BGFX_EMBEDDED_SHADER(imgui_pass_vertex),
    BGFX_EMBEDDED_SHADER(imgui_pass_fragment),
    BGFX_EMBEDDED_SHADER_END(),
};

} // namespace

bool ImGuiBgfxPass::init()
{
    if (m_initialized)
        return true;

    // ImDrawVert: pos float2 + uv float2 + color RGBA8 (normalized).
    m_vertexLayout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .end();

    unsigned char *textureData = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&textureData, &textureWidth,
                                             &textureHeight);
    m_fontTexture = bgfx::createTexture2D(
        std::uint16_t(textureWidth), std::uint16_t(textureHeight), false, 1,
        bgfx::TextureFormat::BGRA8, 0,
        bgfx::copy(textureData,
                   std::uint32_t(textureWidth * textureHeight * 4)));
    m_fontSampler = bgfx::createUniform("s_imgui_texture",
                                        bgfx::UniformType::Sampler);

    const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    const bgfx::ShaderHandle vertexShader = bgfx::createEmbeddedShader(
        kImGuiShaders, renderer, "imgui_pass_vertex");
    const bgfx::ShaderHandle fragmentShader = bgfx::createEmbeddedShader(
        kImGuiShaders, renderer, "imgui_pass_fragment");
    m_program = bgfx::createProgram(vertexShader, fragmentShader, true);

    if (!bgfx::isValid(m_program))
    {
        std::cerr << "Failed to create the ImGui program." << std::endl;
        return false;
    }
    m_initialized = true;
    return true;
}

void ImGuiBgfxPass::shutdown()
{
    if (!m_initialized)
        return;
    auto destroyValid = [](auto &handle) {
        if (bgfx::isValid(handle))
            bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    };
    destroyValid(m_fontTexture);
    destroyValid(m_fontSampler);
    destroyValid(m_program);
    m_initialized = false;
}

void ImGuiBgfxPass::renderDrawData(ImDrawData *drawData,
                                   bgfx::ViewId viewId,
                                   std::uint16_t width,
                                   std::uint16_t height)
{
    if (!m_initialized || drawData == nullptr ||
        drawData->CmdListsCount == 0)
        return;

    const ImVec2 clipOffset = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    for (int listIndex = 0; listIndex < drawData->CmdListsCount;
         ++listIndex)
    {
        const ImDrawList *drawList = drawData->CmdLists[listIndex];
        const auto numVertices =
            std::uint32_t(drawList->VtxBuffer.size());
        const auto numIndices =
            std::uint32_t(drawList->IdxBuffer.size());

        if (!bgfx::getAvailTransientVertexBuffer(numVertices,
                                                 m_vertexLayout) ||
            !bgfx::getAvailTransientIndexBuffer(numIndices))
            break;

        bgfx::TransientVertexBuffer vertexBuffer;
        bgfx::allocTransientVertexBuffer(&vertexBuffer, numVertices,
                                         m_vertexLayout);
        auto *dstVertices =
            reinterpret_cast<ImDrawVert *>(vertexBuffer.data);
        for (std::uint32_t v = 0; v < numVertices; ++v)
        {
            dstVertices[v] = drawList->VtxBuffer[v];
            // DisplaySize is in points; the backbuffer is in pixels.  The
            // framebuffer scale maps the point-space draw data onto the
            // full pixel surface (DPI > 1 otherwise shrinks the UI).
            dstVertices[v].pos.x =
                (drawList->VtxBuffer[v].pos.x - clipOffset.x) * clipScale.x;
            dstVertices[v].pos.y =
                (drawList->VtxBuffer[v].pos.y - clipOffset.y) * clipScale.y;
        }

        bgfx::TransientIndexBuffer indexBuffer;
        bgfx::allocTransientIndexBuffer(&indexBuffer, numIndices);
        std::memcpy(indexBuffer.data, drawList->IdxBuffer.begin(),
                    numIndices * sizeof(ImDrawIdx));

        std::uint32_t indexOffset = 0;
        for (const ImDrawCmd *command = drawList->CmdBuffer.begin();
             command != drawList->CmdBuffer.end(); ++command)
        {
            if (command->UserCallback != nullptr)
            {
                command->UserCallback(drawList, command);
            }
            else if (command->ElemCount != 0)
            {
                bgfx::TextureHandle textureHandle = m_fontTexture;
                if (command->TextureId != nullptr)
                    textureHandle.idx = std::uint16_t(
                        reinterpret_cast<std::uintptr_t>(
                            command->TextureId));

                const ImVec4 clipRect{
                    (command->ClipRect.x - clipOffset.x) * clipScale.x,
                    (command->ClipRect.y - clipOffset.y) * clipScale.y,
                    (command->ClipRect.z - clipOffset.x) * clipScale.x,
                    (command->ClipRect.w - clipOffset.y) * clipScale.y};
                if (clipRect.x < float(width) && clipRect.y < float(height) &&
                    clipRect.z >= 0.f && clipRect.w >= 0.f)
                {
                    const auto x = std::uint16_t(
                        std::max(clipRect.x, 0.f));
                    const auto y = std::uint16_t(
                        std::max(clipRect.y, 0.f));
                    const auto w = std::uint16_t(std::min(
                        clipRect.z, float(width)) - float(x));
                    const auto h = std::uint16_t(std::min(
                        clipRect.w, float(height)) - float(y));
                    bgfx::setScissor(x, y, w, h);

                    bgfx::setVertexBuffer(0, &vertexBuffer, 0,
                                          numVertices);
                    bgfx::setIndexBuffer(&indexBuffer, indexOffset,
                                         command->ElemCount);
                    bgfx::setTexture(0, m_fontSampler, textureHandle);
                    bgfx::setState(
                        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                        BGFX_STATE_BLEND_FUNC_SEPARATE(
                            BGFX_STATE_BLEND_SRC_ALPHA,
                            BGFX_STATE_BLEND_INV_SRC_ALPHA,
                            BGFX_STATE_BLEND_ONE,
                            BGFX_STATE_BLEND_ONE));
                    bgfx::submit(viewId, m_program);
                }
            }
            indexOffset += command->ElemCount;
        }
    }
}

} // namespace rendering
