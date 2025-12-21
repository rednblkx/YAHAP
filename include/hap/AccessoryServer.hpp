#pragma once

#include "hap/core/AttributeDatabase.hpp"
#include "hap/platform/CryptoSRP.hpp"
#include "hap/platform/Network.hpp"
#include "hap/platform/Storage.hpp"
#include "hap/platform/System.hpp"
#include "hap/platform/Ble.hpp"

#include <memory>
#include <string>

namespace hap::common {
class TaskScheduler;
}

namespace hap {

/**
 * @brief Main HAP Accessory Server
 * Orchestrates the HAP protocol using the provided platform interfaces.
 */
class AccessoryServer {
public:
    struct Config {
        platform::CryptoSRP* crypto;
        platform::Network* network;
        platform::Storage* storage;
        platform::System* system;
        platform::Ble* ble = nullptr; // Optional
        
        std::string accessory_id;      // e.g., "12:34:56:78:9A:BC"
        std::string setup_code;        // 8-digit PIN (e.g., "123-45-678")
        std::string device_name;       // e.g., "My Lightbulb"
        uint16_t port = 8080;          // TCP port for HAP over IP
        core::AccessoryCategory category_id = core::AccessoryCategory::Lightbulb;
        
        std::function<void()> on_identify;
    };

    AccessoryServer(Config config);
    ~AccessoryServer();

    void add_accessory(std::shared_ptr<core::Accessory> accessory);

    /**
     * @brief Broadcast an event notification to all subscribed controllers.
     * @param aid Accessory ID
     * @param iid Instance ID
     * @param value New value
     * @param exclude_conn_id Connection ID to exclude (e.g., the one that caused the change), or 0 for none.
     */
    void broadcast_event(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id = 0);

    /**
     * @brief Start the HAP server
     * - Initializes endpoints
     * - Starts TCP listener
     * - Registers mDNS service
     */
    void start();

    /**
     * @brief Stop the HAP server
     */
    void stop();

    /**
     * @brief Factory reset - clears all pairing state and regenerates identifiers.
     *        Call this to make the accessory appear as a completely new device.
     */
    void factory_reset();

    /**
     * @brief Process periodic tasks. Call this regularly from your main loop.
     * 
     * Handles: session timeouts, GSN updates, characteristic change notifications.
     * Recommended call frequency: every 100-500ms.
     */
    void tick();

private:
    Config config_;
    core::AttributeDatabase database_;
    bool mdns_registered_ = false;  // Track whether mDNS service has been registered
    
    // Router, scheduler, and endpoints (forward declared to reduce header dependencies)
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::unique_ptr<common::TaskScheduler> scheduler_;
    
    // Private member functions
    void setup_routes();
    void on_tcp_receive(uint32_t connection_id, std::span<const uint8_t> data);
    void on_tcp_disconnect(uint32_t connection_id);
    void update_mdns();
    void check_and_update_config_number();
    void reset_pairing_state();
};

} // namespace hap
