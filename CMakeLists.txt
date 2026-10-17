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
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" WINDOW_SOURCE_DIR)

# Options must be declared before the bundled targets are created.  bgfx
# requires /Zc:__cplusplus on MSVC, and SDL3's non-ASCII headers need /utf-8.
# /bigobj: main.cpp's entity dispatch (variant visitors + per-type demo
# builders) outgrew the default ~64k-section object file limit (C1128).
if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /Zc:preprocessor /bigobj)
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

# AcGe: value types keep small accessors inline; algorithm-bearing bodies
# live in per-class translation units (declaration/definition split so the
# lib can later be flipped to a DLL behind an export macro).
add_library(acge STATIC
    "${CMAKE_SOURCE_DIR}/external/predicates/predicates.c"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGe2d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeBoundedPlane.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeBspline.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeBvh.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeClipBoundary2d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeConic.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeCurve3d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeCurves2d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeDelaunay2d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeIntersect.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeIntersect3d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeInterval.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeKdTree.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeLinear.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeMatrix.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeMatrix2d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeNurbsCore.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeNurbsSpline.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeOctree.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeOffset.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGePoint.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGePolyline.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeProject.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeQBvh.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeQuadtree.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeQuaternion.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeScale.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeSurface3d.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeTol.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeTriangle.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeUtil.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeKnotVector.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeCubicSpline.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acge/AcGeNurbCurve.cpp"
)
target_include_directories(acge PUBLIC "${CMAKE_SOURCE_DIR}/lib")
target_link_libraries(acge PUBLIC glm::glm)
if(MSVC)
    target_compile_options(acge PRIVATE /fp:precise)
endif()

option(ACGE_BUILD_TESTS "Build standalone AcGe geometry regression tests" OFF)
if(ACGE_BUILD_TESTS)
    enable_testing()
    foreach(test_name IN ITEMS
        AcGeClipBoundaryTests
        AcGeCompositeCurveTests
        AcGeOffsetCurveTests
        AcGeSweptSurfaceTests
        AcGeDelaunay2dTests
        AcGeIntersect3dTests
        AcGeKdTreeTests
        AcGeKnotVectorTests
        AcGeCubicSplineTests
        AcGeNurbCurveTests
        AcGePointOnCurveTests
        AcGePointOnSurfaceTests
        AcGeAugPolylineTests
    )
        add_executable(${test_name}
            "${CMAKE_SOURCE_DIR}/ports/OpenCADImGuiPort/tests/${test_name}.cpp")
        target_link_libraries(${test_name} PRIVATE acge)
        if(MSVC)
            target_compile_options(${test_name} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_name} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

add_library(acgi STATIC
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiDrawable.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiEntityDrawable.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiWorldDraw.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiTextEngine.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiLineType.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgi/AcGiTextQueue.cpp"
    "${CMAKE_SOURCE_DIR}/lib/text/text_font.cpp"
    "${CMAKE_SOURCE_DIR}/lib/text/shx_parser.cpp"
)
target_include_directories(acgi PUBLIC "${CMAKE_SOURCE_DIR}/lib")
target_link_libraries(acgi PUBLIC acge)

add_library(acgs STATIC
    "${CMAKE_SOURCE_DIR}/lib/acgs/AcGsPicking.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/AcGsOrbitCamera.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/AcGsView.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/AcGsManager.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/AcGsSelectionManager.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/DocumentSceneBridge.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/EcsScene.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acgs/model/DrawContext.cpp"
)
target_include_directories(acgs
    PUBLIC "${CMAKE_SOURCE_DIR}/lib"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/lib/SDL/include"
        "${CMAKE_SOURCE_DIR}/lib/SDL/include/SDL3"
)
target_link_libraries(acgs
    PUBLIC acge acgi
    PRIVATE SDL3::SDL3
)

add_library(acdb STATIC
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbDatabase.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbDictionary.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbJson.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbStore.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbFileIo.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbLibredwgFileCodec.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbGltf.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbTransaction.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbDwgBridge.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbDwgWriteBridge.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbTessellate.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbEntityWorldDraw.cpp"
    "${CMAKE_SOURCE_DIR}/lib/acdb/AcDbTransform.cpp"
)
target_include_directories(acdb PUBLIC "${CMAKE_SOURCE_DIR}/lib")
target_link_libraries(acdb PUBLIC acge acgi)

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
    lib/application/ApplicationPlatform.cpp
    lib/application/DemoSceneVisibility.cpp
    lib/rendering/RendererBackend.cpp
    lib/rendering/BgfxRenderer.cpp
    lib/rendering/ImGuiBgfxPass.cpp
    lib/rendering/WebGpuRenderer.cpp
    lib/rendering/ProceduralMesh.cpp
    lib/brep/BRep.cpp
)

