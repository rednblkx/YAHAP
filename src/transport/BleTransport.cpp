#include <charconv>
#include "hap/transport/BleTransport.hpp"
#include "hap/core/HAPStatus.hpp"
#include "hap/common/Log.hpp"
#include <algorithm>
#include <cstring>

#include "hap/core/TLV8.hpp"
#include "hap/pairing/TLVTypes.hpp"
#include "hap/core/TLV8.hpp"
#include "hap/transport/ble/BleTlvBuilder.hpp"
#include "hap/core/CharacteristicSerializer.hpp"

namespace {
// Parse "AA:BB:CC:DD:EE:FF" into 6 bytes. Returns false on malformed input
// (the output is zeroed in that case). Hand-rolled instead of sscanf("%hhx:...")
// so the libc scanf machinery stays out of embedded links.
bool parse_device_id(const std::string& accessory_id, std::array<uint8_t, 6>& out) {
    out.fill(0);
    size_t i = 0;
    for (size_t group = 0; group < 6; ++group) {
        if (group > 0) {
            if (i >= accessory_id.size() || accessory_id[i] != ':') return false;
            ++i;
        }
        unsigned value = 0;
        for (int digit = 0; digit < 2; ++digit) {
            if (i >= accessory_id.size()) return false;
            char c = accessory_id[i++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        out[group] = static_cast<uint8_t>(value);
    }
    return i == accessory_id.size();
}

// Parse a stored config-number string without throwing (from_chars, no locale).
bool parse_config_number(const std::string& str, uint8_t& out) {
    uint32_t parsed = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), parsed);
    if (ec != std::errc() || ptr != str.data() + str.size() || parsed > 255) {
        return false;
    }
    out = static_cast<uint8_t>(parsed);
    return true;
}

namespace {
// HAP-BLE security: the unauthenticated pairing endpoints (Pair Setup 0x4C,
// Pair Verify 0x4E, Pairing Features 0x4F) work without an encrypted session;
// every other characteristic requires one. Named predicate instead of UUID
// string parsing scattered over call sites.
bool characteristic_requires_encryption(uint16_t char_type) {
    // Only the unauthenticated pairing characteristics (Pair Setup 0x4C,
    // Pair Verify 0x4E, Pairing Features 0x4F) work without an encrypted
    // session; everything else requires one (HAP-BLE 7.3.5.2).
    switch (char_type) {
        case 0x4C:
        case 0x4E:
        case 0x4F:
            return false;
        default:
            return true;
    }
}
} // namespace
} // namespace

// Formats a 16-bit short type as 4 uppercase hex digits (for logs).
[[maybe_unused]] static const char* hex4(uint16_t v) {
    static const char* kHex = "0123456789ABCDEF";
    static char buf[5];
    buf[0] = kHex[(v >> 12) & 0xF];
    buf[1] = kHex[(v >> 8) & 0xF];
    buf[2] = kHex[(v >> 4) & 0xF];
    buf[3] = kHex[v & 0xF];
    buf[4] = '\0';
    return buf;
}

// Builds the full 128-bit HAP UUID string for a 16-bit short type
// ("0000XXXX-0000-1000-8000-0026BB765291") — only needed at the PAL boundary.
static std::string type_to_uuid_str(uint16_t type) {
    static const char* kHex = "0123456789ABCDEF";
    std::string uuid = "0000____-0000-1000-8000-0026BB765291";
    uuid[4] = kHex[(type >> 12) & 0xF];
    uuid[5] = kHex[(type >> 8) & 0xF];
    uuid[6] = kHex[(type >> 4) & 0xF];
    uuid[7] = kHex[type & 0xF];
    return uuid;
}

[[maybe_unused]] static std::string to_hex_string(const uint8_t* data, size_t len) {
    static const char* kHex = "0123456789ABCDEF";
    std::string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        s.push_back(kHex[(data[i] >> 4) & 0xF]);
        s.push_back(kHex[data[i] & 0xF]);
    }
    return s;
}

namespace hap::transport {

// Import types from the extracted ble:: namespace components
using ble::PDUOpcode;
using ble::TransactionState;
using ble::HAPBLEPDUTLVType;
using ble::BleTlvBuilder;

BleTransport::BleTransport(Config config) : config_(std::move(config)),
    session_manager_(std::make_unique<ble::BleSessionManager>(config_.system)) {
    if (!config_.ble) {
        if(config_.system) HAP_LOG_WARN(config_.system, "[BleTransport] No BLE platform interface provided");
    }
}

BleTransport::~BleTransport() {
    stop();
}

void BleTransport::start() {
    if (!config_.ble) return;

    HAP_LOG_INFO(config_.system, "[BleTransport] Starting...");

    config_.ble->set_connect_callback([this]([[maybe_unused]] uint16_t connection_id) {
        // Connectable advertising stops when the connection is established.
        advertising_active_ = false;
        HAP_LOG(config_.system, "[BleTransport] Device connected, connection_id=", connection_id);
    });

    config_.ble->set_disconnect_callback([this](uint16_t connection_id) {
        HAP_LOG_INFO(config_.system, "[BleTransport] Device disconnected, connection_id=", connection_id);
        
        session_manager_->remove(connection_id);
        
        HAP_LOG_INFO(config_.system, "[BleTransport] Connection state cleaned up, refreshing advertising");
        
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
    advertising_active_ = false;
}

uint16_t BleTransport::get_ble_iid(const std::string& key) {
    if (config_.iid_manager) {
        return config_.iid_manager->get_or_assign(key);
    }
    // Fallback: deterministic hash of the key (stable per device, non-zero).
    uint16_t iid = 1;
    for (char c : key) iid = static_cast<uint16_t>((iid << 5) + iid) ^ static_cast<uint8_t>(c);
    return static_cast<uint16_t>((iid & 0x7FFF) + 1);
}

bool BleTransport::iid_is_version(uint16_t iid) const {
    // The Protocol Information Service's Version characteristic (0x37) must be
    // readable in the clear: the controller reads it before pairing to decide
    // protocol compatibility (HAP 7.4.3.1, test requirement #3).
    for (const auto& m : pairing_char_metadata_) {
        if (m.instance_id == iid) return m.char_type == 0x37;
    }
    return false;
}

void BleTransport::add_service_instance_id_characteristic(
        platform::Ble::ServiceDefinition& svc, uint16_t svc_iid) {
    platform::Ble::CharacteristicDefinition def;
    def.uuid = kServiceInstanceIdCharUUID;
    def.properties = { .read = true };
    def.on_read = [svc_iid](uint16_t) {
        std::vector<uint8_t> val;
        val.push_back(svc_iid & 0xFF);
        val.push_back((svc_iid >> 8) & 0xFF);
        return val;
    };
    svc.characteristics.push_back(std::move(def));
}

void BleTransport::add_pairing_characteristic(
        platform::Ble::ServiceDefinition& svc, uint16_t svc_iid, uint16_t svc_type,
        const std::string& iid_key, const std::string& uuid,
        uint16_t char_type, uint16_t properties,
        const char* user_description) {
    uint16_t char_iid = get_ble_iid(iid_key);

    platform::Ble::CharacteristicDefinition def;
    def.uuid = uuid;
    def.properties.read = true;
    def.properties.write = true;
    def.on_write = [this, char_type](uint16_t conn, std::span<const uint8_t> data, bool) {
        handle_hap_write(conn, char_type, data);
    };
    def.on_read = [this](uint16_t conn) {
        return handle_hap_read(conn);
    };
    // Characteristic Instance ID descriptor (mandatory for HAP-BLE).
    platform::Ble::DescriptorDefinition desc;
    desc.uuid = kCharacteristicInstanceIdDescUUID;
    desc.properties.read = true;
    desc.on_read = [char_iid](uint16_t) {
        std::vector<uint8_t> val;
        val.push_back(char_iid & 0xFF);
        val.push_back((char_iid >> 8) & 0xFF);
        return val;
    };
    def.descriptors.push_back(std::move(desc));

    CharacteristicMetadata meta;
    meta.instance_id = char_iid;
    meta.service_id = svc_iid;
    meta.char_type = char_type;
    meta.service_type = svc_type;
    meta.properties = properties;
    if (user_description) {
        meta.user_description = user_description;
    }
    for (auto& existing : pairing_char_metadata_) {
        if (existing.instance_id == char_iid) {
            existing = meta;
            return;
        }
    }
    pairing_char_metadata_.push_back(meta);

    HAP_LOG_INFO(config_.system, "[BleTransport] Registered characteristic ", uuid, " IID=", char_iid);
    svc.characteristics.push_back(std::move(def));
}

void BleTransport::setup_hap_service() {
    platform::Ble::ServiceDefinition svc;
    svc.uuid = kHapPairingServiceUUID;
    svc.is_primary = true;
    uint16_t svc_iid = get_ble_iid("BLE:S:0055");
    uint16_t svc_type = 0x55; // Pairing Service

    add_service_instance_id_characteristic(svc, svc_iid);

    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:004C:0055",
        "0000004C-0000-1000-8000-0026BB765291", 0x4C, 0x0003); // Pair Setup: Read|Write
    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:004E:0055",
        "0000004E-0000-1000-8000-0026BB765291", 0x4E, 0x0003); // Pair Verify: Read|Write
    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:004F:0055",
        "0000004F-0000-1000-8000-0026BB765291", 0x4F, 0x0001, "Pairing Features"); // Read only
    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:0050:0055",
        "00000050-0000-1000-8000-0026BB765291", 0x50, 0x0030); // Pairings: Paired Read|Write

    HAP_LOG_INFO(config_.system, "[BleTransport] Registering Service...");
    config_.ble->register_service(svc);

    if (config_.iid_manager) {
        config_.iid_manager->save();
    }
}

