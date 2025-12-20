#pragma once

#include "hap/core/Characteristic.hpp"
#include <vector>
#include <memory>
#include <string>

namespace hap::core {

/**
 * @brief HAP Service
 */
class Service {
public:
    Service(uint64_t type, std::string name, bool is_primary = false)
        : type_(type), name_(std::move(name)), is_primary_(is_primary) {}

    virtual ~Service() = default;

    void add_characteristic(std::shared_ptr<Characteristic> characteristic) {
        characteristics_.push_back(std::move(characteristic));
    }

    const std::vector<std::shared_ptr<Characteristic>>& characteristics() const {
        return characteristics_;
    }

    uint64_t type() const { return type_; }
    const std::string& name() const { return name_; }
    bool is_primary() const { return is_primary_; }
    void set_primary(bool primary) { is_primary_ = primary; }
    
    uint64_t iid() const { return iid_; }
    void set_iid(uint64_t iid) { iid_ = iid; }

    void set_hidden(bool hidden) { hidden_ = hidden; }
    bool is_hidden() const { return hidden_; }

    void add_linked_service(uint64_t iid) { linked_services_.push_back(iid); }
    const std::vector<uint64_t>& linked_services() const { return linked_services_; }

private:
    uint64_t type_;
    std::string name_;
    bool is_primary_;
    bool hidden_ = false;
    uint64_t iid_ = 0;
    
    std::vector<std::shared_ptr<Characteristic>> characteristics_;
    std::vector<uint64_t> linked_services_;
};

} // namespace hap::core
