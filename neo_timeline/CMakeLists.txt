cmake_minimum_required(VERSION 3.18)
project(timeline)

set(DEBUG_FLAGS "-g -O1 -pthread -pedantic -fpermissive")
set(RELEASE_FLAGS "-O3 -pthread -pedantic -fpermissive")
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add custom command to generate state_history.h from components.h
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/include/state_history.h
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/generate_state_history.py
    DEPENDS ${CMAKE_SOURCE_DIR}/include/components.h
            ${CMAKE_SOURCE_DIR}/templates/state_history.h.j2
            ${CMAKE_SOURCE_DIR}/generate_state_history.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating state_history.h from components.h"
    VERBATIM
)

# Create custom target for generated header
add_custom_target(generate_state_history
    DEPENDS ${CMAKE_SOURCE_DIR}/include/state_history.h
)

add_executable(${PROJECT_NAME} src/main.cpp)

# Make executable depend on generated header
add_dependencies(${PROJECT_NAME} generate_state_history)

# Include directories
include_directories("include")
include_directories("../event_sourcing/deps/flecs/include")
include_directories("../project_template_ui/deps/glfw/include")
include_directories("../project_template_ui/deps/glfw/deps")
include_directories("../project_template_ui/deps/nanovg/src")
include_directories("../project_template_ui/deps/glad/include")

# Add flecs
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build Flecs as a shared library" FORCE)
add_subdirectory("../event_sourcing/deps/flecs" "${CMAKE_BINARY_DIR}/flecs")

# Add GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("../project_template_ui/deps/glfw" "${CMAKE_BINARY_DIR}/glfw")

# Add glad
add_library(glad STATIC "../project_template_ui/deps/glad/src/glad.c")
target_include_directories(glad PUBLIC "../project_template_ui/deps/glad/include")

# Add nanovg
add_library(nanovg STATIC
    "../project_template_ui/deps/nanovg/src/nanovg.c"
)
target_include_directories(nanovg PUBLIC "../project_template_ui/deps/nanovg/src")
target_compile_definitions(nanovg PUBLIC NANOVG_GL2_IMPLEMENTATION)

# Find zlib
find_package(ZLIB REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    flecs
    glfw
    glad
    nanovg
    ZLIB::ZLIB
    ${CMAKE_DL_LIBS}
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} pthread m)
endif()
