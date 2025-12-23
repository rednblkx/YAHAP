#include "hap/platform/Ble.hpp"

namespace hap::platform {

Ble::Advertisement Ble::Advertisement::create_hap(
    uint8_t status_flags, 
    uint8_t device_id[6], 
    uint16_t category_id, 
    uint16_t global_state_number,
    uint8_t config_number,
    uint8_t pair_setup_hash[4]
) {
    Ble::Advertisement adv;
    adv.company_id = 0x004C;
    adv.flags = 6; // LE General Discoverable | BR/EDR Not Supported

    // Manufacturer Specific Data Structure:
    // Field      | Size | Description
    // -----------|------|------------------------------------------
    // Type (TY)  | 1    | 0x06 (HomeKit)
    // STL        | 1    | SubType (3 bits) + Length (5 bits)
    // SF         | 1    | Status Flags
    // Device ID  | 6    | 48-bit Device ID
    // ACID       | 2    | Accessory Category Identifier (LE)
    // GSN        | 2    | Global State Number (LE)
    // CN         | 1    | Configuration Number (1-255, wraps to 1)
    // CV         | 1    | Compatible Version (0x02 for HAP-BLE 2.0)
    // SH         | 4    | Setup Hash
    
    adv.manufacturer_data.reserve(19);
    
    // Type: 0x06 (HomeKit Accessory Protocol)
    adv.manufacturer_data.push_back(0x06);
    
    // STL: SubType/Length
    // Table 7-42: 0x31 = SubType 1 (regular advertisement) + Length 17
    // Bits 7-5: SubType = 001 (binary) = 1
    // Bits 4-0: Length = 10001 (binary) = 17
    // Combined: 0b00110001 = 0x31
    adv.manufacturer_data.push_back(0x31);

    // SF: Status Flags (1 byte)
    adv.manufacturer_data.push_back(status_flags);

    // Device ID (6 bytes)
    for(int i=0; i<6; i++) {
        adv.manufacturer_data.push_back(device_id[i]);
    }

    // ACID: Accessory Category Identifier (2 bytes, little-endian)
    adv.manufacturer_data.push_back(category_id & 0xFF);
    adv.manufacturer_data.push_back((category_id >> 8) & 0xFF);

    // GSN: Global State Number (2 bytes, little-endian)
    adv.manufacturer_data.push_back(global_state_number & 0xFF);
    adv.manufacturer_data.push_back((global_state_number >> 8) & 0xFF);

    // CN: Configuration Number (1 byte, range 1-255, wraps to 1)
    adv.manufacturer_data.push_back(config_number);

    // CV: Compatible Version (1 byte) - 0x02 for HAP-BLE 2.0
    adv.manufacturer_data.push_back(0x02);

    // SH: Setup Hash (4 bytes)
    for(int i=0; i<4; i++) {
        adv.manufacturer_data.push_back(pair_setup_hash[i]);
    }
    
    return adv;
}

} // namespace hap::platform
