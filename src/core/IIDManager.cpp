#include "hap/core/IIDManager.hpp"
#include "hap/common/Log.hpp"
#include <charconv>

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
        std::string_view data(data_str);
        size_t pos = 0;
        while (pos < data.size()) {
            size_t eol = data.find('\n', pos);
            if (eol == std::string_view::npos) eol = data.size();
            std::string_view line = data.substr(pos, eol - pos);
            pos = eol + 1;

            auto sep = line.find('=');
            if (sep != std::string_view::npos) {
                std::string key(line.substr(0, sep));
                std::string_view value_str = line.substr(sep + 1);
                uint16_t iid = 0;
                auto [ptr, ec] = std::from_chars(value_str.begin(), value_str.end(), iid);
                if (ec == std::errc() && ptr == value_str.end() && iid != 0) {
                    iid_map_.push_back({std::move(key), iid});
                } else if (system_) {
                    // Corrupted persisted entry: skip rather than throw.
                    HAP_LOG_WARN(system_, "[IIDManager] Skipping malformed IID entry: ", key);
                }
            }
        }
    }
    
    HAP_LOG(system_, "[IIDManager] Loaded ", iid_map_.size(), " entries, next_iid=", next_iid_);
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
    std::string map_str;
    map_str.reserve(iid_map_.size() * 16);
    for (const auto& [key, iid] : iid_map_) {
        map_str += key;
        map_str += '=';
        char digits[6];
        auto [ptr, ec] = std::to_chars(digits, digits + sizeof(digits), iid);
        map_str.append(digits, ptr);
        map_str += '\n';
    }
    std::vector<uint8_t> map_data(map_str.begin(), map_str.end());
    storage_->set(kIIDMapKey, map_data);
    
    dirty_ = false;
    
    HAP_LOG(system_, "[IIDManager] Saved ", iid_map_.size(), " entries, next_iid=", next_iid_);
}

uint16_t IIDManager::get_or_assign(const std::string& key) {
    auto it = find_entry(key);
    if (it != iid_map_.end()) {
        return it->iid;
    }
    
    // Assign new IID
    uint16_t iid = next_iid_++;
    
    // Handle overflow (wrap from 65535 to 1, skip 0)
    if (next_iid_ == 0) {
        next_iid_ = 1;
    }
    
    iid_map_.push_back(IidEntry{key, iid});
    dirty_ = true;
    
    HAP_LOG(system_, "[IIDManager] Assigned IID=", iid, " for key=", key);
    
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

std::vector<IIDManager::IidEntry>::iterator IIDManager::find_entry(const std::string& key) {
    // The map holds a few dozen entries at most and is only walked during
    // startup registration, so a linear scan beats keeping it sorted.
    for (auto it = iid_map_.begin(); it != iid_map_.end(); ++it) {
        if (it->key == key) return it;
    }
    return iid_map_.end();
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
    
    HAP_LOG_INFO(system_, "[IIDManager] Reset - all IIDs cleared");
}

} // namespace hap::core
