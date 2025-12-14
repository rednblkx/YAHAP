/**
 * @file ServiceTypes.hpp
 * @brief Pre-defined HAP Service types and builder classes
 * 
 * This file provides UUID constants and builder classes for all HAP-defined
 * services per HomeKit Accessory Protocol Specification R13.
 * 
 * Usage:
 *   auto info = hap::service::AccessoryInformationBuilder()
 *       .name("My Light")
 *       .manufacturer("ACME")
 *       .model("Light-v1")
 *       .serial_number("00001")
 *       .firmware_revision("1.0.0")
 *       .on_identify([]() { printf("Identify!\n"); })
 *       .build();
 */
#pragma once

#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/types/CharacteristicTypes.hpp"
#include <memory>
#include <string>
#include <functional>

namespace hap::service {

//==============================================================================
// Service UUID Type Constants (HAP Spec R13 Section 9)
//==============================================================================

constexpr uint64_t kType_AccessoryInformation = 0x3E;
constexpr uint64_t kType_GarageDoorOpener = 0x41;
constexpr uint64_t kType_LightBulb = 0x43;
constexpr uint64_t kType_LockManagement = 0x44;
constexpr uint64_t kType_LockMechanism = 0x45;
constexpr uint64_t kType_Outlet = 0x47;
constexpr uint64_t kType_Switch = 0x49;
constexpr uint64_t kType_Thermostat = 0x4A;
constexpr uint64_t kType_Pairing = 0x55;
constexpr uint64_t kType_SecuritySystem = 0x7E;
constexpr uint64_t kType_CarbonMonoxideSensor = 0x7F;
constexpr uint64_t kType_ContactSensor = 0x80;
constexpr uint64_t kType_Door = 0x81;
constexpr uint64_t kType_HumiditySensor = 0x82;
constexpr uint64_t kType_LeakSensor = 0x83;
constexpr uint64_t kType_LightSensor = 0x84;
constexpr uint64_t kType_MotionSensor = 0x85;
constexpr uint64_t kType_OccupancySensor = 0x86;
constexpr uint64_t kType_SmokeSensor = 0x87;
constexpr uint64_t kType_StatelessProgrammableSwitch = 0x89;
constexpr uint64_t kType_TemperatureSensor = 0x8A;
constexpr uint64_t kType_Window = 0x8B;
constexpr uint64_t kType_WindowCovering = 0x8C;
constexpr uint64_t kType_AirQualitySensor = 0x8D;
constexpr uint64_t kType_BatteryService = 0x96;
constexpr uint64_t kType_CarbonDioxideSensor = 0x97;
constexpr uint64_t kType_HAPProtocolInformation = 0xA2;
constexpr uint64_t kType_Fan = 0xB7;
constexpr uint64_t kType_Slat = 0xB9;
constexpr uint64_t kType_FilterMaintenance = 0xBA;
constexpr uint64_t kType_AirPurifier = 0xBB;
constexpr uint64_t kType_HeaterCooler = 0xBC;
constexpr uint64_t kType_HumidifierDehumidifier = 0xBD;
constexpr uint64_t kType_ServiceLabel = 0xCC;
constexpr uint64_t kType_IrrigationSystem = 0xCF;
constexpr uint64_t kType_Valve = 0xD0;
constexpr uint64_t kType_Faucet = 0xD7;
constexpr uint64_t kType_CameraRTPStreamManagement = 0x110;
constexpr uint64_t kType_Microphone = 0x112;
constexpr uint64_t kType_Speaker = 0x113;

//==============================================================================
// Accessory Category Constants (HAP Spec R13 Section 13.2)
//==============================================================================

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

//==============================================================================
// Service Builder Base Class
//==============================================================================

class ServiceBuilderBase {
protected:
    std::shared_ptr<core::Service> service_;
    uint64_t next_iid_ = 2; // IID 1 is typically reserved for the service itself
    
