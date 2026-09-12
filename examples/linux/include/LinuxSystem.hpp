#pragma once

#include "hap/platform/System.hpp"
#include <openssl/rand.h>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace linux_pal {

class LinuxSystem : public hap::platform::System {
public:
    uint64_t millis() override {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    void random_bytes(std::span<uint8_t> buffer) override {
        // OpenSSL's RAND_bytes fails closed (and reports) rather than
        // silently producing garbage the way a short /dev/urandom read would.
        if (buffer.empty()) return;
        if (RAND_bytes(buffer.data(), static_cast<int>(buffer.size())) != 1) {
            std::fprintf(stderr, "[LinuxSystem] RAND_bytes failed\n");
            std::abort();
        }
    }

    void log(LogLevel level, std::string_view message) override {
        const char* level_str;
        switch (level) {
            case LogLevel::Debug: level_str = "DEBUG"; break;
            case LogLevel::Info: level_str = "INFO"; break;
            case LogLevel::Warning: level_str = "WARN"; break;
            case LogLevel::Error: level_str = "ERROR"; break;
            default: level_str = "UNKNOWN"; break;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&time_t, &tm); // thread-safe variant

        printf("[%04d-%02d-%02d %02d:%02d:%02d] [%s] %.*s\n",
               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec,
               level_str,
               static_cast<int>(message.size()), message.data());
    }
};

} // namespace linux_pal
