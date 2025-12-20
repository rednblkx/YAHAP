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

constexpr uint64_t kType_AccessoryInformation = 0x3E;       // 9.1
constexpr uint64_t kType_Fan = 0x40;                        // 9.3 - Legacy Fan (requires On)
constexpr uint64_t kType_GarageDoorOpener = 0x41;           // 9.4
constexpr uint64_t kType_LightBulb = 0x43;                  // 9.5
constexpr uint64_t kType_LockManagement = 0x44;             // 9.6
constexpr uint64_t kType_LockMechanism = 0x45;              // 9.7
constexpr uint64_t kType_Outlet = 0x47;                     // 9.8
constexpr uint64_t kType_Switch = 0x49;                     // 9.9
constexpr uint64_t kType_Thermostat = 0x4A;                 // 9.10
constexpr uint64_t kType_Pairing = 0x55;                    // 5.13.1
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
constexpr uint64_t kType_CameraRTPStreamManagement = 0x110; // 9.28
constexpr uint64_t kType_Microphone = 0x112;                // 9.29
constexpr uint64_t kType_Speaker = 0x113;                   // 9.30
constexpr uint64_t kType_Doorbell = 0x121;                  // 9.31
constexpr uint64_t kType_TargetControlManagement = 0x122;   // 9.42
constexpr uint64_t kType_TargetControl = 0x125;             // 9.43
constexpr uint64_t kType_AudioStreamManagement = 0x127;     // 9.44
constexpr uint64_t kType_DataStreamTransportManagement = 0x129; // 9.45
constexpr uint64_t kType_Siri = 0x133;                      // 9.46

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
// Lock Management Service Builder
// Required: LockControlPoint, Version
// Optional: Logs, AudioFeedback, LockManagementAutoSecurityTimeout,
//           AdministratorOnlyAccess, LockLastKnownAction, CurrentDoorState, MotionDetected
//==============================================================================

class LockManagementBuilder : public ServiceBuilderBase {
public:
    LockManagementBuilder();
    
    /// Add optional audio feedback characteristic
    LockManagementBuilder& with_audio_feedback();
    
    /// Add optional auto security timeout characteristic
    LockManagementBuilder& with_auto_security_timeout();
    
    /// Add optional lock last known action characteristic
    LockManagementBuilder& with_last_known_action();
    
    /// Add optional logs characteristic
    LockManagementBuilder& with_logs();
    
    /// Set callback for Lock Control Point writes
    LockManagementBuilder& on_control_point(std::function<void(const std::vector<uint8_t>& tlv)> callback);
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> lock_control_point() { return lock_control_point_; }
    std::shared_ptr<core::Characteristic> version() { return version_; }

private:
    std::shared_ptr<core::Characteristic> lock_control_point_;
    std::shared_ptr<core::Characteristic> version_;
    std::shared_ptr<core::Characteristic> auto_security_timeout_;
    std::shared_ptr<core::Characteristic> last_known_action_;
    std::shared_ptr<core::Characteristic> audio_feedback_;
    std::shared_ptr<core::Characteristic> logs_;
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

//==============================================================================
// Door Service Builder (9.15)
// Required: CurrentPosition, TargetPosition, PositionState
// Optional: Name, HoldPosition, ObstructionDetected
//==============================================================================

class DoorBuilder : public ServiceBuilderBase {
public:
    DoorBuilder();
    
    DoorBuilder& with_name(std::string name);
    DoorBuilder& with_hold_position();
    DoorBuilder& with_obstruction_detected();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_position() { return current_position_; }
    std::shared_ptr<core::Characteristic> target_position() { return target_position_; }

private:
    std::shared_ptr<core::Characteristic> current_position_;
    std::shared_ptr<core::Characteristic> target_position_;
    std::shared_ptr<core::Characteristic> position_state_;
};

//==============================================================================
// Occupancy Sensor Service Builder (9.20)
// Required: OccupancyDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class OccupancySensorBuilder : public ServiceBuilderBase {
public:
    OccupancySensorBuilder();
    
