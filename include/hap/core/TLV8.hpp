#pragma once

#include <span>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

namespace hap::core {

/**
 * @brief Represents a single TLV item.
 */
struct TLV {
    uint8_t type;
    std::vector<uint8_t> value;

    TLV(uint8_t t, std::vector<uint8_t> v) : type(t), value(std::move(v)) {}
    TLV(uint8_t t, std::span<const uint8_t> v) : type(t), value(v.begin(), v.end()) {}
    TLV(uint8_t t, std::string_view v) : type(t), value(v.begin(), v.end()) {}
    TLV(uint8_t t, uint8_t v) : type(t), value{v} {}
};

/**
 * @brief TLV8 (Type-Length-Value 8-bit) Parser and Encoder.
 * Handles fragmentation for values larger than 255 bytes.
 */
class TLV8 {
public:
    /**
     * @brief Parses a raw byte buffer into a list of TLVs.
     * Handles defragmentation of consecutive items with the same type.
     */
    static std::vector<TLV> parse(std::span<const uint8_t> data);

    /**
     * @brief Encodes a list of TLVs into a raw byte buffer.
     * Handles fragmentation of values larger than 255 bytes.
     */
    static std::vector<uint8_t> encode(const std::vector<TLV>& tlvs);

    // Helpers to find values
    static std::optional<std::vector<uint8_t>> find(const std::vector<TLV>& tlvs, uint8_t type);
    static std::optional<std::string> find_string(const std::vector<TLV>& tlvs, uint8_t type);
    static std::optional<uint8_t> find_uint8(const std::vector<TLV>& tlvs, uint8_t type);
};

} // namespace hap::core
