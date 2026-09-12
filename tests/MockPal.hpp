#pragma once

// Shared mock Platform Abstraction Layer implementations for tests.
// Each test binary links the real library, so mocks only live here —
// do NOT redefine library symbols in test .cpp files.

#include "hap/platform/Crypto.hpp"
#include "hap/platform/CryptoSRP.hpp"
#include "hap/platform/Storage.hpp"
#include "hap/platform/System.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

namespace testmock {

// Aliases so member declarations stay short below.
namespace platform = ::hap::platform;

// ---------------------------------------------------------------------------
// Crypto: deterministic, side-effect-free stubs.
//
// The cipher stubs behave as a trivial "XOR keystream" pair so that
// encrypt -> decrypt round-trips actually work and key material influences
// the output; hash functions are simple deterministic folds.
// ---------------------------------------------------------------------------

class MockCrypto : public platform::CryptoSRP {
public:
    // --- platform::Crypto ---
    void sha512(std::span<const uint8_t> input, std::span<uint8_t, 64> output) override {
        std::fill(output.begin(), output.end(), uint8_t{0});
        uint8_t fold = 0x5A;
        for (auto b : input) fold ^= b;
        for (size_t i = 0; i < output.size(); ++i) output[i] = fold ^ static_cast<uint8_t>(i);
    }

    void hkdf_sha512(std::span<const uint8_t> key, std::span<const uint8_t> salt,
                     std::span<const uint8_t> info, std::span<uint8_t> output) override {
        // Deterministic mix of key/salt/info so different inputs differ.
        uint8_t seed = 0x3C;
        for (auto b : key) seed ^= b;
        for (auto b : salt) seed ^= static_cast<uint8_t>(b + 1);
        for (auto b : info) seed ^= static_cast<uint8_t>(b + 2);
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = static_cast<uint8_t>(seed + i);
        }
    }

    void ed25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 64> private_key) override {
        for (size_t i = 0; i < private_key.size(); ++i) private_key[i] = static_cast<uint8_t>(0x10 + i);
        for (size_t i = 0; i < public_key.size(); ++i) public_key[i] = static_cast<uint8_t>(0x90 + i);
    }

    void ed25519_sign(std::span<const uint8_t, 64> private_key, std::span<const uint8_t> message,
                      std::span<uint8_t, 64> signature) override {
        uint8_t fold = 0x11;
        for (auto b : private_key) fold ^= b;
        for (auto b : message) fold ^= b;
        for (size_t i = 0; i < signature.size(); ++i) signature[i] = static_cast<uint8_t>(fold + i);
    }

    // Accepts any signature; pair with ed25519_sign which always "succeeds".
    bool ed25519_verify(std::span<const uint8_t, 32> public_key, std::span<const uint8_t> message,
                        std::span<const uint8_t, 64> signature) override {
        (void)public_key; (void)message; (void)signature;
        return true;
    }

    void x25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 32> private_key) override {
        for (size_t i = 0; i < private_key.size(); ++i) private_key[i] = static_cast<uint8_t>(0x20 + i);
        for (size_t i = 0; i < public_key.size(); ++i) public_key[i] = static_cast<uint8_t>(0xA0 + i);
    }

    void x25519_shared_secret(std::span<const uint8_t, 32> private_key,
                              std::span<const uint8_t, 32> peer_public_key,
                              std::span<uint8_t, 32> shared_secret) override {
        for (size_t i = 0; i < shared_secret.size(); ++i) {
            shared_secret[i] = static_cast<uint8_t>(private_key[i] ^ peer_public_key[i] ^ 0x5C);
        }
    }

    bool chacha20_poly1305_encrypt_and_tag(std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce,
                                           std::span<const uint8_t> aad, std::span<const uint8_t> plaintext,
                                           std::span<uint8_t> ciphertext, std::span<uint8_t, 16> tag) override {
        if (ciphertext.size() < plaintext.size()) return false;
        for (size_t i = 0; i < plaintext.size(); ++i) {
            ciphertext[i] = static_cast<uint8_t>(plaintext[i] ^ key[i % key.size()] ^ nonce[i % nonce.size()]);
        }
        auto t = compute_tag(key, nonce, aad, plaintext);
        std::copy(t.begin(), t.end(), tag.begin());
        return true;
    }

    bool chacha20_poly1305_decrypt_and_verify(std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce,
                                              std::span<const uint8_t> aad, std::span<const uint8_t> ciphertext,
                                              std::span<const uint8_t, 16> tag, std::span<uint8_t> plaintext) override {
        auto expected = compute_tag(key, nonce, aad, ciphertext);
        if (!std::equal(expected.begin(), expected.end(), tag.begin())) return false;
        for (size_t i = 0; i < plaintext.size() && i < ciphertext.size(); ++i) {
            plaintext[i] = static_cast<uint8_t>(ciphertext[i] ^ key[i % key.size()] ^ nonce[i % nonce.size()]);
        }
        return true;
    }

    // --- platform::CryptoSRP ---
    struct MockSRPSession : platform::SRPSession {
        MockSRPSession()
            : platform::SRPSession({}, {}, "", "") {}
        std::vector<uint8_t> server_public = {0x01, 0x02, 0x03};
        std::vector<uint8_t> session_key = {0x5B, 0x5B, 0x5B};
        bool proof_verified = false;
    };

    std::unique_ptr<platform::SRPSession> srp_new_verifier(std::string_view username,
                                                           std::string_view password) override {
        (void)username; (void)password;
        return std::make_unique<MockSRPSession>();
    }

    std::array<uint8_t, 16> srp_get_salt(platform::SRPSession* session) override {
        (void)session;
        std::array<uint8_t, 16> salt{};
        salt.fill(0x5A);
        return salt;
    }

    std::vector<uint8_t> srp_get_public_key(platform::SRPSession* session) override {
        return static_cast<MockSRPSession*>(session)->server_public;
    }

    bool srp_set_client_public_key(platform::SRPSession* session, std::span<const uint8_t> client_public_key) override {
        (void)session; (void)client_public_key;
        return true;
    }

    bool srp_verify_client_proof(platform::SRPSession* session, std::span<const uint8_t> client_proof) override {
        (void)session; (void)client_proof;
        static_cast<MockSRPSession*>(session)->proof_verified = true;
        return true;
    }

    std::vector<uint8_t> srp_get_server_proof(platform::SRPSession* session) override {
        static_cast<MockSRPSession*>(session); // session must be ours
        return {0xAB, 0xCD};
    }

    std::vector<uint8_t> srp_get_session_key(platform::SRPSession* session) override {
        return static_cast<MockSRPSession*>(session)->session_key;
    }

