cmake_minimum_required(VERSION 3.16)
project(rabbitmq_stream_client_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Library ───────────────────────────────────────────────────────────────────

add_library(stream_client_c STATIC
    src/codec.c
    src/commands.c
    src/transport.c
    src/dispatcher.c
    src/client.c
)
target_include_directories(stream_client_c PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(stream_client_c PUBLIC Threads::Threads)

# ── Unit tests ────────────────────────────────────────────────────────────────

add_executable(test_unit
    tests/unit_tests_main.c
)
target_include_directories(test_unit PRIVATE include tests)
target_link_libraries(test_unit PRIVATE stream_client_c)

enable_testing()
add_test(NAME unit COMMAND test_unit)

# ── Integration tests ─────────────────────────────────────────────────────────

add_executable(test_integration
    tests/integration_test.c
)
target_include_directories(test_integration PRIVATE include)
target_link_libraries(test_integration PRIVATE stream_client_c)