void BleTransport::setup_protocol_info_service() {
    platform::Ble::ServiceDefinition svc;
    svc.uuid = kHapProtocolInformationServiceUUID;
    svc.is_primary = true;
    uint16_t svc_iid = get_ble_iid("BLE:S:00A2");
    uint16_t svc_type = 0xA2; // Protocol Information Service

    add_service_instance_id_characteristic(svc, svc_iid);

    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:00A5:00A2",
        kServiceSignatureCharUUID, 0xA5, 0x0010); // Service Signature: Paired Read
    add_pairing_characteristic(svc, svc_iid, svc_type, "BLE:C:0037:00A2",
        "00000037-0000-1000-8000-0026BB765291", 0x37, 0x0010); // Version: Paired Read

    config_.ble->register_service(svc);

    if (config_.iid_manager) {
        config_.iid_manager->save();
    }
}

void BleTransport::update_advertising() {
    HAP_LOG(config_.system, "[BleTransport] update_advertising entry");
    
    auto setup_id_bytes = config_.storage->get("setup_id");
    std::string setup_id;
    if (setup_id_bytes && setup_id_bytes->size() == 4) {
        setup_id = std::string(setup_id_bytes->begin(), setup_id_bytes->end());
        HAP_LOG(config_.system, "[BleTransport] Using existing Setup ID: ", setup_id);
    } else {
        const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::array<uint8_t, 4> rand_bytes{};
        config_.system->random_bytes(rand_bytes);
        for (int i = 0; i < 4; ++i) {
            setup_id += charset[rand_bytes[i] % 36];
        }
        config_.storage->set("setup_id", std::vector<uint8_t>(setup_id.begin(), setup_id.end()));
        HAP_LOG_INFO(config_.system, "[BleTransport] Generated new Setup ID: ", setup_id);
    }
    
    std::string input = setup_id + config_.accessory_id;
    std::vector<uint8_t> hash_output(64);
    HAP_LOG(config_.system, "[BleTransport] Calculating Setup Hash for: ", input);
    
    if (config_.crypto == nullptr) {
         HAP_LOG_ERROR(config_.system, "[BleTransport] No crypto provider!");
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
    
    std::array<uint8_t, 6> device_id{};
    if (!parse_device_id(config_.accessory_id, device_id)) {
         HAP_LOG_WARN(config_.system, "[BleTransport] Invalid Device ID format: ", config_.accessory_id);
    }

    uint16_t gsn = get_current_gsn();

    uint8_t config_number = 1;
    auto cn_bytes = config_.storage->get("config_number");
    if (cn_bytes && !cn_bytes->empty()) {
        std::string cn_str(cn_bytes->begin(), cn_bytes->end());
        if (!parse_config_number(cn_str, config_number)) {
                config_number = 1;
            }
    }

    auto adv = platform::Ble::Advertisement::create_hap(
        status_flags,
        device_id.data(),
        config_.category_id,
        gsn,
        config_number,
        setup_hash
    );
    
    adv.local_name = config_.device_name;

    HAP_LOG(config_.system, "[BleTransport] Advertisement Data (", adv.manufacturer_data.size(), " bytes): ",
        to_hex_string(adv.manufacturer_data.data(), adv.manufacturer_data.size()));
    HAP_LOG(config_.system, "[BleTransport] SF=", status_flags, " ACID=", config_.category_id, " GSN=", gsn, " CN=", config_number);

    // Only touch the BLE stack when something actually changed AND advertising
    // is already running with the current payload: restarting advertising on
    // every characteristic write stalls the connection. After a disconnect the
    // stack has stopped advertising, so a restart is mandatory even if the
    // payload is unchanged.
    std::vector<uint8_t> mfg_sig(adv.manufacturer_data.begin(), adv.manufacturer_data.end());
    if (advertising_active_ && !adv_dirty_ &&
        mfg_sig == last_adv_payload_ &&
        adv.local_name == last_adv_name_) {
        HAP_LOG(config_.system, "[BleTransport] Advertisement unchanged and active, skipping restart");
        return;
    }
    last_adv_payload_ = mfg_sig;
    last_adv_name_ = adv.local_name;
    adv_dirty_ = false;

    config_.ble->start_advertising(adv, config_.ble->interval_config.normal_interval_ms);
    advertising_active_ = true;
}

void BleTransport::set_accessory_id(const std::string& new_id) {
    config_.accessory_id = new_id;
    adv_dirty_ = true; // device-id in the payload changes with the accessory ID
    HAP_LOG_INFO(config_.system, "[BleTransport] Accessory ID updated to: ", new_id);
}

void BleTransport::notify_value_changed(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id) {
    handle_characteristic_change(aid, iid, value, exclude_conn_id);
}

void BleTransport::increment_gsn() {
    // GSN is cached in RAM and persisted only on change: a flash write on the
    // characteristic-write path stalls the connection for milliseconds.
    uint16_t gsn = get_current_gsn();
    gsn++;
    if (gsn == 0) {
        gsn = 1;
    }
    cached_gsn_ = gsn;
    gsn_loaded_ = true;

    std::vector<uint8_t> gsn_data = {
        static_cast<uint8_t>(gsn & 0xFF),
        static_cast<uint8_t>((gsn >> 8) & 0xFF)
    };
    config_.storage->set("gsn", gsn_data);

    HAP_LOG(config_.system, "[BleTransport] GSN incremented to ", gsn);

    update_advertising();
}

void BleTransport::check_session_timeouts() {
    auto timed_out = session_manager_->check_timeouts();

    for (uint16_t conn_id : timed_out) {
        session_manager_->remove(conn_id);
        config_.ble->disconnect(conn_id);
        HAP_LOG_INFO(config_.system, "[BleTransport] Terminated connection ", conn_id, " due to timeout");
    }
}

void BleTransport::establish_secure_session(
        uint16_t connection_id,
        std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> session_keys,
        const std::array<uint8_t, 32>& shared_secret,
        const std::string& controller_id) {
    auto& session = session_manager_->get_or_create(connection_id);
    if (!session.context) {
        session.context = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
    }
    session.context->upgrade_to_secure(std::move(session_keys), shared_secret, controller_id);
}

void BleTransport::handle_hap_write(uint16_t connection_id, uint16_t char_type, std::span<const uint8_t> data) {
    if (data.empty()) return;

    HAP_LOG(config_.system, "[BleTransport] Write to Char Type: 0x", hex4(char_type));
    HAP_LOG(config_.system, "[BleTransport] Write PDU Fragment (", data.size(), " bytes): ", to_hex_string(data.data(), data.size()));

    bool session_is_secured = false;
    auto* session = session_manager_->get_session(connection_id);
    if (session && session->context) {
        session_is_secured = session->context->is_encrypted();
    }
    
    const bool requires_encryption = characteristic_requires_encryption(char_type);

    std::vector<uint8_t> decrypted_data;
    std::span<const uint8_t> working_data = data;
    
    if (session_is_secured && requires_encryption) {
        auto& session_ref = session_manager_->get_or_create(connection_id);
        if (!session_ref.context) {
            HAP_LOG_ERROR(config_.system, "[BleTransport] Context missing for secured session - disconnecting");
            config_.ble->disconnect(connection_id);
            session_manager_->remove(connection_id);
            return;
        }
        auto decrypted = session_ref.context->get_secure_session()->decrypt_ble_pdu(data);
        if (!decrypted) {
            HAP_LOG_ERROR(config_.system, "[BleTransport] Decryption failed for connection ", connection_id, " - disconnecting");
            // HAP 6.5.2 (Session Security): on decryption failure the
            // connection must be closed immediately.
            config_.ble->disconnect(connection_id);
            session_manager_->remove(connection_id);
            return;
        }
        decrypted_data = std::move(*decrypted);
        working_data = decrypted_data;
        HAP_LOG(config_.system, "[BleTransport] Decrypted PDU (", decrypted_data.size(), " bytes): ", to_hex_string(decrypted_data.data(), decrypted_data.size()));
    }
    
    if (working_data.empty()) return;
    uint8_t control_field = working_data[0];
    bool continuation = (control_field & 0x80) != 0;
    PDUOpcode opcode = PDUOpcode::CharacteristicWrite;
    uint16_t tid = 0;

    if (!continuation) {
        if (working_data.size() < 3) {
            HAP_LOG_ERROR(config_.system, "[BleTransport] PDU too short");
            return;
        }
        opcode = static_cast<PDUOpcode>(working_data[1]);
        tid = working_data[2];

        // HAP test requirement #6: malformed PDUs must fail the request.
        // Reject unknown opcodes with Unsupported-PDU (Table 7-37, 0x01).
        switch (opcode) {
            case PDUOpcode::CharacteristicSignatureRead:
            case PDUOpcode::CharacteristicWrite:
            case PDUOpcode::CharacteristicRead:
            case PDUOpcode::CharacteristicTimedWrite:
            case PDUOpcode::CharacteristicExecuteWrite:
            case PDUOpcode::ServiceSignatureRead:
            case PDUOpcode::CharacteristicConfiguration:
            case PDUOpcode::ProtocolConfiguration:
                break;
            default:
                HAP_LOG_WARN(config_.system, "[BleTransport] Unsupported PDU opcode 0x", hex4(static_cast<uint16_t>(working_data[1])));
                send_response(connection_id, tid, char_type, 0x01, {});
                return;
        }

        const bool is_signature_read =
            opcode == PDUOpcode::CharacteristicSignatureRead ||
            opcode == PDUOpcode::ServiceSignatureRead;
        const bool is_version_read =
            opcode == PDUOpcode::CharacteristicRead &&
            working_data.size() >= 5 &&
            iid_is_version(static_cast<uint16_t>(working_data[3]) |
                           (static_cast<uint16_t>(working_data[4]) << 8));
        if (requires_encryption && !session_is_secured && !is_signature_read && !is_version_read) {
            HAP_LOG_WARN(config_.system, "[BleTransport] Procedure 0x", hex4(static_cast<uint16_t>(opcode)),
                         " on secured characteristic without secure session - rejecting (0x05)");
            send_response(connection_id, tid, char_type, 0x05, {});
            return;
        }

        HAP_LOG_INFO(config_.system, "[BleTransport] New Transaction TID=", tid, " Opcode=", (int)opcode);
        
        auto& state = session_manager_->get_or_create(connection_id).transaction;
        state.opcode = opcode;
        state.transaction_id = tid;
        state.target_char_type = char_type;
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
        auto& state = session_manager_->get_or_create(connection_id).transaction;
        if (!state.active) {
             HAP_LOG_WARN(config_.system, "[BleTransport] Orphaned continuation fragment!");
             return;
        }
        
        // Spec 7.3.3.1: TID is present in every fragment.
        // Packet: CF (1) | TID (1) | Data...
        if (working_data.size() < 2) return;
        uint16_t cont_tid = working_data[1];
        if (cont_tid != state.transaction_id) {
            HAP_LOG_ERROR(config_.system, "[BleTransport] TID mismatch in continuation! Expected ", state.transaction_id, " got ", cont_tid);
            return;
        }

        // Continuation Body starts at index 2 (CF, TID)
        state.buffer.insert(state.buffer.end(), working_data.begin() + 2, working_data.end());
        process_transaction(connection_id, state);
    }
}

std::vector<uint8_t> BleTransport::handle_hap_read(uint16_t connection_id) {
    auto* session = session_manager_->get_session(connection_id);
    if (session) {
        auto& state = session->transaction;

        // HAP test requirement #12: a GATT Read is valid only if preceded by a
        // GATT Write carrying the same transaction ID at most 10 seconds ago.
        // The PAL read callback doesn't know the controller's expected TID, so
        // we serve the buffered response for the most recent completed
        // transaction; anything else (no write at all, or stale by >10s) is
        // rejected with an empty read.
        if (state.last_write_ms == 0 && state.response_buffer.empty()) {
            HAP_LOG_WARN(config_.system, "[BleTransport] Rejecting GATT Read - no preceding write (Req #12)");
            return {};
        }
        uint64_t current_time = config_.system->millis();
        uint64_t time_since_write = current_time - state.last_write_ms;

        if (time_since_write > 10000) { // 10 seconds
            HAP_LOG_WARN(config_.system, "[BleTransport] Rejecting GATT Read - >10s since write (Req #12)");
            return {};
        }

        HAP_LOG_INFO(config_.system, "[BleTransport] Handling GATT Read. Returning ", state.response_buffer.size(), " bytes");
        return state.response_buffer;
    }
    return {};
}


namespace {
// Decode a BLE TLV value blob into a characteristic-typed Value.
// Returns false when the data is too short for the format, so the caller can
// answer with an error instead of writing a garbage/default value.
bool decode_ble_value(core::Format format, const std::vector<uint8_t>& data, core::Value& out) {
    auto le16 = [](const std::vector<uint8_t>& d) {
        return static_cast<uint16_t>(d[0] | (d[1] << 8));
    };
    auto le32 = [](const std::vector<uint8_t>& d) {
        return static_cast<uint32_t>(d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24));
    };
    switch (format) {
        case core::Format::Bool:
            out = static_cast<bool>(data[0] != 0);
            return true;
        case core::Format::UInt8:
            out = data[0];
            return true;
        case core::Format::UInt16:
            if (data.size() < 2) return false;
            out = le16(data);
            return true;
        case core::Format::UInt32:
            if (data.size() < 4) return false;
            out = le32(data);
            return true;
        case core::Format::UInt64:
            if (data.size() < 8) return false;
            {
                uint64_t v = 0;
                for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(data[i]) << (i * 8);
                out = v;
            }
            return true;
        case core::Format::Int:
            if (data.size() < 4) return false;
            out = static_cast<int32_t>(le32(data));
            return true;
        case core::Format::Float:
            if (data.size() < 4) return false;
            {
                uint32_t raw = le32(data);
                float f;
                std::memcpy(&f, &raw, sizeof(f));
                out = f;
            }
            return true;
        case core::Format::String:
            out = std::string(data.begin(), data.end());
            return true;
        case core::Format::TLV8:
        case core::Format::Data:
            out = data;
            return true;
    }
    return false;
}
} // namespace