add_executable(WINDOW ${WINDOW_SOURCES})
target_compile_definitions(WINDOW PRIVATE
    GRID_SOURCE_DIR="${WINDOW_SOURCE_DIR}")

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
    acge
    acgi
    acgs
    acdb
    imgui
    imgui_bgfx
    cadui
    bgfx
    bx
    bimg
    bimg_decode
)
# ---------------------------------------------------------------------------
# libredwg -- DWG/DXF codec library (static, minimal build)
# DISABLE_WRITE must stay OFF: the file-IO bridge (lib/acdb/AcDbFileIo)
# needs encode.c for dwg_write_file (binary DWG out) and in_dxf.c for
# dxf_read_file (DXF in). out_dxf/out_dxfb are compiled regardless.
# ---------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF)
set(LIBREDWG_LIBONLY ON)
set(LIBREDWG_DISABLE_WRITE OFF)
set(LIBREDWG_DISABLE_JSON ON)
set(ENABLE_LTO OFF)
if(MSVC)
    set(WINDOW_LIBREDWG_TARGET libredwg)
else()
    set(WINDOW_LIBREDWG_TARGET redwg)
endif()
add_subdirectory("${CMAKE_SOURCE_DIR}/lib/libredwg"
                 "${CMAKE_BINARY_DIR}/libredwg")

# The DXF writer surface (out_dxf.h) sits behind the library-private
# bits.h, which includes the generated config.h; expose both the source
# dir (bits.h/out_dxf.h) and the build dir (config.h) to the AcDb codec.
target_include_directories(acdb PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/libredwg/src"
    "${CMAKE_BINARY_DIR}/libredwg/src")

# ---------------------------------------------------------------------------
# tinygltf -- glTF 2.0 loader/writer (vendored, single-TU static lib).
# Backs the acdb glTF bridge (lib/acdb/AcDbGltf): import decodes .glb/.gltf
# into AcDbPolyFaceMesh entities, export serializes them back via
# tinygltf::WriteGltfSceneToFile.
# ---------------------------------------------------------------------------
add_subdirectory("${CMAKE_SOURCE_DIR}/lib/tinygltf"
                 "${CMAKE_BINARY_DIR}/lib/tinygltf")

# ---------------------------------------------------------------------------
# entt — header-only ECS. The scene/attribute layer (acgs::SceneStore)
# keys its entities by AcDbHandle so the ECS never becomes a second ID
# space; the topology layer stays in the brep arena per the roadmap
# ("拓扑层不能上 ECS").
# ---------------------------------------------------------------------------
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE
    "${CMAKE_SOURCE_DIR}/lib/entt/include")
target_link_libraries(WINDOW PRIVATE entt)
# SceneStore (acgs/EcsScene) is compiled in the acgs target and includes
# entt directly, so the header-only dependency must be visible here too.
target_link_libraries(acgs PUBLIC entt)

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

# AcDb's codecs and persistence are implementation dependencies.  Link them
# after their bundled targets have been declared.
target_link_libraries(acdb PRIVATE
    "${WINDOW_LIBREDWG_TARGET}"
    SQLiteCpp
    tinygltf
)

# ---------------------------------------------------------------------------
# fileio_repro -- minimal ASAN-instrumented repro for the DXF second-
# generation write path (tools/fileio_repro.cpp).  Not part of WINDOW.
# ---------------------------------------------------------------------------
add_executable(fileio_repro tools/fileio_repro.cpp)
target_compile_definitions(fileio_repro PRIVATE
    GRID_SOURCE_DIR="${WINDOW_SOURCE_DIR}")
target_include_directories(fileio_repro PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
    "${CMAKE_SOURCE_DIR}/lib/libredwg/src"
    "${CMAKE_BINARY_DIR}/libredwg/src")
target_link_libraries(fileio_repro PRIVATE acdb)
if(MSVC)
    target_compile_options(fileio_repro PRIVATE /fsanitize=address)
    target_link_options(fileio_repro PRIVATE /INCREMENTAL:NO)
endif()

# wgpu-native is dynamically loaded so a missing optional runtime still allows
# every legacy backend to build and run.
if(EXISTS "${CMAKE_SOURCE_DIR}/lib/wgpu/bin/wgpu_native-release.dll")
    add_custom_command(TARGET WINDOW POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/lib/wgpu/bin/wgpu_native-release.dll"
                "$<TARGET_FILE_DIR:WINDOW>"
    )
endif()
