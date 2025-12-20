#include "hap/transport/BleTransport.hpp"
#include "hap/core/HAPStatus.hpp"
#include <random>
#include <algorithm>
#include <cstring>
#include "hap/core/TLV8.hpp"
#include <cinttypes>

static std::string to_hex_string(const uint8_t* data, size_t len) {
    std::string s;
    char buf[3];
    for (size_t i = 0; i < len; ++i) {
        snprintf(buf, sizeof(buf), "%02X", data[i]);
        s += buf;
    }
    return s;
}

namespace hap::transport {

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

[[maybe_unused]] static std::vector<uint8_t> uuid_to_bytes(uint64_t type) {
    std::vector<uint8_t> bytes;
    if (type <= 0xFFFF) {
        bytes.push_back(type & 0xFF);
        bytes.push_back((type >> 8) & 0xFF);
    }
    return bytes;
}

// HAP Base UUID: 0000XXXX-0000-1000-8000-0026BB765291
static void append_hap_uuid128(std::vector<uint8_t>& buf, uint16_t short_uuid) {
    // BLE/HAP requires 128-bit UUIDs in little-endian (reversed from UUID string)
    // Little-endian order: node | clock_seq | time_hi | time_mid | time_low
    // Node: 0026BB765291 -> little-endian: 91 52 76 BB 26 00
    buf.push_back(0x91);
    buf.push_back(0x52);
    buf.push_back(0x76);
    buf.push_back(0xBB);
    buf.push_back(0x26);
    buf.push_back(0x00);
    // clock_seq: 8000 -> little-endian: 00 80
    buf.push_back(0x00);
    buf.push_back(0x80);
    // time_hi_and_version: 1000 -> little-endian: 00 10
    buf.push_back(0x00);
    buf.push_back(0x10);
    // time_mid: 0000 -> little-endian: 00 00
    buf.push_back(0x00);
    buf.push_back(0x00);
    // time_low: 0000XXXX with short_uuid -> little-endian: XX XX 00 00
    buf.push_back(short_uuid & 0xFF);
    buf.push_back((short_uuid >> 8) & 0xFF);
    buf.push_back(0x00);
    buf.push_back(0x00);
}

BleTransport::BleTransport(Config config) : config_(std::move(config)) {
    if (!config_.ble) {
        if(config_.system) config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] No BLE platform interface provided");
    }
}

BleTransport::~BleTransport() {
    stop();
}

void BleTransport::start() {
    if (!config_.ble) return;

    config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Starting...");

    config_.ble->set_disconnect_callback([this](uint16_t connection_id) {
        config_.system->log(platform::System::LogLevel::Info, 
            "[BleTransport] Device disconnected, connection_id=" + std::to_string(connection_id));
        
        transactions_.erase(connection_id);
        
        connections_.erase(connection_id);
        
        for (auto& [uuid, subscribers] : subscriptions_) {
            subscribers.erase(
                std::remove(subscribers.begin(), subscribers.end(), connection_id),
                subscribers.end()
            );
        }
        
        config_.system->log(platform::System::LogLevel::Info, 
            "[BleTransport] Connection state cleaned up, refreshing advertising");
        
        update_advertising();
    });

    register_accessory_info_service();
    
    setup_protocol_info_service();
    
    setup_hap_service();
    
    register_user_services();
    
    update_advertising();
    
    config_.ble->start();
}

void BleTransport::stop() {
    if (config_.ble) {
        config_.ble->stop_advertising();
    }
}

void BleTransport::setup_hap_service() {
    uint16_t svc_iid = next_iid_++;
    platform::Ble::ServiceDefinition hap_service;
    hap_service.uuid = kHapPairingServiceUUID;
    hap_service.is_primary = true;

    auto add_iid_descriptor = [](platform::Ble::CharacteristicDefinition& def, uint16_t iid) {
        platform::Ble::DescriptorDefinition desc;
        desc.uuid = kCharacteristicInstanceIdDescUUID;
        desc.properties.read = true;
        desc.on_read = [iid](uint16_t) {
            std::vector<uint8_t> val;
            val.push_back(iid & 0xFF);
            val.push_back((iid >> 8) & 0xFF);
            return val;
        };
        def.descriptors.push_back(std::move(desc));
    };

    {
        platform::Ble::CharacteristicDefinition def;
        def.uuid = kServiceInstanceIdCharUUID;
        def.properties = { .read = true };
        def.on_read = [svc_iid](uint16_t) {
            std::vector<uint8_t> val;
            val.push_back(svc_iid & 0xFF);
            val.push_back((svc_iid >> 8) & 0xFF);
            return val;
        };
        hap_service.characteristics.push_back(std::move(def));
    }

    {
        uint16_t char_iid = next_iid_++;
        platform::Ble::CharacteristicDefinition def;
        def.uuid = "0000004C-0000-1000-8000-0026BB765291";
        def.properties = { .read = true, .write = true };
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Pair Setup Write: " + std::to_string(data.size()) + " bytes");
            handle_hap_write(conn, "0000004C-0000-1000-8000-0026BB765291", data);
        };
        def.on_read = [this](uint16_t conn) {
            return handle_hap_read(conn);
        };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.char_type = 0x4C;  // Pair Setup
        meta.service_type = 0x55;  // Pairing Service
        meta.properties = 0x0003;  // Read | Write (NOT Paired Read/Write!)
        pairing_char_metadata_[char_iid] = meta;
        
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Registered Pair Setup (4C) IID=" + std::to_string(char_iid));
        hap_service.characteristics.push_back(std::move(def));
    }

    {
        uint16_t char_iid = next_iid_++;
        platform::Ble::CharacteristicDefinition def;
        def.uuid = "0000004E-0000-1000-8000-0026BB765291";
        def.properties.read = true;
        def.properties.write = true;
        def.properties.indicate = false;
        def.properties.notify = false;
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            handle_hap_write(conn, "0000004E-0000-1000-8000-0026BB765291", data);
        };
        def.on_read = [this](uint16_t conn) {
            return handle_hap_read(conn);
        };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.service_type = 0x55;  // Pairing Service
        meta.char_type = 0x4E;  // Pair Verify
        meta.properties = 0x0003;  // Read | Write (NOT Paired Read/Write!)
        pairing_char_metadata_[char_iid] = meta;
        
        hap_service.characteristics.push_back(std::move(def));
    }

    {
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Adding Pairing Features...");
         uint16_t char_iid = next_iid_++;
         platform::Ble::CharacteristicDefinition def;
        def.uuid = "0000004F-0000-1000-8000-0026BB765291";
        def.properties.read = true;
        def.properties.write = true;
        def.properties.indicate = false;
        def.properties.notify = false;
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            handle_hap_write(conn, "0000004F-0000-1000-8000-0026BB765291", data);
        };
        def.on_read = [this](uint16_t conn) {
            return handle_hap_read(conn);
        };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.char_type = 0x4F;  // Pairing Features
        meta.service_type = 0x55;  // Pairing Service
        meta.properties = 0x0001;  // Read only (NOT Paired Read!)
        meta.user_description = "Pairing Features";
        pairing_char_metadata_[char_iid] = meta;
        
        hap_service.characteristics.push_back(std::move(def));
    }

    {
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Adding Pairing Pairings...");
        uint16_t char_iid = next_iid_++;
        platform::Ble::CharacteristicDefinition def;
        def.uuid = "00000050-0000-1000-8000-0026BB765291";
        def.properties.read = true;
        def.properties.write = true;
        def.properties.indicate = false;
        def.properties.notify = false;
        
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            handle_hap_write(conn, "00000050-0000-1000-8000-0026BB765291", data);
        };
        
        def.on_read = [this](uint16_t conn) {
            return handle_hap_read(conn);
        };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.char_type = 0x50;  // Pairing Pairings
        meta.service_type = 0x55;  // Pairing Service
        meta.properties = 0x0030;  // Paired Read (0x10) | Paired Write (0x20)
        pairing_char_metadata_[char_iid] = meta;
        
        hap_service.characteristics.push_back(std::move(def));
    }

    config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Registering Service...");
    config_.ble->register_service(hap_service);
}

void BleTransport::setup_protocol_info_service() {
    uint16_t svc_iid = next_iid_++;
    platform::Ble::ServiceDefinition proto_service;
    proto_service.uuid = kHapProtocolInformationServiceUUID;
    proto_service.is_primary = true;

    auto add_iid_descriptor = [](platform::Ble::CharacteristicDefinition& def, uint16_t iid) {
        platform::Ble::DescriptorDefinition desc;
        desc.uuid = kCharacteristicInstanceIdDescUUID;
        desc.properties.read = true;
        desc.on_read = [iid](uint16_t) {
            std::vector<uint8_t> val;
            val.push_back(iid & 0xFF);
            val.push_back((iid >> 8) & 0xFF);
            return val;
        };
        def.descriptors.push_back(std::move(desc));
    };

    {
        platform::Ble::CharacteristicDefinition def;
        def.uuid = kServiceInstanceIdCharUUID;
        def.properties = { .read = true };
        def.on_read = [svc_iid](uint16_t) {
            std::vector<uint8_t> val;
            val.push_back(svc_iid & 0xFF);
            val.push_back((svc_iid >> 8) & 0xFF);
            return val;
        };
        proto_service.characteristics.push_back(std::move(def));
    }

    {
        uint16_t char_iid = next_iid_++;
        platform::Ble::CharacteristicDefinition def;
        def.uuid = kServiceSignatureCharUUID;
        def.properties = { .read = true, .write = true };
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            handle_hap_write(conn, kServiceSignatureCharUUID, data);
        };
        def.on_read = [this](uint16_t conn) {
             return handle_hap_read(conn);
         };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.char_type = 0xA5;  // Service Signature
        meta.service_type = 0xA2;  // Protocol Information Service
        meta.properties = 0x0010;  // Paired Read only
        pairing_char_metadata_[char_iid] = meta;
        
        proto_service.characteristics.push_back(std::move(def));
    }

    {
        uint16_t char_iid = next_iid_++;
        platform::Ble::CharacteristicDefinition def;
        def.uuid = "00000037-0000-1000-8000-0026BB765291";
        def.properties = { .read = true, .write = true };
        def.on_write = [this](uint16_t conn, std::span<const uint8_t> data, bool) {
            handle_hap_write(conn, "00000037-0000-1000-8000-0026BB765291", data);
        };
        def.on_read = [this](uint16_t conn) {
            return handle_hap_read(conn);
        };
        add_iid_descriptor(def, char_iid);
        
        CharacteristicMetadata meta;
        meta.instance_id = char_iid;
        meta.service_id = svc_iid;
        meta.char_type = 0x37;  // Version
        meta.service_type = 0xA2;  // Protocol Information Service
        meta.properties = 0x0010;  // Paired Read only
        pairing_char_metadata_[char_iid] = meta;
        
        proto_service.characteristics.push_back(std::move(def));
    }

    config_.ble->register_service(proto_service);
}

