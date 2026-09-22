cmake_minimum_required(VERSION 3.20)

project(UcsViewCubeImGui VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /Zc:preprocessor)
endif()

set(EXAMPLE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(REPO_ROOT "${EXAMPLE_ROOT}/../..")
set(LIB_ROOT "${REPO_ROOT}/lib")
set(PORT_ROOT "${REPO_ROOT}/ports/OpenCADImGuiPort")

include("${REPO_ROOT}/cmake/Standard.cmake")

# SDL3
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
set(SDL_UNINSTALL    OFF CACHE BOOL "" FORCE)
set(SDL_FRAMEWORK    OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL       OFF CACHE BOOL "" FORCE)
set(SDL_OPENLES      OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES     OFF CACHE BOOL "" FORCE)

add_subdirectory("${LIB_ROOT}/SDL" "${CMAKE_BINARY_DIR}/_subbuild/SDL3" EXCLUDE_FROM_ALL)

# bgfx family
set(BX_ROOT   "${LIB_ROOT}/bx")
set(BIMG_ROOT "${LIB_ROOT}/bimg")
set(BGFX_ROOT "${LIB_ROOT}/bgfx")

add_subdirectory("${LIB_ROOT}/bx"   "${CMAKE_BINARY_DIR}/_subbuild/bx"   EXCLUDE_FROM_ALL)
add_subdirectory("${LIB_ROOT}/bimg" "${CMAKE_BINARY_DIR}/_subbuild/bimg" EXCLUDE_FROM_ALL)
add_subdirectory("${LIB_ROOT}/bgfx" "${CMAKE_BINARY_DIR}/_subbuild/bgfx" EXCLUDE_FROM_ALL)

# GLM
add_library(glm INTERFACE)
add_library(glm::glm ALIAS glm)
target_include_directories(glm INTERFACE "${LIB_ROOT}/glm")
target_compile_definitions(glm INTERFACE GLM_FORCE_RADIANS)

# Dear ImGui
set(IMGUI_CORE_SOURCES
    "${LIB_ROOT}/imgui/imgui.cpp"
    "${LIB_ROOT}/imgui/imgui_draw.cpp"
    "${LIB_ROOT}/imgui/imgui_tables.cpp"
    "${LIB_ROOT}/imgui/imgui_widgets.cpp"
    "${LIB_ROOT}/imgui/imgui_demo.cpp"
)

add_library(imgui STATIC
    ${IMGUI_CORE_SOURCES}
    "${LIB_ROOT}/imgui/backends/imgui_impl_sdl3.cpp"
)

target_include_directories(imgui PUBLIC
    "${LIB_ROOT}/imgui"
    "${LIB_ROOT}/imgui/backends"
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

if(WIN32)
    target_link_libraries(imgui PRIVATE legacy_stdio_definitions)
elseif(UNIX)
    target_link_libraries(imgui PRIVATE m)
endif()

# The bundled ImGui -> bgfx backend from the OpenCADStudio port.
add_library(imgui_bgfx STATIC
    "${PORT_ROOT}/src/backend/imgui_bgfx.cpp"
)

target_include_directories(imgui_bgfx PUBLIC
    "${PORT_ROOT}/src/backend"
    "${LIB_ROOT}/imgui"
    "${LIB_ROOT}/imgui/backends"
    "${LIB_ROOT}/bgfx/3rdparty"
    "${LIB_ROOT}/bgfx/examples/common-/imgui"
    "${LIB_ROOT}/bgfx/include"
    "${LIB_ROOT}/bx/include"
    "${LIB_ROOT}/bimg/include"
)

target_link_libraries(imgui_bgfx PUBLIC imgui bgfx bx bimg)

# Reusable CAD UI widgets.
add_subdirectory("${LIB_ROOT}/cadui" "${CMAKE_BINARY_DIR}/cadui")

add_executable(UcsViewCubeImGui
    src/main.cpp
)

target_include_directories(UcsViewCubeImGui PRIVATE
    "${LIB_ROOT}/SDL/include"
    "${LIB_ROOT}/SDL/include/SDL3"
)

target_link_libraries(UcsViewCubeImGui PRIVATE
    SDL3::SDL3
    imgui
    imgui_bgfx
    cadui
    bgfx
    bx
    bimg
)