    OccupancySensorBuilder& with_name(std::string name);
    OccupancySensorBuilder& with_status_active();
    OccupancySensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> occupancy_detected() { return occupancy_detected_; }

private:
    std::shared_ptr<core::Characteristic> occupancy_detected_;
};

//==============================================================================
// Smoke Sensor Service Builder (9.21)
// Required: SmokeDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class SmokeSensorBuilder : public ServiceBuilderBase {
public:
    SmokeSensorBuilder();
    
    SmokeSensorBuilder& with_name(std::string name);
    SmokeSensorBuilder& with_status_active();
    SmokeSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> smoke_detected() { return smoke_detected_; }

private:
    std::shared_ptr<core::Characteristic> smoke_detected_;
};

//==============================================================================
// Leak Sensor Service Builder (9.17)
// Required: LeakDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class LeakSensorBuilder : public ServiceBuilderBase {
public:
    LeakSensorBuilder();
    
    LeakSensorBuilder& with_name(std::string name);
    LeakSensorBuilder& with_status_active();
    LeakSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> leak_detected() { return leak_detected_; }

private:
    std::shared_ptr<core::Characteristic> leak_detected_;
};

//==============================================================================
// Light Sensor Service Builder (9.18)
// Required: CurrentAmbientLightLevel
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery
//==============================================================================

class LightSensorBuilder : public ServiceBuilderBase {
public:
    LightSensorBuilder();
    
    LightSensorBuilder& with_name(std::string name);
    LightSensorBuilder& with_status_active();
    LightSensorBuilder& with_battery_status();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> current_light_level() { return current_light_level_; }

private:
    std::shared_ptr<core::Characteristic> current_light_level_;
};

//==============================================================================
// Carbon Dioxide Sensor Service Builder (9.27)
// Required: CarbonDioxideDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery,
//           CarbonDioxideLevel, CarbonDioxidePeakLevel
//==============================================================================

class CarbonDioxideSensorBuilder : public ServiceBuilderBase {
public:
    CarbonDioxideSensorBuilder();
    
    CarbonDioxideSensorBuilder& with_name(std::string name);
    CarbonDioxideSensorBuilder& with_status_active();
    CarbonDioxideSensorBuilder& with_level();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> co2_detected() { return co2_detected_; }

private:
    std::shared_ptr<core::Characteristic> co2_detected_;
};

//==============================================================================
// Doorbell Service Builder (9.31)
// Required: ProgrammableSwitchEvent
// Optional: Name, Volume, Brightness
//==============================================================================

class DoorbellBuilder : public ServiceBuilderBase {
public:
    DoorbellBuilder();
    
    DoorbellBuilder& with_name(std::string name);
    DoorbellBuilder& with_volume();
    DoorbellBuilder& with_brightness();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> switch_event_;
};

//==============================================================================
// Air Purifier Service Builder (9.35)
// Required: Active, CurrentAirPurifierState, TargetAirPurifierState
// Optional: Name, RotationSpeed, SwingMode, LockPhysicalControls
//==============================================================================

class AirPurifierBuilder : public ServiceBuilderBase {
public:
    AirPurifierBuilder();
    
    AirPurifierBuilder& with_name(std::string name);
    AirPurifierBuilder& with_rotation_speed();
    AirPurifierBuilder& with_swing_mode();
    AirPurifierBuilder& with_lock_physical_controls();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }

private:
    std::shared_ptr<core::Characteristic> active_;
    std::shared_ptr<core::Characteristic> current_state_;
    std::shared_ptr<core::Characteristic> target_state_;
};

//==============================================================================
// Heater Cooler Service Builder (9.36)
// Required: Active, CurrentTemperature, CurrentHeaterCoolerState, TargetHeaterCoolerState
// Optional: Name, RotationSpeed, TemperatureDisplayUnits, SwingMode,
//           CoolingThresholdTemperature, HeatingThresholdTemperature, LockPhysicalControls
//==============================================================================

