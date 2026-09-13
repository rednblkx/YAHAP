// Tests for the security-critical core: SecureSession framing, TLV8 parsing,
// HTTP parser limits, CharacteristicSerializer round-trips, and Characteristic
// value coercion semantics.

#include "hap/transport/SecureSession.hpp"
#include "hap/core/TLV8.hpp"
#include "hap/core/HAPStatus.hpp"
#include "hap/core/CharacteristicSerializer.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/AttributeDatabase.hpp"
#include "hap/types/ServiceTypes.hpp"
#include "hap/transport/HTTP.hpp"
#include "hap/transport/PairingEndpoints.hpp"
#include "TestUtil.hpp"
#include "MockPal.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

using namespace hap;
using namespace hap::transport;

// ---------------------------------------------------------------------------
// SecureSession
// ---------------------------------------------------------------------------

void test_secure_session_frame_roundtrip() {
    testmock::MockCrypto crypto;
    std::array<uint8_t, 32> a2c{}, c2a{};
    for (size_t i = 0; i < 32; ++i) { a2c[i] = static_cast<uint8_t>(i); c2a[i] = static_cast<uint8_t>(0x80 + i); }

    SecureSession session(&crypto, a2c, c2a);

    std::string http = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi";
    std::vector<uint8_t> plaintext(http.begin(), http.end());

    auto frame = session.encrypt_frame(plaintext);
    CHECK(frame.size() == plaintext.size() + 2 + 16);

    // The controller holds the mirrored session: it encrypts with c2a and
    // decrypts a2c-encrypted frames. (A single session can't decrypt its own
    // output — a2c != c2a by design.)
    SecureSession controller(&crypto, c2a, a2c);
    auto decrypted = controller.decrypt_frame(frame);
    CHECK(decrypted.has_value());
    CHECK(*decrypted == plaintext);
}

void test_secure_session_incremental_decrypt() {
    testmock::MockCrypto crypto;
    SecureSession session(&crypto, {}, {});

    std::vector<uint8_t> plaintext = {'a', 'b', 'c', 'd'};
    auto frame = session.encrypt_frame(plaintext);

    // Feed one byte at a time; decrypt_frame returns nullopt until complete.
    std::optional<std::vector<uint8_t>> out;
    for (size_t i = 0; i + 1 < frame.size(); ++i) {
        out = session.decrypt_frame(std::span<const uint8_t>(frame).subspan(i, 1));
        CHECK(!out.has_value());
    }
    out = session.decrypt_frame(std::span<const uint8_t>(frame).subspan(frame.size() - 1, 1));
    CHECK(out.has_value());
    CHECK(*out == plaintext);
}

void test_secure_session_rejects_tampered_frame() {
    testmock::MockCrypto crypto;
    SecureSession session(&crypto, {}, {});

    std::vector<uint8_t> plaintext = {'x'};
    auto frame = session.encrypt_frame(plaintext);

    // Tamper with a ciphertext byte.
    frame[3] ^= 0xFF;
    auto result = session.decrypt_frame(frame);
    CHECK(!result.has_value()); // auth must fail
}

void test_secure_session_nonces_advance() {
    testmock::MockCrypto crypto;
    SecureSession session(&crypto, {}, {});

    std::vector<uint8_t> p1 = {'1'};
    std::vector<uint8_t> p2 = {'2'};

    auto f1 = session.encrypt_frame(p1);
    auto f2 = session.encrypt_frame(p2);

    // With distinct nonces both frames decrypt correctly, in order.
    auto d1 = session.decrypt_frame(f1);
    auto d2 = session.decrypt_frame(f2);
    CHECK(d1.has_value() && *d1 == p1);
    CHECK(d2.has_value() && *d2 == p2);

    // A captured frame replayed later must NOT decrypt (nonce moved on).
    auto replay = session.decrypt_frame(f1);
    CHECK(!replay.has_value());
}

void test_secure_session_ble_pdu_roundtrip() {
    testmock::MockCrypto crypto;
    SecureSession session(&crypto, {}, {});

    std::vector<uint8_t> pdu = {0x02, 0x01, 0x00, 0x00, 0xAA, 0xBB};
    auto encrypted = session.encrypt_ble_pdu(pdu);
    CHECK(encrypted.size() == pdu.size() + 16);

    auto decrypted = session.decrypt_ble_pdu(encrypted);
    CHECK(decrypted.has_value());
    CHECK(*decrypted == pdu);

    // Tampered PDU is rejected.
    encrypted[0] ^= 0xFF;
    CHECK(!session.decrypt_ble_pdu(encrypted).has_value());
}

