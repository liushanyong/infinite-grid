# ============================================================================
#  lib/bimg/CMakeLists.txt  --  bimg + bimg_decode + bimg_encode (SLN mirror)
#  ============================================================================
#  Mirrors build/bimg/{bimg,bimg_decode,bimg_encode}.vcxproj from the
#  reference SLN.  bimg.vcxproj only compiled image.cpp + image_gnf.cpp, but
#  the CMake aggregate also pulls in image_encode.cpp + image_cubemap_filter.cpp
#  so the same libbimg.a contains decode + encode + astc encoder, matching
#  what bgfx actually links against.

cmake_minimum_required(VERSION 3.10)

# --- bimg (combined decode + encode + astc encoder) ---
project(bimg LANGUAGES CXX)

file(GLOB ASTC_ENCODER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/astc-encoder/source/*.cpp)

add_library(bimg STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image_gnf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image_encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image_cubemap_filter.cpp
    ${ASTC_ENCODER_SOURCES}
)
cad_3rdparty_setup(bimg)

target_include_directories(bimg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(bimg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty                      # for <libsquish/...>, <etc1/...>, <etc2/...>, <nvtt/...>, <pvrtc/...>, <edtaa3/...>, <stb/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/astc-encoder/include # for <astcenc.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/iqa/include         # for <iqa.h>, <iqa_os.h>, <ssim.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bimg PUBLIC bx)

target_compile_definitions(bimg PUBLIC
    BIMG_DECODE BIMG_ENCODE BIMG_STATIC)

if(MSVC)
    target_compile_options(bimg PRIVATE /W3 /bigobj /EHsc)
endif()

# --- bimg_encode (encode-only) ---
if(CAD_USE_BIMG_ENCODE)
    add_library(bimg_encode STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/image_encode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/image_cubemap_filter.cpp
    )
    cad_3rdparty_setup(bimg_encode)
    target_include_directories(bimg_encode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(bimg_encode PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/astc-encoder/include
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/iqa/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bimg_encode PUBLIC bx)
    target_compile_definitions(bimg_encode PUBLIC BIMG_ENCODE BIMG_STATIC)
    if(MSVC)
        target_compile_options(bimg_encode PRIVATE /W3 /bigobj /EHsc)
    endif()
endif()

# --- bimg_decode (decode-only) ---
if(CAD_USE_BIMG_DECODE)
    file(GLOB LODEPNG_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lodepng/*.cpp)
    add_library(bimg_decode STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/image_decode.cpp
        ${LODEPNG_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tinyexr/deps/miniz/miniz.c
    )
    cad_3rdparty_setup(bimg_decode)
    target_include_directories(bimg_decode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(bimg_decode PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lodepng
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tinyexr/deps/miniz
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tinygltf/deps/miniz
    )
    target_link_libraries(bimg_decode PUBLIC bx)
    target_compile_definitions(bimg_decode PUBLIC BIMG_DECODE)
endif()