void BleTransport::update_advertising() {
    config_.system->log(platform::System::LogLevel::Info, "[BleTransport] update_advertising entry");
    
    auto setup_id_bytes = config_.storage->get("setup_id");
    std::string setup_id;
    if (setup_id_bytes && setup_id_bytes->size() == 4) {
        setup_id = std::string(setup_id_bytes->begin(), setup_id_bytes->end());
        config_.system->log(platform::System::LogLevel::Debug, "[BleTransport] Using existing Setup ID: " + setup_id);
    } else {
        const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<> dist(0, 35);
        for (int i = 0; i < 4; ++i) {
            setup_id += charset[dist(rng)];
        }
        config_.storage->set("setup_id", std::vector<uint8_t>(setup_id.begin(), setup_id.end()));
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Generated new Setup ID: " + setup_id);
    }
    
    std::string input = setup_id + config_.accessory_id;
    std::vector<uint8_t> hash_output(64);
    config_.system->log(platform::System::LogLevel::Debug, "[BleTransport] Calculating Setup Hash for: " + input);
    
    if (config_.crypto == nullptr) {
         config_.system->log(platform::System::LogLevel::Error, "[BleTransport] No crypto provider!");
         return;
    }

    config_.crypto->sha512(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(input.data()), input.size()), 
        std::span<uint8_t, 64>(hash_output.data(), 64)
    );
    
    uint8_t setup_hash[4];
    std::copy_n(hash_output.begin(), 4, setup_hash);
    
    auto pairing_list = config_.storage->get("pairing_list");
    bool is_paired = (pairing_list && pairing_list->size() > 2);
    uint8_t status_flags = is_paired ? 0x00 : 0x01;
    
    uint8_t device_id[6] = {0};
    int scanned = sscanf(config_.accessory_id.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
        &device_id[0], &device_id[1], &device_id[2], &device_id[3], &device_id[4], &device_id[5]);
    if (scanned != 6) {
         config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] Invalid Device ID format: " + config_.accessory_id);
    }

    uint16_t gsn = 1;
    auto gsn_bytes = config_.storage->get("gsn");
    if (gsn_bytes && gsn_bytes->size() == 2) {
        gsn = static_cast<uint16_t>((*gsn_bytes)[0]) | (static_cast<uint16_t>((*gsn_bytes)[1]) << 8);
        if (gsn == 0) gsn = 1;
    } else {
        std::vector<uint8_t> gsn_data = {0x01, 0x00};
        config_.storage->set("gsn", gsn_data);
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Initialized GSN to 1");
    }

    uint16_t config_number = 1;
    auto cn_bytes = config_.storage->get("config_number");
    if (cn_bytes && !cn_bytes->empty()) {
        std::string cn_str(cn_bytes->begin(), cn_bytes->end());
        config_number = static_cast<uint16_t>(std::stoi(cn_str));
    }

    auto adv = platform::Ble::Advertisement::create_hap(
        status_flags,
        device_id,
        config_.category_id,
        gsn,
        config_number,
        setup_hash
    );
    
    adv.local_name = config_.device_name;

    std::string adv_hex;
    for (auto byte : adv.manufacturer_data) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", byte);
        adv_hex += buf;
    }
    config_.system->log(platform::System::LogLevel::Info, 
        "[BleTransport] Advertisement Data (" + std::to_string(adv.manufacturer_data.size()) + " bytes): " + adv_hex);
    config_.system->log(platform::System::LogLevel::Info,
        "[BleTransport] SF=" + std::to_string(status_flags) + 
        " ACID=" + std::to_string(config_.category_id) +
        " GSN=" + std::to_string(gsn) +
        " CN=" + std::to_string(config_number));

    config_.ble->start_advertising(adv, config_.ble->interval_config.normal_interval_ms);
}

void BleTransport::set_accessory_id(const std::string& new_id) {
    config_.accessory_id = new_id;
    config_.system->log(platform::System::LogLevel::Info, 
        "[BleTransport] Accessory ID updated to: " + new_id);
}

void BleTransport::notify_value_changed(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id) {
    handle_characteristic_change(aid, iid, value, exclude_conn_id);
}

void BleTransport::increment_gsn() {
    // Per Spec 7.4.6: GSN increments on characteristic changes
    // Range: 1-65535, wraps to 1 on overflow
    auto gsn_bytes = config_.storage->get("gsn");
    uint16_t gsn = 1;
    
    if (gsn_bytes && gsn_bytes->size() == 2) {
        gsn = static_cast<uint16_t>((*gsn_bytes)[0]) | (static_cast<uint16_t>((*gsn_bytes)[1]) << 8);
    }
    
    gsn++;
    if (gsn == 0) {
        gsn = 1;
    }
    
    std::vector<uint8_t> gsn_data = {
        static_cast<uint8_t>(gsn & 0xFF),
        static_cast<uint8_t>((gsn >> 8) & 0xFF)
    };
    config_.storage->set("gsn", gsn_data);
    
    config_.system->log(platform::System::LogLevel::Debug, 
        "[BleTransport] GSN incremented to " + std::to_string(gsn));
    
    update_advertising();
}

void BleTransport::check_session_timeouts() {
    // Per Spec 7.2.5: 30-second idle timeout
    // Per Spec 7.3.1: 10-second HAP procedure timeout
    // Per Spec 7.5 Req #40: 10-second initial procedure timeout
    
    uint64_t current_time = config_.system->millis();
    std::vector<uint16_t> connections_to_terminate;
    
    for (auto& [conn_id, state] : transactions_) {
        // Check 10-second initial procedure timeout (Req #40)
        // After BLE link established, first HAP procedure must begin within 10s
        if (state.connection_established_ms > 0 && !state.active && state.last_activity_ms == 0) {
            uint64_t time_since_connect = current_time - state.connection_established_ms;
            if (time_since_connect > 10000) { // 10 seconds
                config_.system->log(platform::System::LogLevel::Warning,
                    "[BleTransport] Initial procedure timeout for connection " + std::to_string(conn_id) + 
                    " - no HAP procedure started within 10s");
                connections_to_terminate.push_back(conn_id);
                continue;
            }
        }
        
        if (state.active && state.procedure_start_ms > 0) {
            uint64_t procedure_duration = current_time - state.procedure_start_ms;
            if (procedure_duration > 10000) { // 10 seconds
                config_.system->log(platform::System::LogLevel::Warning,
                    "[BleTransport] Procedure timeout for connection " + std::to_string(conn_id));
                connections_to_terminate.push_back(conn_id);
                continue;
            }
        }
        
        if (state.last_activity_ms > 0) {
            uint64_t idle_duration = current_time - state.last_activity_ms;
            if (idle_duration > 30000) { // 30 seconds
                config_.system->log(platform::System::LogLevel::Info,
                    "[BleTransport] Idle timeout for connection " + std::to_string(conn_id));
                connections_to_terminate.push_back(conn_id);
            }
        }
    }
    
    for (uint16_t conn_id : connections_to_terminate) {
        transactions_.erase(conn_id);
        connections_.erase(conn_id);
        config_.ble->disconnect(conn_id);
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Terminated connection " + std::to_string(conn_id) + " due to timeout");
    }
}

void BleTransport::handle_hap_write_with_id(uint16_t connection_id, std::string uuid, std::span<const uint8_t> data) {
    config_.system->log(platform::System::LogLevel::Debug, "[BleTransport] Write to Char UUID: " + uuid);
    handle_hap_write(connection_id, uuid, data);
}

