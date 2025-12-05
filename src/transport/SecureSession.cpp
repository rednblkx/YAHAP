#include "hap/transport/SecureSession.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace hap::transport {

SecureSession::SecureSession(platform::Crypto* crypto, std::array<uint8_t, 32> a2c, std::array<uint8_t, 32> c2a)
    : crypto_(crypto), a2c_(a2c), c2a_(c2a), write_nonce_(0), read_nonce_(0) {}

void SecureSession::reset() {
    write_nonce_ = 0;
    read_nonce_ = 0;
    read_buffer_.clear();
}

std::array<uint8_t, 12> SecureSession::build_nonce(uint64_t counter) {
    std::array<uint8_t, 12> nonce = {};
    // First 4 bytes are zero
    // Little-endian 64-bit counter in last 8 bytes
    for (int i = 0; i < 8; ++i) {
        nonce[4 + i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
    }
    return nonce;
}

std::vector<uint8_t> SecureSession::encrypt_frame(std::span<const uint8_t> plaintext_http) {
    // Frame format: <2-byte length><encrypted data><16-byte auth tag>
    
    uint16_t length = static_cast<uint16_t>(plaintext_http.size());
    std::array<uint8_t, 2> length_bytes = {
        static_cast<uint8_t>(length & 0xFF),
        static_cast<uint8_t>((length >> 8) & 0xFF)
    };
    
    // Encrypt plaintext
    auto nonce = build_nonce(write_nonce_++);
    std::vector<uint8_t> ciphertext(plaintext_http.size());
    std::array<uint8_t, 16> auth_tag;
    
    // AAD is the 2-byte length
    if (!crypto_->chacha20_poly1305_encrypt_and_tag(
            a2c_, nonce, std::span(length_bytes), 
            plaintext_http, ciphertext, auth_tag)) {
        // Encryption failed (shouldn't happen with valid key)
        return {};
    }
    
    // Build frame: length || ciphertext || auth_tag
    std::vector<uint8_t> frame;
    frame.insert(frame.end(), length_bytes.begin(), length_bytes.end());
    frame.insert(frame.end(), ciphertext.begin(), ciphertext.end());
    frame.insert(frame.end(), auth_tag.begin(), auth_tag.end());
    
    return frame;
}

std::optional<std::vector<uint8_t>> SecureSession::decrypt_frame(std::span<const uint8_t> encrypted_data) {
    // Append to read buffer
    read_buffer_.insert(read_buffer_.end(), encrypted_data.begin(), encrypted_data.end());
    
    // Need at least 2 bytes for length
    if (read_buffer_.size() < 2) {
        return std::nullopt; // Not enough data yet
    }
    
    // Read length (little-endian)
    uint16_t length = read_buffer_[0] | (static_cast<uint16_t>(read_buffer_[1]) << 8);

    std::cout << "Decrypted frame with length " << length << std::endl;
    
    // Check if we have the full frame: 2 (length) + length (ciphertext) + 16 (auth tag)
    size_t frame_size = 2 + length + 16;
    if (read_buffer_.size() < frame_size) {
        return std::nullopt; // Not enough data yet
    }
    
    // Extract components
    std::array<uint8_t, 2> length_bytes = {read_buffer_[0], read_buffer_[1]};
    std::cout << "length_bytes: " << std::hex << (int)length_bytes[0] << " " << (int)length_bytes[1] << std::endl;
    std::vector<uint8_t> ciphertext(read_buffer_.begin() + 2, read_buffer_.begin() + 2 + length);
    std::array<uint8_t, 16> auth_tag;
    std::copy_n(read_buffer_.begin() + 2 + length, 16, auth_tag.begin());
    
    // Decrypt
    auto nonce = build_nonce(read_nonce_++);
    std::vector<uint8_t> plaintext(length);
    
    if (!crypto_->chacha20_poly1305_decrypt_and_verify(
            c2a_, nonce, std::span(length_bytes),
            ciphertext, auth_tag, plaintext)) {
        // Authentication failed - this is a security violation
        read_buffer_.clear();
        return std::nullopt;
    }
    
    // Remove processed frame from buffer
    read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + frame_size);
    
    return plaintext;
}

} // namespace hap::transport
