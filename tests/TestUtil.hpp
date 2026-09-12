#pragma once

// Minimal assertion-based test utilities.
//
// Deliberately independent of <cassert> so tests keep their assertions
// under NDEBUG (the library may be built in MinSizeRel/Release modes).

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>

namespace testutil {

[[noreturn]] inline void fail(const char* expr, const char* file, int line, const std::string& detail) {
    std::cerr << "FAILED " << file << ":" << line << "\n  " << expr;
    if (!detail.empty()) std::cerr << "\n  " << detail;
    std::cerr << std::endl;
    std::exit(1);
}

template <typename T>
std::string repr(const T& v) {
    if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(v)); // NOLINT(google-integral-promotions)
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(v);
    } else {
        return "<unprintable>";
    }
}
inline std::string repr(const std::string& v) { return "\"" + v + "\""; }
inline std::string repr(const char* v) { return std::string("\"") + v + "\""; }
inline std::string repr(bool v) { return v ? "true" : "false"; }

template <typename T>
std::string repr_hex_byte(const T& v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned>(v) & 0xFF);
    return buf;
}

} // namespace testutil

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            testutil::fail(#cond, __FILE__, __LINE__, ""); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        const auto& va_ = (a); \
        const auto& vb_ = (b); \
        if (!(va_ == vb_)) { \
            testutil::fail(#a " == " #b, __FILE__, __LINE__, \
                           testutil::repr(va_) + " != " + testutil::repr(vb_)); \
        } \
    } while (0)

#define CHECK_EQ_HEX(a, b) \
    do { \
        const auto& va_ = (a); \
        const auto& vb_ = (b); \
        if (!(va_ == vb_)) { \
            testutil::fail(#a " == " #b, __FILE__, __LINE__, \
                           testutil::repr_hex_byte(va_) + " != " + testutil::repr_hex_byte(vb_)); \
        } \
    } while (0)

// Runs one test function, printing progress; exits non-zero on first failure
// (CHECK macros exit directly, so reaching here means success).
#define RUN_TEST(fn) \
    do { \
        std::cout << "[ RUN  ] " #fn << std::endl; \
        fn(); \
        std::cout << "[  OK  ] " #fn << std::endl; \
    } while (0)