void BleTransport::process_transaction(uint16_t connection_id, TransactionState& state) {
    // Header is 5 bytes: CF(1) | Opcode(1) | TID(1) | IID(2)
    if (state.buffer.size() < 5) return; 
    
    PDUOpcode opcode = state.opcode;
    
    // For Write operations (opcode 0x02, 0x04), the header is 7 bytes:
    // CF(1) | Opcode(1) | TID(1) | IID(2) | BodyLen(2) | Body...
    if (ble::HapPdu::opcode_has_body(opcode)) {
        
        if (state.buffer.size() < 7) return;
        
        uint16_t body_length = state.buffer[5] | (static_cast<uint16_t>(state.buffer[6]) << 8);

        size_t expected_total = 7 + body_length;
        
        if (state.buffer.size() < expected_total) {
            HAP_LOG(config_.system, "[BleTransport] Waiting for more fragments: have ", state.buffer.size(), " bytes, need ", expected_total, " bytes");
            return;
        }
        
        HAP_LOG(config_.system, "[BleTransport] All fragments received: ", state.buffer.size(), " bytes (body=", body_length, ")");
    }
    
    [[maybe_unused]] uint16_t tid = state.buffer[2];
    uint16_t iid = state.buffer[3] | (state.buffer[4] << 8);

    // Pair Verify (0x4E) responses complete Pair Verify; the security upgrade
    // must happen only after the M4 response has been queued (see below).
    const bool is_pair_verify_write = state.target_char_type == 0x4E;

    HAP_LOG_INFO(config_.system, "[BleTransport] Processing Opcode ", (int)opcode, " TID=", tid);
    
    auto find_service = [&](uint16_t target_iid) -> core::Service* {
        return config_.database ? config_.database->find_service_by_iid(target_iid) : nullptr;
    };

    auto find_char_in_db = [&](uint16_t target_iid) -> core::Characteristic* {
        return config_.database ? config_.database->find_characteristic_by_iid(target_iid) : nullptr;
    };

    // Helper to get full characteristic info including AID
    auto find_char_info = [&](uint16_t target_iid) -> core::AttributeDatabase::CharacteristicLocation {
        if (config_.database) {
            return config_.database->find_characteristic_info(target_iid);
        }
        return {};
    };

    if (opcode == PDUOpcode::ServiceSignatureRead) {
        state.active = false;
        state.procedure_start_ms = 0;
        
        std::vector<uint8_t> sig_response;
        bool is_primary = false;
        bool found = false;
        std::vector<uint16_t> linked_services;

        for (const auto& meta : pairing_char_metadata_) {
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
            HAP_LOG_WARN(config_.system, "[BleTransport] Service Signature Read IID=", iid, " Not Found - returning empty");
            // Return empty response with props=0
            BleTlvBuilder builder;
            builder.add_uint16(HAPBLEPDUTLVType::ServiceProperties, 0);
            auto tlv = builder.build();
            sig_response.insert(sig_response.end(), tlv.begin(), tlv.end());
            
            sig_response.push_back(static_cast<uint8_t>(HAPBLEPDUTLVType::LinkedServices));
            sig_response.push_back(0x00);
        } else {
            uint16_t props = is_primary ? 0x0001 : 0x0000;
            
            BleTlvBuilder builder;
            builder.add_uint16(HAPBLEPDUTLVType::ServiceProperties, props);
            auto tlv = builder.build();
            sig_response.insert(sig_response.end(), tlv.begin(), tlv.end());
            
            sig_response.push_back(static_cast<uint8_t>(HAPBLEPDUTLVType::LinkedServices));
            sig_response.push_back(static_cast<uint8_t>(linked_services.size() * 2));
            for (uint16_t linked_iid : linked_services) {
                sig_response.push_back(linked_iid & 0xFF);
                sig_response.push_back((linked_iid >> 8) & 0xFF);
            }
            
            HAP_LOG_INFO(config_.system, "[BleTransport] Service Signature Read IID=", iid, " Primary=", is_primary);
        }
        
        send_response(connection_id, state.transaction_id, state.target_char_type, 0x00, sig_response);
        return;
    }
    
    if (opcode == PDUOpcode::CharacteristicSignatureRead) {
        state.active = false; 
        
        std::vector<uint8_t> sig_response = process_signature_read(connection_id, iid);
        
        uint8_t status = 0x00;
        if (sig_response.empty()) {
             HAP_LOG_WARN(config_.system, "[BleTransport] Char Signature Read IID=", iid, " Not Found");
             status = 0x04; // Invalid Instance ID (Table 7-37)
        } else {
             HAP_LOG_INFO(config_.system, "[BleTransport] Char Signature Read IID=", iid, " Len=", sig_response.size());
             HAP_LOG(config_.system, "[BleTransport] Signature Response: ", to_hex_string(sig_response.data(), sig_response.size()));
        }
        
        send_response(connection_id, state.transaction_id, state.target_char_type, status, sig_response);
        return;
    }

    // For operations with Body (Write, Timed Write, Configuration)
    // Header is 7 bytes: CF(1) | Opcode(1) | TID(1) | IID(2) | BodyLen(2)
    // Body starts at offset 7
    // For Read (0x03), header is only 5 bytes with no body
    
    size_t body_offset = ble::HapPdu::body_offset(opcode);
    
    std::span<const uint8_t> body;
    if (state.buffer.size() > body_offset) {
        body = std::span<const uint8_t>(state.buffer.data() + body_offset, state.buffer.size() - body_offset);
    }
    
    if (opcode == PDUOpcode::CharacteristicRead) {
        
        std::vector<uint8_t> value_bytes;
        uint8_t status = 0x00;
        
        const CharacteristicMetadata* meta_it = nullptr;
        for (const auto& m : pairing_char_metadata_) {
            if (m.instance_id == iid) { meta_it = &m; break; }
        }
        if (meta_it) {
            if (meta_it->char_type == 0x4F) { // Pairing Features
                value_bytes = {0x01, 0x01, 0x00};
                HAP_LOG_INFO(config_.system, "[BleTransport] Pairing Features Read: returning 0x00");
            } else if (meta_it->char_type == 0x37) { // Version
                // HAP Spec 7.4.3.1: BLE protocol version string, "2.2.0" for
                // this version of the spec (test requirement #3: must match).
                std::string version = "2.2.0";
                value_bytes.push_back(0x01); // TLV Type: HAP-Param-Value
                value_bytes.push_back(static_cast<uint8_t>(version.size())); // Length
                value_bytes.insert(value_bytes.end(), version.begin(), version.end()); // Value
                HAP_LOG_INFO(config_.system, "[BleTransport] Version Read: returning ", version);
            } else {
                status = 0x05; // Invalid Request
            }
        } 
        else {
            auto ch = find_char_in_db(iid);
            if (ch) {
                auto read_result = ch->get_value();
                if (std::holds_alternative<core::HAPStatus>(read_result)) {
                    // Read callback returned error
                    status = 0x02; // HAP Error (map HAPStatus to BLE status)
                    HAP_LOG_WARN(config_.system, "[BleTransport] Read IID=", iid, " callback returned error");
                } else {
                    auto raw_value = core::CharacteristicSerializer::to_bytes(std::get<core::Value>(read_result));
                    value_bytes.push_back(0x01); // Type: HAP-Param-Value
                    value_bytes.push_back(static_cast<uint8_t>(raw_value.size()));
                    value_bytes.insert(value_bytes.end(), raw_value.begin(), raw_value.end());
                }
            } else {
                status = 0x04; // Invalid Instance ID (Table 7-37)
                HAP_LOG_WARN(config_.system, "[BleTransport] Read IID=", iid, " Not Found");
            }
        }
        
        send_response(connection_id, state.transaction_id, state.target_char_type, status, value_bytes);
    }
    else if (opcode == PDUOpcode::CharacteristicWrite) {
        uint8_t status = 0x00;
        std::vector<uint8_t> response_body;

        const CharacteristicMetadata* meta_it = nullptr;
        for (const auto& m : pairing_char_metadata_) {
            if (m.instance_id == iid) { meta_it = &m; break; }
        }
        if (meta_it) {
             uint8_t type = meta_it->char_type;

             auto& session = session_manager_->get_or_create(connection_id);
             // HAP 7.4.7.2: a new Pair Verify on an already-secured session
             // must tear down the existing security session first.
             if (type == 0x4E && session.context && session.context->is_encrypted()) {
                 HAP_LOG_INFO(config_.system, "[BleTransport] New Pair Verify on secured session - tearing down old session");
                 session.context = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
             }
             if (!session.context) {
                session.context = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
             }
             auto& ctx = *session.context;

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
                     HAP_LOG_WARN(config_.system, "[BleTransport] Warning: Pairing Write missing Value TLV wrapper. Using raw body.");
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
                HAP_LOG_INFO(config_.system, "[BleTransport] Software Auth Write - Skipping (Success)");
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
                 auto body_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
                 
                 bool return_response_requested = core::TLV8::find(body_tlvs, (uint8_t)HAPBLEPDUTLVType::ReturnResponse).has_value();
                 
                 auto value_tlv = core::TLV8::find(body_tlvs, 0x01);
                 if (value_tlv && !value_tlv->empty()) {
                     core::Value new_value;
                     if (!decode_ble_value(ch->format(), *value_tlv, new_value)) {
                         HAP_LOG_WARN(config_.system, "[BleTransport] Write IID=", iid, " - value too short for format");
                         send_response(connection_id, state.transaction_id, state.target_char_type, 0x06, {});
                         return;
                     }
                     
                     ch->set_value(new_value, core::EventSource::from_connection(connection_id));
                     HAP_LOG_INFO(config_.system, "[BleTransport] Write IID=", iid, " success");
                     
                     // HAP 7.4.6.1: GSN increments once for multiple
                     // characteristic changes while in the connected state
                     if (!state.gsn_incremented) {
                         state.gsn_incremented = true;
                         increment_gsn();
                     }
                     
                     // HAP Spec 7.3.5.5: Write-with-Response - return value if requested
                     if (return_response_requested && 
                         core::has_permission(ch->permissions(), core::Permission::WriteResponse)) {
                         auto response_opt = ch->handle_write_response(new_value);
                         core::Value val_to_send;
                         
                         if (response_opt.has_value()) {
                             auto& response = *response_opt;
                             if (std::holds_alternative<core::HAPStatus>(response)) {
                                 // WriteResponse callback returned error - still send status
                                 status = 0x02; // HAP Error
                                 HAP_LOG_WARN(config_.system, "[BleTransport] WriteResponse IID=", iid, " callback returned error");
                             } else {
                                 val_to_send = std::get<core::Value>(response);
                             }
                         } else {
                             // No callback, use current value
                             auto read_result = ch->get_value();
                             if (std::holds_alternative<core::Value>(read_result)) {
                                 val_to_send = std::get<core::Value>(read_result);
                             }
                         }
                         
                         if (status == 0x00) {
                             auto raw_bytes = core::CharacteristicSerializer::to_bytes(val_to_send);
                             std::vector<core::TLV> resp_tlvs;
                             resp_tlvs.emplace_back(0x01, raw_bytes); // HAP-Param-Value
                             response_body = core::TLV8::encode(resp_tlvs);
                             
                             HAP_LOG_INFO(config_.system, "[BleTransport] Write-Response IID=", iid, " returning ", response_body.size(), " bytes");
                         }
                     }
                     
                     // Get actual AID for this characteristic
                     auto char_info = find_char_info(iid);
                     handle_characteristic_change(char_info.accessory_id, iid, new_value, connection_id);
                 } else {
                     HAP_LOG_WARN(config_.system, "[BleTransport] Write IID=", iid, " - no value TLV found");
                     status = 0x06; // Invalid Request
                 }
            } else {
                status = 0x04; // Invalid Instance ID (Table 7-37)
            }
        }

        send_response(connection_id, state.transaction_id, state.target_char_type, status, response_body);

        // The M4 response was just queued in cleartext; only now may the
        // session switch to encrypted (HAP: security starts after PV ends).
        if (is_pair_verify_write) {
            auto& pv_session = session_manager_->get_or_create(connection_id);
            if (pv_session.context) {
                config_.pairing_endpoints->complete_pair_verify(*pv_session.context);
            }
        }
    }
    else if (opcode == PDUOpcode::CharacteristicTimedWrite) {
        // HAP 7.3.5.4: the body carries the Value TLV (0x01) and a TTL TLV
        // (0x08, 1 byte, in 100ms units). The TTL timer starts when the
        // timed-write response is delivered (i.e., on the controller's GATT
        // read); the queued write is applied only if Execute-Write arrives
        // before the deadline (test requirement #17).
        auto write_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
        uint16_t ttl_ms = 3000; // Spec default when TTL TLV is absent
        if (auto ttl_tlv = core::TLV8::find(write_tlvs, 0x08); ttl_tlv && !ttl_tlv->empty()) {
            ttl_ms = static_cast<uint16_t>((*ttl_tlv)[0]) * 100;
        }

        state.timed_write_body.assign(body.begin(), body.end());
        state.timed_write_iid = iid;
        state.timed_write_expiry_ms = config_.system->millis() + ttl_ms;

        HAP_LOG_INFO(config_.system, "[BleTransport] Timed Write stored for IID=", iid, " Body size=", body.size(), " TTL=", ttl_ms, "ms");

        send_response(connection_id, state.transaction_id, state.target_char_type, 0x00, {});
    }
    else if (opcode == PDUOpcode::CharacteristicExecuteWrite) {
        uint8_t status = 0x00;
        std::vector<uint8_t> response_body;

        if (state.timed_write_body.empty()) {
            HAP_LOG_WARN(config_.system, "[BleTransport] Execute Write with no pending timed write");
            status = 0x06; // Invalid Request
        } else if (config_.system->millis() > state.timed_write_expiry_ms) {
            // HAP 7.3.5.4: Execute-Write after TTL expiry must be ignored and
            // answered with an error; the queued write is discarded.
            HAP_LOG_WARN(config_.system, "[BleTransport] Execute Write after TTL expiry - discarding queued timed write");
            status = 0x06; // Invalid Request
            state.timed_write_body.clear();
            state.timed_write_iid = 0;
            state.timed_write_expiry_ms = 0;
        } else {
            HAP_LOG_INFO(config_.system, "[BleTransport] Executing pending timed write for IID=", state.timed_write_iid);
            
            const CharacteristicMetadata* meta_it = nullptr;
            for (const auto& m : pairing_char_metadata_) {
                if (m.instance_id == state.timed_write_iid) { meta_it = &m; break; }
            }
            if (meta_it) {
                uint8_t type = meta_it->char_type;
                
                auto& session = session_manager_->get_or_create(connection_id);
                if (!session.context) {
                    session.context = std::make_unique<ConnectionContext>(config_.crypto, config_.system, connection_id);
                }
                auto& ctx = *session.context;
                
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
                        if (!decode_ble_value(ch->format(), *value_tlv, new_value)) {
                            HAP_LOG_WARN(config_.system, "[BleTransport] Execute Timed Write IID=", state.timed_write_iid, " - value too short for format");
                            status = 0x06; // Invalid Request
                            send_response(connection_id, state.transaction_id, state.target_char_type, status, response_body);
                            state.timed_write_body.clear();
                            state.timed_write_iid = 0;
                            return;
                        }
                        
                        ch->set_value(new_value, core::EventSource::from_connection(connection_id));
                        HAP_LOG_INFO(config_.system, "[BleTransport] Execute Timed Write IID=", state.timed_write_iid, " success");

                        // Notify subscribers exactly like the direct write path:
                        // a timed write that changes a Notify characteristic must
                        // produce an event, or controllers wait forever (e.g.
                        // "Unlocking..." until the state is re-read).
                        auto char_info = find_char_info(state.timed_write_iid);
                        handle_characteristic_change(char_info.accessory_id,
                                                     state.timed_write_iid, new_value,
                                                     connection_id);

                        // HAP 7.4.6.1: GSN increments once for multiple
                        // characteristic changes while in the connected state
                        if (!state.gsn_incremented) {
                            state.gsn_incremented = true;
                            increment_gsn();
                        }
                        
                    } else {
                        HAP_LOG_WARN(config_.system, "[BleTransport] Execute Timed Write IID=", state.timed_write_iid, " - no value TLV found");
                        status = 0x06; // Invalid Request
                    }
                } else {
                    HAP_LOG_WARN(config_.system, "[BleTransport] Execute Timed Write IID=", state.timed_write_iid, " - characteristic not found");
                    status = 0x04; // Invalid Instance ID (Table 7-37)
                }
            }
            
            state.timed_write_body.clear();
            state.timed_write_iid = 0;
            state.timed_write_expiry_ms = 0;
        }

        send_response(connection_id, state.transaction_id, state.target_char_type, status, response_body);
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
        // HAP Table 7-29 (7.3.4.14): bit 0x0001 = Enable Broadcast Notification
        bool broadcast_enabled = (properties & 0x0001) != 0;
        bool found_bc = false;
        for (auto& bc : broadcast_configs_) {
            if (bc.iid == iid) {
                bc = BroadcastConfig{iid, broadcast_interval, broadcast_enabled};
                found_bc = true;
                break;
            }
        }
        if (!found_bc) {
            broadcast_configs_.push_back(BroadcastConfig{iid, broadcast_interval, broadcast_enabled});
        }
        
        HAP_LOG_INFO(config_.system, "[BleTransport] Characteristic Configuration IID=", iid, " Props=", properties, " Interval=", broadcast_interval, " BroadcastEnabled=", broadcast_enabled);
        
        send_response(connection_id, state.transaction_id, state.target_char_type, status, response_body);
    }
    else if (opcode == PDUOpcode::ProtocolConfiguration) {
        // HAP-Protocol-Configuration-Request/Response
        // Per Spec 7.3.4.16-17, Table 7-32, 7-34
        
        std::vector<uint8_t> response_body;
        uint8_t status = 0x00;
        
        auto config_tlvs = core::TLV8::parse(std::vector<uint8_t>(body.begin(), body.end()));
        
        auto gen_key_tlv = core::TLV8::find(config_tlvs, 0x01);
        
        std::vector<core::TLV> resp_tlvs;
        
        if (gen_key_tlv) {
            // Generate broadcast encryption key per HAP Spec 7.4.7.3:
            // BroadcastEncryptionKey = HKDF-SHA-512(
            //     IKM = Current Session shared secret,
            //     Salt = Controller's Ed25519 long-term public key,
            //     Info = "Broadcast-Encryption-Key",
            //     L = 32 bytes
            // )
            HAP_LOG_INFO(config_.system, "[BleTransport] Protocol Config: Generate Broadcast Encryption Key requested");
            
            auto* session = session_manager_->get_session(connection_id);
            if (!session || !session->context || !session->context->is_encrypted()) {
                HAP_LOG_ERROR(config_.system, "[BleTransport] Protocol Config: Cannot generate key - no secure session");
                status = 0x06; // Invalid Request
            } else {
                auto& ctx = *session->context;
                
                std::string pairing_key = "pairing_" + ctx.controller_id();
                auto controller_ltpk = config_.storage->get(pairing_key);
                
                if (!controller_ltpk || controller_ltpk->size() != 32) {
                    HAP_LOG_ERROR(config_.system, "[BleTransport] Protocol Config: Controller LTPK not found for: ", ctx.controller_id());
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
                    
                    HAP_LOG_INFO(config_.system, "[BleTransport] Protocol Config: Generated and stored Broadcast Encryption Key (GSN start=", broadcast_key_gsn_start_, ")");
                }
            }
        }
        
        {
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
                config_.config_number
            });
            
            // Accessory Advertising Identifier TLV (0x03) - 6 bytes
            // Use Device ID as advertising identifier
            std::array<uint8_t, 6> device_id{};
    (void)parse_device_id(config_.accessory_id, device_id);
            resp_tlvs.emplace_back(0x03, std::vector<uint8_t>(device_id.begin(), device_id.end()));
        }
        
        response_body = core::TLV8::encode(resp_tlvs);
        
        HAP_LOG_INFO(config_.system, "[BleTransport] Protocol Configuration completed");
        
        send_response(connection_id, state.transaction_id, state.target_char_type, status, response_body);
    }
    else {
        send_response(connection_id, state.transaction_id, state.target_char_type, 0x01, {});
    }
}

