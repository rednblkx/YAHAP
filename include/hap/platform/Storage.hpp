#pragma once

#include <span>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

namespace hap::platform {

/**
 * @brief Abstract interface for persistent Key-Value storage.
 * Used for storing pairing information, accessory state, etc.
 */
struct Storage {
    virtual ~Storage() = default;

    // Basic KV operations
    virtual void set(std::string_view key, std::span<const uint8_t> value) = 0;
    virtual std::optional<std::vector<uint8_t>> get(std::string_view key) = 0;
    virtual void remove(std::string_view key) = 0;
    
    // Check if key exists
    virtual bool has(std::string_view key) = 0;
};

} // namespace hap::platform
