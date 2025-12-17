#pragma once

#include "hap/platform/Ble.hpp"
#include "hap/core/AttributeDatabase.hpp"
#include "hap/transport/PairingEndpoints.hpp"
#include "hap/transport/ConnectionContext.hpp"
#include <memory>
#include <map>
#include <vector>

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

private:
    Config config_;

    // UUIDs
    // HAP Base UUID: 00000000-0000-1000-8000-0026BB765291 (Section 6.6.1)
    static constexpr const char* kHapPairingServiceUUID = "00000055-0000-1000-8000-0026BB765291";
    static constexpr const char* kHapProtocolInformationServiceUUID = "000000A2-0000-1000-8000-0026BB765291";
    static constexpr const char* kServiceSignatureCharUUID = "000000A5-0000-1000-8000-0026BB765291";
    static constexpr const char* kServiceInstanceIdCharUUID = "E604E95D-A759-4817-87D3-AA005083A0D1";
    static constexpr const char* kCharacteristicInstanceIdDescUUID = "DC46F0FE-81D2-4616-B5D9-6ABDD796939A";
    static constexpr const char* kHapCharacteristicPropertiesDescUUID = "00000059-0000-1000-8000-0026BB765291";
    
    // HAP BLE PDUs (Table 7-8)
    enum class PDUOpcode : uint8_t {
        CharacteristicSignatureRead = 0x01,
        CharacteristicWrite = 0x02,
        CharacteristicRead = 0x03,
        CharacteristicTimedWrite = 0x04,
        CharacteristicExecuteWrite = 0x05,
        ServiceSignatureRead = 0x06,
        CharacteristicConfiguration = 0x07,
        ProtocolConfiguration = 0x08
    };
    
    // Internal state for fragmentation/reassembly per connection
    struct TransactionState {
        std::vector<uint8_t> buffer;
        PDUOpcode opcode;
        uint16_t transaction_id;
        bool active = false;
        std::string target_uuid; // UUID of the characteristic being written
        uint8_t ttl = 0; // Timed Write TTL
        std::vector<uint8_t> response_buffer; // Buffer for GATT Read response
        uint64_t last_activity_ms = 0; // Timestamp of last HAP transaction (for idle timeout)
        uint64_t procedure_start_ms = 0; // Timestamp when procedure started (for 10s timeout)
        uint64_t connection_established_ms = 0; // Timestamp when connection established (for initial 10s timeout)
        uint64_t last_write_ms = 0; // Timestamp of last write (for GATT read validation)
        uint16_t expected_body_length = 0; // Expected body length from PDU header (for fragmentation)
        bool gsn_incremented = false; // Per spec: GSN increments only once per connection
        std::vector<uint8_t> timed_write_body; // Body data pending for ExecuteWrite
        uint16_t timed_write_iid = 0; // IID for pending timed write
    };
    std::map<uint16_t, TransactionState> transactions_;

    uint16_t next_iid_ = 1;

    std::map<uint16_t, std::unique_ptr<ConnectionContext>> connections_;

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
    
    
    // Subscriptions: UUID -> List of Connection IDs
    std::map<std::string, std::vector<uint16_t>> subscriptions_;


    void setup_hap_service();
    void setup_protocol_info_service();
    void increment_gsn();
    void check_session_timeouts();
    
    void handle_hap_write(uint16_t connection_id, const std::string& uuid, std::span<const uint8_t> data);
    void handle_hap_write_with_id(uint16_t connection_id, std::string uuid, std::span<const uint8_t> data);
    std::vector<uint8_t> handle_hap_read(uint16_t connection_id);
    
    void process_transaction(uint16_t connection_id, TransactionState& state);
    std::vector<uint8_t> process_signature_read(uint16_t connection_id, uint16_t iid);
    std::vector<uint8_t> process_characteristic_read(uint16_t connection_id, std::span<const uint8_t> body);
    bool process_characteristic_write(uint16_t connection_id, uint16_t tid, const std::string& uuid, std::span<const uint8_t> body);
    
    void send_response(uint16_t conn_id, uint16_t tid, const std::string& uuid, uint8_t status, std::span<const uint8_t> body);
    void send_notification_pdu(uint64_t aid, uint64_t iid, const core::Value& value);
    
    static std::vector<uint8_t> serialize_value(const core::Value& value, core::Format format);
    void register_accessory_info_service();
    void register_user_services();
    void register_services_by_type(uint16_t filter_type);
};

} // namespace hap::transport
