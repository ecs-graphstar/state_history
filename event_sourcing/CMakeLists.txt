cmake_minimum_required(VERSION 3.18)
project(project)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory("../project_template_ui/deps/flecs" "deps/flecs")
find_package(ZLIB REQUIRED)

add_library(project_settings INTERFACE)
target_compile_features(project_settings INTERFACE cxx_std_17)
target_link_libraries(project_settings INTERFACE flecs ZLIB::ZLIB)
target_include_directories(project_settings INTERFACE
    "src"
    "include"
    "../snapshot/include"
    "../project_template_ui/deps/flecs/include"
)

set(TEST_TARGETS
    branch_entity_simple
    test_event_sourcing
    test_entity_lifecycle
    test_entity_simple
    test_relationships
    test_track_entity_order
)

# Main Library
add_executable(${PROJECT_NAME} src/main.cpp src/components.cpp ../snapshot/source/state_history.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE project_settings)

# Tests
foreach(TGT IN LISTS TEST_TARGETS)
    add_executable(${TGT} src/${TGT}.cpp src/components.cpp ../snapshot/source/state_history.cpp)
    target_link_libraries(${TGT} PRIVATE project_settings)
endforeach()
