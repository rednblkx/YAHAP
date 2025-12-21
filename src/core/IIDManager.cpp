#include "hap/core/IIDManager.hpp"
#include <sstream>

namespace hap::core {

// Storage keys
static constexpr const char* kIIDMapKey = "iid_map";
static constexpr const char* kIIDNextKey = "iid_next";
static constexpr const char* kDBHashKey = "db_hash";

IIDManager::IIDManager(platform::Storage* storage, platform::System* system)
    : storage_(storage), system_(system) {
    load();
}

void IIDManager::load() {
    if (!storage_) return;
    
    // Load next_iid
    auto next_data = storage_->get(kIIDNextKey);
    if (next_data && next_data->size() >= 2) {
        next_iid_ = static_cast<uint16_t>((*next_data)[0]) | 
                   (static_cast<uint16_t>((*next_data)[1]) << 8);
        if (next_iid_ == 0) next_iid_ = 1;  // Ensure minimum of 1
    }
    
    // Load IID map (simple key=value format, one per line)
    auto map_data = storage_->get(kIIDMapKey);
    if (map_data && !map_data->empty()) {
        std::string data_str(map_data->begin(), map_data->end());
        std::istringstream iss(data_str);
        std::string line;
        while (std::getline(iss, line)) {
            auto sep = line.find('=');
            if (sep != std::string::npos) {
                std::string key = line.substr(0, sep);
                uint16_t iid = static_cast<uint16_t>(std::stoi(line.substr(sep + 1)));
                iid_map_[key] = iid;
            }
        }
    }
    
    if (system_) {
        system_->log(platform::System::LogLevel::Debug, 
            "[IIDManager] Loaded " + std::to_string(iid_map_.size()) + 
            " entries, next_iid=" + std::to_string(next_iid_));
    }
}

void IIDManager::save() {
    if (!storage_ || !dirty_) return;
    
    // Save next_iid (little-endian)
    std::vector<uint8_t> next_data = {
        static_cast<uint8_t>(next_iid_ & 0xFF),
        static_cast<uint8_t>((next_iid_ >> 8) & 0xFF)
    };
    storage_->set(kIIDNextKey, next_data);
    
    // Save IID map (simple key=value format)
    std::ostringstream oss;
    for (const auto& [key, iid] : iid_map_) {
        oss << key << "=" << iid << "\n";
    }
    std::string map_str = oss.str();
    std::vector<uint8_t> map_data(map_str.begin(), map_str.end());
    storage_->set(kIIDMapKey, map_data);
    
    dirty_ = false;
    
    if (system_) {
        system_->log(platform::System::LogLevel::Debug, 
            "[IIDManager] Saved " + std::to_string(iid_map_.size()) + 
            " entries, next_iid=" + std::to_string(next_iid_));
    }
}

uint16_t IIDManager::get_or_assign(const std::string& key) {
    auto it = iid_map_.find(key);
    if (it != iid_map_.end()) {
        return it->second;
    }
    
    // Assign new IID
    uint16_t iid = next_iid_++;
    
    // Handle overflow (wrap from 65535 to 1, skip 0)
    if (next_iid_ == 0) {
        next_iid_ = 1;
    }
    
    iid_map_[key] = iid;
    dirty_ = true;
    
    if (system_) {
        system_->log(platform::System::LogLevel::Debug, 
            "[IIDManager] Assigned IID=" + std::to_string(iid) + " for key=" + key);
    }
    
    return iid;
}

bool IIDManager::has_structure_changed(const std::string& current_hash) {
    if (!storage_) return false;
    
    auto stored_hash_data = storage_->get(kDBHashKey);
    if (!stored_hash_data || stored_hash_data->empty()) {
        return true;  // No stored hash = first run or reset
    }
    
    std::string stored_hash(stored_hash_data->begin(), stored_hash_data->end());
    return stored_hash != current_hash;
}

void IIDManager::update_stored_hash(const std::string& hash) {
    if (!storage_) return;
    
    std::vector<uint8_t> hash_data(hash.begin(), hash.end());
    storage_->set(kDBHashKey, hash_data);
}

void IIDManager::reset() {
    iid_map_.clear();
    next_iid_ = 1;
    dirty_ = true;
    
    if (storage_) {
        storage_->remove(kIIDMapKey);
        storage_->remove(kIIDNextKey);
        storage_->remove(kDBHashKey);
    }
    
    if (system_) {
        system_->log(platform::System::LogLevel::Info, 
            "[IIDManager] Reset - all IIDs cleared");
    }
}

} // namespace hap::core
