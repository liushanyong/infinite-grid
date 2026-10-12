cmake_minimum_required(VERSION 3.10)
project(tinygltf)

set(CMAKE_CXX_STANDARD 11)

add_library(tinygltf STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/json.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/tiny_gltf.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/tiny_gltf.cpp"
)

target_include_directories(tinygltf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

# The upstream /MD->/MT runtime override is dropped on purpose: the rest of
# this project builds with the CMake-default /MD runtime and linking an /MT
# static lib into it fails with a CRT mismatch (LNK2038).