    void add_characteristic(std::shared_ptr<core::Characteristic> c) {
        c->set_iid(next_iid_++);
        service_->add_characteristic(c);
    }
};

//==============================================================================
// Accessory Information Service Builder
// Required: Name, Manufacturer, Model, SerialNumber, FirmwareRevision, Identify
//==============================================================================

class AccessoryInformationBuilder : public ServiceBuilderBase {
public:
    AccessoryInformationBuilder();
    
    AccessoryInformationBuilder& name(std::string value);
    AccessoryInformationBuilder& manufacturer(std::string value);
    AccessoryInformationBuilder& model(std::string value);
    AccessoryInformationBuilder& serial_number(std::string value);
    AccessoryInformationBuilder& firmware_revision(std::string value);
    AccessoryInformationBuilder& hardware_revision(std::string value);
    AccessoryInformationBuilder& on_identify(std::function<void()> callback);
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> name_char_;
    std::shared_ptr<core::Characteristic> manufacturer_char_;
    std::shared_ptr<core::Characteristic> model_char_;
    std::shared_ptr<core::Characteristic> serial_char_;
    std::shared_ptr<core::Characteristic> firmware_char_;
    std::shared_ptr<core::Characteristic> hardware_char_;
    std::shared_ptr<core::Characteristic> identify_char_;
};

//==============================================================================
// LightBulb Service Builder
// Required: On
// Optional: Brightness, Hue, Saturation, ColorTemperature, Name
//==============================================================================

class LightBulbBuilder : public ServiceBuilderBase {
public:
    LightBulbBuilder();
    
    /// Add optional Brightness characteristic (0-100%)
    LightBulbBuilder& with_brightness();
    
    /// Add optional Hue and Saturation characteristics for RGB color
    LightBulbBuilder& with_color();
    
    /// Add optional ColorTemperature characteristic (140-500 mireds)
    LightBulbBuilder& with_color_temperature();
    
    /// Add optional Name characteristic
    LightBulbBuilder& with_name(std::string name);
    
    /// Set callback for when On characteristic changes
    LightBulbBuilder& on_change(std::function<void(bool on)> callback);
    
    /// Set callback for when Brightness changes
    LightBulbBuilder& on_brightness_change(std::function<void(int brightness)> callback);
    
    std::shared_ptr<core::Service> build();
    
    // Access to characteristics for further customization
    std::shared_ptr<core::Characteristic> on_characteristic() { return on_char_; }
    std::shared_ptr<core::Characteristic> brightness_characteristic() { return brightness_char_; }
    std::shared_ptr<core::Characteristic> hue_characteristic() { return hue_char_; }
    std::shared_ptr<core::Characteristic> saturation_characteristic() { return saturation_char_; }

private:
    std::shared_ptr<core::Characteristic> on_char_;
    std::shared_ptr<core::Characteristic> brightness_char_;
    std::shared_ptr<core::Characteristic> hue_char_;
    std::shared_ptr<core::Characteristic> saturation_char_;
    std::shared_ptr<core::Characteristic> color_temp_char_;
};

//==============================================================================
// Switch Service Builder
// Required: On
// Optional: Name
//==============================================================================

class SwitchBuilder : public ServiceBuilderBase {
public:
    SwitchBuilder();
    
    SwitchBuilder& with_name(std::string name);
    SwitchBuilder& on_change(std::function<void(bool on)> callback);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> on_characteristic() { return on_char_; }

private:
    std::shared_ptr<core::Characteristic> on_char_;
};

//==============================================================================
// Outlet Service Builder
// Required: On, OutletInUse
// Optional: Name
//==============================================================================

class OutletBuilder : public ServiceBuilderBase {
public:
    OutletBuilder();
    
