#pragma once

#include <span>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>

namespace hap::platform {

/**
 * @brief Abstract interface for cryptographic primitives required by HAP.
 * 
 * The consuming application must implement this interface using its platform's
 * crypto library (e.g., mbedtls, OpenSSL, hardware crypto).
 */
struct Crypto {
    virtual ~Crypto() = default;

    // SHA-512
    virtual void sha512(std::span<const uint8_t> data, std::span<uint8_t, 64> output) = 0;

    // HKDF-SHA-512
    virtual void hkdf_sha512(
        std::span<const uint8_t> key,
        std::span<const uint8_t> salt,
        std::span<const uint8_t> info,
        std::span<uint8_t> output) = 0;

    // Ed25519
    virtual void ed25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 64> private_key) = 0;
    virtual void ed25519_sign(
        std::span<const uint8_t, 64> private_key,
        std::span<const uint8_t> message,
        std::span<uint8_t, 64> signature) = 0;
    virtual bool ed25519_verify(
        std::span<const uint8_t, 32> public_key,
        std::span<const uint8_t> message,
        std::span<const uint8_t, 64> signature) = 0;

    // Curve25519 (X25519)
    virtual void x25519_generate_keypair(std::span<uint8_t, 32> public_key, std::span<uint8_t, 32> private_key) = 0;
    virtual void x25519_shared_secret(
        std::span<const uint8_t, 32> private_key,
        std::span<const uint8_t, 32> peer_public_key,
        std::span<uint8_t, 32> shared_secret) = 0;

    // ChaCha20-Poly1305
    // Returns true on success, false on auth failure
    virtual bool chacha20_poly1305_encrypt_and_tag(
        std::span<const uint8_t, 32> key,
        std::span<const uint8_t, 12> nonce,
        std::span<const uint8_t> aad,
        std::span<const uint8_t> plaintext,
        std::span<uint8_t> ciphertext,
        std::span<uint8_t, 16> tag) = 0;

    virtual bool chacha20_poly1305_decrypt_and_verify(
        std::span<const uint8_t, 32> key,
        std::span<const uint8_t, 12> nonce,
        std::span<const uint8_t> aad,
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t, 16> tag,
        std::span<uint8_t> plaintext) = 0;

    // SRP6a (Secure Remote Password)
    // SRP interface is defined in CryptoSRP.hpp extending this interface.
};

} // namespace hap::platform
