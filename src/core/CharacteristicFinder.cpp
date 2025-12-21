#include "hap/core/CharacteristicFinder.hpp"

namespace hap::core {

CharacteristicFinder::CharacteristicFinder(const AttributeDatabase& database)
    : database_(database) {}

std::shared_ptr<Characteristic> CharacteristicFinder::by_iid(uint16_t iid) const {
    for (const auto& accessory : database_.accessories()) {
        for (const auto& service : accessory->services()) {
            for (const auto& characteristic : service->characteristics()) {
                if (characteristic->iid() == iid) {
                    return characteristic;
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Characteristic> CharacteristicFinder::by_aid_iid(uint64_t aid, uint64_t iid) const {
    for (const auto& accessory : database_.accessories()) {
        if (accessory->aid() != aid) continue;
        
        for (const auto& service : accessory->services()) {
            for (const auto& characteristic : service->characteristics()) {
                if (characteristic->iid() == iid) {
                    return characteristic;
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Service> CharacteristicFinder::service_by_iid(uint16_t iid) const {
    for (const auto& accessory : database_.accessories()) {
        for (const auto& service : accessory->services()) {
            if (service->iid() == iid) {
                return service;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Service> CharacteristicFinder::service_containing(uint16_t char_iid) const {
    for (const auto& accessory : database_.accessories()) {
        for (const auto& service : accessory->services()) {
            for (const auto& characteristic : service->characteristics()) {
                if (characteristic->iid() == char_iid) {
                    return service;
                }
            }
        }
    }
    return nullptr;
}

CharacteristicFinder::CharacteristicInfo CharacteristicFinder::find_info(uint16_t iid) const {
    CharacteristicInfo info;
    
    for (const auto& accessory : database_.accessories()) {
        for (const auto& service : accessory->services()) {
            for (const auto& characteristic : service->characteristics()) {
                if (characteristic->iid() == iid) {
                    info.characteristic = characteristic;
                    info.service = service;
                    info.accessory_id = accessory->aid();
                    return info;
                }
            }
        }
    }
    
    return info;
}

} // namespace hap::core