void BleTransport::handle_hap_write(uint16_t connection_id, const std::string& uuid, std::span<const uint8_t> data) {
    if (data.empty()) return;
    
    config_.system->log(platform::System::LogLevel::Debug, "[BleTransport] Write PDU Fragment (" + std::to_string(data.size()) + " bytes): " + to_hex_string(data.data(), data.size()));

    bool session_is_secured = false;
    if (connections_.contains(connection_id)) {
        auto& ctx = *connections_[connection_id];
        session_is_secured = ctx.is_encrypted();
    }
    
    bool requires_encryption = true;
    {
        if (uuid.size() >= 8) {
            std::string short_uuid_str = uuid.substr(4, 4);
            unsigned int short_uuid = 0;
            if (sscanf(short_uuid_str.c_str(), "%x", &short_uuid) == 1) {
                if (short_uuid == 0x4C || short_uuid == 0x4E || short_uuid == 0x4F) {
                    requires_encryption = false;
                }
            }
        }
    }

    std::vector<uint8_t> decrypted_data;
    std::span<const uint8_t> working_data = data;
    
    if (session_is_secured && requires_encryption) {
        auto& ctx = *connections_[connection_id];
        auto decrypted = ctx.get_secure_session()->decrypt_ble_pdu(data);
        if (!decrypted) {
            config_.system->log(platform::System::LogLevel::Error, 
                "[BleTransport] Decryption failed for connection " + std::to_string(connection_id) + " - disconnecting");
            // Per HAP Spec 6.5.2.2: Close connection on decryption failure
            config_.ble->disconnect(connection_id);
            connections_.erase(connection_id);
            transactions_.erase(connection_id);
            return;
        }
        decrypted_data = std::move(*decrypted);
        working_data = decrypted_data;
        config_.system->log(platform::System::LogLevel::Debug, 
            "[BleTransport] Decrypted PDU (" + std::to_string(decrypted_data.size()) + " bytes): " + 
            to_hex_string(decrypted_data.data(), decrypted_data.size()));
    }
    
    if (working_data.empty()) return;
    uint8_t control_field = working_data[0];
    bool continuation = (control_field & 0x80) != 0;
    PDUOpcode opcode = PDUOpcode::CharacteristicWrite;
    uint16_t tid = 0;
    
    if (!continuation) {
        if (working_data.size() < 3) {
            config_.system->log(platform::System::LogLevel::Error, "[BleTransport] PDU too short");
            return;
        }
        opcode = static_cast<PDUOpcode>(working_data[1]);
        tid = working_data[2];
        
        config_.system->log(platform::System::LogLevel::Info, "[BleTransport] New Transaction TID=" + std::to_string(tid) + " Opcode=" + std::to_string((int)opcode));
        
        auto& state = transactions_[connection_id];
        state.opcode = opcode;
        state.transaction_id = tid;
        state.target_uuid = uuid;
        state.buffer.clear();
        state.response_buffer.clear();
        state.active = true;
        state.last_activity_ms = config_.system->millis();
        state.procedure_start_ms = config_.system->millis();
        state.last_write_ms = config_.system->millis();
        
        if (state.connection_established_ms == 0) {
            state.connection_established_ms = config_.system->millis();
        }
        
        state.buffer.insert(state.buffer.end(), working_data.begin(), working_data.end());
        process_transaction(connection_id, state);
    } else {
        auto& state = transactions_[connection_id];
        if (!state.active) {
             config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] Orphaned continuation fragment!");
             return;
        }
        
        // Spec 7.3.3.1: TID is present in every fragment.
        // Packet: CF (1) | TID (1) | Data...
        if (working_data.size() < 2) return;
        uint16_t cont_tid = working_data[1];
        if (cont_tid != state.transaction_id) {
            config_.system->log(platform::System::LogLevel::Error, "[BleTransport] TID mismatch in continuation! Expected " + std::to_string(state.transaction_id) + " got " + std::to_string(cont_tid));
            return;
        }

        // Continuation Body starts at index 2 (CF, TID)
        state.buffer.insert(state.buffer.end(), working_data.begin() + 2, working_data.end());
        process_transaction(connection_id, state);
    }
}

std::vector<uint8_t> BleTransport::handle_hap_read(uint16_t connection_id) {
    if (transactions_.contains(connection_id)) {
        auto& state = transactions_[connection_id];
        
        if (state.last_write_ms > 0) {
            uint64_t current_time = config_.system->millis();
            uint64_t time_since_write = current_time - state.last_write_ms;
            
            if (time_since_write > 10000) { // 10 seconds
                config_.system->log(platform::System::LogLevel::Warning,
                    "[BleTransport] Rejecting GATT Read - >10s since write (Req #12)");
                return {};
            }
        }
        
        config_.system->log(platform::System::LogLevel::Info, 
            "[BleTransport] Handling GATT Read. Returning " + 
            std::to_string(state.response_buffer.size()) + " bytes");
        return state.response_buffer;
    }
    return {};
}

