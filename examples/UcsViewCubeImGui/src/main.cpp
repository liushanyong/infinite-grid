#include <cadui/UcsIcon.hpp>
#include <cadui/ViewCube.hpp>
#include <cadui/ViewCubeBgfx.hpp>
#include <imgui_bgfx.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    glm::quat normalizeQuat(const glm::quat& value)
    {
        const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
        return length > 0.0f ? value / length : glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f };
    }

    glm::quat basisToQuat(const glm::vec3& right, const glm::vec3& up, const glm::vec3& eye)
    {
        return normalizeQuat(glm::quat_cast(glm::mat3{ right, up, eye }));
    }

    // Mirrors OpenCADStudio's Camera::yaw_pitch_to_quat().
    glm::quat yawPitchToQuat(float yaw, float pitch)
    {
        const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.0f, 0.0f, 1.0f });
        const glm::quat pitchRotation = glm::angleAxis(glm::radians(90.0f) - pitch, glm::vec3{ 1.0f, 0.0f, 0.0f });
        return normalizeQuat(yawRotation * pitchRotation);
    }

    struct CameraState
    {
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        double yaw{ 60.0 };
        double pitch{ 25.0 };
        double distance{ 12.0 };
        glm::dvec3 target{ 0.0, 0.0, 0.0 };

        CameraState()
        {
            setYawPitch(yaw, pitch);
        }

        glm::vec3 direction() const
        {
            return rotation * glm::vec3{ 0.0f, 0.0f, 1.0f };
        }

        glm::vec3 up() const
        {
            return rotation * glm::vec3{ 0.0f, 1.0f, 0.0f };
        }

        glm::mat4 viewMatrix() const
        {
            const glm::dvec3 eye = target + glm::dvec3(direction()) * distance;
            return glm::lookAt(glm::vec3{ eye }, glm::vec3{ target }, up());
        }

        void syncYawPitch()
        {
            const glm::vec3 eye = direction();
            pitch = glm::degrees(std::asin(std::clamp(eye.z, -1.0f, 1.0f)));
            yaw = std::abs(eye.x) < 1.0e-6f && std::abs(eye.y) < 1.0e-6f
                ? 0.0
                : glm::degrees(std::atan2(eye.x, -eye.y));
        }

        void setYawPitch(double nextYaw, double nextPitch)
        {
            yaw = nextYaw;
            pitch = std::clamp(nextPitch, -89.9, 89.9);
            rotation = yawPitchToQuat(
                static_cast<float>(glm::radians(yaw)),
                static_cast<float>(glm::radians(pitch)));
        }

        void orbitBy(double deltaYaw, double deltaPitch)
        {
            setYawPitch(yaw + deltaYaw, pitch + deltaPitch);
        }

        void snapToFace(const glm::vec3& eyeDir, const glm::vec3& ucsY, const glm::vec3& ucsZ)
        {
            const glm::vec3 newEye = glm::normalize(eyeDir);
            const glm::vec3 rawRef = std::abs(glm::dot(newEye, ucsZ)) > 0.9f ? ucsY : ucsZ;
            glm::vec3 newUp = rawRef - newEye * glm::dot(rawRef, newEye);
            if (glm::length(newUp) < 1.0e-5f)
            {
                newUp = ucsY;
            }
            newUp = glm::normalize(newUp);
            const glm::vec3 newRight = glm::normalize(glm::cross(newUp, newEye));
            rotation = basisToQuat(newRight, newUp, newEye);
            syncYawPitch();
        }

        void snapToDirection(const glm::vec3& eyeDir, const glm::vec3& ucsY, const glm::vec3& ucsZ)
        {
            const glm::vec3 newEye = glm::normalize(eyeDir);
            const glm::vec3 rawRef = std::abs(glm::dot(newEye, ucsZ)) > 0.9f ? ucsY : ucsZ;
            const glm::vec3 currentUp = up();
            const glm::vec3 upRef = glm::dot(currentUp, rawRef) < 0.0f ? -rawRef : rawRef;
            glm::vec3 projected = upRef - newEye * glm::dot(upRef, newEye);
            if (glm::length(projected) < 1.0e-5f)
            {
                projected = std::abs(glm::dot(newEye, ucsZ)) < 0.99f
                    ? ucsZ - newEye * glm::dot(ucsZ, newEye)
                    : ucsY - newEye * glm::dot(ucsY, newEye);
            }
            const glm::vec3 newUp = glm::normalize(projected);
            const glm::vec3 newRight = glm::normalize(glm::cross(newUp, newEye));
            rotation = basisToQuat(newRight, newUp, newEye);
            syncYawPitch();
        }

        void homeView(const glm::vec3& ucsY, const glm::vec3& ucsZ)
        {
            snapToFace(ucsZ, ucsY, ucsZ);
        }

        void rollBy(float angle)
        {
            const glm::quat delta = glm::angleAxis(angle, glm::vec3{ 0.0f, 0.0f, 1.0f });
            rotation = normalizeQuat(rotation * delta);
            syncYawPitch();
        }

        void nudge90(bool horizontal, bool positive)
        {
            const glm::vec3 axis = horizontal ? up() : rotation * glm::vec3{ 1.0f, 0.0f, 0.0f };
            const float angle = positive ? glm::radians(90.0f) : -glm::radians(90.0f);
            const glm::quat delta = glm::angleAxis(angle, glm::normalize(axis));
            rotation = normalizeQuat(delta * rotation);
            syncYawPitch();
        }
    };

    glm::mat3 ucsRotationFor(const std::string& name)
    {
        if (name == "UCS 1")
        {
            return glm::mat3{ glm::rotate(glm::mat4{ 1.0f }, glm::radians(30.0f), glm::vec3{ 0.0f, 0.0f, 1.0f }) };
        }
        return glm::mat3{ 1.0f };
    }

    cadui::ViewCubeRegion oppositeRegion(const cadui::ViewCubeRegion& region)
    {
        const glm::vec3 target = cadui::ViewCubeWidget::snapDirection(region);
        int bestId = 0;
        float bestDistance = std::numeric_limits<float>::max();
        for (int id = 0; id < 26; ++id)
        {
            const glm::vec3 candidate = cadui::ViewCubeWidget::snapDirection(cadui::ViewCubeWidget::regionById(id));
            const float distance = glm::length(candidate + target);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestId = id;
            }
        }
        return cadui::ViewCubeWidget::regionById(bestId);
    }

    cadui::ViewCubeRegion cardinalFace(int cardinalIndex)
    {
        switch (cardinalIndex)
        {
        case 0: return cadui::ViewCubeWidget::regionById(3); // North -> BACK
        case 1: return cadui::ViewCubeWidget::regionById(4); // East  -> RIGHT
        case 2: return cadui::ViewCubeWidget::regionById(2); // South -> FRONT
        case 3: return cadui::ViewCubeWidget::regionById(5); // West  -> LEFT
        default: break;
        }
        return cadui::ViewCubeWidget::regionById(2);
    }

    bool shouldFlip(const CameraState& camera, const glm::vec3& targetDirection)
    {
        return glm::dot(camera.direction(), glm::normalize(targetDirection)) > 0.9999f;
    }

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

    void snapViewRegion(CameraState& camera,
                        const cadui::ViewCubeRegion& region,
                        const glm::mat3& ucs)
    {
        cadui::ViewCubeRegion target = region;
        if (shouldFlip(camera, cadui::ViewCubeWidget::snapDirection(target)))
        {
            target = oppositeRegion(target);
        }

        const glm::vec3 eyeDir = ucs * cadui::ViewCubeWidget::snapDirection(target);
        if (target.kind == cadui::RegionKind::Face)
        {
            camera.snapToFace(eyeDir, glm::normalize(ucs[1]), glm::normalize(ucs[2]));
        }
        else
        {
            camera.snapToDirection(eyeDir, glm::normalize(ucs[1]), glm::normalize(ucs[2]));
        }
    }

    void snapWorldFace(CameraState& camera, const cadui::ViewCubeRegion& region)
    {
        cadui::ViewCubeRegion target = region;
        if (shouldFlip(camera, cadui::ViewCubeWidget::snapDirection(target)))
        {
            target = oppositeRegion(target);
        }
        camera.snapToFace(
            cadui::ViewCubeWidget::snapDirection(target),
            glm::vec3{ 0.0f, 1.0f, 0.0f },
            glm::vec3{ 0.0f, 0.0f, 1.0f });
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

        const glm::mat3 ucs = ucsRotationFor(activeUcs);
        const cadui::ViewCubeResult result = widget.render("CadViewCube", ImVec2(160.0f, 160.0f), viewRotation, ucs, options);
        switch (result.action.kind)
        {
        case cadui::ViewCubeActionKind::Region:
            snapViewRegion(camera, result.action.region, ucs);
            statusText = std::string("View: ") + result.action.region.label;
            break;
        case cadui::ViewCubeActionKind::Cardinal:
        {
            const cadui::ViewCubeRegion worldFace = cardinalFace(result.action.region.index);
            snapWorldFace(camera, worldFace);
            statusText = std::string("View: ") + result.action.region.label;
            break;
        }
        case cadui::ViewCubeActionKind::Home:
            camera.homeView(glm::normalize(ucs[1]), glm::normalize(ucs[2]));
            statusText = "View: Home";
            break;
        case cadui::ViewCubeActionKind::RollLeft:
            camera.rollBy(-glm::radians(90.0f));
            statusText = "ViewCube: roll left";
            break;
        case cadui::ViewCubeActionKind::RollRight:
            camera.rollBy(glm::radians(90.0f));
            statusText = "ViewCube: roll right";
            break;
        case cadui::ViewCubeActionKind::NudgeUp:
            camera.nudge90(false, false);
            statusText = "ViewCube: nudge up";
            break;
        case cadui::ViewCubeActionKind::NudgeDown:
            camera.nudge90(false, true);
            statusText = "ViewCube: nudge down";
            break;
        case cadui::ViewCubeActionKind::NudgeLeft:
            camera.nudge90(true, false);
            statusText = "ViewCube: nudge left";
            break;
        case cadui::ViewCubeActionKind::NudgeRight:
            camera.nudge90(true, true);
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
                        const glm::mat3& ucs,
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
        state.xAxis = glm::normalize(ucs[0]);
        state.yAxis = glm::normalize(ucs[1]);
        state.zAxis = glm::normalize(ucs[2]);
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
    const bool autoCapture = argc > 1 &&
        (SDL_strcasecmp(argv[1], "--screenshot") == 0 ||
         SDL_strcasecmp(argv[1], "/screenshot") == 0);

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

    imguiBgfxCreate(18.0f, "C:/Windows/Fonts/msyh.ttc");

    cadui::ViewCubeBgfxRenderer viewCubeRenderer;
    if (!viewCubeRenderer.create())
    {
        std::cerr << "ViewCube BGFX renderer unavailable; using ImGui DrawList fallback\n";
    }
    else
    {
        viewCubeRenderer.setFontTexture(imguiBgfxGetFontTexture());
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
    uint32_t captureFrame = 0;
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
            camera.orbitBy(
                static_cast<double>(mouseX - previousMouseX) * 0.32,
                static_cast<double>(mouseY - previousMouseY) * 0.32);
        }
        previousMouseX = mouseX;
        previousMouseY = mouseY;

        const glm::mat4 view = camera.viewMatrix();
        const glm::mat3 viewRotation(view);
        // OpenCADStudio rotates the ViewCube and compass with camera.rotation
        // (camera-local -> world).  glm::lookAt gives the inverse view basis,
        // so passing viewRotation transposes the navigation aid.
        const glm::mat3 cameraRotation = glm::mat3_cast(camera.rotation);
        const glm::mat3 ucs = ucsRotationFor(activeUcs);

        bgfx::touch(0);
        drawViewCubeOverlay(viewCube, viewCubeRenderer, cameraRotation,
                            static_cast<float>(width), static_cast<float>(height),
                            camera, ucsNames, activeUcs, statusText);
        drawUcsOverlay(ucsIcon, viewRotation, ucs, static_cast<float>(height), statusText);
        drawStatusBar(statusText, static_cast<float>(width), static_cast<float>(height));

        ImGui::Render();
        imguiBgfxRenderDrawData(ImGui::GetDrawData(), 255);
        bgfx::frame();
        if (autoCapture && ++captureFrame == 45)
        {
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "E:/infinite-grid/viewcube_clone_shot");
        }
        if (autoCapture && captureFrame >= 75)
        {
            running = false;
        }
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
