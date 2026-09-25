#include <SDL3/SDL.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "app/DocumentAppController.hpp"
#include "backend/imgui_bgfx.h"
#include "platform/InputRouter.hpp"
#include "ui/UiHost.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
namespace
{
    void* getNativeWindowHandle(SDL_Window* window)
    {
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if defined(_WIN32)
        return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__linux__)
        void* handle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_POINTER, nullptr);
        if (handle)
        {
            return handle;
        }
        return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
#else
        (void)properties;
        return nullptr;
#endif
    }

    bool initializeBgfx(SDL_Window* window)
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        bgfx::Init init;
        init.type = bgfx::RendererType::Direct3D11;
        init.platformData.nwh = getNativeWindowHandle(window);
        init.resolution.width = static_cast<uint16_t>(std::max(1, width));
        init.resolution.height = static_cast<uint16_t>(std::max(1, height));
        init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4;

        if (!bgfx::init(init))
        {
            std::cerr << "bgfx init failed" << std::endl;
            return false;
        }

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x203040FF, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, init.resolution.width, init.resolution.height);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Open CAD Studio 2026.37 - Drawing1",
        1600,
        900,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    if (!initializeBgfx(window))
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";

    if (!io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon()
    ))
    {
        std::cerr << "Chinese font load failed; using ImGui default font" << std::endl;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowMinSize = ImVec2(120.0f, 80.0f);
    style.FrameBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078f, 0.102f, 0.137f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.059f, 0.075f, 0.102f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.122f, 0.161f, 0.212f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.176f, 0.239f, 0.314f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.133f, 0.424f, 0.765f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.129f, 0.173f, 0.227f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.878f, 0.918f, 0.965f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.165f, 0.165f, 0.165f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.205f, 0.205f, 0.205f, 1.0f);
    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.075f, 0.565f, 0.800f, 0.55f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.086f, 0.086f, 0.086f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.235f, 0.235f, 0.235f, 1.0f);

    if (!ImGui_ImplSDL3_InitForOther(window))
    {
        std::cerr << "ImGui SDL3 backend init failed" << std::endl;
        imguiBgfxDestroy();
        bgfx::shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    imguiBgfxCreate(18.0f);

    DocumentAppController controller;
    InputRouter router;
    ui::UiHost uiHost(controller, router);

    UiAction initialSelection;
    initialSelection.type = UiActionType::SelectObject;
    initialSelection.objectId = 1;
    controller.execute(initialSelection);

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    bool running = true;
    float lastFrameMs = 0.0f;
    uint16_t lastWidth = 0;
    uint16_t lastHeight = 0;

    while (running)
    {
        const auto frameStart = std::chrono::steady_clock::now();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
        const uint16_t width = static_cast<uint16_t>(std::max(1, pixelWidth));
        const uint16_t height = static_cast<uint16_t>(std::max(1, pixelHeight));

        if (width != lastWidth || height != lastHeight)
        {
            bgfx::reset(width, height, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
            bgfx::setViewRect(0, 0, 0, width, height);
            lastWidth = width;
            lastHeight = height;
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        router.update(io.WantCaptureMouse, io.WantCaptureKeyboard, io.WantTextInput);

        uiHost.draw(lastFrameMs);

        if (controller.closeRequested())
        {
            running = false;
        }

        ImGui::Render();

        bgfx::touch(0);
        imguiBgfxRenderDrawData(ImGui::GetDrawData(), 255);

        bgfx::frame();

        const auto frameEnd = std::chrono::steady_clock::now();
        lastFrameMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
    }

    ImGui_ImplSDL3_Shutdown();
    imguiBgfxDestroy();
    ImGui::DestroyContext();
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