void BleTransport::process_transaction(uint16_t connection_id, TransactionState& state) {
    // Header is 5 bytes: CF(1) | Opcode(1) | TID(1) | IID(2)
    if (state.buffer.size() < 5) return; 
    
    PDUOpcode opcode = state.opcode;
    
    // For Write operations (opcode 0x02, 0x04), the header is 7 bytes:
    // CF(1) | Opcode(1) | TID(1) | IID(2) | BodyLen(2) | Body...
    if (opcode == PDUOpcode::CharacteristicWrite || 
        opcode == PDUOpcode::CharacteristicTimedWrite ||
        opcode == PDUOpcode::CharacteristicConfiguration ||
        opcode == PDUOpcode::ProtocolConfiguration) {
        
        if (state.buffer.size() < 7) return;
        
        uint16_t body_length = state.buffer[5] | (static_cast<uint16_t>(state.buffer[6]) << 8);
        state.expected_body_length = body_length;
        
        size_t expected_total = 7 + body_length;
        
        if (state.buffer.size() < expected_total) {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[BleTransport] Waiting for more fragments: have " + 
                std::to_string(state.buffer.size()) + " bytes, need " + 
                std::to_string(expected_total) + " bytes");
            return;
        }
        
        config_.system->log(platform::System::LogLevel::Debug, 
            "[BleTransport] All fragments received: " + 
            std::to_string(state.buffer.size()) + " bytes (body=" + 
            std::to_string(body_length) + ")");
    }
    
    uint16_t tid = state.buffer[2];
    uint16_t iid = state.buffer[3] | (state.buffer[4] << 8);

    config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Processing Opcode " + std::to_string((int)opcode) + " TID=" + std::to_string(tid));
    
    auto find_service = [&](uint16_t target_iid) -> std::shared_ptr<core::Service> {
        if (!config_.database) return nullptr;
        for (const auto& acc : config_.database->accessories()) {
            for (const auto& svc : acc->services()) {
                if (svc->iid() == target_iid) return svc;
            }
        }
        return nullptr;
    };
    (void)find_service;

    auto find_char_in_db = [&](uint16_t target_iid) -> std::shared_ptr<core::Characteristic> {
        if (!config_.database) return nullptr;
        for (const auto& acc : config_.database->accessories()) {
            for (const auto& svc : acc->services()) {
                for (const auto& ch : svc->characteristics()) {
                    if (ch->iid() == target_iid) return ch;
                }
            }
        }
        return nullptr;
    };

    if (opcode == PDUOpcode::ServiceSignatureRead) {
        state.active = false;
        state.procedure_start_ms = 0;
        
        std::vector<uint8_t> sig_response;
        bool is_primary = false;
        bool found = false;
        std::vector<uint16_t> linked_services;

        for (const auto& [cid, meta] : pairing_char_metadata_) {
            if (meta.service_id == iid) {
                is_primary = true;
                found = true;
                break;
            }
        }

        if (!found) {
            auto svc = find_service(iid);
            if (svc) {
                is_primary = svc->is_primary();
                found = true;
                for (uint64_t linked_iid : svc->linked_services()) {
                    linked_services.push_back(static_cast<uint16_t>(linked_iid));
                }
            }
        }

        // Per Spec 7.3.4.13: If invalid Service IID, return props=0 and linked=0 length
        if (!found) {
            config_.system->log(platform::System::LogLevel::Warning, 
                "[BleTransport] Service Signature Read IID=" + std::to_string(iid) + " Not Found - returning empty");
            // Return empty response with props=0
            sig_response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceProperties);
            sig_response.push_back(0x02);
            sig_response.push_back(0x00); // props = 0
            sig_response.push_back(0x00);
            
            // Linked services with length 0
            sig_response.push_back((uint8_t)HAPBLEPDUTLVType::LinkedServices);
            sig_response.push_back(0x00); // length = 0
        } else {
            // Service Properties (TLV Type 0x0F - HAPBLEPDUTLVType::ServiceProperties)
            // Bit 0: Primary Service
            // Bit 1: Hidden Service
            // Bit 2: Supports HAP Protocol Configuration (only for Protocol Info Service 0xA2)
            uint16_t props = 0;
            
            if (is_primary) {
                props |= 0x0001;  // Bit 0: Primary Service
            }
            
            // TLV: Type=0x0F, Len=2, Value=Integer
            sig_response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceProperties);
            sig_response.push_back(0x02);
            sig_response.push_back(props & 0xFF);
            sig_response.push_back((props >> 8) & 0xFF);
            
            // Linked Services (TLV Type 0x10)
            // Format: Array of 16-bit service IIDs (little-endian)
            if (!linked_services.empty()) {
                sig_response.push_back((uint8_t)HAPBLEPDUTLVType::LinkedServices);
                sig_response.push_back(static_cast<uint8_t>(linked_services.size() * 2));
                for (uint16_t linked_iid : linked_services) {
                    sig_response.push_back(linked_iid & 0xFF);
                    sig_response.push_back((linked_iid >> 8) & 0xFF);
                }
            } else {
                sig_response.push_back((uint8_t)HAPBLEPDUTLVType::LinkedServices);
                sig_response.push_back(0x00);
            }
            
            config_.system->log(platform::System::LogLevel::Info, 
                "[BleTransport] Service Signature Read IID=" + std::to_string(iid) + 
                " Primary=" + std::to_string(is_primary));
        }
        
        send_response(connection_id, state.transaction_id, state.target_uuid, 0x00, sig_response);
        return;
    }
    
    if (opcode == PDUOpcode::CharacteristicSignatureRead) {
        state.active = false; 
        
        std::vector<uint8_t> sig_response = process_signature_read(connection_id, iid);
        
        uint8_t status = 0x00;
        if (sig_response.empty()) {
             config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] Char Signature Read IID=" + std::to_string(iid) + " Not Found");
             status = 0x05; // Invalid Request (Attribute Not Found)
        } else {
             config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Char Signature Read IID=" + std::to_string(iid) + " Len=" + std::to_string(sig_response.size()));
             config_.system->log(platform::System::LogLevel::Debug, "[BleTransport] Signature Response: " + to_hex_string(sig_response.data(), sig_response.size()));
        }
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, sig_response);
        return;
    }

    // For operations with Body (Write, Timed Write, Configuration)
    // Header is 7 bytes: CF(1) | Opcode(1) | TID(1) | IID(2) | BodyLen(2)
    // Body starts at offset 7
    // For Read (0x03), header is only 5 bytes with no body
    
    size_t body_offset = 5;
    if (opcode == PDUOpcode::CharacteristicWrite || 
        opcode == PDUOpcode::CharacteristicTimedWrite ||
        opcode == PDUOpcode::CharacteristicConfiguration ||
        opcode == PDUOpcode::ProtocolConfiguration) {
        body_offset = 7;
    }
    
    std::span<const uint8_t> body;
    if (state.buffer.size() > body_offset) {
        body = std::span<const uint8_t>(state.buffer.data() + body_offset, state.buffer.size() - body_offset);
    }
    
    if (opcode == PDUOpcode::CharacteristicRead) {
        
        std::vector<uint8_t> value_bytes;
        uint8_t status = 0x00;
        
        auto meta_it = pairing_char_metadata_.find(iid);
        if (meta_it != pairing_char_metadata_.end()) {
            if (meta_it->second.char_type == 0x4F) { // Pairing Features
                value_bytes = {0x01, 0x01, 0x00};
                config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Pairing Features Read: returning 0x00");
            } else if (meta_it->second.char_type == 0x37) { // Version
                // HAP Spec 7.4.4.5.2: Version characteristic returns protocol version string
                // Format: "major.minor.revision" e.g., "1.1.0"
                // Per HAP spec, current version for BLE is 1.1.0
                std::string version = "1.1.0";
                value_bytes.push_back(0x01); // TLV Type: HAP-Param-Value
                value_bytes.push_back(static_cast<uint8_t>(version.size())); // Length
                value_bytes.insert(value_bytes.end(), version.begin(), version.end()); // Value
                config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Version Read: returning " + version);
            } else {
                status = 0x05; // Invalid Request
            }
        } 
        else {
            auto ch = find_char_in_db(iid);
            if (ch) {
                auto raw_value = serialize_value(ch->get_value(), ch->format());
                // Wrap in TLV: Type=0x01, Length, Value
                value_bytes.push_back(0x01); // Type: HAP-Param-Value
                value_bytes.push_back(static_cast<uint8_t>(raw_value.size())); // Length
                value_bytes.insert(value_bytes.end(), raw_value.begin(), raw_value.end()); // Value
            } else {
                status = 0x05; // Invalid Request (Attribute Not Found)
                config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] Read IID=" + std::to_string(iid) + " Not Found");
            }
        }
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, value_bytes);
    }
    else if (opcode == PDUOpcode::CharacteristicWrite) {
        uint8_t status = 0x00;
        std::vector<uint8_t> response_body;

        auto meta_it = pairing_char_metadata_.find(iid);
        if (meta_it != pairing_char_metadata_.end()) {
             uint8_t type = meta_it->second.char_type;
             
             if (!connections_.contains(connection_id)) {
                connections_[connection_id] = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
             }
             auto& ctx = *connections_[connection_id];
             
             // HAP-BLE Pair Setup/Verify are "Write-with-Response" (Spec 7.3.5.5)
             // The body is a LIST of TLVs:
             // - kTLVType_ReturnResponse (0x09): (Empty implies request response)
             // - kTLVType_Value (0x01): The actual SRP payload (M1, M3, etc)
             
             std::vector<uint8_t> inner_body;
             bool return_response_requested = false;

             // Only parse as BLE TLVs for Pairing Services (Setup 4C, Verify 4E, Pairings 50)
             if (type == 0x4C || type == 0x4E || type == 0x50) {
                 auto ble_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
                 
                 // Check for Return-Response (0x09)
                 if (core::TLV8::find(ble_tlvs, (uint8_t)HAPBLEPDUTLVType::ReturnResponse)) {
                     return_response_requested = true;
                 }
                 
                 // Extract Value (0x01)
                 auto val = core::TLV8::find(ble_tlvs, 0x01); // 0x01 = Param-Value
                 if (val) {
                     inner_body.assign(val->begin(), val->end());
                 } else {
                     config_.system->log(platform::System::LogLevel::Warning, "[BleTransport] Warning: Pairing Write missing Value TLV wrapper. Using raw body.");
                     inner_body.assign(body.begin(), body.end());
                 }
             } else {
                inner_body.assign(body.begin(), body.end());
             }

             Request req;
             req.body = std::move(inner_body);
             req.method = Method::POST;
             
             Response resp;
             if (type == 0x4C) { // Pair Setup
                req.path = "/pair-setup";
                resp = config_.pairing_endpoints->handle_pair_setup(req, ctx);
             } else if (type == 0x4E) { // Pair Verify
                req.path = "/pair-verify";
                resp = config_.pairing_endpoints->handle_pair_verify(req, ctx);
             } else if (type == 0x50) { // Pairings
                req.path = "/pairings";
                resp = config_.pairing_endpoints->handle_pairings(req, ctx);
             } else if (type == 0xA5) { // Service Signature
                config_.system->log(platform::System::LogLevel::Info, "[BleTransport] Software Auth Write - Skipping (Success)");
                resp = Response{Status::OK}; 
             }
             
             status = (resp.status == Status::OK) ? 0x00 : 0x02;

             // If Return-Response was requested, we MUST wrap the response body in Result TLVs
             // TlV 0x01 (Value) -> Response Body
             if (return_response_requested && !resp.body.empty()) {
                 std::vector<core::TLV> resp_tlvs;
                 resp_tlvs.emplace_back(0x01, resp.body); // 0x01 = Param-Value
                 response_body = core::TLV8::encode(resp_tlvs);
             } else {
                 response_body = resp.body;
             }
        }
        else {
             auto ch = find_char_in_db(iid);
             if (ch) {
                 // Parse body as TLVs to extract HAP-Param-Value (0x01)
                 auto body_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
                 auto value_tlv = core::TLV8::find(body_tlvs, 0x01);
                 if (value_tlv && !value_tlv->empty()) {
                     core::Value new_value;
                     switch (ch->format()) {
                         case core::Format::Bool:
                             new_value = static_cast<bool>((*value_tlv)[0] != 0);
                             break;
                         case core::Format::UInt8:
                             new_value = (*value_tlv)[0];
                             break;
                         case core::Format::UInt16:
                             if (value_tlv->size() >= 2) {
                                 new_value = static_cast<uint16_t>((*value_tlv)[0] | ((*value_tlv)[1] << 8));
                             }
                             break;
                         case core::Format::UInt32:
                             if (value_tlv->size() >= 4) {
                                 new_value = static_cast<uint32_t>(
                                     (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                     ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24));
                             }
                             break;
                         case core::Format::Int:
                             if (value_tlv->size() >= 4) {
                                 new_value = static_cast<int32_t>(
                                     (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                     ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24));
                             }
                             break;
                         case core::Format::Float:
                            if (value_tlv->size() >= 4) {
                                uint32_t raw = (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                            ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24);
                                float f;
                                std::memcpy(&f, &raw, sizeof(f));
                                new_value = f;
                            }
                            break;
                         case core::Format::String:
                            new_value = std::string(value_tlv->begin(), value_tlv->end());
                            break;
                         default:
                            break;
                     }
                     
                     // Set the value (this also invokes the write callback)
                     ch->set_value(new_value);
                     config_.system->log(platform::System::LogLevel::Info, 
                         "[BleTransport] Write IID=" + std::to_string(iid) + " success");
                     
                     // Per HAP Spec 7.4.1.8: GSN increments on first characteristic change per connection
                     if (!state.gsn_incremented) {
                         state.gsn_incremented = true;
                         increment_gsn();
                     }
                     
                     // Trigger BLE events (Connected/Broadcasted/Disconnected per Spec 7.4.6)
                     // Exclude the connection that made the write (per spec: don't notify originator)
                     handle_characteristic_change(1, iid, new_value, connection_id);
                 } else {
                     config_.system->log(platform::System::LogLevel::Warning, 
                         "[BleTransport] Write IID=" + std::to_string(iid) + " - no value TLV found");
                     status = 0x06; // Invalid Request
                 }
             } else {
                 status = 0x05; // Not Found
             }
        }
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, response_body);
    }
    else if (opcode == PDUOpcode::CharacteristicTimedWrite) {
        state.timed_write_body.assign(body.begin(), body.end());
        state.timed_write_iid = iid;
        state.target_uuid = state.target_uuid;
        
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Timed Write stored for IID=" + std::to_string(iid) + 
            " Body size=" + std::to_string(body.size()));
        
        send_response(connection_id, state.transaction_id, state.target_uuid, 0x00, {});
    }
    else if (opcode == PDUOpcode::CharacteristicExecuteWrite) {
        uint8_t status = 0x00;
        std::vector<uint8_t> response_body;
        
        if (state.timed_write_body.empty()) {
            config_.system->log(platform::System::LogLevel::Warning,
                "[BleTransport] Execute Write with no pending timed write");
            status = 0x06; // Invalid Request
        } else {
            config_.system->log(platform::System::LogLevel::Info,
                "[BleTransport] Executing pending timed write for IID=" + std::to_string(state.timed_write_iid));
            
            auto meta_it = pairing_char_metadata_.find(state.timed_write_iid);
            if (meta_it != pairing_char_metadata_.end()) {
                uint8_t type = meta_it->second.char_type;
                
                if (!connections_.contains(connection_id)) {
                    connections_[connection_id] = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
                }
                auto& ctx = *connections_[connection_id];
                
                std::vector<uint8_t> inner_body;
                bool return_response_requested = false;
                
                // Parse BLE TLVs for Pairing characteristics
                if (type == 0x4C || type == 0x4E || type == 0x50) {
                    auto ble_tlvs = core::TLV8::parse(state.timed_write_body);
                    
                    if (core::TLV8::find(ble_tlvs, (uint8_t)HAPBLEPDUTLVType::ReturnResponse)) {
                        return_response_requested = true;
                    }
                    
                    auto val = core::TLV8::find(ble_tlvs, 0x01);
                    if (val) {
                        inner_body.assign(val->begin(), val->end());
                    } else {
                        inner_body = state.timed_write_body;
                    }
                } else {
                    inner_body = state.timed_write_body;
                }
                
                Request req;
                req.body = std::move(inner_body);
                req.method = Method::POST;
                
                Response resp;
                if (type == 0x4C) {
                    req.path = "/pair-setup";
                    resp = config_.pairing_endpoints->handle_pair_setup(req, ctx);
                } else if (type == 0x4E) {
                    req.path = "/pair-verify";
                    resp = config_.pairing_endpoints->handle_pair_verify(req, ctx);
                } else if (type == 0x50) {
                    req.path = "/pairings";
                    resp = config_.pairing_endpoints->handle_pairings(req, ctx);
                }
                
                status = (resp.status == Status::OK) ? 0x00 : 0x02;
                
                if (return_response_requested && !resp.body.empty()) {
                    std::vector<core::TLV> resp_tlvs;
                    resp_tlvs.emplace_back(0x01, resp.body);
                    response_body = core::TLV8::encode(resp_tlvs);
                } else {
                    response_body = resp.body;
                }
            } else {
                auto ch = find_char_in_db(state.timed_write_iid);
                if (ch) {
                    auto body_tlvs = core::TLV8::parse(state.timed_write_body);
                    auto value_tlv = core::TLV8::find(body_tlvs, 0x01);
                    if (value_tlv && !value_tlv->empty()) {
                        core::Value new_value;
                        switch (ch->format()) {
                            case core::Format::Bool:
                                new_value = static_cast<bool>((*value_tlv)[0] != 0);
                                break;
                            case core::Format::UInt8:
                                new_value = (*value_tlv)[0];
                                break;
                            case core::Format::UInt16:
                                if (value_tlv->size() >= 2) {
                                    new_value = static_cast<uint16_t>((*value_tlv)[0] | ((*value_tlv)[1] << 8));
                                }
                                break;
                            case core::Format::UInt32:
                                if (value_tlv->size() >= 4) {
                                    new_value = static_cast<uint32_t>(
                                        (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                        ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24));
                                }
                                break;
                            case core::Format::Int:
                                if (value_tlv->size() >= 4) {
                                    new_value = static_cast<int32_t>(
                                        (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                        ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24));
                                }
                                break;
                            case core::Format::Float:
                                if (value_tlv->size() >= 4) {
                                    uint32_t raw = (*value_tlv)[0] | ((*value_tlv)[1] << 8) |
                                                ((*value_tlv)[2] << 16) | ((*value_tlv)[3] << 24);
                                    float f;
                                    std::memcpy(&f, &raw, sizeof(f));
                                    new_value = f;
                                }
                                break;
                            case core::Format::String:
                                new_value = std::string(value_tlv->begin(), value_tlv->end());
                                break;
                            default:
                                break;
                        }
                        
                        // Pass EventSource so the originating connection is excluded from notifications
                        ch->set_value(new_value, core::EventSource::from_connection(connection_id));
                        config_.system->log(platform::System::LogLevel::Info, 
                            "[BleTransport] Execute Timed Write IID=" + std::to_string(state.timed_write_iid) + " success");
                        
                        // Per HAP Spec 7.4.1.8: GSN increments on first characteristic change per connection
                        if (!state.gsn_incremented) {
                            state.gsn_incremented = true;
                            increment_gsn();
                        }
                        
                    } else {
                        config_.system->log(platform::System::LogLevel::Warning, 
                            "[BleTransport] Execute Timed Write IID=" + std::to_string(state.timed_write_iid) + " - no value TLV found");
                        status = 0x06; // Invalid Request
                    }
                } else {
                    config_.system->log(platform::System::LogLevel::Warning, 
                        "[BleTransport] Execute Timed Write IID=" + std::to_string(state.timed_write_iid) + " - characteristic not found");
                    status = 0x05; // Not Found
                }
            }
            
            state.timed_write_body.clear();
            state.timed_write_iid = 0;
        }
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, response_body);
    }
    else if (opcode == PDUOpcode::CharacteristicConfiguration) {
        // HAP-Characteristic-Configuration-Request/Response
        // Per Spec 7.3.4.14-15, Table 7-28, 7-29, 7-30
        
        std::vector<uint8_t> response_body;
        uint8_t status = 0x00;
        
        auto config_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
        
        uint16_t properties = 0x0000; // Broadcast disabled by default
        uint8_t broadcast_interval = 0x01; // 20ms default (Table 7-30)
        
        auto props_tlv = core::TLV8::find(config_tlvs, 0x01);
        if (props_tlv && props_tlv->size() == 2) {
            properties = static_cast<uint16_t>((*props_tlv)[0]) | 
                        (static_cast<uint16_t>((*props_tlv)[1]) << 8);
        }
        
        auto interval_tlv = core::TLV8::find(config_tlvs, 0x02);
        if (interval_tlv && interval_tlv->size() == 1) {
            broadcast_interval = (*interval_tlv)[0];
            if (broadcast_interval > 0x03) {
                broadcast_interval = 0x01;
            }
        }
        
        std::vector<core::TLV> resp_tlvs;
        
        std::vector<uint8_t> props_data = {
            static_cast<uint8_t>(properties & 0xFF),
            static_cast<uint8_t>((properties >> 8) & 0xFF)
        };
        resp_tlvs.emplace_back(0x01, props_data);
        
        resp_tlvs.emplace_back(0x02, std::vector<uint8_t>{broadcast_interval});
        
        response_body = core::TLV8::encode(resp_tlvs);
        
        // Store broadcast configuration for this characteristic
        // Per HAP Spec 7.4.6.2: Properties bit 0x0001 enables broadcast notification
        bool broadcast_enabled = (properties & 0x0001) != 0;
        broadcast_configs_[iid] = BroadcastConfig{iid, broadcast_interval, broadcast_enabled};
        
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Characteristic Configuration IID=" + std::to_string(iid) +
            " Props=" + std::to_string(properties) + " Interval=" + std::to_string(broadcast_interval) +
            " BroadcastEnabled=" + std::to_string(broadcast_enabled));
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, response_body);
    }
    else if (opcode == PDUOpcode::ProtocolConfiguration) {
        // HAP-Protocol-Configuration-Request/Response
        // Per Spec 7.3.4.16-17, Table 7-32, 7-34
        
        std::vector<uint8_t> response_body;
        uint8_t status = 0x00;
        
        auto config_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
        
        auto gen_key_tlv = core::TLV8::find(config_tlvs, 0x01);
        auto get_all_tlv = core::TLV8::find(config_tlvs, 0x02);
        
        std::vector<core::TLV> resp_tlvs;
        
        if (gen_key_tlv) {
            // Generate broadcast encryption key per HAP Spec 7.4.7.3:
            // BroadcastEncryptionKey = HKDF-SHA-512(
            //     IKM = Current Session shared secret,
            //     Salt = Controller's Ed25519 long-term public key,
            //     Info = "Broadcast-Encryption-Key",
            //     L = 32 bytes
            // )
            config_.system->log(platform::System::LogLevel::Info,
                "[BleTransport] Protocol Config: Generate Broadcast Encryption Key requested");
            
            if (!connections_.contains(connection_id) || !connections_[connection_id]->is_encrypted()) {
                config_.system->log(platform::System::LogLevel::Error,
                    "[BleTransport] Protocol Config: Cannot generate key - no secure session");
                status = 0x06; // Invalid Request
            } else {
                auto& ctx = *connections_[connection_id];
                
                // Get controller LTPK from storage
                std::string pairing_key = "pairing_" + ctx.controller_id();
                auto controller_ltpk = config_.storage->get(pairing_key);
                
                if (!controller_ltpk || controller_ltpk->size() != 32) {
                    config_.system->log(platform::System::LogLevel::Error,
                        "[BleTransport] Protocol Config: Controller LTPK not found for: " + ctx.controller_id());
                    status = 0x06; // Invalid Request
                } else {
                    // Derive broadcast encryption key using HKDF-SHA-512
                    std::array<uint8_t, 32> broadcast_key;
                    config_.crypto->hkdf_sha512(
                        ctx.session_shared_secret(),                                         // IKM: Session shared secret
                        std::span<const uint8_t>(controller_ltpk->data(), 32),              // Salt: Controller LTPK
                        std::span(reinterpret_cast<const uint8_t*>("Broadcast-Encryption-Key"), 24), // Info
                        broadcast_key                                                        // Output: 32 bytes
                    );
                    
                    std::copy(broadcast_key.begin(), broadcast_key.end(), broadcast_key_.begin());
                    broadcast_key_valid_ = true;
                    broadcast_key_gsn_start_ = get_current_gsn();
                    
                    resp_tlvs.emplace_back(0x04, std::vector<uint8_t>(broadcast_key.begin(), broadcast_key.end()));
                    
                    config_.system->log(platform::System::LogLevel::Info,
                        "[BleTransport] Protocol Config: Generated and stored Broadcast Encryption Key (GSN start=" + 
                        std::to_string(broadcast_key_gsn_start_) + ")");
                }
            }
        }
        
        if (get_all_tlv || true) {
            uint16_t gsn = 1;
            auto gsn_bytes = config_.storage->get("gsn");
            if (gsn_bytes && gsn_bytes->size() == 2) {
                gsn = static_cast<uint16_t>((*gsn_bytes)[0]) | 
                     (static_cast<uint16_t>((*gsn_bytes)[1]) << 8);
            }
            
            // State Number TLV (0x01)
            std::vector<uint8_t> state_data = {
                static_cast<uint8_t>(gsn & 0xFF),
                static_cast<uint8_t>((gsn >> 8) & 0xFF)
            };
            resp_tlvs.emplace_back(0x01, state_data);
            
            // Configuration Number TLV (0x02)
            resp_tlvs.emplace_back(0x02, std::vector<uint8_t>{
                static_cast<uint8_t>(config_.config_number & 0xFF)
            });
            
            // Accessory Advertising Identifier TLV (0x03) - 6 bytes
            // Use Device ID as advertising identifier
            uint8_t device_id[6] = {0};
            sscanf(config_.accessory_id.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &device_id[0], &device_id[1], &device_id[2], 
                &device_id[3], &device_id[4], &device_id[5]);
            resp_tlvs.emplace_back(0x03, std::vector<uint8_t>(device_id, device_id + 6));
        }
        
        response_body = core::TLV8::encode(resp_tlvs);
        
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Protocol Configuration completed");
        
        send_response(connection_id, state.transaction_id, state.target_uuid, status, response_body);
    }
    else if (opcode == PDUOpcode::ServiceSignatureRead) {
        send_response(connection_id, state.transaction_id, state.target_uuid, 0x00, {});
    }
    else {
        send_response(connection_id, state.transaction_id, state.target_uuid, 0x01, {});
    }
}