void BleTransport::send_response(uint16_t conn_id, uint16_t tid, uint16_t char_type, uint8_t status, std::span<const uint8_t> body) {
    std::vector<uint8_t> packet = ble::HapPdu::build_response(tid, status, body);
    
    bool session_is_secured = false;
    auto* session_ptr = session_manager_->get_session(conn_id);
    if (session_ptr && session_ptr->context) {
        session_is_secured = session_ptr->context->is_encrypted();
    }
    
    const bool requires_encryption = characteristic_requires_encryption(char_type);

    if (session_is_secured && requires_encryption) {
        auto& ctx = *session_ptr->context;
        auto encrypted = ctx.get_secure_session()->encrypt_ble_pdu(packet);
        if (encrypted.empty()) {
            HAP_LOG_ERROR(config_.system, "[BleTransport] Response encryption failed for connection ", conn_id);
            session_manager_->get_or_create(conn_id).transaction.response_buffer = packet;
        } else {
            HAP_LOG(config_.system, "[BleTransport] Encrypted response (", encrypted.size(), " bytes)");
            session_manager_->get_or_create(conn_id).transaction.response_buffer = std::move(encrypted);
        }
    } else {
        session_manager_->get_or_create(conn_id).transaction.response_buffer = packet;
    }

    auto& st = session_manager_->get_or_create(conn_id).transaction;
    st.last_write_ms = config_.system->millis();
    if (st.connection_established_ms == 0) {
        st.connection_established_ms = config_.system->millis();
    }

    // HAP-BLE Spec 7.3.5.1/7.3.5.5: The response is returned in the GATT Read Response.
}

