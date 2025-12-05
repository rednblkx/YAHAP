#pragma once

#include "hap/core/Accessory.hpp"
#include <vector>
#include <memory>

namespace hap::core {

/**
 * @brief HAP Attribute Database
 * Manages the collection of Accessories.
 */
class AttributeDatabase {
public:
    AttributeDatabase() = default;

    void add_accessory(std::shared_ptr<Accessory> accessory) {
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
};

} // namespace hap::core
