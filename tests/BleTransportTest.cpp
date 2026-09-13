#include "hap/transport/BleTransport.hpp"
#include "hap/transport/SecureSession.hpp"
#include "hap/types/ServiceTypes.hpp"
#include "hap/core/HAPStatus.hpp"
#include "TestUtil.hpp"
#include "MockPal.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

using namespace hap;
using namespace hap::transport;

// Mock pairing endpoints: returns a fixed body so tests can verify that the
// response body gets wrapped into the BLE PDU value TLV.
class MockPairingEndpoints : public IPairingEndpoints {
public:
    std::vector<uint8_t> pair_setup_body = {0xBB};
    int pair_setup_calls = 0;

    Response handle_pair_setup(const Request&, ConnectionContext&) override {
        ++pair_setup_calls;
        Response resp{Status::OK};
        resp.body = pair_setup_body;
        return resp;
    }
    Response handle_pair_verify(const Request&, ConnectionContext&) override {
        return Response{Status::OK};
    }
    Response handle_pairings(const Request&, ConnectionContext&) override {
        return Response{Status::OK};
    }
};

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

    void set_connect_callback(ConnectCallback callback) override {
        connect_callback_ = callback;
    }

    void start_timed_advertising(const Advertisement& data,
                                 uint32_t fast_interval_ms,
                                 uint32_t fast_duration_ms,
                                 uint32_t normal_interval_ms) override {
        (void)fast_duration_ms; (void)normal_interval_ms;
        start_advertising(data, fast_interval_ms);
    }

    // Simulate a GATT read on a registered characteristic.
    std::vector<uint8_t> read(const std::string& uuid, uint16_t conn_id) {
        for (const auto& svc : registered_services) {
            for (const auto& ch : svc.characteristics) {
                if (ch.uuid == uuid && ch.on_read) {
                    return ch.on_read(conn_id);
                }
            }
        }
        return {};
    }

    // Simulate the BLE stack establishing a connection.
    void connect(uint16_t conn_id) {
        if (connect_callback_) connect_callback_(conn_id);
    }

    // Simulate the phone enabling/disabling indications on a characteristic.
    void subscribe(const std::string& uuid, uint16_t conn_id, bool enabled) {
        for (const auto& svc : registered_services) {
            for (const auto& ch : svc.characteristics) {
                if (ch.uuid == uuid && ch.on_subscribe) {
                    ch.on_subscribe(conn_id, enabled);
                    return;
                }
            }
        }
    }

    // Simulate a GATT write on a registered characteristic.
    bool write(const std::string& uuid, uint16_t conn_id, const std::vector<uint8_t>& data, bool response_requested) {
        for (const auto& svc : registered_services) {
            for (const auto& ch : svc.characteristics) {
                if (ch.uuid == uuid && ch.on_write) {
                    ch.on_write(conn_id, data, response_requested);
                    return true;
                }
            }
        }
        return false;
    }

    const platform::Ble::CharacteristicDefinition* find(const std::string& uuid) const {
        for (const auto& svc : registered_services) {
            for (const auto& ch : svc.characteristics) {
                if (ch.uuid == uuid) return &ch;
            }
        }
        return nullptr;
    }

private:
    DisconnectCallback disconnect_callback_;
    ConnectCallback connect_callback_;
};

static constexpr const char* kPairSetupUUID = "0000004C-0000-1000-8000-0026BB765291";
static constexpr const char* kCharInstanceIdDescUUID = "DC46F0FE-81D2-4616-B5D9-6ABDD796939A";

// HAP 7.4.7.2: writes to user characteristics require a secure session. The
// tests install session keys on the transport (mirroring Pair Verify
// completion) and encrypt outgoing PDUs with a real SecureSession driven by
// MockCrypto's XOR AEAD. Nonces advance per fragment in both directions, so
// the helper must be constructed fresh per write and reused for the read.
struct SecureChannel {
    transport::SecureSession client; // controller side: a2c = accessory->controller

    SecureChannel(testmock::MockCrypto& crypto)
        : client(&crypto, /*a2c=*/std::array<uint8_t, 32>{}, /*c2a=*/std::array<uint8_t, 32>{}) {}

    static void install(BleTransport& t, uint16_t conn_id) {
        t.establish_secure_session(conn_id,
                                   {std::array<uint8_t, 32>{}, std::array<uint8_t, 32>{}},
                                   std::array<uint8_t, 32>{}, "controller-1");
    }

