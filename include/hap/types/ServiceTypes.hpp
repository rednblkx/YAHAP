/**
 * @file ServiceTypes.hpp
 * @brief HAP service type constants and a generic table-driven ServiceBuilder
 *
 * One ServiceBuilder replaces the ~30 per-service builder classes: it takes
 * the service type/name, adds characteristics by CharId (IIDs assigned
 * sequentially starting at 2), sets values, and installs write callbacks.
 * Characteristic metadata comes from the CharacteristicTypes catalog.
 *
 * add(CharId) returns a CharacteristicRef bound to the just-added
 * characteristic, so set()/on_write*() calls need no type argument. Install
 * callbacks (and one-call values) immediately after the corresponding add();
 * add() forwards to the builder so the fluent chain continues:
 *
 *   auto svc = hap::service::ServiceBuilder(hap::service::kType_LightBulb,
 *                                           "Lightbulb", true)
 *       .add(hap::characteristic::CharId::On)                    // required
 *       .on_write_bool([](bool on) { ... })
 *       .add(hap::characteristic::CharId::Brightness)            // optional
 *       .on_write_int([](int v) { ... })
 *       .build();
 */
#pragma once

#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/types/CharacteristicTypes.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace hap::service {

//==============================================================================
// Service UUID Type Constants
//==============================================================================

constexpr uint64_t kType_AccessoryInformation = 0x3E;       // 9.1
constexpr uint64_t kType_GarageDoorOpener = 0x41;           // 9.4
constexpr uint64_t kType_LightBulb = 0x43;                  // 9.5
constexpr uint64_t kType_LockManagement = 0x44;             // 9.6
constexpr uint64_t kType_LockMechanism = 0x45;              // 9.7
constexpr uint64_t kType_NFCAccess = 0x266;
constexpr uint64_t kType_Outlet = 0x47;                     // 9.8
constexpr uint64_t kType_Switch = 0x49;                     // 9.9
constexpr uint64_t kType_Thermostat = 0x4A;                 // 9.10
constexpr uint64_t kType_SecuritySystem = 0x7E;             // 9.12
constexpr uint64_t kType_CarbonMonoxideSensor = 0x7F;       // 9.13
constexpr uint64_t kType_ContactSensor = 0x80;              // 9.14
constexpr uint64_t kType_Door = 0x81;                       // 9.15
constexpr uint64_t kType_HumiditySensor = 0x82;             // 9.16
constexpr uint64_t kType_LeakSensor = 0x83;                 // 9.17
constexpr uint64_t kType_LightSensor = 0x84;                // 9.18
constexpr uint64_t kType_MotionSensor = 0x85;               // 9.19
constexpr uint64_t kType_OccupancySensor = 0x86;            // 9.20
constexpr uint64_t kType_SmokeSensor = 0x87;                // 9.21
constexpr uint64_t kType_StatelessProgrammableSwitch = 0x89; // 9.22
constexpr uint64_t kType_TemperatureSensor = 0x8A;          // 9.23
constexpr uint64_t kType_Window = 0x8B;                     // 9.24
constexpr uint64_t kType_WindowCovering = 0x8C;             // 9.25
constexpr uint64_t kType_AirQualitySensor = 0x8D;           // 9.11
constexpr uint64_t kType_BatteryService = 0x96;             // 9.26
constexpr uint64_t kType_CarbonDioxideSensor = 0x97;        // 9.27
constexpr uint64_t kType_HAPProtocolInformation = 0xA2;     // 9.2
constexpr uint64_t kType_Fan_v2 = 0xB7;                     // 9.32 - Fan v2 (requires Active)
constexpr uint64_t kType_Slat = 0xB9;                       // 9.33
constexpr uint64_t kType_FilterMaintenance = 0xBA;          // 9.34
constexpr uint64_t kType_AirPurifier = 0xBB;                // 9.35
constexpr uint64_t kType_HeaterCooler = 0xBC;               // 9.36
constexpr uint64_t kType_HumidifierDehumidifier = 0xBD;     // 9.37
constexpr uint64_t kType_ServiceLabel = 0xCC;               // 9.38
constexpr uint64_t kType_IrrigationSystem = 0xCF;           // 9.39
constexpr uint64_t kType_Valve = 0xD0;                      // 9.40
constexpr uint64_t kType_Faucet = 0xD7;                     // 9.41
constexpr uint64_t kType_Microphone = 0x112;                // 9.29
constexpr uint64_t kType_Speaker = 0x113;                   // 9.30
constexpr uint64_t kType_Doorbell = 0x121;                  // 9.31

