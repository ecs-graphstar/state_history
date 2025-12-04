# Default Paphos project starter
# flecs/PyTorch with AOT Inductor pipeline

cmake_minimum_required(VERSION 3.18)
project(project)

# add_definitions(AOTInductor_CUDA)
add_executable(${PROJECT_NAME} src/main.cpp)
include_directories("src")

# Flecs
include_directories("deps/flecs-4.1.1/include")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build Flecs as a shared library" FORCE)
add_subdirectory("deps/flecs-4.1.1")

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("deps/glfw")

# GLAD
add_library(glad STATIC
    deps/glad/src/glad.c
)
target_include_directories(glad PUBLIC deps/glad/include)

# NanoVG
add_library(nanovg STATIC
    deps/nanovg/src/nanovg.c
)
target_include_directories(nanovg PUBLIC deps/nanovg/src)
target_compile_definitions(nanovg PUBLIC NANOVG_GL2_IMPLEMENTATION)

target_link_libraries(${PROJECT_NAME} flecs glfw glad nanovg)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
