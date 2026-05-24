cmake_minimum_required(VERSION 3.15)
project(nightjava-client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Java REQUIRED)
find_package(JNI REQUIRED)

add_library(nightjava-client SHARED
    src/particle_native.cpp
    src/particle_physics.cpp
    src/particle_render.cpp
    src/entity_collision.cpp
    src/render_frustum.cpp
    src/light_native.cpp
    src/light_propagation.cpp
)

target_include_directories(nightjava-client PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(nightjava-client PRIVATE /O2)
else()
    target_compile_options(nightjava-client PRIVATE -O3 -msse4.1 -mavx2)
endif()

set_target_properties(nightjava-client PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