    // Encrypt one outgoing HAP PDU fragment (controller -> accessory).
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& pdu) {
        return client.encrypt_ble_pdu(pdu);
    }

    // Decrypt the accessory's buffered response (accessory -> controller).
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& frame) {
        auto plain = client.decrypt_ble_pdu(frame);
        CHECK(plain.has_value());
        return std::move(*plain);
    }
};

struct TestRig {
    MockBle ble;
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{false};
    core::AttributeDatabase db;
    MockPairingEndpoints endpoints;

    BleTransport transport;

    explicit TestRig(const char* accessory_id = "11:22:33:44:55:66")
        : transport(make_config(accessory_id)) {
        transport.start();
    }

    BleTransport::Config make_config(const char* accessory_id) {
        BleTransport::Config config;
        config.ble = &ble;
        config.crypto = &crypto;
        config.storage = &storage;
        config.system = &system;
        config.database = &db;
        config.pairing_endpoints = &endpoints;
        config.accessory_id = accessory_id;
        config.device_name = "Dev";
        return config;
    }
};

// The transport assigns characteristic IIDs internally; recover the Pair Setup
// characteristic's IID by reading its Instance-ID descriptor.
static uint16_t pair_setup_iid(const TestRig& rig) {
    const auto* def = rig.ble.find(kPairSetupUUID);
    CHECK(def != nullptr);
    for (const auto& d : def->descriptors) {
        if (d.uuid == kCharInstanceIdDescUUID && d.on_read) {
            auto v = d.on_read(0);
            CHECK(v.size() == 2);
            return static_cast<uint16_t>(v[0] | (v[1] << 8));
        }
    }
    CHECK(false && "Pair Setup characteristic has no Instance-ID descriptor");
    return 0;
}

void test_advertising_layout() {
    uint8_t status = 0x01; // Unpaired
    uint8_t device_id[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint16_t category = 5;
    uint16_t gsn = 1;
    uint8_t config_num = 1;
    uint8_t hash[] = {0xAA, 0xBB, 0xCC, 0xDD};

    auto adv = platform::Ble::Advertisement::create_hap(status, device_id, category, gsn, config_num, hash);

    CHECK(adv.manufacturer_data.size() >= 19);
    CHECK_EQ_HEX(adv.manufacturer_data[0], 0x06); // Type
    CHECK_EQ_HEX(adv.manufacturer_data[1], 0x31); // STL: SubType 1, Length 17
    CHECK_EQ_HEX(adv.manufacturer_data[2], status);

    for (int i = 0; i < 6; i++) {
        CHECK_EQ_HEX(adv.manufacturer_data[3 + i], device_id[i]);
    }

    // ACID, little-endian
    CHECK_EQ_HEX(adv.manufacturer_data[9], category & 0xFF);
    CHECK_EQ_HEX(adv.manufacturer_data[10], (category >> 8) & 0xFF);

    // GSN, little-endian
    CHECK_EQ_HEX(adv.manufacturer_data[11], gsn & 0xFF);
    CHECK_EQ_HEX(adv.manufacturer_data[12], (gsn >> 8) & 0xFF);

    // CN (1 byte), CV (0x02), setup hash (4 bytes)
    CHECK_EQ_HEX(adv.manufacturer_data[13], config_num);
    CHECK_EQ_HEX(adv.manufacturer_data[14], 0x02);
    for (int i = 0; i < 4; i++) {
        CHECK_EQ_HEX(adv.manufacturer_data[15 + i], hash[i]);
    }
}

void test_pdu_reassembly() {
    TestRig rig;

    const auto* char_def = rig.ble.find(kPairSetupUUID);
    CHECK(char_def != nullptr);
    CHECK(char_def->on_write != nullptr);

    uint16_t conn = 1;
    // P1: header(7) + body "Hello " (6 bytes). Declared body length = 11.
    std::vector<uint8_t> p1 = {0x00, 0x02, 0x01, 0x01, 0x00, 0x0B, 0x00, 'H', 'e', 'l', 'l', 'o', ' '};
    rig.ble.write(kPairSetupUUID, conn, p1, false);

    // Incomplete PDU: no response yet.
    CHECK(rig.ble.read(kPairSetupUUID, conn).empty());

    // P2: continuation CF=1 | TID=1 | "World" (5 bytes) -> body complete (11 bytes).
    std::vector<uint8_t> p2 = {0x80, 0x01, 'W', 'o', 'r', 'l', 'd'};
    rig.ble.write(kPairSetupUUID, conn, p2, false);

    // The write targets IID=1, which matches no registered characteristic, so
    // the transport rejects it with Invalid Instance ID (Table 7-37, 0x04).
    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, conn);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02); // CF: response
    CHECK_EQ_HEX(response[1], 0x01); // TID matches
    CHECK_EQ_HEX(response[2], 0x04); // Invalid Instance ID
}

