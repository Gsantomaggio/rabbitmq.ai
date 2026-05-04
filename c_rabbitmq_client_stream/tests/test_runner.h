#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static jmp_buf  _test_jmp;
static char     _test_fail_msg[512];
static int      _test_pass_count = 0;
static int      _test_fail_count = 0;

static void _test_fail(const char* file, int line, const char* expr) {
    snprintf(_test_fail_msg, sizeof(_test_fail_msg),
             "%s  at %s:%d", expr, file, line);
    longjmp(_test_jmp, 1);
}

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) _test_fail(__FILE__, __LINE__, #cond " is false"); } while (0)

#define ASSERT_FALSE(cond) \
    do { if ((cond)) _test_fail(__FILE__, __LINE__, #cond " is true"); } while (0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) _test_fail(__FILE__, __LINE__, #a " != " #b); } while (0)

#define ASSERT_NE(a, b) \
    do { if ((a) == (b)) _test_fail(__FILE__, __LINE__, #a " == " #b); } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { if (!strstr((haystack), (needle))) \
        _test_fail(__FILE__, __LINE__, "\"" needle "\" not found in " #haystack); } while (0)

/* Run a test function; uses setjmp/longjmp to catch assertion failures. */
static void run_test(const char* name, void (*fn)(void)) {
    if (setjmp(_test_jmp) == 0) {
        fn();
        printf("[PASS] %s\n", name);
        _test_pass_count++;
    } else {
        printf("[FAIL] %s: %s\n", name, _test_fail_msg);
        _test_fail_count++;
    }
}

static int print_results(void) {
    printf("\n%d passed, %d failed\n", _test_pass_count, _test_fail_count);
    return _test_fail_count == 0 ? 0 : 1;
}

#endif /* TEST_RUNNER_H */
