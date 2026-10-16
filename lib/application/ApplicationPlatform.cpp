#include "ApplicationPlatform.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_bgfx.h>

#include <iostream>

namespace application
{

bool Platform::initializeSdl()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return false;
    }
    sdlInitialized_ = true;
    return true;
}

SDL_Window *Platform::createWindow(const char *title, int width, int height,
                                   Uint32 flags)
{
    window_ = SDL_CreateWindow(title, width, height, flags);
    if (window_ == nullptr)
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    return window_;
}

bool Platform::initializeImGui(SDL_Window *window, float fontSize,
                               const char *fontPath,
                               std::uint32_t clearColor)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiContextCreated_ = true;
    ImGui::GetIO().IniFilename = nullptr;

    if (!ImGui_ImplSDL3_InitForOther(window))
    {
        std::cerr << "ImGui SDL3 backend unavailable; ViewCube disabled"
                  << std::endl;
        ImGui::DestroyContext();
        imguiContextCreated_ = false;
        return false;
    }

    imguiPlatformInitialized_ = true;
    imguiBgfxCreate(fontSize, fontPath);
    imguiBgfxSetViewClear(clearColor);
    imguiEnabled_ = true;
    return true;
}

void Platform::processEvent(const SDL_Event &event) const
{
    if (imguiPlatformInitialized_)
        ImGui_ImplSDL3_ProcessEvent(&event);
}

void Platform::beginImGuiFrame() const
{
    if (!imguiEnabled_)
        return;
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Platform::shutdownImGui()
{
    if (!imguiContextCreated_)
        return;
    if (imguiEnabled_)
        imguiBgfxDestroy();
    if (imguiPlatformInitialized_)
        ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    imguiContextCreated_ = false;
    imguiPlatformInitialized_ = false;
    imguiEnabled_ = false;
}

void Platform::shutdownSdl()
{
    if (window_ != nullptr)
        SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (sdlInitialized_)
        SDL_Quit();
    sdlInitialized_ = false;
}

SDL_Window *Platform::window() const
{
    return window_;
}

bool Platform::imguiEnabled() const
{
    return imguiEnabled_;
}

} // namespace application
