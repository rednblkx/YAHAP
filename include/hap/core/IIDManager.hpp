#pragma once

#include "hap/platform/Storage.hpp"
#include "hap/platform/System.hpp"
#include <map>
#include <string>
#include <cstdint>

namespace hap::core {

/**
 * @brief Manages Instance ID (IID) allocation with persistence.
 * 
 * Per HAP Spec 6.6.4:
 * - IIDs must be stable across reboots while paired
 * - IIDs for removed attributes must not be reused while paired
 * - Provides hash-based tracking for Configuration Number updates
 */
class IIDManager {
public:
    IIDManager(platform::Storage* storage, platform::System* system);
    
    /**
     * @brief Get or assign a stable IID for a service/characteristic.
     * 
     * Key format examples:
     * - Service: "S:003E:1" (type:aid)
     * - Characteristic: "C:0014:003E:1" (type:svc_type:aid)
     * 
     * @param key Unique identifier for the attribute
     * @return Stable IID (1-65535)
     */
    uint16_t get_or_assign(const std::string& key);
    
    /**
     * @brief Check if database structure changed since last check.
     * 
     * @param current_hash Hash of current database structure
     * @return true if structure changed, false otherwise
     */
    bool has_structure_changed(const std::string& current_hash);
    
    /**
     * @brief Update stored hash without triggering change detection.
     * Used after CN has been incremented.
     */
    void update_stored_hash(const std::string& hash);
    
    /**
     * @brief Save current IID map to persistent storage.
     */
    void save();
    
    /**
     * @brief Reset all IIDs (for factory reset when unpaired).
     */
    void reset();
    
    /**
     * @brief Get current next_iid value (for debugging).
     */
    uint16_t get_next_iid() const { return next_iid_; }

private:
    platform::Storage* storage_;
    platform::System* system_;
    std::map<std::string, uint16_t> iid_map_;
    uint16_t next_iid_ = 1;
    bool dirty_ = false;  // Track if save is needed
    
    void load();
};

} // namespace hap::core
