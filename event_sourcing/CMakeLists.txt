cmake_minimum_required(VERSION 3.18)
project(project)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory("../tradewinds/deps/flecs" "deps/flecs")
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("../deps/zstd/build/cmake" "deps/zstd")

add_library(project_settings INTERFACE)
target_compile_features(project_settings INTERFACE cxx_std_17)
target_link_libraries(project_settings INTERFACE flecs libzstd_static)
target_include_directories(project_settings INTERFACE
    "src"
    "include"
    "../snapshot/include"
    "../tradewinds/deps/flecs/include"
    "../deps/zstd/lib"
    "../deps/spdlog/include"
)

set(TEST_TARGETS
    branch_entity_simple
    test_event_sourcing
    test_entity_lifecycle
    test_entity_simple
    test_relationships
    test_step_forward
    test_track_entity_order
    stochastic_world_explorer
)

# Main Library
add_executable(${PROJECT_NAME} src/main.cpp src/components.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE project_settings)

# Tests
foreach(TGT IN LISTS TEST_TARGETS)
    add_executable(${TGT} src/${TGT}.cpp src/components.cpp)
    target_link_libraries(${TGT} PRIVATE project_settings)
endforeach()
