cmake_minimum_required(VERSION 3.16)
project(rabbitmq_stream_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pull in the library from the project root (two directories up).
# Tests are disabled so the build stays fast and requires no internet access
# after the first GoogleTest fetch.
set(STREAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STREAM_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(../../.. ${CMAKE_CURRENT_BINARY_DIR}/lib)

# ---- getting_started -------------------------------------------------------
add_executable(getting_started getting_started.cpp)
target_link_libraries(getting_started PRIVATE rabbitmq_stream_client)
