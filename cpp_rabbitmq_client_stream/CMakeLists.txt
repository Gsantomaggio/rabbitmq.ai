cmake_minimum_required(VERSION 3.16)
project(rabbitmq_stream_client CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Library ───────────────────────────────────────────────────────────────────

add_library(stream_client STATIC
    src/transport.cpp
    src/dispatcher.cpp
    src/client.cpp
)
target_include_directories(stream_client PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(stream_client PUBLIC Threads::Threads)

# ── Unit tests ────────────────────────────────────────────────────────────────

add_executable(test_unit
    tests/unit_tests_main.cpp
)
target_include_directories(test_unit PRIVATE include tests)
# unit_tests_main includes the test .cpp files directly; no library needed.

enable_testing()
add_test(NAME unit COMMAND test_unit)

# ── Integration tests ─────────────────────────────────────────────────────────

add_executable(test_integration
    tests/integration_test.cpp
)
target_include_directories(test_integration PRIVATE include)
target_link_libraries(test_integration PRIVATE stream_client)