std::vector<uint8_t> BleTransport::process_signature_read(uint16_t connection_id, uint16_t char_iid) {
    (void)connection_id;
    std::vector<uint8_t> response;
    
    const CharacteristicMetadata* pairing_meta = nullptr;
    for (const auto& m : pairing_char_metadata_) {
        if (m.instance_id == char_iid) { pairing_meta = &m; break; }
    }
    if (pairing_meta) {
        const auto& meta = *pairing_meta;
        
        {
            BleTlvBuilder builder;
            builder.add_hap_uuid128(HAPBLEPDUTLVType::CharacteristicType, meta.char_type);
            auto tlv = builder.build();
            response.insert(response.end(), tlv.begin(), tlv.end());
        }
        
        {
            BleTlvBuilder builder;
            builder.add_uint16(HAPBLEPDUTLVType::ServiceInstanceID, meta.service_id);
            auto tlv = builder.build();
            response.insert(response.end(), tlv.begin(), tlv.end());
        }
        
        {
            BleTlvBuilder builder;
            builder.add_hap_uuid128(HAPBLEPDUTLVType::ServiceType, meta.service_type);
            auto tlv = builder.build();
            response.insert(response.end(), tlv.begin(), tlv.end());
        }
        
        {
            BleTlvBuilder builder;
            builder.add_uint16(HAPBLEPDUTLVType::CharacteristicProperties, meta.properties);
            auto tlv = builder.build();
            response.insert(response.end(), tlv.begin(), tlv.end());
        }
        
        if (!meta.user_description.empty()) {
            response.push_back((uint8_t)HAPBLEPDUTLVType::GATTUserDescription);
            response.push_back(static_cast<uint8_t>(meta.user_description.size()));
            response.insert(response.end(), meta.user_description.begin(), meta.user_description.end());
        }
        
        {
             uint8_t gatt_format = (meta.char_type == 0x4F) ? 0x04 : 0x1B;
             BleTlvBuilder builder;
             builder.add_gatt_format(gatt_format);
             auto tlv = builder.build();
             response.insert(response.end(), tlv.begin(), tlv.end());
        }

        HAP_LOG_INFO(config_.system, "[BleTransport] Generated signature for special char IID=", char_iid);
        
        return response;
    }
    
    if (config_.database) {
        for (const auto& acc : config_.database->accessories()) {
            for (const auto& svc : acc->services()) {
                for (const auto& ch : svc->characteristics()) {
                     if (ch->iid() == char_iid) {
                            {
                             uint16_t short_uuid = ch->type() & 0xFFFF;
                             BleTlvBuilder builder;
                             builder.add_hap_uuid128(HAPBLEPDUTLVType::CharacteristicType, short_uuid);
                             auto tlv = builder.build();
                             response.insert(response.end(), tlv.begin(), tlv.end());
                         }

                         {
                             BleTlvBuilder builder;
                             builder.add_uint16(HAPBLEPDUTLVType::ServiceInstanceID, svc->iid());
                             auto tlv = builder.build();
                             response.insert(response.end(), tlv.begin(), tlv.end());
                         }
                         
                         {
                             uint16_t svc_short_uuid = svc->type() & 0xFFFF;
                             BleTlvBuilder builder;
                             builder.add_hap_uuid128(HAPBLEPDUTLVType::ServiceType, svc_short_uuid);
                             auto tlv = builder.build();
                             response.insert(response.end(), tlv.begin(), tlv.end());
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
                                 else if (p == core::Permission::Broadcast) props |= 0x0200;
                             }
                             
                             BleTlvBuilder builder;
                             builder.add_uint16(HAPBLEPDUTLVType::CharacteristicProperties, props);
                             auto tlv = builder.build();
                             response.insert(response.end(), tlv.begin(), tlv.end());
                         }

                         {
                             uint8_t gatt_format = core::CharacteristicSerializer::gatt_format_byte(ch->format());
                             BleTlvBuilder builder;
                             builder.add_gatt_format(gatt_format);
                             auto tlv = builder.build();
                             response.insert(response.end(), tlv.begin(), tlv.end());
                         }
                         
                         return response;
                     }
                }
            }
        }
    }

    return response;
}


