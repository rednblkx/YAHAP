#pragma once

#include "hap/platform/Ble.hpp"
#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/IIDManager.hpp"
#include "hap/transport/PairingEndpoints.hpp"
#include "hap/transport/ble/BleSessionManager.hpp"
#include "hap/common/TaskScheduler.hpp"
#include <memory>
#include <map>
#include <vector>
#include <array>

namespace hap::transport {

/**
 * @brief Implements the HAP over BLE transport layer.
 * 
 * Responsibilities:
 * - Manages the HAP Pairing Service (UUID FE59).
 * - Manages Accessory Information Service.
 * - Handles HAP operation PDUs (fragmentation/reassembly).
 * - Interfaces with AttributeDatabase for characteristic access.
 * - Generates Advertising Data.
 */
class BleTransport {
public:
    struct Config {
        platform::Ble* ble;
        platform::CryptoSRP* crypto;
        core::AttributeDatabase* database;
        PairingEndpoints* pairing_endpoints;
        platform::System* system;
        platform::Storage* storage;
        common::TaskScheduler* scheduler = nullptr;
        core::IIDManager* iid_manager = nullptr;
        
        std::string accessory_id;
        std::string device_name;
        uint16_t category_id = 5; // Default to Lightbulb
        uint16_t config_number = 1;
    };

    BleTransport(Config config);
    ~BleTransport();

    /**
     * @brief Initialize services and start advertising.
     */
    void start();

    /**
     * @brief Stop advertising and disconnect clients.
     */
    void stop();

    /**
     * @brief Update BLE advertising data.
     */
    void update_advertising();

    /**
     * @brief Update the accessory ID at runtime.
     */
    void set_accessory_id(const std::string& new_id);

    /**
     * @brief Notify controllers of a characteristic value change.
     * 
     * Call this when a characteristic value changes from a non-BLE source
     * (e.g., physical button, timer, sensor). Automatically selects the
     * appropriate event type per HAP Spec 7.4.6:
     * - Connected Events: Zero-length indication to subscribed controllers
     * - Broadcasted Events: Encrypted advertisement (if configured)
     * - Disconnected Events: GSN increment in regular advertisement
     * 
     * @param aid Accessory ID (usually 1 for single accessory)
     * @param iid Characteristic Instance ID
     * @param value The new characteristic value
     * @param exclude_conn_id Connection ID to exclude (0 = notify all)
     */
    void notify_value_changed(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id = 0);

    /**
     * @brief Check for session timeouts and disconnect idle connections.
     * 
     * Per HAP Spec:
     * - 30-second idle timeout (7.2.5)
     * - 10-second HAP procedure timeout (7.3.1)
     * - 10-second initial procedure timeout (7.5 Req #40)
     */
    void check_session_timeouts();

private:
    Config config_;

    // UUIDs
    // HAP Base UUID: XXXX-0000-1000-8000-0026BB765291 (Section 6.6.1)
    static constexpr const char* kHapPairingServiceUUID = "00000055-0000-1000-8000-0026BB765291";
    static constexpr const char* kHapProtocolInformationServiceUUID = "000000A2-0000-1000-8000-0026BB765291";
    static constexpr const char* kServiceSignatureCharUUID = "000000A5-0000-1000-8000-0026BB765291";
    static constexpr const char* kServiceInstanceIdCharUUID = "E604E95D-A759-4817-87D3-AA005083A0D1";
    static constexpr const char* kCharacteristicInstanceIdDescUUID = "DC46F0FE-81D2-4616-B5D9-6ABDD796939A";
    static constexpr const char* kHapCharacteristicPropertiesDescUUID = "00000059-0000-1000-8000-0026BB765291";
    
    std::unique_ptr<ble::BleSessionManager> session_manager_;

    // Mapping (AID, IID) -> Characteristic UUID
    std::map<std::pair<uint64_t, uint64_t>, std::string> instance_map_;
    
    struct CharacteristicMetadata {
        uint16_t instance_id;     // Characteristic IID
        uint16_t service_id;      // Service IID this characteristic belongs to
        uint64_t char_type;       // Characteristic type UUID (short form for HAP)
        uint64_t service_type;    // Service type UUID (short form for HAP)
        uint16_t properties;      // HAP characteristic properties bitmask
        std::string user_description; // Optional GATT User Description
    };
    std::map<uint16_t, CharacteristicMetadata> pairing_char_metadata_;

    struct BroadcastConfig {
        uint16_t iid = 0;
        uint8_t interval = 0x01;  // 0x01=20ms, 0x02=1280ms, 0x03=2560ms
        bool enabled = false;
    };
    std::map<uint16_t, BroadcastConfig> broadcast_configs_;
    
    // Broadcast encryption key state (per HAP Spec 7.4.7.3-7.4.7.4)
    std::array<uint8_t, 32> broadcast_key_ = {};
    uint16_t broadcast_key_gsn_start_ = 0;  // GSN when key was generated
    bool broadcast_key_valid_ = false;
    
    bool is_connected_ = false;


    void setup_hap_service();
    void setup_protocol_info_service();
    void increment_gsn();
    uint16_t get_current_gsn();
    
    void handle_hap_write(uint16_t connection_id, const std::string& uuid, std::span<const uint8_t> data);
    void handle_hap_write_with_id(uint16_t connection_id, std::string uuid, std::span<const uint8_t> data);
    std::vector<uint8_t> handle_hap_read(uint16_t connection_id);
    
    void process_transaction(uint16_t connection_id, ble::TransactionState& state);
    std::vector<uint8_t> process_signature_read(uint16_t connection_id, uint16_t iid);
    std::vector<uint8_t> process_characteristic_read(uint16_t connection_id, std::span<const uint8_t> body);
    bool process_characteristic_write(uint16_t connection_id, uint16_t tid, const std::string& uuid, std::span<const uint8_t> body);
    
    void send_response(uint16_t conn_id, uint16_t tid, const std::string& uuid, uint8_t status, std::span<const uint8_t> body);
    
    /**
     * @brief Handle characteristic change and dispatch appropriate event type.
     * 
     * Per HAP Spec 7.4.6, selects between Connected, Broadcasted, or Disconnected
     * events based on connection state and characteristic configuration.
     */
    void handle_characteristic_change(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id = 0);
    
    /**
     * @brief Send Connected Event (zero-length GATT indication).
     * Per HAP Spec 7.4.6.1: Sent to controllers that registered for indications.
     */
    void send_connected_event(uint16_t iid);
    
    /**
     * @brief Send Broadcasted Event (encrypted advertisement with value).
     * Per HAP Spec 7.4.6.2: Sent when disconnected and broadcast is configured.
     */
    void send_broadcasted_event(uint16_t iid, const core::Value& value);
    
    /**
     * @brief Handle Disconnected Event (GSN increment + fast advertising).
     * Per HAP Spec 7.4.6.3: Updates GSN and uses 20ms advertising for 3 seconds.
     */
    void send_disconnected_event(uint16_t iid);
    
    /**
     * @brief Build encrypted advertisement payload per HAP Spec 7.4.7.3.
     */
    std::vector<uint8_t> build_encrypted_advertisement_payload(uint16_t iid, const core::Value& value);
    
    /**
     * @brief Check if broadcast encryption key is valid and not expired.
     * Per HAP Spec 7.4.7.4: Key expires after 32767 GSN increments.
     */
    bool is_broadcast_key_valid();
    
    void register_accessory_info_service();
    void register_user_services();
    void register_services_by_type(uint16_t filter_type);
};

} // namespace hap::transport
