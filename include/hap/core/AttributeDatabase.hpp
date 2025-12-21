#pragma once

#include "hap/core/Accessory.hpp"
#include "hap/core/IIDManager.hpp"
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>

namespace hap::core {

/**
 * @brief HAP Attribute Database
 * Manages the collection of Accessories.
 */
class AttributeDatabase {
public:
    AttributeDatabase() = default;

    /**
     * @brief Set the IIDManager for persistent IID allocation.
     * Must be called before adding accessories if persistence is desired.
     */
    void set_iid_manager(IIDManager* manager) {
        iid_manager_ = manager;
    }

    void add_accessory(std::shared_ptr<Accessory> accessory) {
        uint64_t aid = accessory->aid();
        
        for (const auto& service : accessory->services()) {
            uint16_t svc_iid;
            
            if (iid_manager_) {
                // Use IIDManager for stable IIDs
                // Key format: "S:<type>:<aid>"
                std::ostringstream key;
                key << "S:" << std::hex << std::uppercase << std::setfill('0') 
                    << std::setw(4) << (service->type() & 0xFFFF) << ":" << std::dec << aid;
                svc_iid = iid_manager_->get_or_assign(key.str());
            } else {
                // Fallback to sequential assignment
                svc_iid = next_iid_++;
            }
            service->set_iid(svc_iid);
            
            for (const auto& characteristic : service->characteristics()) {
                uint16_t char_iid;
                
                if (iid_manager_) {
                    // Key format: "C:<char_type>:<svc_type>:<aid>"
                    std::ostringstream key;
                    key << "C:" << std::hex << std::uppercase << std::setfill('0')
                        << std::setw(4) << (characteristic->type() & 0xFFFF) << ":"
                        << std::setw(4) << (service->type() & 0xFFFF) << ":" << std::dec << aid;
                    char_iid = iid_manager_->get_or_assign(key.str());
                } else {
                    char_iid = next_iid_++;
                }
                characteristic->set_iid(char_iid);
            }
        }
        
        if (iid_manager_) {
            iid_manager_->save();
        }
        
        accessories_.push_back(std::move(accessory));
    }

    const std::vector<std::shared_ptr<Accessory>>& accessories() const {
        return accessories_;
    }

    // Helper to find characteristic by AID and IID
    std::shared_ptr<Characteristic> find_characteristic(uint64_t aid, uint64_t iid) {
        for (const auto& acc : accessories_) {
            if (acc->aid() == aid) {
                for (const auto& svc : acc->services()) {
                    for (const auto& char_ptr : svc->characteristics()) {
                        if (char_ptr->iid() == iid) {
                            return char_ptr;
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    // Serialize to JSON for /accessories endpoint
    std::string to_json_string() const;

private:
    std::vector<std::shared_ptr<Accessory>> accessories_;
    IIDManager* iid_manager_ = nullptr;
    uint16_t next_iid_ = 1;  // Fallback when no IIDManager
};

} // namespace hap::core