void BleTransport::send_response(uint16_t conn_id, uint16_t tid, const std::string& uuid, uint8_t status, std::span<const uint8_t> body) {
    static constexpr size_t kMaxPayload = 20; 
    
    std::vector<uint8_t> packet;
    packet.reserve(kMaxPayload);
    
    // Header
    // Control Field: Response type (bits 1-3 = 001) = 0x02
    // Per Spec Table 7-5: Bit 3=0, Bit 2=0, Bit 1=1 -> Response
    // Bit 7=0 (first fragment), Bit 4=0 (16-bit IIDs), Bit 0=0 (1 byte CF)
    packet.push_back(0x02); // CF: Response type
    
    // TID: Transaction Identifier (1 byte)
    packet.push_back(tid & 0xFF);
    
    // Status: HAP status code (1 byte)
    packet.push_back(status);
    
    // Body Length (2 bytes, little-endian)
    uint16_t total_len = body.size();
    packet.push_back(total_len & 0xFF);
    packet.push_back((total_len >> 8) & 0xFF);
    
    packet.insert(packet.end(), body.begin(), body.end());
    
    bool session_is_secured = false;
    if (connections_.contains(conn_id)) {
        session_is_secured = connections_[conn_id]->is_encrypted();
    }
    
    bool requires_encryption = true;
    if (uuid.size() >= 8) {
        std::string short_uuid_str = uuid.substr(4, 4);
        unsigned int short_uuid = 0;
        if (sscanf(short_uuid_str.c_str(), "%x", &short_uuid) == 1) {
            if (short_uuid == 0x4C || short_uuid == 0x4E || short_uuid == 0x4F) {
                requires_encryption = false;
            }
        }
    }
    
    if (session_is_secured && requires_encryption) {
        auto& ctx = *connections_[conn_id];
        auto encrypted = ctx.get_secure_session()->encrypt_ble_pdu(packet);
        if (encrypted.empty()) {
            config_.system->log(platform::System::LogLevel::Error, 
                "[BleTransport] Response encryption failed for connection " + std::to_string(conn_id));
            transactions_[conn_id].response_buffer = packet;
        } else {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[BleTransport] Encrypted response (" + std::to_string(encrypted.size()) + " bytes)");
            transactions_[conn_id].response_buffer = std::move(encrypted);
        }
    } else {
        transactions_[conn_id].response_buffer = packet;
    }
    
    // HAP-BLE Spec 7.3.5.1/7.3.5.5: The response is returned in the GATT Read Response.
}