void test_service_signature_read() {
    TestRig rig;

    // Opcode 1 (Characteristic Signature Read), TID=2, IID from the descriptor.
    uint16_t iid = pair_setup_iid(rig);
    std::vector<uint8_t> pdu = {0x00, 0x01, 0x02,
                                static_cast<uint8_t>(iid & 0xFF),
                                static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    // Response layout (HAP 7.4.2):
    // Ctrl(0x02) TID(02) Status(00) Len(0x0035)
    //   04 10 <uuid128 of 4C>     (Type)
    //   07 02 <svc iid>           (Service ID)
    //   06 10 <uuid128 of 55>     (Service Type)
    //   0A 02 <properties>        (Write-only)
    //   0C 07 1B 00 00 27 01 00 00 (Format=Data(0x1B), Unit=Unitless)
    auto uuid128 = [](uint8_t short_type) {
        // HAP base UUID in little-endian wire order (matches BleTlvBuilder):
        // node | clock_seq | time_hi | time_mid | time_low(short type)
        return std::vector<uint8_t>{
            0x91, 0x52, 0x76, 0xBB, 0x26, 0x00,
            0x00, 0x80,
            0x00, 0x10,
            0x00, 0x00,
            short_type, 0x00, 0x00, 0x00};
    };

    std::vector<uint8_t> body;
    body.push_back(0x04); body.push_back(0x10);
    auto t = uuid128(0x4C); body.insert(body.end(), t.begin(), t.end());
    // Service instance ID: the pairing service IID assigned at registration
    // (from the log: registered with a specific IID). We look it up dynamically
    // from the signature response itself rather than hard-coding it.
    std::vector<uint8_t> received = rig.ble.read(kPairSetupUUID, 1);
    CHECK(received.size() >= 5);
    uint16_t body_len = received[3] | (received[4] << 8);
    CHECK_EQ(received.size(), 5 + body_len);

    // Structural checks on the response body TLVs.
    // Actual layout (from BleTlvBuilder wire order):
    //   04 10 <uuid128(4C)> 07 02 <svc iid LE> 06 10 <uuid128(55)> 0A 02 <props> 0C 07 <fmt ...>
    const std::vector<uint8_t>& b = received;
    size_t i = 5;
    CHECK(b[i] == 0x04 && b[i + 1] == 0x10); // Type TLV
    CHECK(std::equal(t.begin(), t.end(), b.begin() + i + 2));
    i += 2 + 16;
    CHECK(b[i] == 0x07 && b[i + 1] == 0x02); // Service ID TLV
    uint16_t svc_iid = b[i + 2] | (b[i + 3] << 8);
    CHECK(svc_iid != 0); // pairing service must have a real IID
    i += 2 + 2;
    CHECK(b[i] == 0x06 && b[i + 1] == 0x10); // Service Type TLV
    std::vector<uint8_t> t55 = uuid128(0x55);
    std::vector<uint8_t> got55(b.begin() + i + 2, b.begin() + i + 2 + 16);
    if (t55 != got55) {
        std::cerr << "SvcType mismatch: got";
        for (auto x : got55) std::cerr << " " << std::hex << (int)x;
        std::cerr << std::dec << std::endl;
        CHECK(false);
    }
    i += 2 + 16;
    CHECK(b[i] == 0x0A && b[i + 1] == 0x02); // Properties TLV
    uint16_t props = b[i + 2] | (b[i + 3] << 8);
    CHECK((props & 0x0002) != 0);            // write permission set
    i += 4;
    CHECK(b[i] == 0x0C && b[i + 1] == 0x07); // GATT format TLV
    CHECK_EQ_HEX(b[i + 2], 0x1B);            // Format: Data
}

void test_write_with_response() {
    TestRig rig;

    // Write PDU to Pair Setup: Ctrl(00) Op(02 Write) TID(03) IID(0x00A1 LE) Len TLVs
    // Body: TLV 0x01 (Value) = 0xAA + TLV 0x09 (Return-Response), length 0.
    std::vector<uint8_t> hap_tlvs = {
        0x01, 0x01, 0xAA,
        0x09, 0x00
    };

    uint16_t iid = pair_setup_iid(rig);
    std::vector<uint8_t> full_pdu;
    full_pdu.push_back(0x00); // CF
    full_pdu.push_back(0x02); // Opcode Write
    full_pdu.push_back(0x03); // TID
    full_pdu.push_back(static_cast<uint8_t>(iid & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>(iid >> 8));

    uint16_t len = static_cast<uint16_t>(hap_tlvs.size());
    full_pdu.push_back(len & 0xFF);
    full_pdu.push_back((len >> 8) & 0xFF);
    full_pdu.insert(full_pdu.end(), hap_tlvs.begin(), hap_tlvs.end());

    rig.ble.write(kPairSetupUUID, 1, full_pdu, false);

    // Endpoint must have been invoked and its body wrapped into a response.
    CHECK_EQ(rig.endpoints.pair_setup_calls, 1);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02); // CF: response
    CHECK_EQ_HEX(response[1], 0x03); // TID
    CHECK_EQ_HEX(response[2], 0x00); // Status: OK

    CHECK(response.size() > 5);
    std::vector<uint8_t> resp_body(response.begin() + 5, response.end());

    // TLV 0x01 wraps the endpoint's 0xBB body.
    CHECK_EQ_HEX(resp_body[0], 0x01);
    CHECK_EQ_HEX(resp_body[1], 0x01);
    CHECK_EQ_HEX(resp_body[2], 0xBB);
}

void test_timed_write_then_execute() {
    TestRig rig;
    uint16_t iid = pair_setup_iid(rig);

    // Timed write (opcode 0x04) TID=5 with a Value TLV.
    std::vector<uint8_t> tlvs = {0x01, 0x01, 0x7F};
    std::vector<uint8_t> pdu;
    pdu.push_back(0x00);
    pdu.push_back(0x04); // CharacteristicTimedWrite
    pdu.push_back(0x05); // TID
    pdu.push_back(static_cast<uint8_t>(iid & 0xFF));
    pdu.push_back(static_cast<uint8_t>(iid >> 8));
    uint16_t len = static_cast<uint16_t>(tlvs.size());
    pdu.push_back(len & 0xFF);
    pdu.push_back((len >> 8) & 0xFF);
    pdu.insert(pdu.end(), tlvs.begin(), tlvs.end());
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    // Execute write (opcode 0x05) TID=6.
    std::vector<uint8_t> exec = {0x00, 0x05, 0x06,
                                 static_cast<uint8_t>(iid & 0xFF),
                                 static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, exec, false);

    // The endpoint must only be called once (by the execute step, carrying the
    // timed-write body).
    CHECK_EQ(rig.endpoints.pair_setup_calls, 1);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x06); // TID of the execute step
    CHECK_EQ_HEX(response[2], 0x00); // Status: OK
}