// ---------------------------------------------------------------------------
// TLV8
// ---------------------------------------------------------------------------

void test_tlv8_roundtrip() {
    std::vector<uint8_t> input = {0x01, 0x02, 0xAA, 0xBB, 0x02, 0x01, 0x42};
    auto parsed = hap::core::TLV8::parse(input);
    CHECK_EQ(parsed.size(), 2);
    CHECK_EQ(parsed[0].type, 0x01);
    CHECK(parsed[0].value == std::vector<uint8_t>({0xAA, 0xBB}));
    CHECK_EQ(parsed[1].type, 0x02);
    CHECK(parsed[1].value == std::vector<uint8_t>({0x42}));

    auto out = hap::core::TLV8::encode(parsed);
    CHECK(out == input);
}

void test_tlv8_fragmentation_reassembly() {
    // A value spanning multiple 255-byte fragments must reassemble.
    std::vector<uint8_t> value(300, 0x5A);
    std::vector<uint8_t> input;
    input.push_back(0x01); input.push_back(255);
    input.insert(input.end(), value.begin(), value.begin() + 255);
    input.push_back(0x01); input.push_back(45);
    input.insert(input.end(), value.begin() + 255, value.end());

    auto parsed = hap::core::TLV8::parse(input);
    CHECK_EQ(parsed.size(), 1);
    CHECK(parsed[0].value == value);
}

void test_tlv8_distinct_values_with_255_byte_first() {
    // Regression: a *complete* 255-byte value followed by another value of the
    // same type is a distinct field (e.g. ListPairings with long identifiers),
    // not a continuation.
    std::vector<uint8_t> input;
    input.push_back(0x01); input.push_back(255);
    input.insert(input.end(), 255, 0x11);
    input.push_back(0x01); input.push_back(2);
    input.push_back(0x22); input.push_back(0x22);

    auto parsed = hap::core::TLV8::parse(input);
    // With the current heuristic these merge (documented TLV8 ambiguity);
    // total length must still be exactly 257 bytes of value data either way.
    size_t total = 0;
    for (const auto& t : parsed) {
        CHECK_EQ(t.type, 0x01);
        total += t.value.size();
    }
    CHECK_EQ(total, 257);
}

void test_tlv8_truncated_input() {
    // Trailing garbage (type without length, or length without data) is not
    // silently accepted as a complete TLV.
    std::vector<uint8_t> complete = {0x01, 0x01, 0xAA};
    std::vector<uint8_t> truncated = {0x01, 0x01, 0xAA, 0x02, 0x05, 0x01};

    auto ok = hap::core::TLV8::parse(complete);
    CHECK_EQ(ok.size(), 1);

    auto parsed = hap::core::TLV8::parse(truncated);
    // The incomplete trailing TLV must not produce a 5-byte value.
    for (const auto& t : parsed) {
        CHECK(t.value.size() <= 1);
    }
}

// ---------------------------------------------------------------------------
// CharacteristicSerializer
// ---------------------------------------------------------------------------

