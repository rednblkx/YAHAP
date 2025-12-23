#include "hap/transport/BleTransport.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

using namespace hap;
using namespace hap::transport;

// --- MOCK DEFINITIONS for Linker ---
namespace hap::transport {

PairingEndpoints::PairingEndpoints(Config config) : config_(config) {}

Response PairingEndpoints::handle_pair_setup(const Request& req, ConnectionContext& ctx) {
    Response resp{Status::OK};
    resp.body = {0xBB}; // Return dummy body to test wrapping
    return resp;
}
Response PairingEndpoints::handle_pair_verify(const Request& req, ConnectionContext& ctx) {
    return Response{Status::OK};
}
Response PairingEndpoints::handle_pairings(const Request& req, ConnectionContext& ctx) {
    return Response{Status::OK};
}

// ConnectionContext Stub
ConnectionContext::ConnectionContext(platform::Crypto* c, platform::System* s, uint32_t cid) 
    : crypto_(c), system_(s), connection_id_(cid) {}
    
} // namespace hap::transport

namespace hap::pairing {
    PairSetup::~PairSetup() {}
    PairVerify::~PairVerify() {}
}

// Simple Test Framework
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "Assertion failed: " << #a << " (" << (a) << ") != " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

// Mock BLE Platform
class MockBle : public platform::Ble {
public:
    struct Notification {
        uint16_t conn_id;
        std::string uuid;
        std::vector<uint8_t> data;
    };
    std::vector<Notification> sent_notifications;
    std::vector<ServiceDefinition> registered_services;


    void register_service(const ServiceDefinition& def) override { 
        registered_services.push_back(def);
    }
    
    void start_advertising(const hap::platform::Ble::Advertisement& adv, uint32_t interval_ms) override {
        // Capture adv data if needed
        (void)adv; (void)interval_ms;
    }
    
    void stop_advertising() override {}
    void start() override {}
    
    bool send_indication(uint16_t connection_id, const std::string& char_uuid, std::span<const uint8_t> data) override {
        sent_notifications.push_back({connection_id, char_uuid, std::vector<uint8_t>(data.begin(), data.end())});
        return true;
    }
    
    void disconnect(uint16_t connection_id) override { (void)connection_id; }
    
    void set_disconnect_callback(DisconnectCallback callback) override { 
        disconnect_callback_ = callback;
    }
    
    void start_timed_advertising(const Advertisement& data,
                                  uint32_t fast_interval_ms,
                                  uint32_t fast_duration_ms,
                                  uint32_t normal_interval_ms) override {
        // In tests, just use the fast interval (no actual timer)
        (void)fast_duration_ms; (void)normal_interval_ms;
        start_advertising(data, fast_interval_ms);
    }
    
private:
    DisconnectCallback disconnect_callback_;
};

// Mock Crypto (Minimal)
class MockCrypto : public platform::CryptoSRP {
public:
    // Crypto Base
    void sha512(std::span<const uint8_t> input, std::span<uint8_t, 64> output) override {
        (void)input;
        std::fill(output.begin(), output.end(), 0xAA);
    }

    void hkdf_sha512(std::span<const uint8_t> key, std::span<const uint8_t> salt, std::span<const uint8_t> info, std::span<uint8_t> output) override {
       (void)key; (void)salt; (void)info; (void)output;
    }

