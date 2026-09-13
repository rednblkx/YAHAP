#include "hap/transport/ble/BleSessionManager.hpp"
#include "hap/common/Log.hpp"
#include <algorithm>

namespace hap::transport::ble {

const std::vector<uint16_t> BleSessionManager::kEmptySubscribers = {};

BleSessionManager::BleSessionManager(platform::System* system)
    : system_(system) {}

BleSession& BleSessionManager::get_or_create(uint16_t connection_id) {
    for (auto& session : sessions_) {
        if (session.connection_id == connection_id) return session;
    }
    sessions_.emplace_back(connection_id);
    return sessions_.back();
}

BleSession* BleSessionManager::get_session(uint16_t connection_id) {
    for (auto& session : sessions_) {
        if (session.connection_id == connection_id) return &session;
    }
    return nullptr;
}

const BleSession* BleSessionManager::get_session(uint16_t connection_id) const {
    for (const auto& session : sessions_) {
        if (session.connection_id == connection_id) return &session;
    }
    return nullptr;
}

void BleSessionManager::remove(uint16_t connection_id) {
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it->connection_id == connection_id) {
            sessions_.erase(it);
            break;
        }
    }
    
    // Remove from all subscriptions
    for (auto& [uuid, subscribers] : subscriptions_) {
        subscribers.erase(
            std::remove(subscribers.begin(), subscribers.end(), connection_id),
            subscribers.end()
        );
    }
}

std::vector<uint16_t> BleSessionManager::check_timeouts() {
    std::vector<uint16_t> timed_out;
    
    if (!system_) {
        return timed_out;
    }
    
    uint64_t current_time = system_->millis();
    
    for (auto& session : sessions_) {
        auto& state = session.transaction;
        
        if (state.connection_established_ms > 0 && 
            !state.active && 
            state.last_activity_ms == 0) {
            uint64_t time_since_connect = current_time - state.connection_established_ms;
            if (time_since_connect > kInitialTimeoutMs) {
                HAP_LOG_WARN(system_, "[BleSessionManager] Initial procedure timeout for connection ", session.connection_id);
                timed_out.push_back(session.connection_id);
                continue;
            }
        }
        
        // Check procedure timeout (10 seconds)
        if (state.active && state.procedure_start_ms > 0) {
            uint64_t procedure_duration = current_time - state.procedure_start_ms;
            if (procedure_duration > kProcedureTimeoutMs) {
                HAP_LOG_WARN(system_, "[BleSessionManager] Procedure timeout for connection ", session.connection_id);
                timed_out.push_back(session.connection_id);
                continue;
            }
        }
        
        // Check idle timeout (30 seconds)
        if (state.last_activity_ms > 0) {
            uint64_t idle_duration = current_time - state.last_activity_ms;
            if (idle_duration > kIdleTimeoutMs) {
                HAP_LOG_INFO(system_, "[BleSessionManager] Idle timeout for connection ", session.connection_id);
                timed_out.push_back(session.connection_id);
            }
        }
    }
    
    return timed_out;
}

void BleSessionManager::add_subscription(uint16_t char_type, uint16_t connection_id) {
    for (auto& [type, subscribers] : subscriptions_) {
        if (type == char_type) {
            if (std::find(subscribers.begin(), subscribers.end(), connection_id) == subscribers.end()) {
                subscribers.push_back(connection_id);
            }
            return;
        }
    }
    subscriptions_.emplace_back(char_type, std::vector<uint16_t>{connection_id});
}

void BleSessionManager::remove_subscription(uint16_t char_type, uint16_t connection_id) {
    for (auto& [type, subscribers] : subscriptions_) {
        if (type == char_type) {
            subscribers.erase(
                std::remove(subscribers.begin(), subscribers.end(), connection_id),
                subscribers.end());
            return;
        }
    }
}

const std::vector<uint16_t>& BleSessionManager::get_subscribers(uint16_t char_type) const {
    for (const auto& [type, subscribers] : subscriptions_) {
        if (type == char_type) return subscribers;
    }
    return kEmptySubscribers;
}

bool BleSessionManager::has_subscribers(uint16_t char_type) const {
    for (const auto& [type, subscribers] : subscriptions_) {
        if (type == char_type) return !subscribers.empty();
    }
    return false;
}

} // namespace hap::transport::ble
