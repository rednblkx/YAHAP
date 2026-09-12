#pragma once

// Compile-time gated logging helper.
//
// Usage:
//   HAP_LOG(system, Info, "[BleTransport] TID=", tid, " opcode=", opcode);
//
// Arguments are appended with no separator; integers and pointers are
// formatted with a tiny unsigned converter (no <sstream>/<cstdio>).

#include "hap/platform/System.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifndef HAP_LOG_LEVEL
#define HAP_LOG_LEVEL 1 // Info
#endif

namespace hap::common {

// 0=Debug 1=Info 2=Warning 3=Error 4=Nothing
constexpr int kLogLevel = HAP_LOG_LEVEL;

namespace detail {

// Appends an unsigned integer in decimal into buf, returning the digit count.
inline size_t format_uint(char* buf, unsigned long long v) {
    char digits[20]; // 2^64-1 has 20 digits
    int n = 0;
    do {
        digits[n++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    for (int i = 0; i < n; ++i) {
        buf[i] = digits[n - 1 - i];
    }
    return static_cast<size_t>(n);
}

// Recursion terminator.
inline void append_parts(std::string&, char*) {}

template<typename T, typename... Rest>
void append_parts(std::string& out, char* buf, T&& first, Rest&&... rest) {
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, const char*>) {
        // GCC flags a null check on a known-nonnull parameter, so only guard
        // when the type could actually be null (callers pass string literals).
        if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
            out.append(first);
        } else {
            if (first) out.append(first);
        }
    } else if constexpr (std::is_same_v<U, std::string>) {
        out.append(first);
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        out.append(first);
    } else if constexpr (std::is_same_v<U, bool>) {
        out += first ? "true" : "false";
    } else if constexpr (std::is_same_v<U, char>) {
        out.push_back(first);
    } else if constexpr (std::is_enum_v<U>) {
        size_t n = format_uint(buf, static_cast<unsigned long long>(first));
        out.append(buf, n);
    } else if constexpr (std::is_integral_v<U>) {
        auto v = static_cast<unsigned long long>(first);
        if constexpr (std::is_signed_v<U>) {
            if (first < 0) {
                out.push_back('-');
                v = ~v + 1; // two's complement magnitude, no UB at LLONG_MIN
            }
        }
        size_t n = format_uint(buf, v);
        out.append(buf, n);
    } else if constexpr (std::is_pointer_v<U>) {
        static const char* kHex = "0123456789abcdef";
        out += "0x";
        auto v = reinterpret_cast<uintptr_t>(first);
        char tmp[16];
        int n = 0;
        do { tmp[n++] = kHex[v & 0xF]; v >>= 4; } while (v > 0);
        while (n > 0) out.push_back(tmp[--n]);
    } else {
        out += "?";
    }
    append_parts(out, buf, std::forward<Rest>(rest)...);
}

} // namespace detail

// Internal: build the message string from parts and forward to the PAL.
template<typename... Parts>
inline void log_parts(platform::System* system, platform::System::LogLevel level, Parts&&... parts) {
    if (!system) return;
    std::string msg;
    msg.reserve(64);
    char buf[24];
    detail::append_parts(msg, buf, std::forward<Parts>(parts)...);
    system->log(level, msg);
}

} // namespace hap::common

// Compile-time gated log macros. `sys` may be a null pointer; the runtime
// check stays (cheap) so call sites don't need their own guards.
#if HAP_LOG_LEVEL <= 0
#define HAP_LOG(sys, ...) \
    ::hap::common::log_parts((sys), ::hap::platform::System::LogLevel::Debug __VA_OPT__(,) __VA_ARGS__)
#else
#define HAP_LOG(sys, ...) ((void)0)
#endif
#if HAP_LOG_LEVEL <= 1
#define HAP_LOG_INFO(sys, ...) \
    ::hap::common::log_parts((sys), ::hap::platform::System::LogLevel::Info __VA_OPT__(,) __VA_ARGS__)
#else
#define HAP_LOG_INFO(sys, ...) ((void)0)
#endif
#if HAP_LOG_LEVEL <= 2
#define HAP_LOG_WARN(sys, ...) \
    ::hap::common::log_parts((sys), ::hap::platform::System::LogLevel::Warning __VA_OPT__(,) __VA_ARGS__)
#else
#define HAP_LOG_WARN(sys, ...) ((void)0)
#endif
#if HAP_LOG_LEVEL <= 3
#define HAP_LOG_ERROR(sys, ...) \
    ::hap::common::log_parts((sys), ::hap::platform::System::LogLevel::Error __VA_OPT__(,) __VA_ARGS__)
#else
#define HAP_LOG_ERROR(sys, ...) ((void)0)
#endif
