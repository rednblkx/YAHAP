#include "hap/transport/ConnectionContext.hpp"

namespace hap::transport {

ConnectionContext::ConnectionContext(platform::Crypto* crypto, platform::System* system, uint32_t connection_id)
    : crypto_(crypto), system_(system), connection_id_(connection_id) {}

void ConnectionContext::upgrade_to_secure(std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> session_keys, std::string controller_id) {
    auto [a_key, c_key] = session_keys;
    secure_session_ = std::make_unique<SecureSession>(crypto_, a_key, c_key);
    controller_id_ = std::move(controller_id);
}

void ConnectionContext::reset() {
    secure_session_.reset();
    rx_encrypted_ = false;
    subscriptions_.clear();
    controller_id_.clear();
    timed_write_.reset();
}

void ConnectionContext::add_subscription(uint64_t aid, uint64_t iid) {
    subscriptions_.insert({aid, iid});
}

void ConnectionContext::remove_subscription(uint64_t aid, uint64_t iid) {
    subscriptions_.erase({aid, iid});
}

bool ConnectionContext::has_subscription(uint64_t aid, uint64_t iid) const {
    return subscriptions_.find({aid, iid}) != subscriptions_.end();
}

void ConnectionContext::prepare_timed_write(uint64_t pid, uint64_t ttl) {
    if (system_) {
        uint64_t now = system_->millis();
        timed_write_ = TimedWriteTransaction{pid, now + ttl};
    }
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