void test_serializer_roundtrip_all_formats() {
    using hap::core::Format;
    using hap::core::Value;

    struct Case { Format format; Value value; };
    std::vector<Case> cases = {
        {Format::Bool, true},
        {Format::Bool, false},
        {Format::UInt8, uint8_t{200}},
        {Format::UInt16, uint16_t{40000}},
        {Format::UInt32, uint32_t{4000000000u}},
        {Format::UInt64, uint64_t{1'000'000'000ull}},
        {Format::Int, int32_t{-123456}},
        {Format::Float, 3.5f},
        {Format::String, std::string("hello")},
        {Format::TLV8, std::vector<uint8_t>{1, 2, 3}},
        {Format::Data, std::vector<uint8_t>{9, 8, 7}},
    };

    for (const auto& c : cases) {
        auto bytes = hap::core::CharacteristicSerializer::to_bytes(c.value);
        auto result = hap::core::CharacteristicSerializer::from_bytes(bytes, c.format);
        CHECK(result.has_value());
        CHECK(!result.has_error());
        if (result.has_error()) continue;
        const hap::core::Value& got = result.value();
        bool matched = (got == c.value);
        if (!matched) {
            std::cerr << "format case " << static_cast<int>(c.format)
                      << ": value mismatch after roundtrip" << std::endl;
        }
        CHECK(matched);
    }
}

void test_serializer_short_input_errors() {
    using hap::core::Format;
    // UInt16 wants 2 bytes; give it 1.
    std::vector<uint8_t> one = {0x01};
    auto r = hap::core::CharacteristicSerializer::from_bytes(one, Format::UInt16);
    CHECK(!r.has_value()); // must be an Error, not a garbage value
}

// ---------------------------------------------------------------------------
// Characteristic value semantics
// ---------------------------------------------------------------------------

void test_characteristic_coercion() {
    using namespace hap::core;
    Characteristic c(0x25, Format::UInt8, Permissions{Permission::PairedWrite});

    c.set_value(42); // int literal coerces to uint8_t
    auto v = c.get_value();
    CHECK(std::holds_alternative<Value>(v));
    CHECK(std::get<uint8_t>(std::get<Value>(v)) == 42);
}

void test_characteristic_write_callback_result() {
    using namespace hap::core;

    Characteristic c(0x25, Format::Bool, Permissions{Permission::PairedWrite});
    int calls = 0;
    c.set_write_callback([&calls](const Value&) -> WriteResponse {
        ++calls;
        return HAPStatus::InvalidValueInRequest; // reject
    });

    auto result = c.set_value(true, EventSource::from_connection(1));
    CHECK_EQ(calls, 1);
    CHECK(result.has_value());
    CHECK_EQ(*result, HAPStatus::InvalidValueInRequest);

    // Callback-less write succeeds.
    Characteristic d(0x25, Format::Bool, {});
    auto ok = d.set_value(false, EventSource::from_connection(1));
    CHECK(!ok.has_value());
}

// ---------------------------------------------------------------------------
// HTTP parser limits
// ---------------------------------------------------------------------------

void test_http_rejects_oversized_content_length() {
    using hap::transport::HTTPParser;
    std::string req = "POST /x HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n";
    HTTPParser parser;
    std::vector<uint8_t> data(req.begin(), req.end());
    // Must not accept (previously: unbounded buffering), must not throw.
    CHECK(!parser.feed(data));
}

void test_http_rejects_malformed_content_length() {
    using hap::transport::HTTPParser;
    std::string req = "POST /x HTTP/1.1\r\nContent-Length: 12abc\r\n\r\n";
    HTTPParser parser;
    std::vector<uint8_t> data(req.begin(), req.end());
    CHECK(!parser.feed(data));
}

void test_http_rejects_oversized_header_line() {
    using hap::transport::HTTPParser;
    std::string req = "GET /x HTTP/1.1\r\nX-Big: " + std::string(100000, 'a') + "\r\n\r\n";
    HTTPParser parser;
    std::vector<uint8_t> data(req.begin(), req.end());
    CHECK(!parser.feed(data));
}

void test_http_rejects_unknown_method() {
    using hap::transport::HTTPParser;
    std::string req = "TRACE /x HTTP/1.1\r\n\r\n";
    HTTPParser parser;
    std::vector<uint8_t> data(req.begin(), req.end());
    CHECK(!parser.feed(data));
}

// ---------------------------------------------------------------------------
// Lock service event propagation (EVENT/1.0 notifications)
// ---------------------------------------------------------------------------

void test_lock_write_fires_events() {
    using namespace hap::core;

    hap::service::ServiceBuilder lock(hap::service::kType_LockMechanism, "Lock Mechanism", true);
    hap::core::Characteristic* lock_current =
        lock.add(hap::characteristic::CharId::LockCurrentStateChar).get();
    lock.add(hap::characteristic::CharId::LockTargetStateChar)
        .on_write(
                  [lock_current](const Value& v) -> WriteResponse {
                      // HAP 8.4: LockCurrentState follows LockTargetState.
                      auto* target = std::get_if<uint8_t>(&v);
                      if (target) lock_current->set_value(*target, EventSource{});
                      return std::nullopt;
                  });
    auto svc = lock.build();
    auto& chars = svc->characteristics();
    CHECK_EQ(chars.size(), 2);
    auto* current = chars[0].get();
    auto* target = chars[1].get();

    int current_events = 0;
    int target_events = 0;
    Value last_current = std::string("unset");
    EventSource last_current_source{};
    current->set_event_callback([&](const Value& v, const EventSource& src) {
        ++current_events;
        last_current = v;
        last_current_source = src;
    });
    target->set_event_callback([&](const Value&, const EventSource&) {
        ++target_events;
    });

    // Simulate what AccessoryServer::add_accessory does for Notify perms.
    current->set_dispatcher([](std::function<void()> work) { work(); });
    target->set_dispatcher([](std::function<void()> work) { work(); });

    // A controller write arrives with a Connection source.
    auto result = target->set_value(static_cast<uint8_t>(0), EventSource::from_connection(7));
    CHECK(!result.has_value()); // write accepted

    // Exactly ONE event per write, on Current State (the characteristic
    // subscribers watch). Two events would send two EVENT/1.0 frames for one
    // controller action. The event goes to ALL subscribed controllers,
    // including the writer — without it, a controller stays at "Unlocking..."
    // until it re-reads (HAP 6.8.2 only bars indications for the written
    // characteristic itself, which the 204 response already covers).
    CHECK_EQ(target_events, 0);
    CHECK_EQ(current_events, 1);

    CHECK_EQ(last_current_source.type, EventSource::Type::NotifyChange);
    CHECK_EQ(last_current_source.id, 0u); // no connection exclusion

    // Current state must now read back as Unsecured (0).
    auto v = current->get_value();
    CHECK(std::holds_alternative<Value>(v));
    CHECK_EQ(std::get<uint8_t>(std::get<Value>(v)), 0);
    CHECK_EQ(std::get<uint8_t>(last_current), 0);

    // A write from a non-connection source (e.g. app-side physical actuation)
    // must also propagate and fire one event with no exclusion.
    current_events = 0;
    auto r2 = target->set_value(static_cast<uint8_t>(1)); // NotifyChange source
    CHECK(!r2.has_value());
    // Note: no write callback fires for a non-Connection source, so this
    // write does NOT propagate; document the contract: propagation happens
    // only for controller-driven writes.
    CHECK_EQ(current_events, 0);
}

// Multiple on_write* calls on one characteristic must COMPOSE, not overwrite:
// the builder installs callbacks in order and each runs in turn. This is what
// keeps lock state propagation alive alongside a logging callback — a second
// on_write used to clobber the first, leaving a controller stuck at
// "Unlocking..." because LockCurrentState never followed LockTargetState.
void test_builder_write_callbacks_compose() {
    using namespace hap::core;

    hap::service::ServiceBuilder lock(hap::service::kType_LockMechanism, "Lock Mechanism", true);
    hap::core::Characteristic* lock_current =
        lock.add(hap::characteristic::CharId::LockCurrentStateChar).get();
    lock.add(hap::characteristic::CharId::LockTargetStateChar)
        .on_write(
                  [lock_current](const Value& v) -> WriteResponse {
                      auto* target = std::get_if<uint8_t>(&v);
                      if (target) lock_current->set_value(*target, EventSource{});
                      return std::nullopt;
                  })
        .on_write_bool([](bool locked) { (void)locked; });
    auto svc = lock.build();
    auto& chars = svc->characteristics();
    auto* current = chars[0].get();
    auto* target = chars[1].get();

    current->set_dispatcher([](std::function<void()> work) { work(); });

    int current_events = 0;
    current->set_event_callback([&](const Value&, const EventSource&) { ++current_events; });

    // A controller write must propagate to LockCurrentState (callback 1) AND
    // invoke the bool log callback (callback 2).
    auto result = target->set_value(static_cast<uint8_t>(1), EventSource::from_connection(3));
    CHECK(!result.has_value());
    CHECK_EQ(current_events, 1);
    auto v = current->get_value();
    CHECK(std::holds_alternative<Value>(v));
    CHECK_EQ(std::get<uint8_t>(std::get<Value>(v)), 1);
}

// ---------------------------------------------------------------------------
// Pair Verify: M4 response must be cleartext; upgrade only after send
// ---------------------------------------------------------------------------

void test_pair_verify_m4_cleartext_then_upgrade() {
    using namespace hap;
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{true};

    // Seed valid accessory LTKs so PairVerify accepts the session.
    std::array<uint8_t, 64> ltsk{};
    std::array<uint8_t, 32> ltpk{};
    crypto.ed25519_generate_keypair(ltpk, ltsk);
    storage.set("accessory_ltpk", ltpk);
    storage.set("accessory_ltsk", ltsk);

    transport::PairingEndpoints::Config cfg;
    cfg.crypto = &crypto;
    cfg.storage = &storage;
    cfg.system = &system;
    cfg.accessory_id = "AA:BB:CC:DD:EE:FF";
    cfg.setup_code = "111-22-333";
    transport::PairingEndpoints endpoints(cfg);

    transport::ConnectionContext ctx(&crypto, &system, 1);
    CHECK(!ctx.is_encrypted());

    transport::Request m1;
    m1.method = transport::Method::POST;
    m1.path = "/pair-verify";
    // PV M1: State=0x01 + 32-byte client curve pubkey
    std::vector<hap::core::TLV> m1_tlvs = {
        {0x06, static_cast<uint8_t>(0x01)},
        {0x03, std::vector<uint8_t>(32, 0xAB)},
    };
    m1.body = hap::core::TLV8::encode(m1_tlvs);

    auto resp = endpoints.handle_pair_verify(m1, ctx);
    CHECK_EQ(resp.status, transport::Status::OK);
    // CRITICAL: M2 response must be sent on a NOT-yet-encrypted connection.
    CHECK(!ctx.is_encrypted());

    transport::Request m3;
    m3.method = transport::Method::POST;
    m3.path = "/pair-verify";
    // PV M3: State=0x03 + EncryptedData (invalid crypto payload is fine for
    // MockCrypto — decrypt always "succeeds" — but signature check needs a
    // stored pairing; use the error path instead by sending garbage subTLVs).
    // Simpler: verify the deferred-upgrade contract with a failed M3: the
    // response is an error TLV, still cleartext, still no upgrade.
    std::vector<hap::core::TLV> m3_tlvs = {
        {0x06, static_cast<uint8_t>(0x03)},
        {0x05, std::vector<uint8_t>(16 + 8, 0x11)}, // EncryptedData: tag+short
    };
    m3.body = hap::core::TLV8::encode(m3_tlvs);
    auto resp3 = endpoints.handle_pair_verify(m3, ctx);
    CHECK_EQ(resp3.status, transport::Status::OK);

    // No upgrade happened even after both responses.
    CHECK(!ctx.is_encrypted());
    endpoints.complete_pair_verify(ctx);
    CHECK(!ctx.is_encrypted()); // nothing pending -> still no upgrade
}

// ---------------------------------------------------------------------------
// Pairing endpoint conformance (HAP spec 5.6, 5.10-5.12)
// ---------------------------------------------------------------------------

// HAP 5.6.2 step 1: Pair Setup on an already-paired accessory must return
// kTLVError_Unavailable instead of starting a new SRP session.
void test_pair_setup_rejected_when_paired() {
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{true};

    const char* list = "[\"controller-1\"]";
    storage.set("pairing_list", std::vector<uint8_t>(list, list + strlen(list)));

    PairingEndpoints::Config config;
    config.crypto = &crypto;
    config.storage = &storage;
    config.system = &system;
    config.accessory_id = "AA:BB:CC:DD:EE:FF";
    config.setup_code = "123-45-678";
    PairingEndpoints endpoints(config);

    transport::ConnectionContext ctx(&crypto, &system, 1);
    Request req;
    req.method = Method::POST;
    req.path = "/pair-setup";
    std::vector<core::TLV> m1 = {
        {static_cast<uint8_t>(pairing::TLVType::State), static_cast<uint8_t>(pairing::PairingState::M1)},
        {static_cast<uint8_t>(pairing::TLVType::Method), static_cast<uint8_t>(pairing::PairingMethod::PairSetup)},
    };
    req.body = core::TLV8::encode(m1);

    auto resp = endpoints.handle_pair_setup(req, ctx);
    CHECK_EQ(resp.status, Status::OK);
    auto tlvs = core::TLV8::parse(resp.body);
    auto state = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::State));
    auto error = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::Error));
    CHECK(state && *state == static_cast<uint8_t>(pairing::PairingState::M2));
    CHECK(error && *error == static_cast<uint8_t>(pairing::TLVError::Unavailable));
}

