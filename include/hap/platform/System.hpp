#pragma once

#include <span>
#include <cstdint>
#include <string_view>

namespace hap::platform {

/**
 * @brief Abstract interface for system-level utilities.
 */
struct System {
    virtual ~System() = default;

    // Time (milliseconds since boot or epoch)
    virtual uint64_t millis() = 0;

    // Random number generation (cryptographically secure)
    virtual void random_bytes(std::span<uint8_t> buffer) = 0;

    // Logging
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };
    virtual void log(LogLevel level, std::string_view message) = 0;
};

} // namespace hap::platform