    void ed25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 64> private_key) override {
        (void)public_key; (void)private_key;
    }

    void ed25519_sign(std::span<const uint8_t, 64> private_key, std::span<const uint8_t> message, std::span<uint8_t, 64> signature) override {
        (void)private_key; (void)message; (void)signature;
    }

    bool ed25519_verify(std::span<const uint8_t, 32> public_key, std::span<const uint8_t> message, std::span<const uint8_t, 64> signature) override {
        (void)public_key; (void)message; (void)signature;
        return true;
    }

    void x25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 32> private_key) override {
        (void)public_key; (void)private_key;
    }

    void x25519_shared_secret(std::span<const uint8_t, 32> private_key, std::span<const uint8_t, 32> peer_public_key, std::span<uint8_t, 32> shared_secret) override {
        (void)private_key; (void)peer_public_key; (void)shared_secret;
    }

    bool chacha20_poly1305_encrypt_and_tag(std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce, std::span<const uint8_t> aad, std::span<const uint8_t> plaintext, std::span<uint8_t> ciphertext, std::span<uint8_t, 16> tag) override {
        (void)key; (void)nonce; (void)aad; (void)plaintext; (void)ciphertext; (void)tag;
        return true;
    }

    bool chacha20_poly1305_decrypt_and_verify(std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce, std::span<const uint8_t> aad, std::span<const uint8_t> ciphertext, std::span<const uint8_t, 16> tag, std::span<uint8_t> plaintext) override {
        (void)key; (void)nonce; (void)aad; (void)ciphertext; (void)tag; (void)plaintext;
        return true;
    }

    // CryptoSRP
    std::unique_ptr<platform::SRPSession> srp_new_verifier(std::string_view username, std::string_view password) override {
        (void)username; (void)password;
        return nullptr; 
    }

    std::array<uint8_t, 16> srp_get_salt(platform::SRPSession* session) override { (void)session; return {}; }
    std::vector<uint8_t> srp_get_public_key(platform::SRPSession* session) override { (void)session; return {}; }
    
    bool srp_set_client_public_key(platform::SRPSession* session, std::span<const uint8_t> client_public_key) override { 
        (void)session; (void)client_public_key; return true; 
    }
    
    bool srp_verify_client_proof(platform::SRPSession* session, std::span<const uint8_t> client_proof) override {
        (void)session; (void)client_proof; return true;
    }
    
    std::vector<uint8_t> srp_get_server_proof(platform::SRPSession* session) override { (void)session; return {}; }
    std::vector<uint8_t> srp_get_session_key(platform::SRPSession* session) override { (void)session; return {}; }
};

// Mock Storage
class MockStorage : public platform::Storage {
public:
     std::optional<std::vector<uint8_t>> get(std::string_view key) override { 
         (void)key;
         return std::nullopt; 
    }
     void set(std::string_view key, std::span<const uint8_t> value) override {
         (void)key; (void)value;
     }
     void remove(std::string_view key) override { (void)key; }
     bool has(std::string_view key) override { (void)key; return false; }
};

// Mock System
class MockSystem : public platform::System {
public:
    void log(LogLevel level, std::string_view message) override {
        (void)level;
        std::cout << "[LOG] " << message << std::endl;
    }
    uint64_t millis() override { return 0; }
    void random_bytes(std::span<uint8_t> buffer) override {
        std::fill(buffer.begin(), buffer.end(), 0x00);
    }
};

void run_advertising_test() {
    std::cout << "Running Advertising Test..." << std::endl;
    uint8_t status = 0x01; // Unpaired
    uint8_t device_id[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint16_t category = 5;
    uint16_t gsn = 1;
    uint8_t config_num = 1;
    uint8_t hash[] = {0xAA, 0xBB, 0xCC, 0xDD};

    auto adv = platform::Ble::Advertisement::create_hap(status, device_id, category, gsn, config_num, hash);

    ASSERT_TRUE(adv.manufacturer_data.size() >= 19); 
    ASSERT_EQ((int)adv.manufacturer_data[0], 0x06); // Type
    ASSERT_EQ((int)adv.manufacturer_data[1], 0x31); // STL: SubType 1, Length 17 (FIXED from 0x11)
    ASSERT_EQ((int)adv.manufacturer_data[2], status);
    
    // Verify Device ID
    for (int i = 0; i < 6; i++) {
        ASSERT_EQ((int)adv.manufacturer_data[3 + i], (int)device_id[i]);
    }
    
    // Verify ACID (little-endian)
    ASSERT_EQ((int)adv.manufacturer_data[9], category & 0xFF);
    ASSERT_EQ((int)adv.manufacturer_data[10], (category >> 8) & 0xFF);
    
    // Verify GSN (little-endian)
    ASSERT_EQ((int)adv.manufacturer_data[11], gsn & 0xFF);
    ASSERT_EQ((int)adv.manufacturer_data[12], (gsn >> 8) & 0xFF);
    
    // Verify CN (1 byte)
    ASSERT_EQ((int)adv.manufacturer_data[13], config_num);
    
    // Verify CV (Compatible Version = 0x02)
    ASSERT_EQ((int)adv.manufacturer_data[14], 0x02);
    
    // Verify Setup Hash (4 bytes)
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ((int)adv.manufacturer_data[15 + i], (int)hash[i]);
    }
    
    std::cout << "Advertising Test Passed." << std::endl;
}