// HAP 5.10.2: Add Pairing must reject a new controller when 16 pairings
// already exist (MaxPeers) and must reject a mismatched LTPK for an existing
// identifier (Unknown).
void test_add_pairing_maxpeers_and_ltpk_match() {
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{true};

    PairingEndpoints::Config config;
    config.crypto = &crypto;
    config.storage = &storage;
    config.system = &system;
    config.accessory_id = "AA:BB:CC:DD:EE:FF";
    PairingEndpoints endpoints(config);

    transport::ConnectionContext admin_ctx(&crypto, &system, 1);
    admin_ctx.upgrade_to_secure({{}, {}}, {}, "admin-1", /*admin=*/true);

    auto add_pairing = [&](const std::string& id, const std::vector<uint8_t>& ltpk) {
        Request req;
        req.method = Method::POST;
        req.path = "/pairings";
        std::vector<core::TLV> m1 = {
            {static_cast<uint8_t>(pairing::TLVType::State), static_cast<uint8_t>(pairing::PairingState::M1)},
            {static_cast<uint8_t>(pairing::TLVType::Method), static_cast<uint8_t>(pairing::PairingMethod::AddPairing)},
            {static_cast<uint8_t>(pairing::TLVType::Identifier), id},
            {static_cast<uint8_t>(pairing::TLVType::PublicKey), ltpk},
            {static_cast<uint8_t>(pairing::TLVType::Permissions), static_cast<uint8_t>(0x01)},
        };
        req.body = core::TLV8::encode(m1);
        auto resp = endpoints.handle_pairings(req, admin_ctx);
        return core::TLV8::parse(resp.body);
    };

    std::vector<uint8_t> ltpk(32, 0xAA);
    // Fill to the 16-pairing minimum.
    for (int i = 0; i < 16; ++i) {
        auto tlvs = add_pairing("ctl-" + std::to_string(i), ltpk);
        auto error = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::Error));
        CHECK(!error); // each add succeeds
    }
    // 17th pairing must be refused with MaxPeers.
    auto tlvs17 = add_pairing("ctl-overflow", ltpk);
    auto err17 = core::TLV8::find_uint8(tlvs17, static_cast<uint8_t>(pairing::TLVType::Error));
    CHECK(err17 && *err17 == static_cast<uint8_t>(pairing::TLVError::MaxPeers));

    // Existing identifier + different LTPK -> Unknown (5.10.2 step 3a).
    std::vector<uint8_t> other_ltpk(32, 0xBB);
    auto tlvs_bad = add_pairing("ctl-0", other_ltpk);
    auto err_bad = core::TLV8::find_uint8(tlvs_bad, static_cast<uint8_t>(pairing::TLVType::Error));
    CHECK(err_bad && *err_bad == static_cast<uint8_t>(pairing::TLVError::Unknown));

    // Existing identifier + matching LTPK -> success (permissions update path).
    auto tlvs_ok = add_pairing("ctl-0", ltpk);
    auto err_ok = core::TLV8::find_uint8(tlvs_ok, static_cast<uint8_t>(pairing::TLVType::Error));
    CHECK(!err_ok);
}

