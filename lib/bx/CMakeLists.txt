# ============================================================================
#  lib/bx/CMakeLists.txt  --  bx static library (SLN mirror)
#  ============================================================================
#  Mirrors build/bx/bx.vcxproj from E:\CADplatformer\build\CADplatformer.sln.
#  Compiled source list taken from the original vcxproj ClCompile entries.

cmake_minimum_required(VERSION 3.10)

project(bx LANGUAGES CXX)

# Source files as listed in build/bx/bx.vcxproj
file(GLOB BX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# The individual translation units are sufficient; amalgamated.cpp would
# compile the same implementation files a second time into this target.
list(REMOVE_ITEM BX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/amalgamated.cpp")

add_library(bx STATIC ${BX_SOURCES})
cad_3rdparty_setup(bx)

target_include_directories(bx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/msvc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
)

target_compile_definitions(bx PUBLIC BX_CONFIG_DEBUG=0)

if(MSVC)
    target_compile_options(bx PRIVATE /W3)
endif()
