#include "hap/transport/ble/BleSessionManager.hpp"
#include <algorithm>

namespace hap::transport::ble {

const std::vector<uint16_t> BleSessionManager::kEmptySubscribers = {};

BleSessionManager::BleSessionManager(platform::System* system)
    : system_(system) {}

BleSession& BleSessionManager::get_or_create(uint16_t connection_id) {
    auto it = sessions_.find(connection_id);
    if (it == sessions_.end()) {
        auto [iter, _] = sessions_.emplace(connection_id, BleSession{connection_id});
        return iter->second;
    }
    return it->second;
}

bool BleSessionManager::has_session(uint16_t connection_id) const {
    return sessions_.find(connection_id) != sessions_.end();
}

BleSession* BleSessionManager::get_session(uint16_t connection_id) {
    auto it = sessions_.find(connection_id);
    return it != sessions_.end() ? &it->second : nullptr;
}

const BleSession* BleSessionManager::get_session(uint16_t connection_id) const {
    auto it = sessions_.find(connection_id);
    return it != sessions_.end() ? &it->second : nullptr;
}

void BleSessionManager::remove(uint16_t connection_id) {
    sessions_.erase(connection_id);
    
    // Remove from all subscriptions
    for (auto& [uuid, subscribers] : subscriptions_) {
        subscribers.erase(
            std::remove(subscribers.begin(), subscribers.end(), connection_id),
            subscribers.end()
        );
    }
}

void BleSessionManager::clear() {
    sessions_.clear();
    subscriptions_.clear();
}

std::vector<uint16_t> BleSessionManager::check_timeouts() {
    std::vector<uint16_t> timed_out;
    
    if (!system_) {
        return timed_out;
    }
    
    uint64_t current_time = system_->millis();
    
    for (auto& [conn_id, session] : sessions_) {
        auto& state = session.transaction;
        
        if (state.connection_established_ms > 0 && 
            !state.active && 
            state.last_activity_ms == 0) {
            uint64_t time_since_connect = current_time - state.connection_established_ms;
            if (time_since_connect > kInitialTimeoutMs) {
                system_->log(platform::System::LogLevel::Warning,
                    "[BleSessionManager] Initial procedure timeout for connection " + 
                    std::to_string(conn_id));
                timed_out.push_back(conn_id);
                continue;
            }
        }
        
        // Check procedure timeout (10 seconds)
        if (state.active && state.procedure_start_ms > 0) {
            uint64_t procedure_duration = current_time - state.procedure_start_ms;
            if (procedure_duration > kProcedureTimeoutMs) {
                system_->log(platform::System::LogLevel::Warning,
                    "[BleSessionManager] Procedure timeout for connection " + 
                    std::to_string(conn_id));
                timed_out.push_back(conn_id);
                continue;
            }
        }
        
        // Check idle timeout (30 seconds)
        if (state.last_activity_ms > 0) {
            uint64_t idle_duration = current_time - state.last_activity_ms;
            if (idle_duration > kIdleTimeoutMs) {
                system_->log(platform::System::LogLevel::Info,
                    "[BleSessionManager] Idle timeout for connection " + 
                    std::to_string(conn_id));
                timed_out.push_back(conn_id);
            }
        }
    }
    
    return timed_out;
}

std::vector<uint16_t> BleSessionManager::get_connection_ids() const {
    std::vector<uint16_t> ids;
    ids.reserve(sessions_.size());
    for (const auto& [conn_id, _] : sessions_) {
        ids.push_back(conn_id);
    }
    return ids;
}

void BleSessionManager::add_subscription(const std::string& uuid, uint16_t connection_id) {
    auto& subscribers = subscriptions_[uuid];
    if (std::find(subscribers.begin(), subscribers.end(), connection_id) == subscribers.end()) {
        subscribers.push_back(connection_id);
    }
}

void BleSessionManager::remove_subscription(const std::string& uuid, uint16_t connection_id) {
    auto it = subscriptions_.find(uuid);
    if (it != subscriptions_.end()) {
        auto& subscribers = it->second;
        subscribers.erase(
            std::remove(subscribers.begin(), subscribers.end(), connection_id),
            subscribers.end()
        );
    }
}

const std::vector<uint16_t>& BleSessionManager::get_subscribers(const std::string& uuid) const {
    auto it = subscriptions_.find(uuid);
    return it != subscriptions_.end() ? it->second : kEmptySubscribers;
}

bool BleSessionManager::has_subscribers(const std::string& uuid) const {
    auto it = subscriptions_.find(uuid);
    return it != subscriptions_.end() && !it->second.empty();
}

} // namespace hap::transport::ble
