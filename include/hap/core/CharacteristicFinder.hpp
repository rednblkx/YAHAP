#pragma once

#include "hap/core/AttributeDatabase.hpp"
#include <memory>

namespace hap::core {

/**
 * @brief Utility for finding characteristics and services in the database.
 * 
 * Provides centralized lookup methods to avoid duplicating search logic
 * across transport layers.
 */
class CharacteristicFinder {
public:
    explicit CharacteristicFinder(const AttributeDatabase& database);
    
    /**
     * @brief Find a characteristic by its instance ID.
     * @param iid Instance ID
     * @return Characteristic pointer or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<Characteristic> by_iid(uint16_t iid) const;
    
    /**
     * @brief Find a characteristic by accessory ID and instance ID.
     * @param aid Accessory ID
     * @param iid Instance ID
     * @return Characteristic pointer or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<Characteristic> by_aid_iid(uint64_t aid, uint64_t iid) const;
    
    /**
     * @brief Find a service by its instance ID.
     * @param iid Instance ID
     * @return Service pointer or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<Service> service_by_iid(uint16_t iid) const;
    
    /**
     * @brief Find the service that contains a characteristic.
     * @param char_iid Characteristic instance ID
     * @return Service pointer or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<Service> service_containing(uint16_t char_iid) const;
    
    /**
     * @brief Find characteristic and containing service information.
     */
    struct CharacteristicInfo {
        std::shared_ptr<Characteristic> characteristic;
        std::shared_ptr<Service> service;
        uint64_t accessory_id = 0;
    };
    
    /**
     * @brief Find characteristic info by IID.
     */
    [[nodiscard]] CharacteristicInfo find_info(uint16_t iid) const;

private:
    const AttributeDatabase& database_;
};

} // namespace hap::core
