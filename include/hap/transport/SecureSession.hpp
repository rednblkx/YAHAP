#pragma once

#include "hap/platform/Crypto.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <optional>

namespace hap::transport {

/**
 * @brief Secure Session Wrapper for HTTP Transport (HAP Spec 6.5.2)
 * 
 * Encrypts/decrypts HTTP frames using ChaCha20-Poly1305 AEAD.
 * Frame format: <2-byte length><encrypted data><16-byte auth tag>
 * Nonce: 96-bit counter (little-endian)
 */
class SecureSession {
public:
    SecureSession(platform::Crypto* crypto, std::array<uint8_t, 32> a2c, std::array<uint8_t, 32> c2a);

    /**
     * @brief Encrypt an HTTP response before sending.
     * @param plaintext_http Raw HTTP response bytes
     * @return Encrypted frame ready to send over TCP
     */
    std::vector<uint8_t> encrypt_frame(std::span<const uint8_t> plaintext_http);

    /**
     * @brief Decrypt received TCP data and extract HTTP frames.
     * Feed this method with incoming TCP chunks.
     * @param encrypted_data Raw TCP data (may contain partial frames)
     * @return Decrypted HTTP data, or nullopt if frame is incomplete or auth fails
     */
    std::optional<std::vector<uint8_t>> decrypt_frame(std::span<const uint8_t> encrypted_data);

    /**
     * @brief Reset nonces (e.g., after re-verification).
     */
    void reset();

private:
    platform::Crypto* crypto_;
    std::array<uint8_t, 32> a2c_;
    std::array<uint8_t, 32> c2a_;
    
    // Nonces (separate for read/write)
    uint64_t write_nonce_;
    uint64_t read_nonce_;
    
    // Buffer for partial frames
    std::vector<uint8_t> read_buffer_;
    
    // Build 96-bit nonce from counter
    std::array<uint8_t, 12> build_nonce(uint64_t counter);
};

} // namespace hap::transport
