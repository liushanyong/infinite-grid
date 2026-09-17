# ============================================================================
#  lib/glm/CMakeLists.txt  --  glm header-only interface (SLN mirror)
#  ============================================================================
#  Mirrors build/glm/glm.vcxproj from E:\CADplatformer\build\CADplatformer.sln.
#  glm is header-only; we expose it as an INTERFACE library so callers can do
#  target_link_libraries(glm ...).

cmake_minimum_required(VERSION 3.10)

project(glm)

# Header-only libraries do not generate a vcxproj.  Use a minimal
# static lib with one no-op TU to produce lib/glm/glm.vcxproj matching the
# reference SLN entry.
add_library(glm STATIC ${CMAKE_CURRENT_SOURCE_DIR}/glm_dummy.cpp)
cad_3rdparty_setup(glm)

target_include_directories(glm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