void run_reassembly_test() {
    std::cout << "Running Reassembly Test..." << std::endl;
    MockBle ble;
    MockCrypto crypto;
    MockStorage storage;
    MockSystem system;
    core::AttributeDatabase db;
    PairingEndpoints::Config pe_config;
    pe_config.crypto = &crypto;
    pe_config.storage = &storage;
    pe_config.system = &system;
    pe_config.accessory_id = "11:22:33:44:55:66";
    pe_config.setup_code = "123-45-678";
    
    PairingEndpoints pairing_endpoints(pe_config);

    BleTransport::Config config;
    config.ble = &ble;
    config.crypto = &crypto;
    config.storage = &storage;
    config.system = &system;
    config.database = &db;
    config.pairing_endpoints = &pairing_endpoints;
    config.accessory_id = "11:22:33:44:55:66";
    
    BleTransport transport(config);
    transport.start();

    // Friend access helper (or expose send_response publicly for testing? 
    // It's private. But we can trigger it via process_characteristic_write if we mock PairingEndpoints response?
    // Hard to mock internal logic of PairingEndpoints without virtuals.
    // PairingEndpoints is concrete.
    
    // However, I declared `send_response` in `BleTransport.hpp` under private.
    // I can make it public for testing or friend the test class?
    // Or simpler: Inspect `BleTransport.cpp` again.
    // If I can't call `send_response`, I can test reassembly `handle_hap_write` easily.
    
    // Find target characteristic (Pair Setup 4C)
    // UUID: 0000004C-0000-1000-8000-0026BB765291
    const std::string kPairSetupUUID = "0000004C-0000-1000-8000-0026BB765291";
    platform::Ble::CharacteristicDefinition* char_def = nullptr;
    
    for (auto& svc : ble.registered_services) {
        for (auto& ch : svc.characteristics) {
            if (ch.uuid == kPairSetupUUID) {
                char_def = &ch;
                break;
            }
        }
        if (char_def) break;
    }
    
    ASSERT_TRUE(char_def != nullptr);
    ASSERT_TRUE(char_def->on_write != nullptr);

    // Test Reassembly
    uint16_t conn = 1;
    // P1: Header(7) + "Hello ". Len=11.
    // CF(00) Op(02) TID(01) IID(01 00) Len(0B 00) Body("Hello ")
    std::vector<uint8_t> p1 = {0x00, 0x02, 0x01, 0x01, 0x00, 0x0B, 0x00, 'H', 'e', 'l', 'l', 'o', ' '};
    
    // Call on_write directly
    char_def->on_write(conn, p1, false);
    
    // Check if response sent? No, incomplete PDU.
    ASSERT_TRUE(ble.sent_notifications.empty());
    
    // Packet 2: CF=1 | TID=1 | "World"
    std::vector<uint8_t> p2 = {0x80, 0x01, 'W', 'o', 'r', 'l', 'd'};
    
    char_def->on_write(conn, p2, false);
    
    // Now it should be complete (11 bytes).
    
    // Response should be ready for Read.
    // Verify Error Response via GATT Read
    auto read_characteristic = [&](const std::string& uuid, uint16_t conn_id) -> std::vector<uint8_t> {
        for (const auto& svc : ble.registered_services) {
             for (const auto& ch : svc.characteristics) {
                 if (ch.uuid == uuid && ch.on_read) {
                     return ch.on_read(conn_id);
                 }
             }
        }
        return {};
    };

    std::vector<uint8_t> response = read_characteristic(kPairSetupUUID, conn);
    
    // Header: CF(0x02) TID(1) Status(5) Len(0)...
    // CF should be 0x02 for Response type (not 0x00)
    ASSERT_TRUE(!response.empty());
    ASSERT_EQ((int)response[0], 0x02); // CF: Response type (FIXED from 0x00)
    ASSERT_EQ((int)response[1], 0x01); // TID matches
    ASSERT_EQ((int)response[2], 0x05); // Status: 0x05 (Not Found / Auth / Invalid PDU)
    
    std::cout << "Reassembly Test Passed." << std::endl;
}

