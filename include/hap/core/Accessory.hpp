#pragma once

#include "hap/core/Service.hpp"
#include <vector>
#include <memory>

namespace hap::core {

/**
 * @brief HAP Accessory
 */
class Accessory {
public:
    Accessory(uint64_t aid) : aid_(aid) {}
    virtual ~Accessory() = default;

    void add_service(std::shared_ptr<Service> service) {
        services_.push_back(std::move(service));
    }

    const std::vector<std::shared_ptr<Service>>& services() const {
        return services_;
    }

    uint64_t aid() const { return aid_; }

private:
    uint64_t aid_;
    std::vector<std::shared_ptr<Service>> services_;
};

} // namespace hap::core
