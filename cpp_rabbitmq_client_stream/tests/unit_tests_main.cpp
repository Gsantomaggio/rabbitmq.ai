#include "test_runner.hpp"

// Include test translation units so all TEST() registrations happen in main's
// link unit via static initializers.
#include "test_codec.cpp"
#include "test_commands.cpp"
#include "test_errors.cpp"

int main() {
    return run_tests();
}
