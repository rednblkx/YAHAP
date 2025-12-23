#pragma once

#include "hap/core/Accessory.hpp"
#include "hap/core/HAPValidation.hpp"
#include "hap/core/IIDManager.hpp"
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>

namespace hap::core {

static inline std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char* encoding_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);
    
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t val = (data[i] << 16);
        if (i + 1 < data.size()) val |= (data[i + 1] << 8);
        if (i + 2 < data.size()) val |= data[i + 2];
        
        encoded.push_back(encoding_table[(val >> 18) & 0x3F]);
        encoded.push_back(encoding_table[(val >> 12) & 0x3F]);
        encoded.push_back(i + 1 < data.size() ? encoding_table[(val >> 6) & 0x3F] : '=');
        encoded.push_back(i + 2 < data.size() ? encoding_table[val & 0x3F] : '=');
    }
    return encoded;
}

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

    /**
     * @brief Validate an accessory against HAP specification limits.
     * 
     * Universal limits (HAP Spec 6.11):
     * - Max 100 services per accessory
     * - Max 100 characteristics per service
     * 
     * Bridge-only limit (HAP Spec 2.5.3.2):
     * - Max 150 accessories (only checked when adding 2nd+ accessory)
     * 
     * @param accessory The accessory to validate
     * @return ValidationResult indicating success or specific failure
     */
    ValidationResult validate_accessory(const std::shared_ptr<Accessory>& accessory) const {
        // Check for duplicate AID (always applies)
        for (const auto& existing : accessories_) {
            if (existing->aid() == accessory->aid()) {
                return ValidationResult::DuplicateAccessoryId;
            }
        }
        
        // Check bridge accessory limit (only applies when adding 2nd+ accessory)
        // HAP Spec 2.5.3.2: "A bridge must not expose more than 150 HAP accessory objects"
        if (!accessories_.empty() && 
            accessories_.size() >= HAPValidation::kMaxAccessoriesPerBridge) {
            return ValidationResult::TooManyAccessories;
        }
        
        // Check service limit - universal (HAP Spec 6.11 test 16)
        if (accessory->services().size() > HAPValidation::kMaxServicesPerAccessory) {
            return ValidationResult::TooManyServices;
        }
        
        // Check characteristic limit - universal (HAP Spec 6.11 test 15)
        for (const auto& service : accessory->services()) {
            if (service->characteristics().size() > HAPValidation::kMaxCharacteristicsPerService) {
                return ValidationResult::TooManyCharacteristics;
            }
        }
        
        return ValidationResult::Success;
    }

    /**
     * @brief Add an accessory with HAP specification validation.
     * @param accessory The accessory to add
     * @return ValidationResult indicating success or specific failure
     */
    ValidationResult add_accessory(std::shared_ptr<Accessory> accessory) {
        ValidationResult result = validate_accessory(accessory);
        if (result != ValidationResult::Success) {
            return result;
        }
        
        assign_iids(accessory);
        accessories_.push_back(std::move(accessory));
        return ValidationResult::Success;
    }


    const std::vector<std::shared_ptr<Accessory>>& accessories() const {
        return accessories_;
    }

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

    std::string to_json_string() const;

private:
    /**
     * @brief Assign IIDs to all services and characteristics in an accessory.
     */
    void assign_iids(const std::shared_ptr<Accessory>& accessory) {
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
    }

    std::vector<std::shared_ptr<Accessory>> accessories_;
    IIDManager* iid_manager_ = nullptr;
    uint16_t next_iid_ = 1;  // Fallback when no IIDManager
};

} // namespace hap::core