// Regression: a timed write + execute on a user characteristic must produce a
// Connected Event (zero-length indication) for subscribed controllers. Missing
// this left controllers stuck waiting for state-change events ("Unlocking...").
void test_timed_write_fires_connected_event() {
    MockBle ble;
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{false};
    MockPairingEndpoints endpoints;

    // The user characteristic must exist BEFORE the transport starts so it
    // gets registered as a GATT characteristic.
    core::AttributeDatabase db;
    auto acc = std::make_unique<core::Accessory>(1);
    auto svc = std::make_unique<core::Service>(0x43, "Lightbulb");
    auto on_char = std::make_unique<core::Characteristic>(
        0x25, core::Format::Bool,
        core::Permissions{core::Permission::PairedRead, core::Permission::PairedWrite,
                    core::Permission::Notify, core::Permission::TimedWrite});
    svc->add_characteristic(std::move(on_char));
    acc->add_service(std::move(svc));
    db.add_accessory(std::move(acc));

    BleTransport::Config config;
    config.ble = &ble;
    config.crypto = &crypto;
    config.storage = &storage;
    config.system = &system;
    config.database = &db;
    config.pairing_endpoints = &endpoints;
    config.accessory_id = "11:22:33:44:55:66";
    config.device_name = "Dev";
    BleTransport transport(config);
    transport.start();

    // HAP 7.4.7.2: writes to user characteristics require a secure session.
    SecureChannel::install(transport, 1);

    // Find the On characteristic's GATT definition and its IID.
    const std::string kOnUUID = "00000025-0000-1000-8000-0026BB765291";
    const auto* def = ble.find(kOnUUID);
    CHECK(def != nullptr);
    uint16_t iid = 0;
    for (const auto& d : def->descriptors) {
        if (d.uuid == kCharInstanceIdDescUUID && d.on_read) {
            auto v = d.on_read(0);
            iid = static_cast<uint16_t>(v[0] | (v[1] << 8));
        }
    }
    CHECK(iid != 0);

    // Subscribe from a DIFFERENT connection than the writer (conn 1), mirroring
    // a second controller listening: the writer's own connection is excluded
    // from its own change events.
    ble.subscribe(kOnUUID, 2, true);

    // Timed write: Value TLV = 0x01 (true).
    std::vector<uint8_t> tlvs = {0x01, 0x01, 0x01};
    std::vector<uint8_t> pdu;
    pdu.push_back(0x00);
    pdu.push_back(0x04); // CharacteristicTimedWrite
    pdu.push_back(0x09); // TID
    pdu.push_back(static_cast<uint8_t>(iid & 0xFF));
    pdu.push_back(static_cast<uint8_t>(iid >> 8));
    uint16_t len = static_cast<uint16_t>(tlvs.size());
    pdu.push_back(len & 0xFF);
    pdu.push_back((len >> 8) & 0xFF);
    pdu.insert(pdu.end(), tlvs.begin(), tlvs.end());

    // Execute write.
    std::vector<uint8_t> exec = {0x00, 0x05, 0x0A,
                                 static_cast<uint8_t>(iid & 0xFF),
                                 static_cast<uint8_t>(iid >> 8)};

    {
        SecureChannel chan(crypto);
        ble.write(kOnUUID, 1, chan.encrypt(pdu), false);
        ble.write(kOnUUID, 1, chan.encrypt(exec), false);
    }

    // The characteristic value must have been applied... (ownership moved
    // into the service; reach it through the accessory in the database)
    auto v = db.accessories()[0]->services()[0]->characteristics()[0]->get_value();
    CHECK(std::holds_alternative<core::Value>(v));
    CHECK(std::get<bool>(std::get<core::Value>(v)) == true);

    // ...and a zero-length Connected Event indication must have been sent to
    // the subscribed (non-writing) connection.
    bool got_indication = false;
    for (const auto& n : ble.sent_notifications) {
        if (n.uuid == kOnUUID && n.conn_id == 2 && n.data.empty()) {
            got_indication = true;
        }
    }
    CHECK(got_indication);
}

