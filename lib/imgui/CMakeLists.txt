cmake_minimum_required(VERSION 3.10)
project(ImGui)

# Set C++17 standard for ImGuizmo compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link against static runtime library (MT) to match main project
if(WIN32)
    set(cxx_compiler_flags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
    )
    foreach(cxx_compiler_flag ${cxx_compiler_flags})
        string(REPLACE "/MD" "/MT" ${cxx_compiler_flag} "${${cxx_compiler_flag}}")
    endforeach()
endif()

# ImGui core sources
set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imstb_rectpack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imstb_textedit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imstb_truetype.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h
)

# Backends - use SDL3 backend
set(IMGUI_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_sdl3.h
)

# ImGuizmo sources
set(IMGUIZMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo.h
)

# Add header-only sources as STATIC library
add_library(ImGui STATIC ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES} ${IMGUIZMO_SOURCES})
target_include_directories(ImGui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ImGui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../SDL/include)

# Link required libraries
if(WIN32)
    target_link_libraries(ImGui PRIVATE legacy_stdio_definitions)
endif()

# Link math library for non-Windows platforms
if(UNIX)
    target_link_libraries(ImGui PRIVATE m)
endif()
