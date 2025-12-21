#include "hap/transport/ble/BleTlvBuilder.hpp"

namespace hap::transport::ble {

BleTlvBuilder& BleTlvBuilder::add(HAPBLEPDUTLVType type, std::span<const uint8_t> value) {
    buffer_.push_back(static_cast<uint8_t>(type));
    buffer_.push_back(static_cast<uint8_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
    return *this;
}

BleTlvBuilder& BleTlvBuilder::add_uint8(HAPBLEPDUTLVType type, uint8_t value) {
    buffer_.push_back(static_cast<uint8_t>(type));
    buffer_.push_back(1);  // Length
    buffer_.push_back(value);
    return *this;
}

BleTlvBuilder& BleTlvBuilder::add_uint16(HAPBLEPDUTLVType type, uint16_t value) {
    buffer_.push_back(static_cast<uint8_t>(type));
    buffer_.push_back(2);  // Length
    buffer_.push_back(value & 0xFF);
    buffer_.push_back((value >> 8) & 0xFF);
    return *this;
}

BleTlvBuilder& BleTlvBuilder::add_hap_uuid128(HAPBLEPDUTLVType type, uint16_t short_uuid) {
    // HAP Base UUID: 0000XXXX-0000-1000-8000-0026BB765291
    // BLE/HAP requires 128-bit UUIDs in little-endian order
    
    buffer_.push_back(static_cast<uint8_t>(type));
    buffer_.push_back(16);  // Length
    
    // Little-endian order: node | clock_seq | time_hi | time_mid | time_low
    // Node: 0026BB765291 -> 91 52 76 BB 26 00
    buffer_.push_back(0x91);
    buffer_.push_back(0x52);
    buffer_.push_back(0x76);
    buffer_.push_back(0xBB);
    buffer_.push_back(0x26);
    buffer_.push_back(0x00);
    
    // clock_seq: 8000 -> 00 80
    buffer_.push_back(0x00);
    buffer_.push_back(0x80);
    
    // time_hi_and_version: 1000 -> 00 10
    buffer_.push_back(0x00);
    buffer_.push_back(0x10);
    
    // time_mid: 0000 -> 00 00
    buffer_.push_back(0x00);
    buffer_.push_back(0x00);
    
    // time_low: 0000XXXX with short_uuid -> XX XX 00 00
    buffer_.push_back(short_uuid & 0xFF);
    buffer_.push_back((short_uuid >> 8) & 0xFF);
    buffer_.push_back(0x00);
    buffer_.push_back(0x00);
    
    return *this;
}

BleTlvBuilder& BleTlvBuilder::add_gatt_format(uint8_t bt_sig_format, uint16_t unit) {
    // GATT Presentation Format: 7 bytes
    // Format(1) | Exponent(1) | Unit(2) | Namespace(1) | Description(2)
    
    buffer_.push_back(static_cast<uint8_t>(HAPBLEPDUTLVType::GATTPresentationFormat));
    buffer_.push_back(7);  // Length
    
    buffer_.push_back(bt_sig_format);  // Format
    buffer_.push_back(0x00);           // Exponent (0)
    buffer_.push_back(unit & 0xFF);    // Unit low
    buffer_.push_back((unit >> 8) & 0xFF);  // Unit high
    buffer_.push_back(0x01);           // Namespace (Bluetooth SIG)
    buffer_.push_back(0x00);           // Description low
    buffer_.push_back(0x00);           // Description high
    
    return *this;
}

BleTlvBuilder& BleTlvBuilder::add_string(HAPBLEPDUTLVType type, const std::string& value) {
    buffer_.push_back(static_cast<uint8_t>(type));
    buffer_.push_back(static_cast<uint8_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
    return *this;
}

} // namespace hap::transport::ble