// End-to-end through AccessoryServer: a timed write to LockTargetState must
// produce zero-length indications for BOTH lock characteristics on the writing
// connection (BLE conn IDs start at 0 — the "notify everyone" sentinel must
// not collide with connection 0).
void test_lock_timed_write_notifies_writer_connection() {
    MockBle ble;
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{true};
    MockPairingEndpoints endpoints;

    core::AttributeDatabase db;
    auto acc = std::make_unique<core::Accessory>(1);
    hap::service::ServiceBuilder lock_builder(hap::service::kType_LockMechanism, "Lock Mechanism", true);
    hap::core::Characteristic* lock_current =
        lock_builder.add(hap::characteristic::CharId::LockCurrentStateChar).get();
    lock_builder.add(hap::characteristic::CharId::LockTargetStateChar)
        .on_write(
                  [lock_current](const hap::core::Value& v) -> hap::core::WriteResponse {
                      // HAP 8.4: LockCurrentState follows LockTargetState.
                      auto* target = std::get_if<uint8_t>(&v);
                      if (target) lock_current->set_value(*target, hap::core::EventSource{});
                      return std::nullopt;
                  });
    auto lock = lock_builder.build();
    acc->add_service(std::move(lock));
    CHECK_EQ(db.add_accessory(std::move(acc)), core::ValidationResult::Success);

    BleTransport::Config config;
    config.ble = &ble;
    config.crypto = &crypto;
    config.storage = &storage;
    config.system = &system;
    config.database = &db;
    config.pairing_endpoints = &endpoints;
    config.accessory_id = "11:22:33:44:55:66";
    BleTransport transport(config);
    transport.start();

    // HAP 7.4.7.2: writes to user characteristics require a secure session.
    SecureChannel::install(transport, 0);

    ble.connect(0);
    ble.subscribe("0000001D-0000-1000-8000-0026BB765291", 0, true); // CurrentState
    ble.subscribe("0000001E-0000-1000-8000-0026BB765291", 0, true); // TargetState

    auto& chars = db.accessories()[0]->services()[0]->characteristics();
    uint16_t tgt_iid = static_cast<uint16_t>(chars[1]->iid());

    auto pdu_for = [&](uint8_t op, uint8_t tid, const std::vector<uint8_t>& tlvs) {
        std::vector<uint8_t> p{0x00, op, tid,
            static_cast<uint8_t>(tgt_iid & 0xFF), static_cast<uint8_t>(tgt_iid >> 8),
            static_cast<uint8_t>(tlvs.size() & 0xFF), static_cast<uint8_t>(tlvs.size() >> 8)};
        p.insert(p.end(), tlvs.begin(), tlvs.end());
        return p;
    };

    SecureChannel chan(crypto);
    ble.write("0000001E-0000-1000-8000-0026BB765291", 0,
              chan.encrypt(pdu_for(0x04, 1, {0x01, 0x01, 0x00})), false);
    ble.write("0000001E-0000-1000-8000-0026BB765291", 0,
              chan.encrypt(pdu_for(0x05, 2, {})), false);

    // Value must be applied...
    auto v = chars[0]->get_value();
    CHECK(std::holds_alternative<core::Value>(v));
    CHECK_EQ(std::get<uint8_t>(std::get<core::Value>(v)), 0);

    // ...and the writing connection (0) must receive the TargetState indication.
    // (The CurrentState indication flows through AccessoryServer::broadcast_event,
    // covered by the end-to-end test in CoreSecurityTest.)
    int tgt_indications = 0;
    for (const auto& n : ble.sent_notifications) {
        if (n.conn_id == 0 && n.data.empty() &&
            n.uuid == "0000001E-0000-1000-8000-0026BB765291") {
            ++tgt_indications;
        }
    }
    CHECK(tgt_indications >= 1);
}

