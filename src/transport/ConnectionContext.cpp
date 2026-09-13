#include "hap/transport/ConnectionContext.hpp"

namespace hap::transport {

ConnectionContext::ConnectionContext(platform::Crypto* crypto, platform::System* system, uint32_t connection_id)
    : crypto_(crypto), system_(system), connection_id_(connection_id) {}

void ConnectionContext::upgrade_to_secure(
    std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> session_keys,
    const std::array<uint8_t, 32>& shared_secret,
    std::string controller_id) {
    auto [a_key, c_key] = session_keys;
    secure_session_ = std::make_unique<SecureSession>(crypto_, a_key, c_key);
    session_shared_secret_ = shared_secret;
    controller_id_ = std::move(controller_id);
}

void ConnectionContext::add_subscription(uint64_t aid, uint64_t iid) {
    for (const auto& sub : subscriptions_) {
        if (sub.first == aid && sub.second == iid) return;
    }
    subscriptions_.emplace_back(aid, iid);
}

void ConnectionContext::remove_subscription(uint64_t aid, uint64_t iid) {
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        if (it->first == aid && it->second == iid) {
            subscriptions_.erase(it);
            return;
        }
    }
}

bool ConnectionContext::has_subscription(uint64_t aid, uint64_t iid) const {
    for (const auto& sub : subscriptions_) {
        if (sub.first == aid && sub.second == iid) return true;
    }
    return false;
}

void ConnectionContext::prepare_timed_write(uint64_t pid, uint64_t ttl) {
    if (!system_) return;

    // Clamp the TTL: the wire value is attacker-controlled and an unclamped
    // now + ttl could wrap and make the transaction appear valid forever.
    constexpr uint64_t kMaxTimedWriteTtlMs = 60 * 1000; // HAP allows up to 60s
    if (ttl == 0 || ttl > kMaxTimedWriteTtlMs) {
        ttl = kMaxTimedWriteTtlMs;
    }
    uint64_t now = system_->millis();
    timed_write_ = TimedWriteTransaction{pid, now + ttl};
}

bool ConnectionContext::validate_timed_write(uint64_t pid) {
    if (!timed_write_ || !system_) return false;
    
    uint64_t now = system_->millis();
    if (now > timed_write_->expiration_time) {
        timed_write_.reset();
        return false;
    }
    
    if (timed_write_->pid != pid) {
        return false;
    }
    
    // Transaction consumed
    timed_write_.reset();
    return true;
}

} // namespace hap::transport