bool BleTransport::process_characteristic_write(uint16_t connection_id, uint16_t tid, const std::string& uuid, std::span<const uint8_t> body) {
    if (uuid == "0000004C-0000-1000-8000-0026BB765291") { // Pair Setup
        if (!connections_.contains(connection_id)) {
            connections_[connection_id] = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
        }
        
        Request req;
        req.body.assign(body.begin(), body.end());
        req.method = Method::POST; 
        req.path = "/pair-setup";
        
        auto resp = config_.pairing_endpoints->handle_pair_setup(req, *connections_[connection_id]);
        
        uint8_t status = (resp.status == Status::OK) ? 0x00 : 0x05; 
        send_response(connection_id, tid, uuid, status, resp.body);
        return true; 
    }
    else if (uuid == "0000004E-0000-1000-8000-0026BB765291") { // Pair Verify
         if (!connections_.contains(connection_id)) {
             connections_[connection_id] = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
        }
        Request req;
        req.body.assign(body.begin(), body.end());
        req.method = Method::POST;
        req.path = "/pair-verify";
        
        auto resp = config_.pairing_endpoints->handle_pair_verify(req, *connections_[connection_id]);
        uint8_t status = (resp.status == Status::OK) ? 0x00 : 0x05;
        send_response(connection_id, tid, uuid, status, resp.body);
        return true;
    }
    else if (uuid == "00000050-0000-1000-8000-0026BB765291") { // Pairing Mappings
         if (!connections_.contains(connection_id)) return false;
        
        Request req;
        req.body.assign(body.begin(), body.end());
        req.method = Method::POST;
        req.path = "/pairings";
        
        auto resp = config_.pairing_endpoints->handle_pairings(req, *connections_[connection_id]);
        uint8_t status = (resp.status == Status::OK) ? 0x00 : 0x05;
        send_response(connection_id, tid, uuid, status, resp.body);
        return true;
    }
    
    return false;
}

static void append_tlv(std::vector<uint8_t>& buf, uint8_t type, std::span<const uint8_t> value) {
    buf.push_back(type);
    buf.push_back(static_cast<uint8_t>(value.size())); // Length < 255 assumed
    buf.insert(buf.end(), value.begin(), value.end());
}

static void append_tlv_gatt_format(std::vector<uint8_t>& buf, uint8_t bt_sig_format, uint16_t bt_sig_unit = 0x2700) {
    uint8_t format_bytes[7] = {
        bt_sig_format,                               // Format
        0x00,                                         // Exponent
        static_cast<uint8_t>(bt_sig_unit & 0xFF),    // Unit (LE)
        static_cast<uint8_t>((bt_sig_unit >> 8) & 0xFF),
        0x01,                                         // Namespace (Bluetooth SIG)
        0x00, 0x00                                    // Description (LE)
    };
    append_tlv(buf, 0x0C, std::span<const uint8_t>(format_bytes, 7));
}



std::vector<uint8_t> BleTransport::process_signature_read(uint16_t connection_id, uint16_t char_iid) {
    (void)connection_id;
    std::vector<uint8_t> response;
    
    if (pairing_char_metadata_.count(char_iid)) {
        const auto& meta = pairing_char_metadata_[char_iid];
        
        auto append_uuid128 = [&](uint16_t short_uuid) {
            // HAP Base UUID: 0000XXXX-0000-1000-8000-0026BB765291
            // BLE/HAP requires 128-bit UUIDs in little-endian (reversed from UUID string)
            // 
            // UUID string format:  time_low - time_mid - time_hi - clock_seq - node
            // HAP Base:            0000XXXX - 0000     - 1000    - 8000      - 0026BB765291
            //
            // Little-endian order (reversed): node | clock_seq | time_hi | time_mid | time_low
            // Bytes 0-5:   node (reversed):    91 52 76 BB 26 00
            // Bytes 6-7:   clock_seq (reversed): 00 80
            // Bytes 8-9:   time_hi_and_version (reversed): 00 10
            // Bytes 10-11: time_mid (reversed): 00 00  
            // Bytes 12-15: time_low with short UUID (reversed): XX XX 00 00
            
            // Node: 0026BB765291 -> little-endian: 91 52 76 BB 26 00
            response.push_back(0x91);
            response.push_back(0x52);
            response.push_back(0x76);
            response.push_back(0xBB);
            response.push_back(0x26);
            response.push_back(0x00);
            // clock_seq: 8000 -> little-endian: 00 80
            response.push_back(0x00);
            response.push_back(0x80);
            // time_hi_and_version: 1000 -> little-endian: 00 10
            response.push_back(0x00);
            response.push_back(0x10);
            // time_mid: 0000 -> little-endian: 00 00
            response.push_back(0x00);
            response.push_back(0x00);
            // time_low: 0000XXXX with short_uuid -> little-endian: XX XX 00 00
            response.push_back(short_uuid & 0xFF);
            response.push_back((short_uuid >> 8) & 0xFF);
            response.push_back(0x00);
            response.push_back(0x00);
        };


        {
            response.push_back((uint8_t)HAPBLEPDUTLVType::CharacteristicType);
            response.push_back(16); 
            append_uuid128(meta.char_type);
        }
        
        {
            response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceInstanceID);
            response.push_back(2);
            response.push_back(meta.service_id & 0xFF);
            response.push_back((meta.service_id >> 8) & 0xFF);
        }
        
        {
            response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceType);
            response.push_back(16);
            append_uuid128(meta.service_type);
        }
        
        {
            response.push_back((uint8_t)HAPBLEPDUTLVType::CharacteristicProperties);
            response.push_back(2);
            response.push_back(meta.properties & 0xFF);
            response.push_back((meta.properties >> 8) & 0xFF);
        }
        
        if (!meta.user_description.empty()) {
            response.push_back((uint8_t)HAPBLEPDUTLVType::GATTUserDescription);
            response.push_back(static_cast<uint8_t>(meta.user_description.size()));
            response.insert(response.end(), meta.user_description.begin(), meta.user_description.end());
        }
        
        {
             uint8_t gatt_format = (meta.char_type == 0x4F) ? 0x04 : 0x1B;
             append_tlv_gatt_format(response, gatt_format);
        }

        config_.system->log(platform::System::LogLevel::Info, 
            "[BleTransport] Generated signature for special char IID=" + std::to_string(char_iid));
        
        return response;
    }
    
    if (config_.database) {
        for (const auto& acc : config_.database->accessories()) {
            for (const auto& svc : acc->services()) {
                for (const auto& ch : svc->characteristics()) {
                     if (ch->iid() == char_iid) {
                            {
                             uint16_t short_uuid = ch->type() & 0xFFFF;
                             response.push_back((uint8_t)HAPBLEPDUTLVType::CharacteristicType);
                             response.push_back(16);  // 128-bit UUID = 16 bytes
                             append_hap_uuid128(response, short_uuid);
                         }

                         {
                             response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceInstanceID);
                             response.push_back(2);
                             response.push_back(svc->iid() & 0xFF);
                             response.push_back((svc->iid() >> 8) & 0xFF);
                         }
                         
                         {
                             uint16_t svc_short_uuid = svc->type() & 0xFFFF;
                             response.push_back((uint8_t)HAPBLEPDUTLVType::ServiceType);
                             response.push_back(16);  // 128-bit UUID = 16 bytes
                             append_hap_uuid128(response, svc_short_uuid);
                         }
                         
                         {
                             uint16_t props = 0;
                             auto perms = ch->permissions();
                             for (auto p : perms) {
                                 if (p == core::Permission::PairedRead) props |= 0x0010;
                                 else if (p == core::Permission::PairedWrite) props |= 0x0020;
                                 else if (p == core::Permission::Notify) props |= (0x0080 | 0x0100); 
                                 else if (p == core::Permission::TimedWrite) props |= 0x0008;
                                 else if (p == core::Permission::Hidden) props |= 0x0040;
                                 else if (p == core::Permission::AdditionalAuthorization) props |= 0x0004;
                             }
                             
                             response.push_back((uint8_t)HAPBLEPDUTLVType::CharacteristicProperties);
                             response.push_back(2);
                             response.push_back(props & 0xFF);
                             response.push_back((props >> 8) & 0xFF);
                         }

                         {
                             uint8_t gatt_format = 0x1B;
                             auto fmt = ch->format();
                             if (fmt == core::Format::Bool) gatt_format = 0x01;
                             else if (fmt == core::Format::UInt8) gatt_format = 0x04;
                             else if (fmt == core::Format::UInt16) gatt_format = 0x06;
                             else if (fmt == core::Format::UInt32) gatt_format = 0x08;
                             else if (fmt == core::Format::Int) gatt_format = 0x10;
                             else if (fmt == core::Format::Float) gatt_format = 0x14;
                             else if (fmt == core::Format::String) gatt_format = 0x19;
                             
                             append_tlv_gatt_format(response, gatt_format);
                         }
                         
                         return response;
                     }
                }
            }
        }
    }

    return response;
}