//==============================================================================
// Generic service builder
//==============================================================================

class ServiceBuilder;

/**
 * @brief Handle to the characteristic most recently added via ServiceBuilder.
 *
 * Returned by ServiceBuilder::add(); set()/on_write*() operate on the bound
 * characteristic without a type argument, and add() forwards to the owning
 * builder so a fluent chain can continue after configuring a characteristic.
 * The ref is invalidated by ServiceBuilder::build() (the builder is single-use
 * anyway) and holds no ownership.
 */
class CharacteristicRef {
public:
    /// Overwrite the current value of the bound characteristic.
    CharacteristicRef& set(core::Value value);

    /// Install a write callback that receives the raw Value.
    CharacteristicRef& on_write(core::Characteristic::WriteCallback cb);

    /// Install a write callback receiving the value as bool (On style).
    CharacteristicRef& on_write_bool(std::function<void(bool)> cb);

    /// Zero-argument variant (Identify style: the write itself is the signal).
    CharacteristicRef& on_write_bool(std::function<void()> cb);

    /// Install a write callback receiving the value as int (Brightness style).
    CharacteristicRef& on_write_int(std::function<void(int)> cb);

    /// Install a write callback receiving the TLV8/Data bytes (control points).
    CharacteristicRef& on_write_tlv(std::function<void(const std::vector<uint8_t>&)> cb);

    /// Install a write-with-response callback (returns a Value or HAPStatus).
    CharacteristicRef& on_write_response_tlv(
            std::function<std::optional<core::HAPResponse<core::Value>>(const std::vector<uint8_t>&)> cb);

    /// The bound characteristic (nullptr after build() moved it out).
    core::Characteristic* get() const { return characteristic_; }

    /// Finish the chain when it ends on a callback (forwards to the builder).
    std::unique_ptr<core::Service> build();

    /// Continue the chain: add the next characteristic to the owning builder.
    CharacteristicRef add(characteristic::CharId id);

    /// Continue the chain with a one-call add+set (see ServiceBuilder::add).
    CharacteristicRef add(characteristic::CharId id, const char* value);

    template<typename T>
    CharacteristicRef add(characteristic::CharId id, T&& value);

private:
    friend class ServiceBuilder;
    CharacteristicRef(core::Characteristic* characteristic, ServiceBuilder* builder)
        : characteristic_(characteristic), builder_(builder) {}

    core::Characteristic* characteristic_;
    ServiceBuilder* builder_;
};

class ServiceBuilder {
public:
    ServiceBuilder(uint64_t type, const char* name, bool primary = false)
        : service_(std::make_unique<core::Service>(type, name, primary)) {}

    /// Add a characteristic from the catalog (IIDs assigned in add order,
    /// starting at 2). Returns a ref for set()/on_write*()/continued chaining.
    CharacteristicRef add(characteristic::CharId id);

    /// Add a characteristic and set its value to a string in one call. The
    /// const char* overload exists so string literals unambiguously construct
    /// the std::string alternative of core::Value (a generic T&& would leave
    /// bool as the only viable alternative).
    CharacteristicRef add(characteristic::CharId id, const char* value) {
        return add(id).set(core::Value(std::string(value)));
    }

    /// Add a characteristic and set its value in one call.
    template<typename T>
    CharacteristicRef add(characteristic::CharId id, T&& value) {
        static_assert(std::is_constructible_v<core::Value, T&&>,
                      "value must construct a hap::core::Value (bool, integer, "
                      "float, std::string, std::vector<uint8_t>)");
        return add(id).set(core::Value(std::forward<T>(value)));
    }

    std::unique_ptr<core::Service> build() { return std::move(service_); }

private:
    std::unique_ptr<core::Service> service_;
    uint64_t next_iid_ = 2; // IID 1 is reserved for the service itself
};

// Out-of-line in the header (after ServiceBuilder is complete) so the fluent
// chain works; these call back into the builder.
inline CharacteristicRef CharacteristicRef::add(characteristic::CharId id) {
    return builder_->add(id);
}
inline CharacteristicRef CharacteristicRef::add(characteristic::CharId id, const char* value) {
    return builder_->add(id, value);
}
template<typename T>
CharacteristicRef CharacteristicRef::add(characteristic::CharId id, T&& value) {
    return builder_->add(id, std::forward<T>(value));
}
inline std::unique_ptr<core::Service> CharacteristicRef::build() {
    return builder_->build();
}

} // namespace hap::service