void run_service_signature_test() {
    std::cout << "Running Characteristic Signature Test..." << std::endl;
    // Mock Environment
    MockBle ble; MockCrypto crypto; MockStorage storage; MockSystem system;
    core::AttributeDatabase db; 
    PairingEndpoints::Config pe_config;
    pe_config.crypto = &crypto;
    pe_config.storage = &storage;
    pe_config.system = &system;
    pe_config.accessory_id = "ID";
    pe_config.setup_code = "Code";
    PairingEndpoints pe(pe_config);
    
    BleTransport::Config config;
    config.ble = &ble;
    config.crypto = &crypto;
    config.database = &db;
    config.pairing_endpoints = &pe;
    config.system = &system;
    config.storage = &storage;
    
    config.accessory_id = "ID";
    config.device_name = "Dev";
    
    transport::BleTransport transport(config);
    transport.start();
    
    // Find Pair Setup Char
    const std::string kPairSetupUUID = "0000004C-0000-1000-8000-0026BB765291";
    platform::Ble::CharacteristicDefinition* char_def = nullptr;
    for (auto& svc : ble.registered_services) {
        for (auto& ch : svc.characteristics) {
             if (ch.uuid == kPairSetupUUID) { char_def = &ch; break; }
        }
    }
    ASSERT_TRUE(char_def != nullptr);

    // Send Opcode 1 (Char Sig Read) TID=2 IID=01A0 (A001)
    std::vector<uint8_t> pdu = {0x00, 0x01, 0x02, 0x01, 0xA0};
    char_def->on_write(1, pdu, false);

    // Expected response for Pair Setup (IID 40961)
    // Ctrl(0x02) TID(02) Status(00) Len(0035 - 53 bytes)
    // 04 10 915276BB2600008000100000 4C 00 00 00 (Type 4C)
    // 07 02 00A0 (Svc ID A000)
    // 06 10 915276BB2600008000100000 55 00 00 00 (Svc Type 55)
    // 0A 02 0200 (Properties: Write Only = 0x0002)
    // 0C 07 1B 00 0027 01 0000 (Format Data, Unit Unitless)
    
    std::vector<uint8_t> expected_response = {
        0x02, 0x02, 0x00, 0x35, 0x00, // CF=0x02 (Response), TID=0x02, Status=0x00, Len=0x0035
        0x04, 0x10, 0x91, 0x52, 0x76, 0xBB, 0x26, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x00,
        0x07, 0x02, 0x00, 0xA0,
        0x06, 0x10, 0x91, 0x52, 0x76, 0xBB, 0x26, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,
        0x0A, 0x02, 0x02, 0x00, // Properties: 0x0002
        0x0C, 0x07, 0x1B, 0x00, 0x00, 0x27, 0x01, 0x00, 0x00
    };

    // Helper to simulate a GATT Read on a characteristic
    auto read_characteristic = [&](const std::string& uuid, uint16_t conn_id) -> std::vector<uint8_t> {
        for (const auto& svc : ble.registered_services) {
             for (const auto& ch : svc.characteristics) {
                 if (ch.uuid == uuid && ch.on_read) {
                     return ch.on_read(conn_id);
                 }
             }
        }
        return {};
    };

    // 2. Read the response via GATT Read (Signature Read)
    // The response should be buffered and returned in on_read
    std::vector<uint8_t> received_response = read_characteristic("0000004C-0000-1000-8000-0026BB765291", 1);

    if (received_response.size() != expected_response.size()) {
        std::cout << "Size Mismatch! Expected " << expected_response.size() << " Got " << received_response.size() << std::endl;
        std::cout << "Received Hex: ";
        for (auto b : received_response) printf("%02X ", b);
        std::cout << std::endl;
    }
    
    ASSERT_EQ(received_response.size(), expected_response.size());
    for(size_t i=0; i<expected_response.size(); ++i) {
        ASSERT_EQ(received_response[i], expected_response[i]);
    }

    std::cout << "Characteristic Signature Test Passed." << std::endl;
}

