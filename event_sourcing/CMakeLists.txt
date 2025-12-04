# Default Paphos project starter
# flecs/PyTorch with AOT Inductor pipeline

cmake_minimum_required(VERSION 3.18)
project(project)

# Add custom command to generate state_history.h from components.h using Python
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/include/state_history.h
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/generate_state_history.py
    DEPENDS ${CMAKE_SOURCE_DIR}/include/components.h
            ${CMAKE_SOURCE_DIR}/templates/state_history.h.j2
            ${CMAKE_SOURCE_DIR}/generate_state_history.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating state_history.h from components.h using libclang"
    VERBATIM
)

# Create a custom target for the generated header
add_custom_target(generate_state_history
    DEPENDS ${CMAKE_SOURCE_DIR}/include/state_history.h
)

# add_definitions(AOTInductor_CUDA)
add_executable(${PROJECT_NAME} src/main.cpp)
add_executable(test_event_sourcing src/test_event_sourcing.cpp)
add_executable(test_entity_lifecycle src/test_entity_lifecycle.cpp)
add_executable(test_entity_simple src/test_entity_simple.cpp)
add_executable(test_relationships src/test_relationships.cpp)
add_executable(test_track_entity_order src/test_track_entity_order.cpp)
include_directories("src")
include_directories("include")

# Make the executables depend on the generated header
add_dependencies(${PROJECT_NAME} generate_state_history)
add_dependencies(test_event_sourcing generate_state_history)
add_dependencies(test_entity_lifecycle generate_state_history)
add_dependencies(test_entity_simple generate_state_history)
add_dependencies(test_relationships generate_state_history)
add_dependencies(test_track_entity_order generate_state_history)

include_directories("deps/flecs/include")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build Flecs as a shared library" FORCE)
add_subdirectory("deps/flecs")

# Find and link zlib
find_package(ZLIB REQUIRED)

target_link_libraries(${PROJECT_NAME}
    flecs
    ZLIB::ZLIB
)

target_link_libraries(test_event_sourcing
    flecs
    ZLIB::ZLIB
)

target_link_libraries(test_entity_lifecycle
    flecs
    ZLIB::ZLIB
)

target_link_libraries(test_entity_simple
    flecs
    ZLIB::ZLIB
)

target_link_libraries(test_relationships
    flecs
    ZLIB::ZLIB
)

target_link_libraries(test_track_entity_order
    flecs
    ZLIB::ZLIB
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET test_event_sourcing PROPERTY CXX_STANDARD 17)
set_property(TARGET test_entity_lifecycle PROPERTY CXX_STANDARD 17)
set_property(TARGET test_entity_simple PROPERTY CXX_STANDARD 17)
set_property(TARGET test_relationships PROPERTY CXX_STANDARD 17)
set_property(TARGET test_track_entity_order PROPERTY CXX_STANDARD 17)