private:
    // Tag = fold(key, nonce, aad, message); used by both directions.
    static std::array<uint8_t, 16> compute_tag(std::span<const uint8_t, 32> key,
                                               std::span<const uint8_t, 12> nonce,
                                               std::span<const uint8_t> aad,
                                               std::span<const uint8_t> message) {
        uint8_t fold = 0xE0;
        for (auto b : key) fold ^= b;
        for (auto b : nonce) fold ^= b;
        for (auto b : aad) fold ^= b;
        for (auto b : message) fold ^= b;
        std::array<uint8_t, 16> tag{};
        tag.fill(fold);
        return tag;
    }
};

// ---------------------------------------------------------------------------
// Storage: in-memory key-value store.
// ---------------------------------------------------------------------------

class MockStorage : public platform::Storage {
public:
    std::optional<std::vector<uint8_t>> get(std::string_view key) override {
        auto it = store_.find(std::string(key));
        if (it == store_.end()) return std::nullopt;
        return it->second;
    }

    void set(std::string_view key, std::span<const uint8_t> value) override {
        store_[std::string(key)] = std::vector<uint8_t>(value.begin(), value.end());
    }

    void remove(std::string_view key) override { store_.erase(std::string(key)); }

    bool has(std::string_view key) override { return store_.count(std::string(key)) > 0; }

    void clear() { store_.clear(); }

private:
    std::map<std::string, std::vector<uint8_t>> store_;
};

// ---------------------------------------------------------------------------
// System: logging to stdout, fake clock, deterministic RNG.
// ---------------------------------------------------------------------------

class MockSystem : public platform::System {
public:
    explicit MockSystem(bool quiet = true) : quiet_(quiet) {}

    void log(LogLevel level, std::string_view message) override {
        if (!quiet_) std::cout << "[LOG] " << message << std::endl;
        (void)level;
    }

    uint64_t millis() override { return clock_ms_; }
    void advance_ms(uint64_t ms) { clock_ms_ += ms; }

    void random_bytes(std::span<uint8_t> buffer) override {
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = static_cast<uint8_t>(rng_state_ ^ (i * 7));
            rng_state_ = rng_state_ * 1103515245 + 12345;
        }
    }

private:
    bool quiet_;
    uint64_t clock_ms_ = 1000;
    uint32_t rng_state_ = 0x1234;
};

} // namespace testmock