void run_write_with_response_test() {
    MockBle ble;
    MockCrypto crypto;
    MockSystem system;
    MockStorage storage;
    
    // Setup Transport
    hap::transport::BleTransport::Config config;
    config.ble = &ble;
    config.crypto = &crypto;
    config.system = &system;
    config.storage = &storage;
    
    // Mock Database
    core::AttributeDatabase db; 
    config.database = &db;

    // Use stack allocated endpoints to match pointer type in Config
    PairingEndpoints::Config pe_config;
    // ... populate pe_config if needed (mocks handle it)
    PairingEndpoints endpoints(pe_config); 
    config.pairing_endpoints = &endpoints;

    BleTransport transport(config);
    
    // Simulate Setup
    transport.start(); // calls register_accessory_services + update_advertising
    
    // Find the Pair Setup characteristic definition to call on_write
    platform::Ble::CharacteristicDefinition* pair_setup_def = nullptr;
    for (auto& svc : ble.registered_services) {
        for (auto& ch : svc.characteristics) {
            if (ch.uuid == "0000004C-0000-1000-8000-0026BB765291") {
                pair_setup_def = const_cast<platform::Ble::CharacteristicDefinition*>(&ch);
                break;
            }
        }
    }
    ASSERT_TRUE(pair_setup_def != nullptr);

    std::cout << "Running Write-with-Response Test..." << std::endl;

    // 1. Send Write Request (Pair Setup)
    // HAP-PDU: Control(00) | Opcode(02 Write) | TID(03) | ID(0xA001) | Len(...) | Body...
    // Body: TLV 0x01 (Value) + TLV 0x09 (Return-Response)
    
    // TLV 0x01: Value = 0xAA (Dummy Request)
    // TLV 0x09: Length 0
    std::vector<uint8_t> hap_tlvs = {
        0x01, 0x01, 0xAA,
        0x09, 0x00
    };
    
    std::vector<uint8_t> pdu_body = hap_tlvs;
    
    // Call on_write directly (simulating BLE stack)
    // Transaction ID = 3
    // BLE Transport process_characteristic_write expects just the BODY (it unwraps header? No, process_characteristic_write handles BODY logic)
    // Wait, process_characteristic_write(conn, tid, uuid, body)
    // on_write(conn, data, response)
    // BLE Stack passes the RAW GATT payload.
    // BleTransport::handle_hap_write expects HAP PDU from the beginning?
    // Let's check handle_hap_write (which is called by on_write).
    // handle_hap_write expects Header?
    // "Request Header start: Control Field (1) | Opcode (1) | TID (1) | Param ID (2)"
    // So `on_write` receives the FULL HAP PDU.
    
    std::vector<uint8_t> full_pdu;
    full_pdu.push_back(0x00); // CF
    full_pdu.push_back(0x02); // Opcode Write
    full_pdu.push_back(0x03); // TID
    full_pdu.push_back(0x01); // IID L
    full_pdu.push_back(0xA0); // IID H
    
    uint16_t len = pdu_body.size();
    full_pdu.push_back(len & 0xFF);
    full_pdu.push_back((len >> 8) & 0xFF);
    full_pdu.insert(full_pdu.end(), pdu_body.begin(), pdu_body.end());
    
    pair_setup_def->on_write(1, full_pdu, false);
    
    // 2. Read Response via GATT Read
    // Helper to simulate a GATT Read on a characteristic
    auto read_characteristic = [&](const std::string& uuid, uint16_t conn_id) -> std::vector<uint8_t> {
        for (const auto& svc : ble.registered_services) {
             for (const auto& ch : svc.characteristics) {
                 if (ch.uuid == uuid && ch.on_read) {
                     return ch.on_read(conn_id);
                 }
             }
        }
        return {};
    };

    std::vector<uint8_t> response = read_characteristic("0000004C-0000-1000-8000-0026BB765291", 1);
    
    // Expected Response:
    // Header: CF(0x02) | TID(03) | Status(00) | Len(...)
    // Body: TLV 0x01 (Value) -> 0xBB (Mock Response)
    
    ASSERT_TRUE(!response.empty());
    ASSERT_EQ(response[0], 0x02); // CF: Response type (FIXED from 0x00)
    ASSERT_EQ(response[1], 0x03); // TID
    ASSERT_EQ(response[2], 0x00); // Status
    
    // Parse Payload TLVs
    // Offset 3+2=5.
    ASSERT_TRUE(response.size() > 5);
    std::vector<uint8_t> resp_body(response.begin() + 5, response.end());
    
    // Verify TLV 0x01 wraps 0xBB
    // 01 01 BB
    ASSERT_EQ(resp_body[0], 0x01);
    ASSERT_EQ(resp_body[1], 0x01);
    ASSERT_EQ(resp_body[2], 0xBB);
    
    std::cout << "Write-with-Response Test Passed." << std::endl;
}

int main() {
    run_advertising_test();
    run_reassembly_test();
    run_service_signature_test();
    run_write_with_response_test();
    return 0;
}