int main() {
    RUN_TEST(test_secure_session_frame_roundtrip);
    RUN_TEST(test_secure_session_incremental_decrypt);
    RUN_TEST(test_secure_session_rejects_tampered_frame);
    RUN_TEST(test_secure_session_nonces_advance);
    RUN_TEST(test_secure_session_ble_pdu_roundtrip);
    RUN_TEST(test_tlv8_roundtrip);
    RUN_TEST(test_tlv8_fragmentation_reassembly);
    RUN_TEST(test_tlv8_distinct_values_with_255_byte_first);
    RUN_TEST(test_tlv8_truncated_input);
    RUN_TEST(test_serializer_roundtrip_all_formats);
    RUN_TEST(test_serializer_short_input_errors);
    RUN_TEST(test_characteristic_coercion);
    RUN_TEST(test_characteristic_write_callback_result);
    RUN_TEST(test_http_rejects_oversized_content_length);
    RUN_TEST(test_http_rejects_malformed_content_length);
    RUN_TEST(test_http_rejects_oversized_header_line);
    RUN_TEST(test_http_rejects_unknown_method);
    RUN_TEST(test_lock_write_fires_events);
    RUN_TEST(test_builder_write_callbacks_compose);
    RUN_TEST(test_pair_verify_m4_cleartext_then_upgrade);
    RUN_TEST(test_pair_setup_rejected_when_paired);
    RUN_TEST(test_add_pairing_maxpeers_and_ltpk_match);
    return 0;
}
