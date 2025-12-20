#pragma once

#include "hap/transport/SecureSession.hpp"
#include "hap/platform/System.hpp"
#include <memory>
#include <optional>
#include <set>
#include <utility>

namespace hap::transport {

/**
 * @brief Per-connection state
 */
class ConnectionContext {
public:
    ConnectionContext(platform::Crypto* crypto, platform::System* system, uint32_t connection_id);

    /**
     * @brief Get connection ID from Network PAL.
     */
    uint32_t connection_id() const { return connection_id_; }
    
    /**
     * @brief Check if this connection is encrypted.
     */
    bool is_encrypted() const { return secure_session_ != nullptr; }

    bool rx_encrypted() const { return rx_encrypted_; }
    void set_rx_encrypted(bool encrypted) { rx_encrypted_ = encrypted; }
    
    /**
     * @brief Get the secure session (only valid if is_encrypted() == true).
     */
    SecureSession* get_secure_session() { 
        return secure_session_.get(); 
    }

    /**
     * @brief Upgrade connection to secure session
     * @param session_keys Tuple of (A2C key, C2A key)
     * @param shared_secret X25519 shared secret
     * @param controller_id Authenticated controller identifier
     */
    void upgrade_to_secure(
        std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> session_keys,
        const std::array<uint8_t, 32>& shared_secret,
        std::string controller_id);

    /**
     * @brief Get session shared secret
     */
    const std::array<uint8_t, 32>& session_shared_secret() const { return session_shared_secret_; }

    const std::string& controller_id() const { return controller_id_; }
    bool is_admin() const { return !controller_id_.empty(); } // Currently all paired controllers are admins

    /**
     * @brief Reset pairing state (e.g., on disconnect).
     */
    void reset();

    void request_close() { should_close_ = true; }
    bool should_close() const { return should_close_; }

    // Subscription management
    void add_subscription(uint64_t aid, uint64_t iid);
    void remove_subscription(uint64_t aid, uint64_t iid);
    bool has_subscription(uint64_t aid, uint64_t iid) const;
    
    // Timed Write support
    void prepare_timed_write(uint64_t pid, uint64_t ttl);
    bool validate_timed_write(uint64_t pid);

private:
    platform::Crypto* crypto_;
    platform::System* system_ = nullptr;
    uint32_t connection_id_;
    std::unique_ptr<SecureSession> secure_session_;
    bool rx_encrypted_ = false;
    std::string controller_id_;
    std::array<uint8_t, 32> session_shared_secret_ = {};
    bool should_close_ = false;
    
    // Subscriptions (AID, IID)
    std::set<std::pair<uint64_t, uint64_t>> subscriptions_;
    
    // Timed Write Transaction
    struct TimedWriteTransaction {
        uint64_t pid;
        uint64_t expiration_time;
    };
    std::optional<TimedWriteTransaction> timed_write_;
};

} // namespace hap::transport
