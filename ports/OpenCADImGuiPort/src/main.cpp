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
        "OpenCADStudio ImGui Port",
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

    SDL_ShowWindow(window);

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