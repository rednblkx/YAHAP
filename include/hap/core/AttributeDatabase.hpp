#pragma once

#include "hap/core/Accessory.hpp"
#include "hap/core/HAPValidation.hpp"
#include "hap/core/IIDManager.hpp"
#include "hap/common/JsonValue.hpp"
#include <charconv>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include <memory>

namespace hap::core {

/**
 * @brief Convert a characteristic Value to its JSON representation.
 *
 * Binary values (TLV8/Data) are base64-encoded; this is the single source of
 * truth for value serialization in event notifications, /accessories, and
 * /characteristics responses.
 */
hap::common::JsonValue value_to_json(const Value& value);

// Base64 encoding used by value_to_json; exposed for the endpoints that
// decode values back from JSON.
std::string base64_encode(const std::vector<uint8_t>& data);

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
    ValidationResult validate_accessory(const Accessory& accessory) const {
        // Check for duplicate AID (always applies)
        for (const auto& existing : accessories_) {
            if (existing->aid() == accessory.aid()) {
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
        if (accessory.services().size() > HAPValidation::kMaxServicesPerAccessory) {
            return ValidationResult::TooManyServices;
        }
        
        // Check characteristic limit - universal (HAP Spec 6.11 test 15)
        for (const auto& service : accessory.services()) {
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
    ValidationResult add_accessory(std::unique_ptr<Accessory> accessory) {
        ValidationResult result = validate_accessory(*accessory);
        if (result != ValidationResult::Success) {
            return result;
        }
        
        assign_iids(*accessory);
        accessories_.push_back(std::move(accessory));
        return ValidationResult::Success;
    }


    const std::vector<std::unique_ptr<Accessory>>& accessories() const {
        return accessories_;
    }

    Characteristic* find_characteristic(uint64_t aid, uint64_t iid) {
        for (const auto& acc : accessories_) {
            if (acc->aid() == aid) {
                for (const auto& svc : acc->services()) {
                    for (const auto& char_ptr : svc->characteristics()) {
                        if (char_ptr->iid() == iid) {
                            return char_ptr.get();
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Find a characteristic anywhere in the database by IID.
     * IIDs are unique across a bridge, so the first match is authoritative.
     */
    Characteristic* find_characteristic_by_iid(uint16_t iid) {
        for (const auto& acc : accessories_) {
            for (const auto& svc : acc->services()) {
                for (const auto& char_ptr : svc->characteristics()) {
                    if (char_ptr->iid() == iid) {
                        return char_ptr.get();
                    }
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Find a service by IID (services are also uniquely numbered).
     */
    Service* find_service_by_iid(uint16_t iid) {
        for (const auto& acc : accessories_) {
            for (const auto& svc : acc->services()) {
                if (svc->iid() == iid) {
                    return svc.get();
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Locate a characteristic plus its containing service and accessory.
     */
    struct CharacteristicLocation {
        Characteristic* characteristic = nullptr;
        Service* service = nullptr;
        uint64_t accessory_id = 0;
    };

    [[nodiscard]] CharacteristicLocation find_characteristic_info(uint16_t iid) {
        for (const auto& acc : accessories_) {
            for (const auto& svc : acc->services()) {
                for (const auto& char_ptr : svc->characteristics()) {
                    if (char_ptr->iid() == iid) {
                        return {char_ptr.get(), svc.get(), acc->aid()};
                    }
                }
            }
        }
        return {};
    }

    std::string to_json_string() const;

private:
    /**
     * @brief Assign IIDs to all services and characteristics in an accessory.
     */
    void assign_iids(Accessory& accessory) {
        uint64_t aid = accessory.aid();

        // Stable keys embed the attribute type as 4 uppercase hex digits.
        auto append_hex4 = [](std::string& out, uint16_t v) {
            static const char* kHex = "0123456789ABCDEF";
            out.push_back(kHex[(v >> 12) & 0xF]);
            out.push_back(kHex[(v >> 8) & 0xF]);
            out.push_back(kHex[(v >> 4) & 0xF]);
            out.push_back(kHex[v & 0xF]);
        };
        auto append_decimal = [](std::string& out, uint64_t v) {
            char digits[20];
            auto [ptr, ec] = std::to_chars(digits, digits + sizeof(digits), v);
            out.append(digits, ptr);
        };

        for (const auto& service : accessory.services()) {
            uint16_t svc_iid;

            if (iid_manager_) {
                // Use IIDManager for stable IIDs
                // Key format: "S:<type>:<aid>"
                std::string key = "S:";
                append_hex4(key, static_cast<uint16_t>(service->type() & 0xFFFF));
                key += ":";
                append_decimal(key, aid);
                svc_iid = iid_manager_->get_or_assign(key);
            } else {
                // Fallback to sequential assignment
                svc_iid = next_iid_++;
            }
            service->set_iid(svc_iid);

            for (const auto& characteristic : service->characteristics()) {
                uint16_t char_iid;

                if (iid_manager_) {
                    // Key format: "C:<char_type>:<svc_type>:<aid>"
                    std::string key = "C:";
                    append_hex4(key, static_cast<uint16_t>(characteristic->type() & 0xFFFF));
                    key += ":";
                    append_hex4(key, static_cast<uint16_t>(service->type() & 0xFFFF));
                    key += ":";
                    append_decimal(key, aid);
                    char_iid = iid_manager_->get_or_assign(key);
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

    std::vector<std::unique_ptr<Accessory>> accessories_;
    IIDManager* iid_manager_ = nullptr;
    uint16_t next_iid_ = 1;  // Fallback when no IIDManager
};

} // namespace hap::core