std::vector<uint8_t> BleTransport::process_characteristic_read(uint16_t connection_id, std::span<const uint8_t> body) { (void)connection_id; (void)body; return {}; }

void BleTransport::register_accessory_info_service() {
    register_services_by_type(0x3E);
}

void BleTransport::register_user_services() {
    register_services_by_type(0);
}

void BleTransport::register_services_by_type(uint16_t filter_type) {
    if (!config_.database) return;
    auto type_to_uuid_str = [](uint64_t type) {
        char buffer[37];
        snprintf(buffer, sizeof(buffer), "0000%04X-0000-1000-8000-0026BB765291", (unsigned int)(type & 0xFFFF));
        return std::string(buffer);
    };

    for (const auto& acc : config_.database->accessories()) {
        for (const auto& svc : acc->services()) {
            uint16_t svc_type = svc->type() & 0xFFFF;
            
            if (filter_type == 0 && svc_type == 0x3E) continue;
            if (filter_type != 0 && svc_type != filter_type) continue;
            
            uint16_t svc_iid = next_iid_++;
            svc->set_iid(svc_iid);

            platform::Ble::ServiceDefinition def;
            def.uuid = type_to_uuid_str(svc->type());
            def.is_primary = svc->is_primary();
            
            {
                platform::Ble::CharacteristicDefinition svc_iid_char;
                svc_iid_char.uuid = kServiceInstanceIdCharUUID;
                svc_iid_char.properties.read = true;
                svc_iid_char.properties.write = false;
                svc_iid_char.properties.indicate = false;
                svc_iid_char.properties.notify = false;
                svc_iid_char.on_read = [svc_iid](uint16_t) {
                    std::vector<uint8_t> val;
                    val.push_back(svc_iid & 0xFF);
                    val.push_back((svc_iid >> 8) & 0xFF);
                    return val;
                };
                def.characteristics.push_back(std::move(svc_iid_char));
            }
            
            auto add_iid_descriptor = [](platform::Ble::CharacteristicDefinition& d, uint16_t iid) {
                platform::Ble::DescriptorDefinition desc;
                desc.uuid = kCharacteristicInstanceIdDescUUID;
                desc.properties.read = true;
                desc.on_read = [iid](uint16_t) {
                    std::vector<uint8_t> val;
                    val.push_back(iid & 0xFF);
                    val.push_back((iid >> 8) & 0xFF);
                    return val;
                };
                d.descriptors.push_back(std::move(desc));
            };

            for (const auto& ch : svc->characteristics()) {
                uint16_t char_iid = next_iid_++;
                ch->set_iid(char_iid);

                std::string char_uuid = type_to_uuid_str(ch->type());
                platform::Ble::CharacteristicDefinition cdef;
                cdef.uuid = char_uuid;
                
                instance_map_[{acc->aid(), ch->iid()}] = char_uuid;
                
                auto perms = ch->permissions();
                cdef.properties.read = true;
                cdef.properties.write = true;
                bool has_notify = core::has_permission(perms, core::Permission::Notify);
                cdef.properties.notify = false;
                cdef.properties.indicate = has_notify;

                add_iid_descriptor(cdef, char_iid);

                if (ch->description().has_value()) {
                    platform::Ble::DescriptorDefinition desc;
                    desc.uuid = "2901"; 
                    desc.properties.read = true;
                    std::string d = ch->description().value();
                    desc.on_read = [d](uint16_t) {
                        return std::vector<uint8_t>(d.begin(), d.end());
                    };
                    cdef.descriptors.push_back(std::move(desc));
                }
                
                cdef.on_read = [this](uint16_t conn_id) {
                    return handle_hap_read(conn_id);
                };
                
                cdef.on_write = [this, uuid=char_uuid](uint16_t conn_id, std::span<const uint8_t> data, bool response) {
                    (void)response;
                    handle_hap_write_with_id(conn_id, uuid, data);
                };
                
                cdef.on_subscribe = [this, uuid=char_uuid](uint16_t conn_id, bool enabled) {
                     if (enabled) {
                         subscriptions_[uuid].push_back(conn_id);
                     } else {
                         auto& subs = subscriptions_[uuid];
                         std::erase(subs, conn_id);
                     }
                };

                def.characteristics.push_back(std::move(cdef));
            }
            config_.ble->register_service(def);
        }
    }
}

std::vector<uint8_t> BleTransport::serialize_value(const core::Value& value, core::Format format) {
    (void)format;
    std::vector<uint8_t> data;
    
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
             data.push_back(arg ? 1 : 0);
        }
        else if constexpr (std::is_same_v<T, uint8_t>) {
             data.push_back(arg);
        }
        else if constexpr (std::is_same_v<T, uint16_t>) {
             data.push_back(arg & 0xFF);
             data.push_back((arg >> 8) & 0xFF);
        }
        else if constexpr (std::is_same_v<T, uint32_t>) {
             data.push_back(arg & 0xFF);
             data.push_back((arg >> 8) & 0xFF);
             data.push_back((arg >> 16) & 0xFF);
             data.push_back((arg >> 24) & 0xFF);
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
             for(int i=0; i<8; ++i) data.push_back((arg >> (i*8)) & 0xFF);
        }
        else if constexpr (std::is_same_v<T, int32_t>) {
             uint32_t u = static_cast<uint32_t>(arg);
             for(int i=0; i<4; ++i) data.push_back((u >> (i*8)) & 0xFF);
        }
        else if constexpr (std::is_same_v<T, float>) {
             // IEEE-754 LE
             static_assert(sizeof(float) == 4);
             uint32_t u;
             std::memcpy(&u, &arg, 4);
             for(int i=0; i<4; ++i) data.push_back((u >> (i*8)) & 0xFF);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
             data.assign(arg.begin(), arg.end());
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
             data = arg;
        }
    }, value);
    
    return data;
}

uint16_t BleTransport::get_current_gsn() {
    auto gsn_bytes = config_.storage->get("gsn");
    uint16_t gsn = 1;
    if (gsn_bytes && gsn_bytes->size() == 2) {
        gsn = static_cast<uint16_t>((*gsn_bytes)[0]) | (static_cast<uint16_t>((*gsn_bytes)[1]) << 8);
    }
    return gsn;
}

bool BleTransport::is_broadcast_key_valid() {
    if (!broadcast_key_valid_) return false;
    
    // Per HAP Spec 7.4.7.4: Key expires after 32767 GSN increments
    uint16_t current_gsn = get_current_gsn();
    uint16_t gsn_diff = 0;
    
    // Handle GSN wraparound (1-65535, wraps to 1)
    if (current_gsn >= broadcast_key_gsn_start_) {
        gsn_diff = current_gsn - broadcast_key_gsn_start_;
    } else {
        // GSN wrapped around
        gsn_diff = (65535 - broadcast_key_gsn_start_) + current_gsn;
    }
    
    if (gsn_diff >= 32767) {
        config_.system->log(platform::System::LogLevel::Warning,
            "[BleTransport] Broadcast encryption key expired (GSN diff=" + std::to_string(gsn_diff) + ")");
        broadcast_key_valid_ = false;
        return false;
    }
    
    return true;
}

