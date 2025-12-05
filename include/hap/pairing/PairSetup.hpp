#pragma once

#include "hap/pairing/TLVTypes.hpp"
#include "hap/platform/CryptoSRP.hpp"
#include "hap/platform/Storage.hpp"
#include "hap/platform/System.hpp"
#include "hap/core/TLV8.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <functional>

namespace hap::pairing {

/**
 * @brief Pair Setup Protocol Handler (HAP Spec 5.6)
 * 
 * Implements the 6-message SRP-based pairing handshake:
 * M1: SRP Start Request
 * M2: SRP Start Response  
 * M3: SRP Verify Request
 * M4: SRP Verify Response
 * M5: Exchange Request
 * M6: Exchange Response
 */
class PairSetup {
public:
    struct Config {
        platform::CryptoSRP* crypto;
        platform::Storage* storage;
        platform::System* system;
        std::string accessory_id;      // e.g., "12:34:56:78:9A:BC"
        std::string setup_code;        // 8-digit PIN (e.g., "123-45-678")
        std::function<void()> on_pairings_changed = nullptr;
    };

    PairSetup(Config config);
    ~PairSetup();

    /**
     * @brief Process incoming Pair Setup request.
     * @param request_tlv Raw TLV8 data from POST /pair-setup
     * @return Response TLV8 data, or nullopt on internal error
     */
    std::optional<std::vector<uint8_t>> handle_request(std::span<const uint8_t> request_tlv);

    /**
     * @brief Reset the pairing state (e.g., after timeout or error).
     */
    void reset();

private:
    Config config_;
    
    // State
    enum class State {
        M1_AwaitingSRPStartRequest,
        M3_AwaitingSRPVerifyRequest,
        M5_AwaitingExchangeRequest,
        Completed
    };
    State state_;
    
    // SRP session (lives across M1-M4)
    std::unique_ptr<platform::SRPSession> srp_session_;
    std::vector<uint8_t> session_key_; // K from SRP, used in M5-M6
    
    // Accessory's long-term keys
    std::array<uint8_t, 32> accessory_ltpk_;  // Long-term public key
    std::array<uint8_t, 64> accessory_ltsk_;  // Long-term secret key
    
    // Message handlers
    std::optional<std::vector<uint8_t>> handle_m1(const std::vector<core::TLV>& request);
    std::optional<std::vector<uint8_t>> handle_m3(const std::vector<core::TLV>& request);
    std::optional<std::vector<uint8_t>> handle_m5(const std::vector<core::TLV>& request);
    
    // Helper to build error response
    std::vector<uint8_t> build_error_response(PairingState state, TLVError error);
    
    // Load or generate accessory LTPK/LTSK
    void ensure_long_term_keys();
};

} // namespace hap::pairing
