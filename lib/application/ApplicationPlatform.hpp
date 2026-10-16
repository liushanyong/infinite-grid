#pragma once

#include <SDL.h>
#include <cstdint>

namespace application
{

class Platform
{
public:
    bool initializeSdl();
    SDL_Window *createWindow(const char *title, int width, int height,
                             Uint32 flags);
    bool initializeImGui(SDL_Window *window, float fontSize,
                         const char *fontPath, std::uint32_t clearColor);
    void processEvent(const SDL_Event &event) const;
    void beginImGuiFrame() const;
    void shutdownImGui();
    void shutdownSdl();

    SDL_Window *window() const;
    bool imguiEnabled() const;

private:
    SDL_Window *window_ = nullptr;
    bool sdlInitialized_ = false;
    bool imguiContextCreated_ = false;
    bool imguiPlatformInitialized_ = false;
    bool imguiEnabled_ = false;
};

} // namespace application