void BleTransport::register_accessory_info_service() {
    register_services_by_type(0x3E);
}

void BleTransport::register_user_services() {
    register_services_by_type(0);
}

void BleTransport::register_services_by_type(uint16_t filter_type) {
    if (!config_.database) return;
    for (const auto& acc : config_.database->accessories()) {
        for (const auto& svc : acc->services()) {
            uint16_t svc_type = svc->type() & 0xFFFF;
            
            if (filter_type == 0 && svc_type == 0x3E) continue;
            if (filter_type != 0 && svc_type != filter_type) continue;
            
            // Use IID already assigned by AttributeDatabase (via IIDManager)
            uint16_t svc_iid = static_cast<uint16_t>(svc->iid());

            platform::Ble::ServiceDefinition def;
            def.uuid = type_to_uuid_str(static_cast<uint16_t>(svc->type() & 0xFFFF));
            // HAP Spec 7.4.1: All HAP services must be primary GATT services.
            def.is_primary = true;
            
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
                // Use IID already assigned by AttributeDatabase (via IIDManager)
                uint16_t char_iid = static_cast<uint16_t>(ch->iid());

                std::string char_uuid = type_to_uuid_str(static_cast<uint16_t>(ch->type() & 0xFFFF));
                platform::Ble::CharacteristicDefinition cdef;
                cdef.uuid = char_uuid;
                
                {
                    std::pair<uint64_t, uint64_t> key{acc->aid(), ch->iid()};
                    bool found_instance = false;
                    for (auto& entry : instance_map_) {
                        if (entry.first == key) {
                            entry.second = static_cast<uint16_t>(ch->type() & 0xFFFF);
                            found_instance = true;
                            break;
                        }
                    }
                    if (!found_instance) {
                        instance_map_.emplace_back(key, static_cast<uint16_t>(ch->type() & 0xFFFF));
                    }
                }
                
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
                
                cdef.on_write = [this, char_type = static_cast<uint16_t>(ch->type() & 0xFFFF)](uint16_t conn_id, std::span<const uint8_t> data, bool response) {
                    (void)response;
                    handle_hap_write(conn_id, char_type, data);
                };
                
                cdef.on_subscribe = [this, char_type = static_cast<uint16_t>(ch->type() & 0xFFFF)](uint16_t conn_id, bool enabled) {
                     if (enabled) {
                         session_manager_->add_subscription(char_type, conn_id);
                     } else {
                         session_manager_->remove_subscription(char_type, conn_id);
                     }
                };

                def.characteristics.push_back(std::move(cdef));
            }
            config_.ble->register_service(def);
        }
    }
}

