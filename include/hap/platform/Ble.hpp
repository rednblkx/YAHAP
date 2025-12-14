#pragma once

#include <span>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <optional>

namespace hap::platform {

/**
 * @brief Abstract interface for BLE operations.
 * This interface decouples the HAP library from the specific BLE stack (e.g., BlueZ, ESP32 Bluedroid/NimBLE).
 */
struct Ble {
    virtual ~Ble() = default;

    struct Advertisement {
        static Advertisement create_hap(
            uint8_t status_flags, 
            uint8_t device_id[6], 
            uint16_t category_id, 
            uint16_t global_state_number,
            uint16_t config_number,
            uint8_t pair_setup_hash[4]
        );

        uint16_t company_id = 0x004C;
        std::vector<uint8_t> manufacturer_data;
        uint8_t flags = 0x06;
        std::optional<std::string> local_name;
    };

    /**
     * @brief Start BLE advertising with the given data.
     * @param data The advertisement payload and configuration.
     * @param interval_ms Advertising interval in milliseconds (default 20ms recommended for HAP).
     */
    virtual void start_advertising(const Advertisement& data, uint32_t interval_ms = 20) = 0;

    /**
     * @brief Stop BLE advertising.
     */
    virtual void stop_advertising() = 0;

    struct DescriptorDefinition {
        std::string uuid;
        struct Properties {
            bool read = false;
            bool write = false;
        } properties;
        
        using ReadCallback = std::function<std::vector<uint8_t>(uint16_t connection_id)>;
        ReadCallback on_read;

        using WriteCallback = std::function<void(uint16_t connection_id, std::span<const uint8_t> data)>;
        WriteCallback on_write;
    };

    struct CharacteristicDefinition {
        std::string uuid; // 128-bit UUID string
        
        struct Properties {
            bool read = false;
            bool write = false;
            bool write_without_response = false;
            bool notify = false;
            bool indicate = false;
        } properties;

        using WriteCallback = std::function<void(uint16_t connection_id, std::span<const uint8_t> data, bool response_needed)>;
        WriteCallback on_write;

        using ReadCallback = std::function<std::vector<uint8_t>(uint16_t connection_id)>;
        ReadCallback on_read;

        using SubscribeCallback = std::function<void(uint16_t connection_id, bool enabled)>;
        SubscribeCallback on_subscribe;

        std::vector<DescriptorDefinition> descriptors;
    };

    struct ServiceDefinition {
        std::string uuid;
        bool is_primary = true;
        std::vector<CharacteristicDefinition> characteristics;
        std::vector<ServiceDefinition> included_services;
    };

    /**
     * @brief Register a GATT service.
     * This should be called before starting advertising.
     * @param service The service definition.
     */
    virtual void register_service(const ServiceDefinition& service) = 0;

    /**
     * @brief Send a notification (or indication) to a connected client.
     * @param connection_id The ID of the connection to send to.
     * @param characteristic_uuid The UUID of the characteristic (must match a registered one).
     * @param data The data to send.
     */
    virtual void send_notification(uint16_t connection_id, const std::string& characteristic_uuid, std::span<const uint8_t> data) = 0;

    /**
     * @brief Disconnect a specific client.
     */
    virtual void disconnect(uint16_t connection_id) = 0;

    /**
     * @brief Callback type for disconnect events.
     * @param connection_id The ID of the disconnected connection.
     */
    using DisconnectCallback = std::function<void(uint16_t connection_id)>;

    /**
     * @brief Set a callback to be invoked when a device disconnects.
     * @param callback The callback function.
     */
    virtual void set_disconnect_callback(DisconnectCallback callback) = 0;
};

} // namespace hap::platform
