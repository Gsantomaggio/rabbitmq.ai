#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& all_tests() {
    static std::vector<TestCase> v;
    return v;
}

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> fn) {
        all_tests().push_back({name, fn});
    }
};

#define TEST(name)                                                     \
    static void test_fn_##name();                                      \
    static TestRegistrar reg_##name(#name, test_fn_##name);            \
    static void test_fn_##name()

template <typename A, typename B>
inline void assert_eq_impl(const A& a, const B& b,
                           const char* file, int line,
                           const char* expr_a, const char* expr_b)
{
    if (!(a == b)) {
        std::ostringstream oss;
        oss << "ASSERT_EQ failed at " << file << ":" << line
            << " (" << expr_a << " == " << expr_b << ")";
        throw std::runtime_error(oss.str());
    }
}

#define ASSERT_EQ(a, b) assert_eq_impl((a), (b), __FILE__, __LINE__, #a, #b)

#define ASSERT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::ostringstream _oss;                                          \
            _oss << "ASSERT_TRUE failed at " << __FILE__ << ":" << __LINE__  \
                 << " (" #cond ")";                                           \
            throw std::runtime_error(_oss.str());                             \
        }                                                                     \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_THROWS(expr, ExType)                                           \
    do {                                                                      \
        bool _caught_ = false;                                                \
        try { expr; } catch (const ExType&) { _caught_ = true; }             \
        catch (...) {}                                                        \
        if (!_caught_) {                                                      \
            std::ostringstream _oss;                                          \
            _oss << "ASSERT_THROWS: expected " #ExType " not thrown at "      \
                 << __FILE__ << ":" << __LINE__;                              \
            throw std::runtime_error(_oss.str());                             \
        }                                                                     \
    } while (0)

inline int run_tests() {
    int passed = 0, failed = 0;
    for (auto& t : all_tests()) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << ": " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