uint16_t BleTransport::get_current_gsn() {
    if (gsn_loaded_) {
        return cached_gsn_;
    }
    auto gsn_bytes = config_.storage->get("gsn");
    uint16_t gsn = 1;
    if (gsn_bytes && gsn_bytes->size() == 2) {
        gsn = static_cast<uint16_t>((*gsn_bytes)[0]) | (static_cast<uint16_t>((*gsn_bytes)[1]) << 8);
    }
    if (gsn == 0) gsn = 1;
    cached_gsn_ = gsn;
    gsn_loaded_ = true;
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
        HAP_LOG_WARN(config_.system, "[BleTransport] Broadcast encryption key expired (GSN diff=", gsn_diff, ")");
        broadcast_key_valid_ = false;
        return false;
    }
    
    return true;
}

void BleTransport::handle_characteristic_change(uint64_t aid, uint64_t iid, 
                                                 const core::Value& value, 
                                                 uint32_t exclude_conn_id) {
    HAP_LOG(config_.system, "[BleTransport] Characteristic change: AID=", aid, " IID=", iid);
    
    // Find the characteristic to check its event properties
    auto ch = config_.database ? config_.database->find_characteristic(aid, iid) : nullptr;
    if (!ch) {
        HAP_LOG_WARN(config_.system, "[BleTransport] Cannot find characteristic for event: IID=", iid);
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
        if (perm == core::Permission::Broadcast) {
            supports_broadcast = true;
        }
    }
    
    bool broadcast_enabled = false;
    for (const auto& bc : broadcast_configs_) {
        if (bc.iid == static_cast<uint16_t>(iid)) {
            broadcast_enabled = bc.enabled;
            break;
        }
    }
    
    uint16_t char_type = 0;
    for (const auto& entry : instance_map_) {
        if (entry.first == std::pair<uint64_t, uint64_t>{aid, iid}) {
            char_type = entry.second;
            break;
        }
    }
    if (char_type == 0) {
        HAP_LOG_WARN(config_.system, "[BleTransport] No UUID mapping for IID=", iid);
        return;
    }
    
    // exclude_conn_id == kNoConnectionExclusion (0) means "notify everyone";
    // otherwise skip the connection that caused the change. Note BLE connection
    // IDs start at 0, so the sentinel must be checked explicitly.
    bool has_connected_subscribers = false;
    if (session_manager_->has_subscribers(char_type)) {
        for (uint16_t conn_id : session_manager_->get_subscribers(char_type)) {
            if (exclude_conn_id == kNoConnectionExclusion || conn_id != exclude_conn_id) {
                has_connected_subscribers = true;
                break;
            }
        }
    }
    
    const bool is_connected = session_manager_->session_count() > 0;
    
    if (is_connected && has_connected_subscribers && supports_connected) {
        HAP_LOG_INFO(config_.system, "[BleTransport] Sending Connected Event for IID=", iid);
        send_connected_event(static_cast<uint16_t>(iid));
    }
    else if (!is_connected && supports_broadcast && broadcast_enabled && is_broadcast_key_valid()) {
        HAP_LOG_INFO(config_.system, "[BleTransport] Sending Broadcasted Event for IID=", iid);
        send_broadcasted_event(static_cast<uint16_t>(iid), value);
    }
    else if (!is_connected && supports_disconnected) {
        HAP_LOG_INFO(config_.system, "[BleTransport] Sending Disconnected Event for IID=", iid);
        send_disconnected_event(static_cast<uint16_t>(iid));
    }
    else {
        HAP_LOG(config_.system, "[BleTransport] No event sent for IID=", iid, " (connected=", is_connected, ", has_subs=", has_connected_subscribers +
            ", supports_connected=", supports_connected, ")");
    }
}

