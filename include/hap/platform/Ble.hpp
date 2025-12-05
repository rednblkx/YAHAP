#pragma once

#include <span>
#include <cstdint>
#include <functional>
#include <vector>

namespace hap::platform {

/**
 * @brief Abstract interface for BLE operations.
 * Only required if the accessory supports HAP over BLE.
 */
struct Ble {
    virtual ~Ble() = default;

    // Advertising
    struct AdvertisementData {
        // ... flags, manufacturer data, etc.
        std::vector<uint8_t> payload;
    };
    virtual void start_advertising(const AdvertisementData& data, uint32_t interval_ms) = 0;
    virtual void stop_advertising() = 0;

    // GATT Server
    // This is highly simplified; a real implementation would need a more complex
    // definition of services and characteristics.
    // For now, we'll assume the core logic constructs the DB and passes it here,
    // or we provide hooks for read/write requests.
    
    using WriteCallback = std::function<void(uint16_t handle, std::span<const uint8_t> data)>;
    using ReadCallback = std::function<std::vector<uint8_t>(uint16_t handle)>;

    virtual void set_write_callback(WriteCallback cb) = 0;
    virtual void set_read_callback(ReadCallback cb) = 0;
    
    virtual void send_notification(uint16_t handle, std::span<const uint8_t> data) = 0;
};

} // namespace hap::platform
