cmake_minimum_required(VERSION 3.16)
project(rabbitmq_stream_client
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "C++ TCP client for the RabbitMQ Stream protocol")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(STREAM_BUILD_TESTS "Build unit tests" ON)
option(STREAM_BUILD_INTEGRATION_TESTS "Build integration tests (require live broker)" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

# ---- Library ---------------------------------------------------------------

add_library(rabbitmq_stream_client STATIC
    src/buffer.cpp
    src/codec.cpp
    src/errors.cpp
    src/frame.cpp
    src/dispatcher.cpp
    src/socket.cpp
    src/handshake.cpp
    src/client.cpp
    src/utf8.cpp
)

target_include_directories(rabbitmq_stream_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
)

find_package(Threads REQUIRED)
target_link_libraries(rabbitmq_stream_client PUBLIC Threads::Threads)

# ---- Tests -----------------------------------------------------------------

if(STREAM_BUILD_TESTS OR STREAM_BUILD_INTEGRATION_TESTS)
    include(FetchContent)
    set(FETCHCONTENT_QUIET FALSE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

if(STREAM_BUILD_TESTS)
    add_executable(unit_tests
        tests/unit/test_buffer.cpp
        tests/unit/test_codec.cpp
        tests/unit/test_frame.cpp
        tests/unit/test_dispatcher.cpp
        tests/unit/test_commands.cpp
        tests/unit/test_socket.cpp
        tests/unit/test_handshake.cpp
        tests/unit/test_lifecycle.cpp
        tests/unit/test_client.cpp
        tests/unit/fake_server.cpp
    )
    target_link_libraries(unit_tests PRIVATE rabbitmq_stream_client GTest::gtest_main)
    target_include_directories(unit_tests PRIVATE tests/unit)
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

if(STREAM_BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests
        tests/integration/test_lifecycle_e2e.cpp
    )
    target_link_libraries(integration_tests PRIVATE rabbitmq_stream_client GTest::gtest_main)
    add_test(NAME integration_tests COMMAND integration_tests)
endif()