    OutletBuilder& with_name(std::string name);
    OutletBuilder& on_change(std::function<void(bool on)> callback);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> on_characteristic() { return on_char_; }
    std::shared_ptr<core::Characteristic> outlet_in_use_characteristic() { return outlet_in_use_char_; }

private:
    std::shared_ptr<core::Characteristic> on_char_;
    std::shared_ptr<core::Characteristic> outlet_in_use_char_;
};

//==============================================================================
// Thermostat Service Builder
// Required: CurrentHeatingCoolingState, TargetHeatingCoolingState,
//           CurrentTemperature, TargetTemperature, TemperatureDisplayUnits
// Optional: CoolingThresholdTemperature, HeatingThresholdTemperature,
//           CurrentRelativeHumidity, TargetRelativeHumidity, Name
//==============================================================================

class ThermostatBuilder : public ServiceBuilderBase {
public:
    ThermostatBuilder();
    
    ThermostatBuilder& with_humidity();
    ThermostatBuilder& with_threshold_temperatures();
    ThermostatBuilder& with_name(std::string name);
    
    std::shared_ptr<core::Service> build();
    
    std::shared_ptr<core::Characteristic> current_temperature() { return current_temp_; }
    std::shared_ptr<core::Characteristic> target_temperature() { return target_temp_; }
    std::shared_ptr<core::Characteristic> current_heating_cooling_state() { return current_hc_state_; }
    std::shared_ptr<core::Characteristic> target_heating_cooling_state() { return target_hc_state_; }

private:
    std::shared_ptr<core::Characteristic> current_hc_state_;
    std::shared_ptr<core::Characteristic> target_hc_state_;
    std::shared_ptr<core::Characteristic> current_temp_;
    std::shared_ptr<core::Characteristic> target_temp_;
    std::shared_ptr<core::Characteristic> display_units_;
    std::shared_ptr<core::Characteristic> current_humidity_;
    std::shared_ptr<core::Characteristic> target_humidity_;
};

//==============================================================================
// Temperature Sensor Service Builder
// Required: CurrentTemperature
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class TemperatureSensorBuilder : public ServiceBuilderBase {
public:
    TemperatureSensorBuilder();
    
    TemperatureSensorBuilder& with_name(std::string name);
    TemperatureSensorBuilder& with_status_active();
    TemperatureSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_temperature() { return current_temp_; }

private:
    std::shared_ptr<core::Characteristic> current_temp_;
};

//==============================================================================
// Humidity Sensor Service Builder
// Required: CurrentRelativeHumidity
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class HumiditySensorBuilder : public ServiceBuilderBase {
public:
    HumiditySensorBuilder();
    
    HumiditySensorBuilder& with_name(std::string name);
    HumiditySensorBuilder& with_status_active();
    HumiditySensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_humidity() { return current_humidity_; }

private:
    std::shared_ptr<core::Characteristic> current_humidity_;
};

//==============================================================================
// Motion Sensor Service Builder
// Required: MotionDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class MotionSensorBuilder : public ServiceBuilderBase {
public:
    MotionSensorBuilder();
    
    MotionSensorBuilder& with_name(std::string name);
    MotionSensorBuilder& with_status_active();
    MotionSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> motion_detected() { return motion_detected_; }

private:
    std::shared_ptr<core::Characteristic> motion_detected_;
};

//==============================================================================
// Contact Sensor Service Builder
// Required: ContactSensorState
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class ContactSensorBuilder : public ServiceBuilderBase {
public:
    ContactSensorBuilder();
    
    ContactSensorBuilder& with_name(std::string name);
    ContactSensorBuilder& with_status_active();
    ContactSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> contact_sensor_state() { return contact_state_; }

private:
    std::shared_ptr<core::Characteristic> contact_state_;
};

//==============================================================================
// Garage Door Opener Service Builder
// Required: CurrentDoorState, TargetDoorState, ObstructionDetected
// Optional: LockCurrentState, LockTargetState, Name
//==============================================================================

class GarageDoorOpenerBuilder : public ServiceBuilderBase {
public:
    GarageDoorOpenerBuilder();
    
    GarageDoorOpenerBuilder& with_name(std::string name);
    GarageDoorOpenerBuilder& with_lock_state();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_door_state() { return current_door_state_; }
    std::shared_ptr<core::Characteristic> target_door_state() { return target_door_state_; }

private:
    std::shared_ptr<core::Characteristic> current_door_state_;
    std::shared_ptr<core::Characteristic> target_door_state_;
    std::shared_ptr<core::Characteristic> obstruction_detected_;
};

