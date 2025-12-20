#pragma once

#include "hap/platform/Crypto.hpp"
#include <memory>
#include <array>
#include <string>
#include <vector>

namespace hap::platform {

/**
 * @brief SRP6a Session State
 * Opaque handle to platform-specific SRP session data.
 */
struct SRPSession {
    SRPSession(std::array<uint8_t, 16> salt, std::vector<uint8_t> verifier, std::string username, std::string password) : salt(std::move(salt)), verifier(std::move(verifier)), username(std::move(username)), password(std::move(password)) {}
    virtual ~SRPSession() = default;
    std::array<uint8_t, 16> salt;
    std::vector<uint8_t> verifier;
    std::string username;
    std::string password;
    std::vector<uint8_t> client_public_key;
    std::vector<uint8_t> server_public_key;
    std::vector<uint8_t> M1;
    std::vector<uint8_t> M2;
};

/**
 * @brief Extended Crypto interface with SRP6a support.
 * 
 * This extends the base Crypto interface with methods needed for
 * Secure Remote Password (SRP) protocol used in HAP Pair Setup.
 */
struct CryptoSRP : public Crypto {
    /**
     * @brief Create a new SRP verifier session.
     * 
     * @param username "Pair-Setup" (fixed for HAP)
     * @param password Setup code (8-digit PIN from accessory label)
     * @return Unique pointer to SRP session state
     */
    virtual std::unique_ptr<SRPSession> srp_new_verifier(
        std::string_view username,
        std::string_view password) = 0;

    /**
     * @brief Get the salt used in SRP.
     * 16 bytes, randomly generated on first pairing.
     */
    virtual std::array<uint8_t, 16> srp_get_salt(SRPSession* session) = 0;

    /**
     * @brief Get the server's public key (B).
     * 384 bytes for SRP-3072.
     */
    virtual std::vector<uint8_t> srp_get_public_key(SRPSession* session) = 0;

    /**
     * @brief Set client's public key (A) and compute shared secret.
     * 
     * @param session SRP session
     * @param client_public_key Client's public key A (384 bytes)
     * @return true if successful, false if A is invalid
     */
    virtual bool srp_set_client_public_key(
        SRPSession* session,
        std::span<const uint8_t> client_public_key) = 0;

    /**
     * @brief Verify client's proof M1.
     * 
     * @param session SRP session
     * @param client_proof Client's proof M1 (64 bytes for SHA-512)
     * @return true if proof is valid
     */
    virtual bool srp_verify_client_proof(
        SRPSession* session,
        std::span<const uint8_t> client_proof) = 0;

    /**
     * @brief Get server's proof M2.
     * 
     * @param session SRP session  
     * @return Server's proof M2 (64 bytes for SHA-512)
     */
    virtual std::vector<uint8_t> srp_get_server_proof(SRPSession* session) = 0;

    /**
     * @brief Get the shared session key K.
     * 
     * @param session SRP session
     * @return Session key (64 bytes for SHA-512)
     */
    virtual std::vector<uint8_t> srp_get_session_key(SRPSession* session) = 0;
};

} // namespace hap::platform
