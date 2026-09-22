set(CADUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CADUI_REPO_ROOT "${CADUI_ROOT}/../..")
set(CADUI_LIB_ROOT "${CADUI_REPO_ROOT}/lib")

add_library(cadui STATIC
    UcsIcon.cpp
    ViewCube.cpp
    ViewCubeBgfx.cpp
)

target_include_directories(cadui
    PUBLIC
        "${CADUI_ROOT}"
        "${CADUI_LIB_ROOT}"
        "${CADUI_LIB_ROOT}/shaders/viewcubeFace"
)

target_link_libraries(cadui
    PUBLIC
        imgui
        glm::glm
        bgfx
        bx
)

if(MSVC)
    target_compile_options(cadui PRIVATE /utf-8 /Zc:__cplusplus /Zc:preprocessor)
endif()