void BleTransport::handle_characteristic_change(uint64_t aid, uint64_t iid, 
                                                 const core::Value& value, 
                                                 uint32_t exclude_conn_id) {
    config_.system->log(platform::System::LogLevel::Debug,
        "[BleTransport] Characteristic change: AID=" + std::to_string(aid) + 
        " IID=" + std::to_string(iid));
    
    // Find the characteristic to check its event properties
    auto ch = config_.database ? config_.database->find_characteristic(aid, iid) : nullptr;
    if (!ch) {
        config_.system->log(platform::System::LogLevel::Warning,
            "[BleTransport] Cannot find characteristic for event: IID=" + std::to_string(iid));
        return;
    }
    
    // Check HAP characteristic properties for event support
    // Per HAP Spec Table 7-50:
    // - 0x0080: Notifies Events in Connected State
    // - 0x0100: Notifies Events in Disconnected State
    // - 0x0200: Supports Broadcast Notify
    bool supports_connected = false;
    bool supports_disconnected = false;
    bool supports_broadcast = false;
    
    for (const auto& perm : ch->permissions()) {
        if (perm == core::Permission::Notify) {
            supports_connected = true;
            supports_disconnected = true;
        }
    }
    
    bool broadcast_enabled = false;
    if (broadcast_configs_.count(static_cast<uint16_t>(iid))) {
        auto& bc = broadcast_configs_[static_cast<uint16_t>(iid)];
        broadcast_enabled = bc.enabled;
        supports_broadcast = bc.enabled;
    }
    
    auto it = instance_map_.find({aid, iid});
    if (it == instance_map_.end()) {
        config_.system->log(platform::System::LogLevel::Warning,
            "[BleTransport] No UUID mapping for IID=" + std::to_string(iid));
        return;
    }
    std::string uuid = it->second;
    
    bool has_connected_subscribers = false;
    if (subscriptions_.count(uuid) && !subscriptions_[uuid].empty()) {
        for (uint16_t conn_id : subscriptions_[uuid]) {
            if (conn_id != exclude_conn_id) {
                has_connected_subscribers = true;
                break;
            }
        }
    }
    
    is_connected_ = !connections_.empty();
    
    if (is_connected_ && has_connected_subscribers && supports_connected) {
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Sending Connected Event for IID=" + std::to_string(iid));
        send_connected_event(static_cast<uint16_t>(iid));
    }
    else if (!is_connected_ && supports_broadcast && broadcast_enabled && is_broadcast_key_valid()) {
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Sending Broadcasted Event for IID=" + std::to_string(iid));
        send_broadcasted_event(static_cast<uint16_t>(iid), value);
    }
    else if (!is_connected_ && supports_disconnected) {
        config_.system->log(platform::System::LogLevel::Info,
            "[BleTransport] Sending Disconnected Event for IID=" + std::to_string(iid));
        send_disconnected_event(static_cast<uint16_t>(iid));
    }
    else {
        config_.system->log(platform::System::LogLevel::Debug,
            "[BleTransport] No event sent for IID=" + std::to_string(iid) + 
            " (connected=" + std::to_string(is_connected_) +
            ", has_subs=" + std::to_string(has_connected_subscribers) +
            ", supports_connected=" + std::to_string(supports_connected) + ")");
    }
}

void BleTransport::send_connected_event(uint16_t iid) {
    // Per HAP Spec 7.4.6.1 Connected Events:
    // Send a ZERO-LENGTH indication to controllers that registered for indications.
    
    std::string uuid;
    for (const auto& [key, val] : instance_map_) {
        if (key.second == iid) {
            uuid = val;
            break;
        }
    }
    
    if (uuid.empty()) {
        config_.system->log(platform::System::LogLevel::Warning,
            "[BleTransport] Cannot send Connected Event - no UUID for IID=" + std::to_string(iid));
        return;
    }
    
    if (!subscriptions_.count(uuid) || subscriptions_[uuid].empty()) {
        config_.system->log(platform::System::LogLevel::Debug,
            "[BleTransport] No subscribers for Connected Event IID=" + std::to_string(iid));
        return;
    }
    
    std::vector<uint8_t> empty_indication;
    
    for (uint16_t conn_id : subscriptions_[uuid]) {
        config_.system->log(platform::System::LogLevel::Debug,
            "[BleTransport] Sending zero-length indication to conn=" + std::to_string(conn_id) + 
            " for IID=" + std::to_string(iid));
        
        config_.ble->send_indication(conn_id, uuid, empty_indication);
    }
}

void BleTransport::send_broadcasted_event(uint16_t iid, const core::Value& value) {
    // Per HAP Spec 7.4.6.2 Broadcasted Events:
    // When disconnected and broadcast is configured, send encrypted advertisement
    // containing the characteristic value.
    
    if (!is_broadcast_key_valid()) {
        config_.system->log(platform::System::LogLevel::Warning,
            "[BleTransport] Cannot send Broadcasted Event - no valid broadcast key");
        send_disconnected_event(iid);
        return;
    }
    
    std::vector<uint8_t> encrypted_payload = build_encrypted_advertisement_payload(iid, value);
    if (encrypted_payload.empty()) {
        config_.system->log(platform::System::LogLevel::Error,
            "[BleTransport] Failed to build encrypted advertisement payload");
        send_disconnected_event(iid);
        return;
    }
    
    increment_gsn();
    
    std::array<uint8_t, 6> adv_id = {};
    sscanf(config_.accessory_id.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &adv_id[0], &adv_id[1], &adv_id[2], 
        &adv_id[3], &adv_id[4], &adv_id[5]);
    
    uint32_t interval_ms = 20;  // Default 20ms
    if (broadcast_configs_.count(iid)) {
        switch (broadcast_configs_[iid].interval) {
            case 0x01: interval_ms = 20; break;
            case 0x02: interval_ms = 1280; break;
            case 0x03: interval_ms = 2560; break;
            default: interval_ms = 20; break;
        }
    }
    
    platform::Ble::EncryptedAdvertisement enc_adv;
    enc_adv.advertising_id = adv_id;
    enc_adv.encrypted_payload = std::move(encrypted_payload);
    enc_adv.gsn = get_current_gsn();
    
    config_.system->log(platform::System::LogLevel::Info,
        "[BleTransport] Starting encrypted advertisement for IID=" + std::to_string(iid) +
        " interval=" + std::to_string(interval_ms) + "ms duration=3000ms");
    
    config_.ble->start_encrypted_advertising(enc_adv, interval_ms, 3000);
}

void BleTransport::send_disconnected_event(uint16_t iid) {
    // Per HAP Spec 7.4.6.3 Disconnected Events:
    // Increment GSN (once per disconnected period until connected) and
    // use 20ms advertising for at least 3 seconds, then revert to normal.
    
    config_.system->log(platform::System::LogLevel::Debug,
        "[BleTransport] Disconnected Event for IID=" + std::to_string(iid));
    
    increment_gsn();
    
    std::string setup_id;
    auto setup_id_bytes = config_.storage->get("setup_id");
    if (setup_id_bytes && setup_id_bytes->size() == 4) {
        setup_id = std::string(setup_id_bytes->begin(), setup_id_bytes->end());
    } else {
        setup_id = "X-HZ";
    }
    
    std::string input = setup_id + config_.accessory_id;
    std::array<uint8_t, 64> hash_output = {};
    config_.crypto->sha512(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(input.data()), input.size()),
        std::span<uint8_t, 64>(hash_output.data(), 64)
    );
    uint8_t setup_hash[4];
    std::copy_n(hash_output.begin(), 4, setup_hash);
    
    auto pairing_list = config_.storage->get("pairing_list");
    bool is_paired = (pairing_list && pairing_list->size() > 2);
    uint8_t status_flags = is_paired ? 0x00 : 0x01;
    
    uint8_t device_id[6] = {0};
    sscanf(config_.accessory_id.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &device_id[0], &device_id[1], &device_id[2],
        &device_id[3], &device_id[4], &device_id[5]);
    
    uint16_t gsn = get_current_gsn();
    
    uint16_t config_number = 1;
    auto cn_bytes = config_.storage->get("config_number");
    if (cn_bytes && !cn_bytes->empty()) {
        std::string cn_str(cn_bytes->begin(), cn_bytes->end());
        config_number = static_cast<uint16_t>(std::stoi(cn_str));
    }
    
    auto adv = platform::Ble::Advertisement::create_hap(
        status_flags, device_id, config_.category_id, gsn, config_number, setup_hash);
    adv.local_name = config_.device_name;
    
    // Per HAP Spec 7.4.6.3: Use fast interval (20 ms) for 3 seconds, then normal interval
    config_.system->log(platform::System::LogLevel::Info,
        "[BleTransport] Starting timed advertising for Disconnected Event (" + 
        std::to_string(config_.ble->interval_config.fast_interval_ms) + "ms for " +
        std::to_string(config_.ble->interval_config.fast_duration_ms) + "ms)");
    config_.ble->start_timed_advertising(
        adv, 
        config_.ble->interval_config.fast_interval_ms,
        config_.ble->interval_config.fast_duration_ms,
        config_.ble->interval_config.normal_interval_ms
    );
    
    (void)iid;
}

std::vector<uint8_t> BleTransport::build_encrypted_advertisement_payload(uint16_t iid, const core::Value& value) {
    // Per HAP Spec 7.4.7.3 Broadcast Encryption:
    // Payload: 12 bytes = GSN(2) + IID(2) + Value(8, with padding)
    // Nonce: GSN padded to 12 bytes with zeros
    // AAD: 6-byte advertising identifier
    // AuthTag: First 4 bytes of 16-byte ChaCha20-Poly1305 tag
    
    if (!is_broadcast_key_valid()) {
        return {};
    }
    
    uint16_t gsn = get_current_gsn();
    
    std::vector<uint8_t> plaintext(12, 0);
    plaintext[0] = gsn & 0xFF;
    plaintext[1] = (gsn >> 8) & 0xFF;
    plaintext[2] = iid & 0xFF;
    plaintext[3] = (iid >> 8) & 0xFF;
    
    core::Format format = core::Format::Data;
    if (config_.database) {
        auto ch = config_.database->find_characteristic(1, iid);
        if (ch) format = ch->format();
    }
    std::vector<uint8_t> value_bytes = serialize_value(value, format);
    for (size_t i = 0; i < 8 && i < value_bytes.size(); ++i) {
        plaintext[4 + i] = value_bytes[i];
    }
    
    std::array<uint8_t, 12> nonce = {};
    nonce[0] = gsn & 0xFF;
    nonce[1] = (gsn >> 8) & 0xFF;
    
    std::array<uint8_t, 6> aad = {};
    sscanf(config_.accessory_id.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &aad[0], &aad[1], &aad[2], &aad[3], &aad[4], &aad[5]);
    
    std::vector<uint8_t> ciphertext(12);
    std::array<uint8_t, 16> full_tag = {};
    
    std::array<uint8_t, 32> key_copy;
    std::copy(broadcast_key_.begin(), broadcast_key_.end(), key_copy.begin());
    
    bool success = config_.crypto->chacha20_poly1305_encrypt_and_tag(
        key_copy,
        nonce,
        std::span<const uint8_t>(aad.data(), 6),
        plaintext,
        ciphertext,
        full_tag
    );
    
    if (!success) {
        config_.system->log(platform::System::LogLevel::Error,
            "[BleTransport] Broadcast encryption failed");
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(16);
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), full_tag.begin(), full_tag.begin() + 4);
    
    return result;
}

} // namespace hap::transport