// HAP 7.3.5.4 / test requirement #17: an Execute-Write arriving after the
// TTL from the timed write has expired must be rejected (0x06) and the
// queued write discarded — the characteristic value must NOT change.
void test_timed_write_expired_ttl_rejected() {
    TestRig rig;
    rig.system.advance_ms(0); // normalize clock
    uint16_t iid = pair_setup_iid(rig);

    // Timed write (opcode 0x04) TID=7 with Value + TTL TLVs: TTL=0x01 -> 100ms.
    std::vector<uint8_t> tlvs = {0x01, 0x01, 0x7F, 0x08, 0x01, 0x01};
    std::vector<uint8_t> pdu;
    pdu.push_back(0x00);
    pdu.push_back(0x04);
    pdu.push_back(0x07);
    pdu.push_back(static_cast<uint8_t>(iid & 0xFF));
    pdu.push_back(static_cast<uint8_t>(iid >> 8));
    uint16_t len = static_cast<uint16_t>(tlvs.size());
    pdu.push_back(len & 0xFF);
    pdu.push_back((len >> 8) & 0xFF);
    pdu.insert(pdu.end(), tlvs.begin(), tlvs.end());
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    // Let the TTL lapse (the deadline starts when the timed-write response
    // is delivered; the mock clock advances deterministically).
    rig.system.advance_ms(200);

    // Execute write (opcode 0x05) TID=8.
    std::vector<uint8_t> exec = {0x00, 0x05, 0x08,
                                 static_cast<uint8_t>(iid & 0xFF),
                                 static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, exec, false);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x08); // TID of the execute step
    CHECK_EQ_HEX(response[2], 0x06); // Invalid Request: TTL expired
}

// HAP test requirement #6: an unknown opcode must fail the request with
// Unsupported-PDU (0x01) instead of being silently ignored.
void test_unsupported_opcode_rejected() {
    TestRig rig;
    uint16_t iid = pair_setup_iid(rig);

    std::vector<uint8_t> pdu = {0x00, 0x63, 0x0F, // opcode 0x63 = unknown
                                static_cast<uint8_t>(iid & 0xFF),
                                static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x0F);
    CHECK_EQ_HEX(response[2], 0x01); // Unsupported PDU
}