void BleTransport::send_connected_event(uint16_t iid) {
    // Per HAP Spec 7.4.6.1 Connected Events:
    // Send a ZERO-LENGTH indication to controllers that registered for indications.
    
    uint16_t char_type = 0;
    for (const auto& entry : instance_map_) {
        if (entry.first.second == iid) {
            char_type = entry.second;
            break;
        }
    }
    
    if (char_type == 0) {
        HAP_LOG_WARN(config_.system, "[BleTransport] Cannot send Connected Event - no UUID for IID=", iid);
        return;
    }
    
    if (!session_manager_->has_subscribers(char_type)) {
        HAP_LOG(config_.system, "[BleTransport] No subscribers for Connected Event IID=", iid);
        return;
    }
    
    std::vector<uint8_t> empty_indication;
    std::string uuid = type_to_uuid_str(char_type);
    
    for (uint16_t conn_id : session_manager_->get_subscribers(char_type)) {
        HAP_LOG(config_.system, "[BleTransport] Sending zero-length indication to conn=", conn_id, " for IID=", iid);
        
        config_.ble->send_indication(conn_id, uuid, empty_indication);
    }
}

void BleTransport::send_broadcasted_event(uint16_t iid, const core::Value& value) {
    // Per HAP Spec 7.4.6.2 Broadcasted Events:
    // When disconnected and broadcast is configured, send encrypted advertisement
    // containing the characteristic value.
    
    if (!is_broadcast_key_valid()) {
        HAP_LOG_WARN(config_.system, "[BleTransport] Cannot send Broadcasted Event - no valid broadcast key");
        send_disconnected_event(iid);
        return;
    }
    
    std::vector<uint8_t> encrypted_payload = build_encrypted_advertisement_payload(iid, value);
    if (encrypted_payload.empty()) {
        HAP_LOG_ERROR(config_.system, "[BleTransport] Failed to build encrypted advertisement payload");
        send_disconnected_event(iid);
        return;
    }
    
    increment_gsn();

    std::array<uint8_t, 6> adv_id{};
    (void)parse_device_id(config_.accessory_id, adv_id);
    
    uint32_t interval_ms = 20;  // Default 20ms
    for (const auto& bc : broadcast_configs_) {
        if (bc.iid == iid) {
            switch (bc.interval) {
                case 0x01: interval_ms = 20; break;
                case 0x02: interval_ms = 1280; break;
                case 0x03: interval_ms = 2560; break;
                default: interval_ms = 20; break;
            }
            break;
        }
    }
    
    platform::Ble::EncryptedAdvertisement enc_adv;
    enc_adv.advertising_id = adv_id;
    enc_adv.encrypted_payload = std::move(encrypted_payload);
    enc_adv.gsn = get_current_gsn();
    
    HAP_LOG_INFO(config_.system, "[BleTransport] Starting encrypted advertisement for IID=", iid, " interval=", interval_ms, "ms duration=3000ms");
    
    config_.ble->start_encrypted_advertising(enc_adv, interval_ms, 3000);
}

void BleTransport::send_disconnected_event(uint16_t iid) {
    // Per HAP Spec 7.4.6.3 Disconnected Events:
    // Increment GSN (once per disconnected period until connected) and
    // use 20ms advertising for at least 3 seconds, then revert to normal.
    
    HAP_LOG(config_.system, "[BleTransport] Disconnected Event for IID=", iid);
    
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
    
    std::array<uint8_t, 6> device_id{};
    (void)parse_device_id(config_.accessory_id, device_id);
    
    uint16_t gsn = get_current_gsn();
    
    uint8_t config_number = 1;
    auto cn_bytes = config_.storage->get("config_number");
    if (cn_bytes && !cn_bytes->empty()) {
        std::string cn_str(cn_bytes->begin(), cn_bytes->end());
        if (!parse_config_number(cn_str, config_number)) {
                config_number = 1;
            }
    }
    
    auto adv = platform::Ble::Advertisement::create_hap(
        status_flags, device_id.data(), config_.category_id, gsn, config_number, setup_hash);
    adv.local_name = config_.device_name;
    
    // Per HAP Spec 7.4.6.3: Use fast interval (20 ms) for 3 seconds, then normal interval
    HAP_LOG_INFO(config_.system, "[BleTransport] Starting timed advertising for Disconnected Event (", config_.ble->interval_config.fast_interval_ms, "ms for ", config_.ble->interval_config.fast_duration_ms, "ms)");
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
    
    std::vector<uint8_t> value_bytes = core::CharacteristicSerializer::to_bytes(value);
    for (size_t i = 0; i < 8 && i < value_bytes.size(); ++i) {
        plaintext[4 + i] = value_bytes[i];
    }
    
    std::array<uint8_t, 12> nonce = {};
    nonce[0] = gsn & 0xFF;
    nonce[1] = (gsn >> 8) & 0xFF;
    
    std::array<uint8_t, 6> aad{};
    (void)parse_device_id(config_.accessory_id, aad);

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
        HAP_LOG_ERROR(config_.system, "[BleTransport] Broadcast encryption failed");
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(16);
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), full_tag.begin(), full_tag.begin() + 4);
    
    return result;
}

} // namespace hap::transport

