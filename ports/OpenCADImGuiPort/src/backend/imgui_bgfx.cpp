#include "imgui_bgfx.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <vs_ocornut_imgui.bin.h>
#include <fs_ocornut_imgui.bin.h>
#include <vs_imgui_image.bin.h>
#include <fs_imgui_image.bin.h>
#include <roboto_regular.ttf.h>
#include <robotomono_regular.ttf.h>

namespace
{

const bgfx::EmbeddedShader kEmbeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(vs_imgui_image),
    BGFX_EMBEDDED_SHADER(fs_imgui_image),
    BGFX_EMBEDDED_SHADER_END()
};

inline bool checkAvailTransientBuffers(uint32_t numVertices, const bgfx::VertexLayout& layout, uint32_t numIndices)
{
    return numVertices == bgfx::getAvailTransientVertexBuffer(numVertices, layout)
        && (0 == numIndices || numIndices == bgfx::getAvailTransientIndexBuffer(numIndices));
}

struct BgfxImGuiContext
{
    bgfx::VertexLayout layout;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle imageProgram = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle imageLodEnabled = BGFX_INVALID_HANDLE;
    bool created = false;

    void create(float fontSize)
    {
        IMGUI_CHECKVERSION();

        const bgfx::RendererType::Enum rendererType = bgfx::getRendererType();
        program = bgfx::createProgram(
              bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "vs_ocornut_imgui")
            , bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "fs_ocornut_imgui")
            , true
        );

        imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
        imageProgram = bgfx::createProgram(
              bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "vs_imgui_image")
            , bgfx::createEmbeddedShader(kEmbeddedShaders, rendererType, "fs_imgui_image")
            , true
        );

        layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        sampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        config.MergeMode = false;

        const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<void*>(static_cast<const void*>(s_robotoRegularTtf)),
            static_cast<int>(sizeof(s_robotoRegularTtf)),
            fontSize,
            &config,
            ranges
        );
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<void*>(static_cast<const void*>(s_robotoMonoRegularTtf)),
            static_cast<int>(sizeof(s_robotoMonoRegularTtf)),
            fontSize - 3.0f,
            &config,
            ranges
        );

        uint8_t* pixels = nullptr;
        int32_t width = 0;
        int32_t height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        texture = bgfx::createTexture2D(
              static_cast<uint16_t>(width)
            , static_cast<uint16_t>(height)
            , false
            , 1
            , bgfx::TextureFormat::BGRA8
            , 0
            , bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4))
        );

        created = bgfx::isValid(program) && bgfx::isValid(texture) && bgfx::isValid(sampler);
    }

    void destroy()
    {
        if (!created)
        {
            return;
        }

        if (bgfx::isValid(sampler))
        {
            bgfx::destroy(sampler);
        }
        if (bgfx::isValid(texture))
        {
            bgfx::destroy(texture);
        }
        if (bgfx::isValid(imageLodEnabled))
        {
            bgfx::destroy(imageLodEnabled);
        }
        if (bgfx::isValid(imageProgram))
        {
            bgfx::destroy(imageProgram);
        }
        if (bgfx::isValid(program))
        {
            bgfx::destroy(program);
        }

        created = false;
    }

    void render(ImDrawData* drawData, bgfx::ViewId viewId)
    {
        if (!created || drawData == nullptr)
        {
            return;
        }

        const int32_t displayWidth = static_cast<int32_t>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        const int32_t displayHeight = static_cast<int32_t>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (displayWidth <= 0 || displayHeight <= 0)
        {
            return;
        }

        bgfx::setViewName(viewId, "ImGui");
        bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);

        const bgfx::Caps* caps = bgfx::getCaps();
        {
            float ortho[16];
            const float x = drawData->DisplayPos.x;
            const float y = drawData->DisplayPos.y;
            const float width = drawData->DisplaySize.x;
            const float height = drawData->DisplaySize.y;
            bx::mtxOrtho(ortho, x, x + width, y + height, y, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
            bgfx::setViewTransform(viewId, nullptr, ortho);
            bgfx::setViewRect(viewId, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        }

        const ImVec2 clipPos = drawData->DisplayPos;
        const ImVec2 clipScale = drawData->FramebufferScale;

        for (int32_t listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
        {
            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer tib;

            const ImDrawList* drawList = drawData->CmdLists[listIndex];
            const uint32_t numVertices = static_cast<uint32_t>(drawList->VtxBuffer.size());
            const uint32_t numIndices = static_cast<uint32_t>(drawList->IdxBuffer.size());

            if (!checkAvailTransientBuffers(numVertices, layout, numIndices))
            {
                break;
            }

            bgfx::allocTransientVertexBuffer(&tvb, numVertices, layout);
            bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

            ImDrawVert* vertices = reinterpret_cast<ImDrawVert*>(tvb.data);
            ImDrawIdx* indices = reinterpret_cast<ImDrawIdx*>(tib.data);

            std::memcpy(vertices, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));
            std::memcpy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

            bgfx::Encoder* encoder = bgfx::begin();

            for (const ImDrawCmd* cmd = drawList->CmdBuffer.begin(); cmd != drawList->CmdBuffer.end(); ++cmd)
            {
                if (cmd->UserCallback)
                {
                    cmd->UserCallback(drawList, cmd);
                    continue;
                }

                if (cmd->ElemCount == 0)
                {
                    continue;
                }

                uint64_t state = 0
                    | BGFX_STATE_WRITE_RGB
                    | BGFX_STATE_WRITE_A
                    | BGFX_STATE_MSAA
                    | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

                bgfx::TextureHandle textureHandle = texture;
                bgfx::ProgramHandle currentProgram = program;

                if (static_cast<ImU64>(0) != static_cast<ImU64>(reinterpret_cast<uintptr_t>(cmd->TextureId)))
                {
                    union
                    {
                        ImTextureID pointer;
                        struct { bgfx::TextureHandle handle; uint8_t flags; uint8_t mip; } packed;
                    } textureData;
                    textureData.pointer = cmd->TextureId;

                    if (0 != (1 & textureData.packed.flags))
                    {
                        state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
                    }

                    textureHandle = textureData.packed.handle;

                    if (0 != textureData.packed.mip)
                    {
                        const float lodEnabled[4] = { static_cast<float>(textureData.packed.mip), 1.0f, 0.0f, 0.0f };
                        bgfx::setUniform(imageLodEnabled, lodEnabled);
                        currentProgram = imageProgram;
                    }
                }

                ImVec4 clipRect;
                clipRect.x = (cmd->ClipRect.x - clipPos.x) * clipScale.x;
                clipRect.y = (cmd->ClipRect.y - clipPos.y) * clipScale.y;
                clipRect.z = (cmd->ClipRect.z - clipPos.x) * clipScale.x;
                clipRect.w = (cmd->ClipRect.w - clipPos.y) * clipScale.y;

                if (clipRect.x < static_cast<float>(displayWidth)
                    && clipRect.y < static_cast<float>(displayHeight)
                    && clipRect.z >= 0.0f
                    && clipRect.w >= 0.0f)
                {
                    const uint16_t xx = static_cast<uint16_t>(bx::max(clipRect.x, 0.0f));
                    const uint16_t yy = static_cast<uint16_t>(bx::max(clipRect.y, 0.0f));
                    encoder->setScissor(
                        xx,
                        yy,
                        static_cast<uint16_t>(bx::min(clipRect.z, 65535.0f) - xx),
                        static_cast<uint16_t>(bx::min(clipRect.w, 65535.0f) - yy)
                    );

                    encoder->setState(state);
                    encoder->setTexture(0, sampler, textureHandle);
                    encoder->setVertexBuffer(0, &tvb, cmd->VtxOffset, numVertices);
                    encoder->setIndexBuffer(&tib, cmd->IdxOffset, cmd->ElemCount);
                    encoder->submit(viewId, currentProgram);
                }
            }

            bgfx::end(encoder);
        }
    }
};

BgfxImGuiContext s_context;

} // namespace

void imguiBgfxCreate(float fontSize)
{
    s_context.create(fontSize);
}

void imguiBgfxDestroy()
{
    s_context.destroy();
}

void imguiBgfxRenderDrawData(ImDrawData* drawData, bgfx::ViewId viewId)
{
    s_context.render(drawData, viewId);
}
