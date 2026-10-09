cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Project setup
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Put runtime files (DLLs, shaders/...) next to the produced executable so
# the app can be launched directly from the build folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

project(WINDOW VERSION 1.0.0 LANGUAGES C CXX)

# Options must be declared before the bundled targets are created.  bgfx
# requires /Zc:__cplusplus on MSVC, and SDL3's non-ASCII headers need /utf-8.
if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /Zc:preprocessor)
endif()

# ---------------------------------------------------------------------------
# Third-party locations bundled inside the repository
# ---------------------------------------------------------------------------
set(WINDOW_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/lib")
set(WINDOW_SDL_DIR          "${CMAKE_SOURCE_DIR}/lib/SDL")
set(WINDOW_GLM_DIR          "${CMAKE_SOURCE_DIR}/lib/glm")
set(BX_ROOT                "${CMAKE_SOURCE_DIR}/lib/bx")
set(BIMG_ROOT              "${CMAKE_SOURCE_DIR}/lib/bimg")
set(BGFX_ROOT              "${CMAKE_SOURCE_DIR}/lib/bgfx")

# Mesh CAD rendering needs runtime decoding of PNG/JPG albedo textures.
set(CAD_USE_BIMG_DECODE ON CACHE BOOL "" FORCE)

# The bundled bgfx family uses these helpers from its reference build.
include("${CMAKE_SOURCE_DIR}/cmake/Standard.cmake")

# ---------------------------------------------------------------------------
# SDL3 -- built from source under lib/SDL
# ---------------------------------------------------------------------------
# Build only the static library and skip the test/example sub-projects so
# the configure/build stays fast.  Override with -DSDL_SHARED=ON if you
# would rather have a SDL3.dll next to the executable.
set(SDL_SHARED       OFF CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "Build SDL3 as a static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3_test library"     FORCE)
set(SDL_TESTS        OFF CACHE BOOL "Build the SDL3 test programs"    FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "Build the SDL3 example programs" FORCE)

# We pull SDL3 in as a sub-project, so disable its install / uninstall
# rules -- they only make sense when SDL3 is the top-level project.
set(SDL_INSTALL      OFF CACHE BOOL "Generate SDL3 install rules"     FORCE)
set(SDL_UNINSTALL    OFF CACHE BOOL "Generate SDL3 uninstall target"  FORCE)
set(SDL_FRAMEWORK    OFF CACHE BOOL "Build SDL3 as an Apple framework" FORCE)

# Skip OpenGL and OpenGL ES support in SDL for this project build.
set(SDL_OPENGL       OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES     OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_SOURCE_DIR}/lib/SDL"
                   "${CMAKE_BINARY_DIR}/_subbuild/SDL3"
                   EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# bgfx -- rendering library under lib/bgfx
# ---------------------------------------------------------------------------
# bgfx depends on bx and bimg, so configure them first.  Keeping all three
# out of the default "all" target avoids building upstream tools and tests.
add_subdirectory("${BX_ROOT}"
                  "${CMAKE_BINARY_DIR}/_subbuild/bx"
                  EXCLUDE_FROM_ALL)

add_subdirectory("${BIMG_ROOT}"
                  "${CMAKE_BINARY_DIR}/_subbuild/bimg"
                  EXCLUDE_FROM_ALL)

add_subdirectory("${BGFX_ROOT}"
                  "${CMAKE_BINARY_DIR}/_subbuild/bgfx"
                  EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# glm -- header-only math library under lib/glm
# ---------------------------------------------------------------------------
# Keep glm as a root INTERFACE target instead of the bundled SLN mirror; this
# avoids building its dummy static-library source while preserving the same
# public include layout and alias.
add_library(glm INTERFACE)
add_library(glm::glm ALIAS glm)
target_include_directories(glm INTERFACE "${WINDOW_GLM_DIR}")
# GLM uses some intrinsics; the upstream defaults are the right baseline.
target_compile_definitions(glm INTERFACE GLM_FORCE_RADIANS)

# ---------------------------------------------------------------------------
# Dear ImGui + BGFX overlay widgets
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    "${CMAKE_SOURCE_DIR}/lib/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/imgui_demo.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/backends/imgui_impl_sdl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${CMAKE_SOURCE_DIR}/lib/imgui"
    "${CMAKE_SOURCE_DIR}/lib/imgui/backends"
)
target_compile_definitions(imgui PUBLIC IMGUI_API=)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
if(WIN32)
    target_link_libraries(imgui PRIVATE legacy_stdio_definitions)
elseif(UNIX)
    target_link_libraries(imgui PRIVATE m)
endif()

add_library(imgui_bgfx STATIC
    "${CMAKE_SOURCE_DIR}/ports/OpenCADImGuiPort/src/backend/imgui_bgfx.cpp"
)
target_include_directories(imgui_bgfx PUBLIC
    "${CMAKE_SOURCE_DIR}/ports/OpenCADImGuiPort/src/backend"
    "${CMAKE_SOURCE_DIR}/lib/imgui"
    "${CMAKE_SOURCE_DIR}/lib/imgui/backends"
    "${CMAKE_SOURCE_DIR}/lib/bgfx/3rdparty"
    "${CMAKE_SOURCE_DIR}/lib/bgfx/examples/common-/imgui"
    "${CMAKE_SOURCE_DIR}/lib/bgfx/include"
    "${CMAKE_SOURCE_DIR}/lib/bx/include"
    "${CMAKE_SOURCE_DIR}/lib/bimg/include"
)
target_link_libraries(imgui_bgfx PUBLIC imgui bgfx bx bimg)

add_subdirectory("${CMAKE_SOURCE_DIR}/lib/cadui"
                 "${CMAKE_BINARY_DIR}/lib/cadui")


# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
set(WINDOW_SOURCES
    main.cpp
    lib/rendering/RendererBackend.cpp
    lib/rendering/BgfxRenderer.cpp
    lib/rendering/WebGpuRenderer.cpp
    lib/rendering/ProceduralMesh.cpp
    lib/text/text_font.cpp
    lib/text/shx_parser.cpp
    lib/acgi/AcGiTextEngine.cpp
    lib/acgs/AcGsView.cpp
    lib/acgs/AcGsManager.cpp
    lib/acdb/AcDbDatabase.cpp
    lib/acdb/AcDbStore.cpp
    lib/brep/BRep.cpp
    lib/acdb/AcDbTransaction.cpp
    lib/acgs/AcGsSelectionHighlighter.cpp
    lib/acgs/AcGsSelectionManager.cpp
)

add_executable(WINDOW ${WINDOW_SOURCES})

# Headers:
#   * lib/SDL/include           -- so SDL3's own headers can do
#                                  #include <SDL3/SDL_xxx.h>
#   * lib/SDL/include/SDL3      -- for the legacy #include <SDL.h> style
#   * lib                        -- for "lib/camera/*.h" and the shader
#                                  directory layout
target_include_directories(WINDOW PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/SDL/include"
    "${CMAKE_SOURCE_DIR}/lib/SDL/include/SDL3"
    "${CMAKE_SOURCE_DIR}/lib"
    "${CMAKE_SOURCE_DIR}/lib/bgfx/3rdparty"
    "${CMAKE_SOURCE_DIR}/lib/wgpu/include"
)

target_link_libraries(WINDOW PRIVATE
    SDL3::SDL3
    glm::glm
    imgui
    imgui_bgfx
    cadui
    bgfx
    bx
    bimg
    bimg_decode
)
# ---------------------------------------------------------------------------
# libredwg -- DWG decode library (static, read-only, minimal build)
# ---------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF)
set(LIBREDWG_LIBONLY ON)
set(LIBREDWG_DISABLE_WRITE ON)
set(LIBREDWG_DISABLE_JSON ON)
set(ENABLE_LTO OFF)
add_subdirectory("${CMAKE_SOURCE_DIR}/lib/libredwg"
                 "${CMAKE_BINARY_DIR}/libredwg")

target_link_libraries(WINDOW PRIVATE libredwg)

# Shewchuk robust predicates (public domain): exact-sign orient2d/3d,
# incircle, insphere for CPU-side sign decisions (picking degeneracies,
# future triangulation/intersection).  The translation unit MUST keep
# IEEE double semantics: fast-math silently destroys the exactness that
# the ge:: wrappers rely on, so /fp:precise is locked on this target.
add_library(predicates STATIC
    "${CMAKE_SOURCE_DIR}/external/predicates/predicates.c"
    "${CMAKE_SOURCE_DIR}/external/predicates/predicates.h")
target_include_directories(predicates PUBLIC
    "${CMAKE_SOURCE_DIR}/external/predicates")
if(MSVC)
    target_compile_options(predicates PRIVATE /fp:precise)
endif()

target_link_libraries(WINDOW PRIVATE predicates)

# ---------------------------------------------------------------------------
# entt — header-only ECS.  The scene/attribute layer (acgs::SceneStore)
# keys its entities by AcDbHandle so the ECS never becomes a second ID
# space; the topology layer stays in the brep arena per the roadmap
# ("拓扑层不能上 ECS").
# ---------------------------------------------------------------------------
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE
    "${CMAKE_SOURCE_DIR}/lib/entt/include")
target_link_libraries(WINDOW PRIVATE entt)

# ---------------------------------------------------------------------------
# SQLiteCpp — MIT C++ wrapper over its bundled public-domain sqlite3
# amalgamation (no external sqlite dependency).  Backs the AcDb document
# store (lib/acdb/AcDbStore): entities + symbol tables + the operation
# journal, opened in WAL mode.
# ---------------------------------------------------------------------------
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SQLITECPP_INTERNAL_SQLITE ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/lib/SQLiteCpp"
                 "${CMAKE_BINARY_DIR}/SQLiteCpp")
target_link_libraries(WINDOW PRIVATE SQLiteCpp)

# wgpu-native is dynamically loaded so a missing optional runtime still allows
# every legacy backend to build and run.
if(EXISTS "${CMAKE_SOURCE_DIR}/lib/wgpu/bin/wgpu_native-release.dll")
    add_custom_command(TARGET WINDOW POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/lib/wgpu/bin/wgpu_native-release.dll"
                "$<TARGET_FILE_DIR:WINDOW>"
    )
endif()
