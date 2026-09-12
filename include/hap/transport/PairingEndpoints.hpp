#pragma once

#include "hap/transport/HTTP.hpp"
#include "hap/transport/ConnectionContext.hpp"
#include "hap/transport/IPairingEndpoints.hpp"
#include "hap/pairing/PairSetup.hpp"
#include "hap/pairing/PairVerify.hpp"
#include "hap/platform/Storage.hpp"
#include "hap/platform/System.hpp"
#include "hap/platform/CryptoSRP.hpp"
#include <string>
#include <array>
#include <map>
#include <functional>

namespace hap::transport {

/**
 * @brief Pairing endpoint handlers
 */
class PairingEndpoints : public IPairingEndpoints {
public:
    struct Config {
        platform::CryptoSRP* crypto;
        platform::Storage* storage;
        platform::System* system;
        std::string accessory_id;
        std::string setup_code;
        /// Callback for pairing changes: (pairing_id, ltpk, is_add)
        std::function<void(const std::string&, const std::array<uint8_t, 32>&, bool)> on_pairings_changed = nullptr;
    };

    PairingEndpoints(Config config);

    /**
     * @brief POST /pair-setup handler
     */
    Response handle_pair_setup(const Request& req, ConnectionContext& ctx) override;

    /**
     * @brief POST /pair-verify handler.
     *
     * Does NOT upgrade the connection to encrypted: per HAP, the M4 response
     * travels in cleartext and session security starts only after Pair Verify
     * completes. Call complete_pair_verify() after the response has been sent.
     */
    Response handle_pair_verify(const Request& req, ConnectionContext& ctx) override;

    /**
     * @brief Upgrade the connection to encrypted if pair-verify just completed
     * on it. Call exactly once, after the M4 response has been sent.
     */
    void complete_pair_verify(ConnectionContext& ctx);

    /**
     * @brief POST /pairings handler
     */
    Response handle_pairings(const Request& req, ConnectionContext& ctx) override;

    /**
     * @brief Update accessory ID
     */
    void set_accessory_id(const std::string& new_id);

    /**
     * @brief Reset all session state
     */
    void reset();

private:
    Config config_;
    
    // Per-connection pairing state
    std::map<uint32_t, std::unique_ptr<pairing::PairSetup>> pair_setup_sessions_;
    std::map<uint32_t, std::unique_ptr<pairing::PairVerify>> pair_verify_sessions_;
    // Connections whose pair-verify succeeded and await the post-response upgrade.
    std::map<uint32_t, std::unique_ptr<pairing::PairVerify>> pending_verify_upgrades_;
};

} // namespace hap::transport
