cmake_minimum_required(VERSION 3.15)
project(nightjava-particles VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Java REQUIRED)
find_package(JNI REQUIRED)

add_library(nightjava-particles SHARED
    src/particle_native.cpp
    src/particle_physics.cpp
    src/particle_render.cpp
    src/datafixer_native.cpp
)

target_include_directories(nightjava-particles PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(nightjava-particles PRIVATE /O2)
else()
    target_compile_options(nightjava-particles PRIVATE -O3 -msse4.1 -mavx2)
endif()

set_target_properties(nightjava-particles PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
