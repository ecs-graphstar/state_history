cmake_minimum_required(VERSION 3.18)
project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TRADEWINDS_DIR "../../tradewinds")
set(UI_TEMPLATE_DIR "../../project_template_ui")

set(COMMON_FLAGS "-pthread -pedantic -fpermissive")
set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_FLAGS} -g -O1")
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS} -O3")

add_executable(${PROJECT_NAME} src/main.cpp ${TRADEWINDS_DIR}/src/tradewinds.cpp)

include_directories("src")
include_directories(${TRADEWINDS_DIR}/src)
include_directories(${TRADEWINDS_DIR}/deps/msgpack-c/include)

target_include_directories(${PROJECT_NAME} PRIVATE ${TRADEWINDS_DIR}/deps/cppzmq)

add_subdirectory("${TRADEWINDS_DIR}/deps/libzmq" "deps/libzmq")
add_subdirectory("${TRADEWINDS_DIR}/deps/msgpack-c" "deps/msgpack-c")

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory("${UI_TEMPLATE_DIR}/deps/flecs" "${CMAKE_BINARY_DIR}/flecs")

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    msgpack-cxx
    libzmq
    flecs
)