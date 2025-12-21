#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <string>

namespace hap::transport::ble {

/**
 * @brief HAP-BLE PDU TLV Types (Table 7-9)
 */
enum class HAPBLEPDUTLVType : uint8_t {
    CharacteristicType = 0x04,
    CharacteristicInstanceID = 0x05,
    ServiceType = 0x06,
    ServiceInstanceID = 0x07,
    TTL = 0x08,
    ReturnResponse = 0x09,
    CharacteristicProperties = 0x0A,
    GATTUserDescription = 0x0B,
    GATTPresentationFormat = 0x0C,
    GATTValidRange = 0x0D,
    StepValue = 0x0E,
    ServiceProperties = 0x0F,
    LinkedServices = 0x10,
    ValidValues = 0x11,
    ValidValuesRange = 0x12
};

/**
 * @brief Builder for HAP-BLE TLV structures.
 * 
 * Provides a fluent interface for constructing TLV payloads
 * used in HAP-BLE responses (signature reads, etc).
 */
class BleTlvBuilder {
public:
    BleTlvBuilder() = default;
    
    /**
     * @brief Add a raw TLV.
     */
    BleTlvBuilder& add(HAPBLEPDUTLVType type, std::span<const uint8_t> value);
    
    /**
     * @brief Add a uint8_t value.
     */
    BleTlvBuilder& add_uint8(HAPBLEPDUTLVType type, uint8_t value);
    
    /**
     * @brief Add a uint16_t value (little-endian).
     */
    BleTlvBuilder& add_uint16(HAPBLEPDUTLVType type, uint16_t value);
    
    /**
     * @brief Add a HAP 128-bit UUID from a short UUID.
     * 
     * HAP Base UUID: 0000XXXX-0000-1000-8000-0026BB765291
     * Stored as 16 bytes in little-endian order.
     */
    BleTlvBuilder& add_hap_uuid128(HAPBLEPDUTLVType type, uint16_t short_uuid);
    
    /**
     * @brief Add GATT Presentation Format descriptor.
     * @param bt_sig_format Bluetooth SIG assigned format number
     * @param unit Bluetooth SIG assigned unit (default: unitless 0x2700)
     */
    BleTlvBuilder& add_gatt_format(uint8_t bt_sig_format, uint16_t unit = 0x2700);
    
    /**
     * @brief Add a string value.
     */
    BleTlvBuilder& add_string(HAPBLEPDUTLVType type, const std::string& value);
    
    /**
     * @brief Build and return the TLV data.
     */
    [[nodiscard]] std::vector<uint8_t> build() const { return buffer_; }
    
    /**
     * @brief Get current buffer size.
     */
    [[nodiscard]] size_t size() const { return buffer_.size(); }
    
    /**
     * @brief Clear the builder.
     */
    void clear() { buffer_.clear(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace hap::transport::ble