class HeaterCoolerBuilder : public ServiceBuilderBase {
public:
    HeaterCoolerBuilder();
    
    HeaterCoolerBuilder& with_name(std::string name);
    HeaterCoolerBuilder& with_rotation_speed();
    HeaterCoolerBuilder& with_cooling_threshold();
    HeaterCoolerBuilder& with_heating_threshold();
    HeaterCoolerBuilder& with_swing_mode();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }
    std::shared_ptr<core::Characteristic> current_temperature() { return current_temp_; }

private:
    std::shared_ptr<core::Characteristic> active_;
    std::shared_ptr<core::Characteristic> current_temp_;
    std::shared_ptr<core::Characteristic> current_state_;
    std::shared_ptr<core::Characteristic> target_state_;
};

//==============================================================================
// Humidifier Dehumidifier Service Builder (9.37)
// Required: Active, CurrentRelativeHumidity, CurrentHumidifierDehumidifierState,
//           TargetHumidifierDehumidifierState
// Optional: Name, RelativeHumidityDehumidifierThreshold, RelativeHumidityHumidifierThreshold,
//           RotationSpeed, SwingMode, WaterLevel, LockPhysicalControls
//==============================================================================

class HumidifierDehumidifierBuilder : public ServiceBuilderBase {
public:
    HumidifierDehumidifierBuilder();
    
    HumidifierDehumidifierBuilder& with_name(std::string name);
    HumidifierDehumidifierBuilder& with_dehumidifier_threshold();
    HumidifierDehumidifierBuilder& with_humidifier_threshold();
    HumidifierDehumidifierBuilder& with_water_level();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }

private:
    std::shared_ptr<core::Characteristic> active_;
    std::shared_ptr<core::Characteristic> current_humidity_;
    std::shared_ptr<core::Characteristic> current_state_;
    std::shared_ptr<core::Characteristic> target_state_;
};

//==============================================================================
// Filter Maintenance Service Builder (9.34)
// Required: FilterChangeIndication
// Optional: Name, FilterLifeLevel, ResetFilterIndication
//==============================================================================

class FilterMaintenanceBuilder : public ServiceBuilderBase {
public:
    FilterMaintenanceBuilder();
    
    FilterMaintenanceBuilder& with_name(std::string name);
    FilterMaintenanceBuilder& with_filter_life_level();
    FilterMaintenanceBuilder& with_reset_filter_indication();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> filter_change_;
};

//==============================================================================
// Slat Service Builder (9.33)
// Required: CurrentSlatState, SlatType
// Optional: Name, SwingMode, CurrentTiltAngle, TargetTiltAngle
//==============================================================================

class SlatBuilder : public ServiceBuilderBase {
public:
    SlatBuilder();
    
    SlatBuilder& with_name(std::string name);
    SlatBuilder& with_swing_mode();
    SlatBuilder& with_tilt_angle();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> current_state_;
    std::shared_ptr<core::Characteristic> slat_type_;
};

//==============================================================================
// Valve Service Builder (9.40)
// Required: Active, InUse, ValveType
// Optional: SetDuration, RemainingDuration, IsConfigured, ServiceLabelIndex, StatusFault, Name
//==============================================================================

class ValveBuilder : public ServiceBuilderBase {
public:
    ValveBuilder();
    
    ValveBuilder& with_name(std::string name);
    ValveBuilder& with_duration();
    ValveBuilder& with_is_configured();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }
    std::shared_ptr<core::Characteristic> in_use() { return in_use_; }

private:
    std::shared_ptr<core::Characteristic> active_;
    std::shared_ptr<core::Characteristic> in_use_;
    std::shared_ptr<core::Characteristic> valve_type_;
};

//==============================================================================
// Irrigation System Service Builder (9.39)
// Required: Active, ProgramMode, InUse
// Optional: RemainingDuration, Name, StatusFault
//==============================================================================

class IrrigationSystemBuilder : public ServiceBuilderBase {
public:
    IrrigationSystemBuilder();
    
    IrrigationSystemBuilder& with_name(std::string name);
    IrrigationSystemBuilder& with_remaining_duration();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }

