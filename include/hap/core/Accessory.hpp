#pragma once

#include "hap/core/Service.hpp"
#include <vector>
#include <memory>

namespace hap::core {

enum class AccessoryCategory : uint16_t {
    Other = 1,
    Bridge = 2,
    Fan = 3,
    GarageDoorOpener = 4,
    Lightbulb = 5,
    DoorLock = 6,
    Outlet = 7,
    Switch = 8,
    Thermostat = 9,
    Sensor = 10,
    SecuritySystem = 11,
    Door = 12,
    Window = 13,
    WindowCovering = 14,
    ProgrammableSwitch = 15,
    RangeExtender = 16,
    IPCamera = 17,
    VideoDoorbell = 18,
    AirPurifier = 19,
    Heater = 20,
    AirConditioner = 21,
    Humidifier = 22,
    Dehumidifier = 23,
    AppleTV = 24,
    HomePodMini = 25,
    Speaker = 26,
    AirPort = 27,
    Sprinkler = 28,
    Faucet = 29,
    ShowerSystem = 30,
    Television = 31,
    RemoteControl = 32,
    WiFiRouter = 33,
    AudioReceiver = 34,
    TVSetTopBox = 35,
    TVStreamingStick = 36
};

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
