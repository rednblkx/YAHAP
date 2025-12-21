#pragma once

#include "hap/core/Characteristic.hpp"
#include "hap/common/Result.hpp"
#include <vector>
#include <span>

namespace hap::core {

/**
 * @brief Unified value serialization for HAP characteristics.
 * 
 * Provides consistent serialization/deserialization for both
 * IP (JSON) and BLE (binary) transport layers.
 */
class CharacteristicSerializer {
public:
    /**
     * @brief Serialize a value to binary format (for BLE).
     * @param value The value to serialize
     * @return Binary representation
     */
    static std::vector<uint8_t> to_bytes(const Value& value);
    
    /**
     * @brief Deserialize a value from binary format (for BLE).
     * @param data Binary data
     * @param format Expected format
     * @return Parsed value or error
     */
    static common::Result<Value> from_bytes(std::span<const uint8_t> data, Format format);
    
    /**
     * @brief Get the GATT Presentation Format byte for a format.
     * 
     * Returns the Bluetooth SIG assigned format number for use in
     * GATT Presentation Format descriptors.
     * 
     * @param format HAP characteristic format
     * @return Bluetooth SIG format byte
     */
    static uint8_t gatt_format_byte(Format format);
    
    /**
     * @brief Check if a format represents binary data.
     */
    static bool is_binary_format(Format format);
};

} // namespace hap::core
