cmake_minimum_required(VERSION 3.18)
project(client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_FLAGS "-pthread -pedantic -fpermissive")
set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_FLAGS} -g -O1")
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS} -O3")

# --- Dependencies ---
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("../../deps/zstd/build/cmake" "${CMAKE_BINARY_DIR}/zstd")

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory("../../project_template_ui/deps/flecs" "${CMAKE_BINARY_DIR}/flecs")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("../../project_template_ui/deps/glfw" "${CMAKE_BINARY_DIR}/glfw")

add_subdirectory("../../project_template_ui/deps/tracy" "${CMAKE_BINARY_DIR}/tracy")

add_library(glad STATIC "../../project_template_ui/deps/glad/src/glad.c")
target_include_directories(glad PUBLIC "../../project_template_ui/deps/glad/include")

add_library(nanovg STATIC "../../project_template_ui/deps/nanovg/src/nanovg.c")
target_include_directories(nanovg PUBLIC "../../project_template_ui/deps/nanovg/src")
target_compile_definitions(nanovg PUBLIC NANOVG_GL2_IMPLEMENTATION)

# --- Targets ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DTRACY_ENABLE)

target_include_directories(${PROJECT_NAME} PRIVATE
    "include"
    "../snapshot/include"
    "../../project_template_ui/deps/flecs/include"
    "../../project_template_ui/deps/glfw/include"
    "../../project_template_ui/deps/nanovg/src"
    "../../project_template_ui/deps/glad/include"
    "../../project_template_ui/deps/tracy/public"
    "../../deps/zstd/lib"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    flecs
    glfw
    glad
    nanovg
    libzstd_static
    Tracy::TracyClient
    ${CMAKE_DL_LIBS}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread m)
endif()