private:
    std::shared_ptr<core::Characteristic> active_;
    std::shared_ptr<core::Characteristic> program_mode_;
    std::shared_ptr<core::Characteristic> in_use_;
};

//==============================================================================
// Faucet Service Builder (9.41)
// Required: Active
// Optional: StatusFault, Name
//==============================================================================

class FaucetBuilder : public ServiceBuilderBase {
public:
    FaucetBuilder();
    
    FaucetBuilder& with_name(std::string name);
    FaucetBuilder& with_status_fault();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> active() { return active_; }

private:
    std::shared_ptr<core::Characteristic> active_;
};

//==============================================================================
// Service Label Service Builder (9.38)
// Required: ServiceLabelNamespace
//==============================================================================

class ServiceLabelBuilder : public ServiceBuilderBase {
public:
    ServiceLabelBuilder();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> namespace_char_;
};

//==============================================================================
// Carbon Monoxide Sensor Service Builder (9.13)
// Required: CarbonMonoxideDetected
// Optional: Name, StatusActive, StatusFault, StatusTampered, StatusLowBattery,
//           CarbonMonoxideLevel, CarbonMonoxidePeakLevel
//==============================================================================

class CarbonMonoxideSensorBuilder : public ServiceBuilderBase {
public:
    CarbonMonoxideSensorBuilder();
    
    CarbonMonoxideSensorBuilder& with_name(std::string name);
    CarbonMonoxideSensorBuilder& with_status_active();
    CarbonMonoxideSensorBuilder& with_level();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> co_detected() { return co_detected_; }

private:
    std::shared_ptr<core::Characteristic> co_detected_;
};

//==============================================================================
// Air Quality Sensor Service Builder (9.11)
// Required: AirQuality
// Optional: Name, OzoneDensity, NitrogenDioxideDensity, SulphurDioxideDensity,
//           PM2.5Density, PM10Density, VOCDensity, StatusActive, StatusFault,
//           StatusTampered, StatusLowBattery
//==============================================================================

class AirQualitySensorBuilder : public ServiceBuilderBase {
public:
    AirQualitySensorBuilder();
    
    AirQualitySensorBuilder& with_name(std::string name);
    AirQualitySensorBuilder& with_pm25();
    AirQualitySensorBuilder& with_pm10();
    AirQualitySensorBuilder& with_voc();
    AirQualitySensorBuilder& with_status_active();
    
    std::shared_ptr<core::Service> build();
    std::shared_ptr<core::Characteristic> air_quality() { return air_quality_; }

private:
    std::shared_ptr<core::Characteristic> air_quality_;
};

//==============================================================================
// Stateless Programmable Switch Service Builder (9.22)
// Required: ProgrammableSwitchEvent
// Optional: Name, ServiceLabelIndex
//==============================================================================

class StatelessProgrammableSwitchBuilder : public ServiceBuilderBase {
public:
    StatelessProgrammableSwitchBuilder();
    
    StatelessProgrammableSwitchBuilder& with_name(std::string name);
    StatelessProgrammableSwitchBuilder& with_service_label_index(uint8_t index);
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> switch_event_;
};

//==============================================================================
// Microphone Service Builder (9.29)
// Required: Mute
// Optional: Name, Volume
//==============================================================================

class MicrophoneBuilder : public ServiceBuilderBase {
public:
    MicrophoneBuilder();
    
    MicrophoneBuilder& with_name(std::string name);
    MicrophoneBuilder& with_volume();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> mute_;
};

//==============================================================================
// Speaker Service Builder (9.30)
// Required: Mute
// Optional: Name, Volume
//==============================================================================

class SpeakerBuilder : public ServiceBuilderBase {
public:
    SpeakerBuilder();
    
    SpeakerBuilder& with_name(std::string name);
    SpeakerBuilder& with_volume();
    
    std::shared_ptr<core::Service> build();

private:
    std::shared_ptr<core::Characteristic> mute_;
};

} // namespace hap::service