//==============================================================================
// Lock Mechanism Service Builder
// Required: LockCurrentState, LockTargetState
// Optional: Name
//==============================================================================

class LockMechanismBuilder : public ServiceBuilderBase {
public:
    LockMechanismBuilder();
    
    LockMechanismBuilder& with_name(std::string name);
    LockMechanismBuilder& on_lock_change(std::function<void(bool locked)> callback);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> lock_current_state() { return lock_current_state_; }
    std::shared_ptr<core::Characteristic> lock_target_state() { return lock_target_state_; }

private:
    std::shared_ptr<core::Characteristic> lock_current_state_;
    std::shared_ptr<core::Characteristic> lock_target_state_;
};

//==============================================================================
// Fan Service Builder
// Required: Active
// Optional: CurrentFanState, TargetFanState, RotationDirection, RotationSpeed,
//           SwingMode, LockPhysicalControls, Name
//==============================================================================

class FanBuilder : public ServiceBuilderBase {
public:
    FanBuilder();
    
    FanBuilder& with_name(std::string name);
    FanBuilder& with_rotation_speed();
    FanBuilder& with_rotation_direction();
    FanBuilder& with_swing_mode();
    FanBuilder& on_active_change(std::function<void(bool active)> callback);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_char_; }
    std::shared_ptr<core::Characteristic> rotation_speed() { return rotation_speed_; }

private:
    std::shared_ptr<core::Characteristic> active_char_;
    std::shared_ptr<core::Characteristic> rotation_speed_;
    std::shared_ptr<core::Characteristic> rotation_dir_;
    std::shared_ptr<core::Characteristic> swing_mode_;
};

//==============================================================================
// Window Covering Service Builder
// Required: CurrentPosition, TargetPosition, PositionState
// Optional: HoldPosition, ObstructionDetected, Name
//==============================================================================

class WindowCoveringBuilder : public ServiceBuilderBase {
public:
    WindowCoveringBuilder();
    
    WindowCoveringBuilder& with_name(std::string name);
    WindowCoveringBuilder& with_hold_position();
    WindowCoveringBuilder& with_obstruction_detected();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_position() { return current_position_; }
    std::shared_ptr<core::Characteristic> target_position() { return target_position_; }

private:
    std::shared_ptr<core::Characteristic> current_position_;
    std::shared_ptr<core::Characteristic> target_position_;
    std::shared_ptr<core::Characteristic> position_state_;
};

//==============================================================================
// Battery Service Builder
// Required: BatteryLevel, ChargingState, StatusLowBattery
// Optional: Name
//==============================================================================

class BatteryServiceBuilder : public ServiceBuilderBase {
public:
    BatteryServiceBuilder();
    
    BatteryServiceBuilder& with_name(std::string name);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> battery_level() { return battery_level_; }
    std::shared_ptr<core::Characteristic> charging_state() { return charging_state_; }
    std::shared_ptr<core::Characteristic> status_low_battery() { return status_low_battery_; }

private:
    std::shared_ptr<core::Characteristic> battery_level_;
    std::shared_ptr<core::Characteristic> charging_state_;
    std::shared_ptr<core::Characteristic> status_low_battery_;
};

//==============================================================================
// Security System Service Builder
// Required: SecuritySystemCurrentState, SecuritySystemTargetState
// Optional: Name, SecuritySystemAlarmType, StatusFault, StatusTampered
//==============================================================================

class SecuritySystemBuilder : public ServiceBuilderBase {
public:
    SecuritySystemBuilder();
    
    SecuritySystemBuilder& with_name(std::string name);
    SecuritySystemBuilder& with_alarm_type();
    SecuritySystemBuilder& with_fault_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_state() { return current_state_; }
    std::shared_ptr<core::Characteristic> target_state() { return target_state_; }

private:
    std::shared_ptr<core::Characteristic> current_state_;
    std::shared_ptr<core::Characteristic> target_state_;
};

} // namespace hap::service