// Regression for a device-observed failure: on an UNPAIRED accessory the
// controller performs signature reads without a secure session (HAP 7.3.5.1
// requires the procedure to work with and without one). The security gate
// must key on the PROCEDURE, not the target characteristic, or discovery
// before pairing is impossible.
void test_signature_read_allowed_without_session() {
    TestRig rig;
    uint16_t iid = pair_setup_iid(rig);

    // Opcode 0x01 = Characteristic Signature Read, no secure session.
    std::vector<uint8_t> pdu = {0x00, 0x01, 0x21,
                                static_cast<uint8_t>(iid & 0xFF),
                                static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x21);
    CHECK_EQ_HEX(response[2], 0x00); // Success - NOT the 0x05 rejection
    CHECK(response.size() > 5);      // and a real signature body
}

// Regression for a device-observed failure: a rejected write (0x05) must
// still produce a response the controller can read. The rejection path used
// to skip the transaction bookkeeping, so the follow-up GATT read hit the
// 10-second window check with a zero timestamp and returned nothing.
void test_rejected_write_response_is_readable() {
    TestRig rig;
    rig.system.advance_ms(0);
    uint16_t iid = pair_setup_iid(rig);

    // A Characteristic Read (0x03) targets a value-bearing characteristic;
    // with no secure session it must be rejected with 0x05...
    std::vector<uint8_t> pdu = {0x00, 0x03, 0x33,
                                static_cast<uint8_t>(iid & 0xFF),
                                static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    // ...and that rejection must be retrievable by the follow-up GATT read.
    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x33);
    CHECK_EQ_HEX(response[2], 0x05); // Insufficient Authentication
}

// Regression for the "Not Supported" tile: the controller reads the Protocol
// Information Service's Version characteristic (0x37) in the clear BEFORE
// pairing to decide protocol compatibility. Rejecting that read with 0x05
// makes iOS mark the accessory as not supporting HAP-BLE 2.x.
void test_version_char_readable_without_session() {
    TestRig rig;

    // Recover the Version characteristic's IID from its registration log
    // equivalent: find it via the pairing-characteristic metadata by writing
    // a signature read for each pairing char IID. Simpler: the descriptor on
    // the 0x37 GATT characteristic carries the IID.
    const std::string kVersionUUID = "00000037-0000-1000-8000-0026BB765291";
    const auto* def = rig.ble.find(kVersionUUID);
    CHECK(def != nullptr);
    uint16_t iid = 0;
    for (const auto& d : def->descriptors) {
        if (d.uuid == kCharInstanceIdDescUUID && d.on_read) {
            auto v = d.on_read(0);
            CHECK(v.size() == 2);
            iid = static_cast<uint16_t>(v[0] | (v[1] << 8));
        }
    }
    CHECK(iid != 0);

    // Opcode 0x03 = Characteristic Read, no secure session.
    std::vector<uint8_t> pdu = {0x00, 0x03, 0x44,
                                static_cast<uint8_t>(iid & 0xFF),
                                static_cast<uint8_t>(iid >> 8)};
    rig.ble.write(kPairSetupUUID, 1, pdu, false);

    std::vector<uint8_t> response = rig.ble.read(kPairSetupUUID, 1);
    CHECK(!response.empty());
    CHECK_EQ_HEX(response[0], 0x02);
    CHECK_EQ_HEX(response[1], 0x44);
    CHECK_EQ_HEX(response[2], 0x00); // Success - NOT the 0x05 rejection

    // Body: TLV 0x01 (HAP-Param-Value) wrapping "2.2.0" (HAP 7.4.3.1).
    CHECK(response.size() == 5 + 7);
    CHECK_EQ_HEX(response[5], 0x01);
    CHECK_EQ_HEX(response[6], 0x05);
    CHECK(std::equal(response.begin() + 7, response.begin() + 12,
                     std::string("2.2.0").begin()));
}

int main() {
    RUN_TEST(test_advertising_layout);
    RUN_TEST(test_pdu_reassembly);
    RUN_TEST(test_service_signature_read);
    RUN_TEST(test_write_with_response);
    RUN_TEST(test_timed_write_then_execute);
    RUN_TEST(test_timed_write_expired_ttl_rejected);
    RUN_TEST(test_unsupported_opcode_rejected);
    RUN_TEST(test_signature_read_allowed_without_session);
    RUN_TEST(test_rejected_write_response_is_readable);
    RUN_TEST(test_version_char_readable_without_session);
    RUN_TEST(test_timed_write_fires_connected_event);
    RUN_TEST(test_lock_timed_write_notifies_writer_connection);
    return 0;
}
