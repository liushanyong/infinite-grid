#include <cadui/UcsIcon.hpp>
#include <cadui/ViewCube.hpp>
#include <cadui/ViewCubeBgfx.hpp>
#include <imgui_bgfx.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    struct CameraState
    {
        double yaw{ 60.0 };
        double pitch{ 25.0 };
        double roll{ 0.0 };
        double distance{ 12.0 };
        glm::dvec3 target{ 0.0, 0.0, 0.0 };

        glm::vec3 direction() const
        {
            const double yawRadians = glm::radians(yaw);
            const double pitchRadians = glm::radians(pitch);
            const double cosine = std::cos(pitchRadians);
            return glm::normalize(glm::vec3{
                static_cast<float>(cosine * std::cos(yawRadians)),
                static_cast<float>(cosine * std::sin(yawRadians)),
                static_cast<float>(std::sin(pitchRadians))
            });
        }

        glm::mat4 viewMatrix() const
        {
            const glm::dvec3 eye = target + glm::dvec3(direction()) * distance;
            const glm::vec3 up = std::abs(direction().z) > 0.999f
                ? glm::vec3{ 0.0f, 1.0f, 0.0f }
                : glm::vec3{ 0.0f, 0.0f, 1.0f };
            glm::mat4 view = glm::lookAt(glm::vec3(eye), glm::vec3(target), up);
            return glm::rotate(view, static_cast<float>(glm::radians(roll)), glm::vec3{ 0.0f, 0.0f, 1.0f });
        }

        void snapToDirection(const glm::vec3& worldDirection)
        {
            const glm::vec3 direction = glm::normalize(worldDirection);
            pitch = glm::degrees(std::asin(std::clamp(direction.z, -1.0f, 1.0f)));
            yaw = glm::degrees(std::atan2(direction.y, direction.x));
            roll = 0.0;
        }
    };

    void* getNativeWindowHandle(SDL_Window* window)
    {
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if defined(_WIN32)
        return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
        (void)properties;
        return nullptr;
#endif
    }

    bool initializeBgfx(SDL_Window* window, uint16_t& outWidth, uint16_t& outHeight)
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        outWidth = static_cast<uint16_t>(std::max(1, width));
        outHeight = static_cast<uint16_t>(std::max(1, height));

        bgfx::Init init;
        init.type = bgfx::RendererType::Direct3D11;
        init.platformData.nwh = getNativeWindowHandle(window);
        init.resolution.width = outWidth;
        init.resolution.height = outHeight;
        init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4;

        if (!bgfx::init(init))
        {
            std::cerr << "bgfx::init failed\n";
            return false;
        }

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x182029ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, outWidth, outHeight);
        return true;
    }

    void drawViewCubeOverlay(cadui::ViewCubeWidget& widget,
                             cadui::ViewCubeBgfxRenderer& renderer,
                             const glm::mat3& viewRotation,
                             float width,
                             float height,
                             CameraState& camera,
                             const std::vector<std::string>& ucsNames,
                             std::string& activeUcs,
                             std::string& statusText)
    {
        constexpr float overlayWidth = 174.0f;
        constexpr float overlayHeight = 188.0f;
        ImGui::SetNextWindowPos(ImVec2(width - overlayWidth - 12.0f, 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;

        if (!ImGui::Begin("CadViewCubeOverlay", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        widget.setBgfxRenderer(&renderer);
        cadui::ViewCubeOptions options;
        options.faceLabels = { "\u4e0a", "\u4e0b", "\u524d", "\u540e", "\u53f3", "\u5de6" };
        options.showControls = true;
        options.showUcsPicker = true;
        options.activeUcs = activeUcs.c_str();
        options.ucsNames = &ucsNames;

        const cadui::ViewCubeResult result = widget.render("CadViewCube", ImVec2(160.0f, 160.0f), viewRotation);
        switch (result.action.kind)
        {
        case cadui::ViewCubeActionKind::Region:
            statusText = "ViewCube: ";
            statusText += result.action.region.label;
            camera.snapToDirection(cadui::ViewCubeWidget::snapDirection(result.action.region));
            break;
        case cadui::ViewCubeActionKind::Cardinal:
            statusText = "ViewCube: ";
            statusText += result.action.region.label;
            camera.snapToDirection(cadui::ViewCubeWidget::cardinalDirection(result.action.region.index));
            break;
        case cadui::ViewCubeActionKind::Home:
            camera.yaw = 60.0;
            camera.pitch = 25.0;
            camera.roll = 0.0;
            statusText = "ViewCube: home";
            break;
        case cadui::ViewCubeActionKind::RollLeft:
            camera.roll += 90.0;
            statusText = "ViewCube: roll left";
            break;
        case cadui::ViewCubeActionKind::RollRight:
            camera.roll -= 90.0;
            statusText = "ViewCube: roll right";
            break;
        case cadui::ViewCubeActionKind::NudgeUp:
            camera.pitch += 90.0;
            statusText = "ViewCube: nudge up";
            break;
        case cadui::ViewCubeActionKind::NudgeDown:
            camera.pitch -= 90.0;
            statusText = "ViewCube: nudge down";
            break;
        case cadui::ViewCubeActionKind::NudgeLeft:
            camera.yaw -= 90.0;
            statusText = "ViewCube: nudge left";
            break;
        case cadui::ViewCubeActionKind::NudgeRight:
            camera.yaw += 90.0;
            statusText = "ViewCube: nudge right";
            break;
        case cadui::ViewCubeActionKind::UcsChanged:
            activeUcs = result.action.ucs;
            statusText = "UCS: " + activeUcs;
            break;
        default:
            break;
        }
        ImGui::End();
    }

    void drawUcsOverlay(cadui::UcsIconWidget& widget,
                        const glm::mat3& viewRotation,
                        float height,
                        std::string& statusText)
    {
        constexpr float overlaySize = 124.0f;
        ImGui::SetNextWindowPos(ImVec2(12.0f, height - overlaySize - 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(overlaySize, overlaySize), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;

        if (!ImGui::Begin("CadUcsOverlay", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        cadui::UcsIconState state;
        state.viewRotation = viewRotation;
        state.selected = true;
        state.backgroundLuminance = 0.11f;

        const cadui::UcsIconResult result = widget.render("CadUcsIcon", ImVec2(overlaySize, overlaySize), state);
        if (result.hoveredAxis != cadui::UcsAxis::None)
        {
            const char* name =
                result.hoveredAxis == cadui::UcsAxis::X ? "UCS X" :
                result.hoveredAxis == cadui::UcsAxis::Y ? "UCS Y" : "UCS Z";
            statusText = name;
        }
        if (result.clickedAxis != cadui::UcsAxis::None)
        {
            const char* name =
                result.clickedAxis == cadui::UcsAxis::X ? "UCS X clicked" :
                result.clickedAxis == cadui::UcsAxis::Y ? "UCS Y clicked" : "UCS Z clicked";
            statusText = name;
        }
        if (result.clickedGrip == cadui::UcsGripKind::Origin)
        {
            statusText = "UCS origin clicked";
        }
        ImGui::End();
    }

    void drawStatusBar(const std::string& statusText, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, height - 28.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, 28.0f), ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin("##CadUiStatusBar", nullptr, flags))
        {
            ImGui::TextUnformatted(statusText.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("| drag viewport: orbit | ViewCube/UCS: click controls |");
        }
        ImGui::End();
    }
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "UCS Icon + ViewCube - BGFX/ImGui",
        1600,
        900,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);

    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    uint16_t width = 0;
    uint16_t height = 0;
    if (!initializeBgfx(window, width, height))
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
    io.IniFilename = "imgui_ucs_viewcube.ini";

    if (!ImGui_ImplSDL3_InitForOther(window))
    {
        std::cerr << "ImGui SDL3 backend failed\n";
        bgfx::shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    imguiBgfxCreate(18.0f);

    cadui::ViewCubeBgfxRenderer viewCubeRenderer;
    if (!viewCubeRenderer.create())
    {
        std::cerr << "ViewCube BGFX renderer unavailable; using ImGui DrawList fallback\n";
    }
    cadui::ViewCubeWidget viewCube;
    cadui::UcsIconWidget ucsIcon;

    CameraState camera;
    std::vector<std::string> ucsNames{ "WCS", "UCS 1" };
    std::string activeUcs = "WCS";
    std::string statusText = "Ready";
    float previousMouseX = 0.0f;
    float previousMouseY = 0.0f;
    bool running = true;
    bool firstMouse = true;
    uint16_t lastWidth = width;
    uint16_t lastHeight = height;

    SDL_ShowWindow(window);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
        width = static_cast<uint16_t>(std::max(1, pixelWidth));
        height = static_cast<uint16_t>(std::max(1, pixelHeight));
        if (width != lastWidth || height != lastHeight)
        {
            bgfx::reset(width, height, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
            bgfx::setViewRect(0, 0, 0, width, height);
            lastWidth = width;
            lastHeight = height;
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_GetMouseState(&mouseX, &mouseY);
        if (firstMouse)
        {
            previousMouseX = mouseX;
            previousMouseY = mouseY;
            firstMouse = false;
        }

        const bool mouseInViewport = !io.WantCaptureMouse;
        if (mouseInViewport && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0)
        {
            camera.yaw += static_cast<double>(mouseX - previousMouseX) * 0.32;
            camera.pitch += static_cast<double>(mouseY - previousMouseY) * 0.32;
            camera.pitch = std::clamp(camera.pitch, -89.5, 89.5);
        }
        previousMouseX = mouseX;
        previousMouseY = mouseY;

        const glm::mat4 view = camera.viewMatrix();
        const glm::mat3 viewRotation(view);

        bgfx::touch(0);
        drawViewCubeOverlay(viewCube, viewCubeRenderer, viewRotation,
                            static_cast<float>(width), static_cast<float>(height),
                            camera, ucsNames, activeUcs, statusText);
        drawUcsOverlay(ucsIcon, viewRotation, static_cast<float>(height), statusText);
        drawStatusBar(statusText, static_cast<float>(width), static_cast<float>(height));

        ImGui::Render();
        imguiBgfxRenderDrawData(ImGui::GetDrawData(), 255);
        bgfx::frame();
    }

    viewCubeRenderer.destroy();
    ImGui_ImplSDL3_Shutdown();
    imguiBgfxDestroy();
    ImGui::DestroyContext();
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
