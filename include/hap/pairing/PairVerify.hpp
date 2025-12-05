#pragma once

#include "hap/pairing/TLVTypes.hpp"
#include "hap/platform/Crypto.hpp"
#include "hap/platform/Storage.hpp"
#include "hap/core/TLV8.hpp"
#include <vector>
#include <optional>
#include <string>
#include <array>

namespace hap::pairing {

/**
 * @brief Pair Verify Protocol Handler (HAP Spec 5.7)
 * 
 * Implements the 4-message Station-to-Station pairing verification:
 * M1: Verify Start Request
 * M2: Verify Start Response
 * M3: Verify Finish Request
 * M4: Verify Finish Response
 */
class PairVerify {
public:
    struct Config {
        platform::Crypto* crypto;
        platform::Storage* storage;
        std::string accessory_id;
    };

    PairVerify(Config config);
    ~PairVerify();

    /**
     * @brief Process incoming Pair Verify request.
     * @param request_tlv Raw TLV8 data from POST /pair-verify
     * @return Response TLV8 data, or nullopt on internal error
     */
    std::optional<std::vector<uint8_t>> handle_request(std::span<const uint8_t> request_tlv);

    /**
     * @brief Check if verification is complete and session key is ready.
     */
    bool is_verified() const { return state_ == State::Verified; }

    /**
     * @brief Get the derived session keys (only valid after verification).
     * @return Tuple of (Accessory Session Key, Controller Session Key)
     */
    const std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> get_session_keys() const { return std::make_tuple(a_session_key_, c_session_key_); }

    /**
     * @brief Reset the verification state.
     */
    /**
     * @brief Reset the verification state.
     */
    void reset();

    /**
     * @brief Get the authenticated controller ID.
     */
    const std::string& get_controller_id() const { return controller_id_; }

private:
    Config config_;
    std::string controller_id_;
    
    enum class State {
        M1_AwaitingVerifyStartRequest,
        M3_AwaitingVerifyFinishRequest,
        Verified
    };
    State state_;
    
    // Ephemeral Curve25519 keys
    std::array<uint8_t, 32> accessory_curve_public_;
    std::array<uint8_t, 32> accessory_curve_secret_;
    std::array<uint8_t, 32> client_curve_public_;
    
    // Shared secret and derived session key
    std::array<uint8_t, 32> shared_secret_;
    std::array<uint8_t, 32> p_session_key_;

    std::array<uint8_t, 32> a_session_key_;
    std::array<uint8_t, 32> c_session_key_;
    
    // Accessory's long-term keys
    std::array<uint8_t, 32> accessory_ltpk_;
    std::array<uint8_t, 64> accessory_ltsk_;
    
    // Message handlers
    std::optional<std::vector<uint8_t>> handle_m1(const std::vector<core::TLV>& request);
    std::optional<std::vector<uint8_t>> handle_m3(const std::vector<core::TLV>& request);
    
    // Helper to build error response
    std::vector<uint8_t> build_error_response(PairingState state, TLVError error);
    
    // Load long-term keys
    void load_long_term_keys();
};

} // namespace hap::pairing
