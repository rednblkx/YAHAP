/**
 * @file ServiceTypes.cpp
 * @brief Implementation of HAP Service builder classes
 */

#include "hap/types/ServiceTypes.hpp"

namespace hap::service {

using namespace hap::core;
namespace chr = hap::characteristic;

//==============================================================================
// AccessoryInformationBuilder
//==============================================================================

AccessoryInformationBuilder::AccessoryInformationBuilder() {
    service_ = std::make_shared<Service>(kType_AccessoryInformation, "Accessory Information");
    
    // Required characteristics (create them upfront)
    name_char_ = chr::Name();
    manufacturer_char_ = chr::Manufacturer();
    model_char_ = chr::Model();
    serial_char_ = chr::SerialNumber();
    firmware_char_ = chr::FirmwareRevision();
    identify_char_ = chr::Identify();
    hardware_char_ = nullptr; // Optional
    
    add_characteristic(name_char_);
    add_characteristic(manufacturer_char_);
    add_characteristic(model_char_);
    add_characteristic(serial_char_);
    add_characteristic(firmware_char_);
    add_characteristic(identify_char_);
}

AccessoryInformationBuilder& AccessoryInformationBuilder::name(std::string value) {
    name_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::manufacturer(std::string value) {
    manufacturer_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::model(std::string value) {
    model_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::serial_number(std::string value) {
    serial_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::firmware_revision(std::string value) {
    firmware_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::hardware_revision(std::string value) {
    if (!hardware_char_) {
        hardware_char_ = chr::HardwareRevision();
        add_characteristic(hardware_char_);
    }
    hardware_char_->set_value(std::move(value));
    return *this;
}

AccessoryInformationBuilder& AccessoryInformationBuilder::on_identify(std::function<void()> callback) {
    identify_char_->set_write_callback([callback](const Value& v) {
        bool identify = std::get<bool>(v);
        if (identify && callback) {
            callback();
        }
    });
    return *this;
}

std::shared_ptr<Service> AccessoryInformationBuilder::build() {
    return service_;
}

//==============================================================================
// LightBulbBuilder
//==============================================================================

LightBulbBuilder::LightBulbBuilder() {
    service_ = std::make_shared<Service>(kType_LightBulb, "Lightbulb", true);
    
    // Required characteristic
    on_char_ = chr::On();
    add_characteristic(on_char_);
}

LightBulbBuilder& LightBulbBuilder::with_brightness() {
    if (!brightness_char_) {
        brightness_char_ = chr::Brightness();
        add_characteristic(brightness_char_);
    }
    return *this;
}

LightBulbBuilder& LightBulbBuilder::with_color() {
    if (!hue_char_) {
        hue_char_ = chr::Hue();
        add_characteristic(hue_char_);
    }
    if (!saturation_char_) {
        saturation_char_ = chr::Saturation();
        add_characteristic(saturation_char_);
    }
    return *this;
}

LightBulbBuilder& LightBulbBuilder::with_color_temperature() {
    if (!color_temp_char_) {
        color_temp_char_ = chr::ColorTemperature();
        add_characteristic(color_temp_char_);
    }
    return *this;
}

LightBulbBuilder& LightBulbBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

LightBulbBuilder& LightBulbBuilder::on_change(std::function<void(bool on)> callback) {
    on_char_->set_write_callback([callback](const Value& v) {
        bool on = std::get<bool>(v);
        if (callback) callback(on);
    });
    return *this;
}

LightBulbBuilder& LightBulbBuilder::on_brightness_change(std::function<void(int brightness)> callback) {
    if (!brightness_char_) {
        with_brightness();
    }
    brightness_char_->set_write_callback([callback](const Value& v) {
        int brightness = std::get<int32_t>(v);
        if (callback) callback(brightness);
    });
    return *this;
}

std::shared_ptr<Service> LightBulbBuilder::build() {
    return service_;
}

//==============================================================================
// SwitchBuilder
//==============================================================================

SwitchBuilder::SwitchBuilder() {
    service_ = std::make_shared<Service>(kType_Switch, "Switch", true);
    on_char_ = chr::On();
    add_characteristic(on_char_);
}

SwitchBuilder& SwitchBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

SwitchBuilder& SwitchBuilder::on_change(std::function<void(bool on)> callback) {
    on_char_->set_write_callback([callback](const Value& v) {
        bool on = std::get<bool>(v);
        if (callback) callback(on);
    });
    return *this;
}

std::shared_ptr<Service> SwitchBuilder::build() {
    return service_;
}

//==============================================================================
// OutletBuilder
//==============================================================================

OutletBuilder::OutletBuilder() {
    service_ = std::make_shared<Service>(kType_Outlet, "Outlet", true);
    
    on_char_ = chr::On();
    outlet_in_use_char_ = chr::OutletInUse();
    
    add_characteristic(on_char_);
    add_characteristic(outlet_in_use_char_);
}

OutletBuilder& OutletBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

OutletBuilder& OutletBuilder::on_change(std::function<void(bool on)> callback) {
    on_char_->set_write_callback([callback](const Value& v) {
        bool on = std::get<bool>(v);
        if (callback) callback(on);
    });
    return *this;
}

std::shared_ptr<Service> OutletBuilder::build() {
    return service_;
}

//==============================================================================
// ThermostatBuilder
//==============================================================================

ThermostatBuilder::ThermostatBuilder() {
    service_ = std::make_shared<Service>(kType_Thermostat, "Thermostat", true);
    
    current_hc_state_ = chr::CurrentHeatingCoolingStateChar();
    target_hc_state_ = chr::TargetHeatingCoolingStateChar();
    current_temp_ = chr::CurrentTemperature();
    target_temp_ = chr::TargetTemperature();
    display_units_ = chr::TemperatureDisplayUnitsChar();
    
    add_characteristic(current_hc_state_);
    add_characteristic(target_hc_state_);
    add_characteristic(current_temp_);
    add_characteristic(target_temp_);
    add_characteristic(display_units_);
}

ThermostatBuilder& ThermostatBuilder::with_humidity() {
    if (!current_humidity_) {
        current_humidity_ = chr::CurrentRelativeHumidity();
        target_humidity_ = chr::TargetRelativeHumidity();
        add_characteristic(current_humidity_);
        add_characteristic(target_humidity_);
    }
    return *this;
}

ThermostatBuilder& ThermostatBuilder::with_threshold_temperatures() {
    auto cooling_threshold = chr::CoolingThresholdTemperature();
    auto heating_threshold = chr::HeatingThresholdTemperature();
    add_characteristic(cooling_threshold);
    add_characteristic(heating_threshold);
    return *this;
}

ThermostatBuilder& ThermostatBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

std::shared_ptr<Service> ThermostatBuilder::build() {
    return service_;
}

//==============================================================================
// TemperatureSensorBuilder
//==============================================================================

TemperatureSensorBuilder::TemperatureSensorBuilder() {
    service_ = std::make_shared<Service>(kType_TemperatureSensor, "Temperature Sensor", true);
    current_temp_ = chr::CurrentTemperature();
    add_characteristic(current_temp_);
}

TemperatureSensorBuilder& TemperatureSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

TemperatureSensorBuilder& TemperatureSensorBuilder::with_status_active() {
    auto status = chr::StatusActive();
    add_characteristic(status);
    return *this;
}

TemperatureSensorBuilder& TemperatureSensorBuilder::with_battery_status() {
    auto status = chr::StatusLowBatteryChar();
    add_characteristic(status);
    return *this;
}

std::shared_ptr<Service> TemperatureSensorBuilder::build() {
    return service_;
}

//==============================================================================
// HumiditySensorBuilder
//==============================================================================

HumiditySensorBuilder::HumiditySensorBuilder() {
    service_ = std::make_shared<Service>(kType_HumiditySensor, "Humidity Sensor", true);
    current_humidity_ = chr::CurrentRelativeHumidity();
    add_characteristic(current_humidity_);
}

HumiditySensorBuilder& HumiditySensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

HumiditySensorBuilder& HumiditySensorBuilder::with_status_active() {
    auto status = chr::StatusActive();
    add_characteristic(status);
    return *this;
}

HumiditySensorBuilder& HumiditySensorBuilder::with_battery_status() {
    auto status = chr::StatusLowBatteryChar();
    add_characteristic(status);
    return *this;
}

std::shared_ptr<Service> HumiditySensorBuilder::build() {
    return service_;
}

//==============================================================================
// MotionSensorBuilder
//==============================================================================

MotionSensorBuilder::MotionSensorBuilder() {
    service_ = std::make_shared<Service>(kType_MotionSensor, "Motion Sensor", true);
    motion_detected_ = chr::MotionDetected();
    add_characteristic(motion_detected_);
}

MotionSensorBuilder& MotionSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

MotionSensorBuilder& MotionSensorBuilder::with_status_active() {
    auto status = chr::StatusActive();
    add_characteristic(status);
    return *this;
}

MotionSensorBuilder& MotionSensorBuilder::with_battery_status() {
    auto status = chr::StatusLowBatteryChar();
    add_characteristic(status);
    return *this;
}

std::shared_ptr<Service> MotionSensorBuilder::build() {
    return service_;
}

//==============================================================================
// ContactSensorBuilder
//==============================================================================

ContactSensorBuilder::ContactSensorBuilder() {
    service_ = std::make_shared<Service>(kType_ContactSensor, "Contact Sensor", true);
    contact_state_ = chr::ContactSensorStateChar();
    add_characteristic(contact_state_);
}

ContactSensorBuilder& ContactSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

ContactSensorBuilder& ContactSensorBuilder::with_status_active() {
    auto status = chr::StatusActive();
    add_characteristic(status);
    return *this;
}

ContactSensorBuilder& ContactSensorBuilder::with_battery_status() {
    auto status = chr::StatusLowBatteryChar();
    add_characteristic(status);
    return *this;
}

std::shared_ptr<Service> ContactSensorBuilder::build() {
    return service_;
}

//==============================================================================
// GarageDoorOpenerBuilder
//==============================================================================

GarageDoorOpenerBuilder::GarageDoorOpenerBuilder() {
    service_ = std::make_shared<Service>(kType_GarageDoorOpener, "Garage Door Opener", true);
    
    current_door_state_ = chr::CurrentDoorStateChar();
    target_door_state_ = chr::TargetDoorStateChar();
    obstruction_detected_ = chr::ObstructionDetected();
    
    add_characteristic(current_door_state_);
    add_characteristic(target_door_state_);
    add_characteristic(obstruction_detected_);
}

GarageDoorOpenerBuilder& GarageDoorOpenerBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

GarageDoorOpenerBuilder& GarageDoorOpenerBuilder::with_lock_state() {
    auto lock_current = chr::LockCurrentStateChar();
    auto lock_target = chr::LockTargetStateChar();
    add_characteristic(lock_current);
    add_characteristic(lock_target);
    return *this;
}

std::shared_ptr<Service> GarageDoorOpenerBuilder::build() {
    return service_;
}

//==============================================================================
// LockMechanismBuilder
//==============================================================================

LockMechanismBuilder::LockMechanismBuilder() {
    // Lock Mechanism MUST be a primary service per HAP Spec 11.2.1
    service_ = std::make_shared<Service>(kType_LockMechanism, "Lock Mechanism", true);
    
    lock_current_state_ = chr::LockCurrentStateChar();
    lock_target_state_ = chr::LockTargetStateChar();
    
    add_characteristic(lock_current_state_);
    add_characteristic(lock_target_state_);
}

LockMechanismBuilder& LockMechanismBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

LockMechanismBuilder& LockMechanismBuilder::on_lock_change(std::function<void(bool locked)> callback) {
    lock_target_state_->set_write_callback([callback](const Value& v) {
        uint8_t target = std::get<uint8_t>(v);
        if (callback) callback(target == 1); // 1 = Secured
    });
    return *this;
}

std::shared_ptr<Service> LockMechanismBuilder::build() {
    return service_;
}

//==============================================================================
// LockManagementBuilder (9.6) - MANDATORY for Lock Profile per HAP Spec 11.2
//==============================================================================

LockManagementBuilder::LockManagementBuilder() {
    service_ = std::make_shared<Service>(kType_LockManagement, "Lock Management");
    
    // Required characteristics
    lock_control_point_ = chr::LockControlPoint();
    version_ = chr::Version();
    version_->set_value(std::string("1.0")); // HAP Spec 11.2.2.2: "The value of this characteristic must be the string '1.0'."
    
    add_characteristic(lock_control_point_);
    add_characteristic(version_);
}

LockManagementBuilder& LockManagementBuilder::with_audio_feedback() {
    if (!audio_feedback_) {
        audio_feedback_ = chr::AudioFeedback();
        add_characteristic(audio_feedback_);
    }
    return *this;
}

LockManagementBuilder& LockManagementBuilder::with_auto_security_timeout() {
    if (!auto_security_timeout_) {
        auto_security_timeout_ = chr::LockManagementAutoSecurityTimeout();
        add_characteristic(auto_security_timeout_);
    }
    return *this;
}

LockManagementBuilder& LockManagementBuilder::with_last_known_action() {
    if (!last_known_action_) {
        last_known_action_ = chr::LockLastKnownAction();
        add_characteristic(last_known_action_);
    }
    return *this;
}

LockManagementBuilder& LockManagementBuilder::with_logs() {
    if (!logs_) {
        logs_ = chr::Logs();
        add_characteristic(logs_);
    }
    return *this;
}

LockManagementBuilder& LockManagementBuilder::on_control_point(std::function<void(const std::vector<uint8_t>& tlv)> callback) {
    lock_control_point_->set_write_callback([callback](const Value& v) {
        if (callback) {
            auto& tlv_data = std::get<std::vector<uint8_t>>(v);
            callback(tlv_data);
        }
    });
    return *this;
}

std::shared_ptr<Service> LockManagementBuilder::build() {
    return service_;
}

//==============================================================================
// FanBuilder
//==============================================================================

FanBuilder::FanBuilder() {
    service_ = std::make_shared<Service>(kType_Fan_v2, "Fan", true);
    active_char_ = chr::ActiveChar();
    add_characteristic(active_char_);
}

FanBuilder& FanBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

FanBuilder& FanBuilder::with_rotation_speed() {
    if (!rotation_speed_) {
        rotation_speed_ = chr::RotationSpeed();
        add_characteristic(rotation_speed_);
    }
    return *this;
}

FanBuilder& FanBuilder::with_rotation_direction() {
    if (!rotation_dir_) {
        rotation_dir_ = chr::RotationDirectionChar();
        add_characteristic(rotation_dir_);
    }
    return *this;
}

FanBuilder& FanBuilder::with_swing_mode() {
    if (!swing_mode_) {
        swing_mode_ = chr::SwingModeChar();
        add_characteristic(swing_mode_);
    }
    return *this;
}

FanBuilder& FanBuilder::on_active_change(std::function<void(bool active)> callback) {
    active_char_->set_write_callback([callback](const Value& v) {
        uint8_t active = std::get<uint8_t>(v);
        if (callback) callback(active == 1);
    });
    return *this;
}

std::shared_ptr<Service> FanBuilder::build() {
    return service_;
}

//==============================================================================
// WindowCoveringBuilder
//==============================================================================

WindowCoveringBuilder::WindowCoveringBuilder() {
    service_ = std::make_shared<Service>(kType_WindowCovering, "Window Covering", true);
    
    current_position_ = chr::CurrentPosition();
    target_position_ = chr::TargetPosition();
    position_state_ = chr::PositionStateChar();
    
    add_characteristic(current_position_);
    add_characteristic(target_position_);
    add_characteristic(position_state_);
}

WindowCoveringBuilder& WindowCoveringBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

WindowCoveringBuilder& WindowCoveringBuilder::with_hold_position() {
    auto hold = chr::HoldPositionChar();
    add_characteristic(hold);
    return *this;
}

WindowCoveringBuilder& WindowCoveringBuilder::with_obstruction_detected() {
    auto obstruction = chr::ObstructionDetected();
    add_characteristic(obstruction);
    return *this;
}

std::shared_ptr<Service> WindowCoveringBuilder::build() {
    return service_;
}

//==============================================================================
// BatteryServiceBuilder
//==============================================================================

BatteryServiceBuilder::BatteryServiceBuilder() {
    service_ = std::make_shared<Service>(kType_BatteryService, "Battery");
    
    battery_level_ = chr::BatteryLevel();
    charging_state_ = chr::ChargingStateChar();
    status_low_battery_ = chr::StatusLowBatteryChar();
    
    add_characteristic(battery_level_);
    add_characteristic(charging_state_);
    add_characteristic(status_low_battery_);
}

BatteryServiceBuilder& BatteryServiceBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

std::shared_ptr<Service> BatteryServiceBuilder::build() {
    return service_;
}

//==============================================================================
// SecuritySystemBuilder
//==============================================================================

SecuritySystemBuilder::SecuritySystemBuilder() {
    service_ = std::make_shared<Service>(kType_SecuritySystem, "Security System", true);
    
    current_state_ = chr::SecuritySystemCurrentStateChar();
    target_state_ = chr::SecuritySystemTargetStateChar();
    
    add_characteristic(current_state_);
    add_characteristic(target_state_);
}

SecuritySystemBuilder& SecuritySystemBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

SecuritySystemBuilder& SecuritySystemBuilder::with_alarm_type() {
    auto alarm_type = std::make_shared<Characteristic>(
        chr::kType_SecuritySystemAlarmType, Format::UInt8,
        std::vector{Permission::PairedRead, Permission::Notify}
    );
    alarm_type->set_min_value(0);
    alarm_type->set_max_value(1);
    alarm_type->set_value(static_cast<uint8_t>(0));
    add_characteristic(alarm_type);
    return *this;
}

SecuritySystemBuilder& SecuritySystemBuilder::with_fault_status() {
    auto fault = chr::StatusFault();
    add_characteristic(fault);
    return *this;
}

std::shared_ptr<Service> SecuritySystemBuilder::build() {
    return service_;
}

//==============================================================================
// Door Service Builder (9.15)
//==============================================================================

DoorBuilder::DoorBuilder() {
    service_ = std::make_shared<Service>(kType_Door, "Door", true);
    
    current_position_ = chr::CurrentPosition();
    target_position_ = chr::TargetPosition();
    position_state_ = chr::PositionStateChar();
    
    add_characteristic(current_position_);
    add_characteristic(target_position_);
    add_characteristic(position_state_);
}

DoorBuilder& DoorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

DoorBuilder& DoorBuilder::with_hold_position() {
    add_characteristic(chr::HoldPositionChar());
    return *this;
}

DoorBuilder& DoorBuilder::with_obstruction_detected() {
    add_characteristic(chr::ObstructionDetected());
    return *this;
}

std::shared_ptr<Service> DoorBuilder::build() {
    return service_;
}

//==============================================================================
// Occupancy Sensor Service Builder (9.20)
//==============================================================================

OccupancySensorBuilder::OccupancySensorBuilder() {
    service_ = std::make_shared<Service>(kType_OccupancySensor, "Occupancy Sensor", true);
    
    occupancy_detected_ = chr::OccupancyDetected();
    add_characteristic(occupancy_detected_);
}

OccupancySensorBuilder& OccupancySensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

OccupancySensorBuilder& OccupancySensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

OccupancySensorBuilder& OccupancySensorBuilder::with_battery_status() {
    add_characteristic(chr::StatusLowBatteryChar());
    return *this;
}

std::shared_ptr<Service> OccupancySensorBuilder::build() {
    return service_;
}

//==============================================================================
// Smoke Sensor Service Builder (9.21)
//==============================================================================

SmokeSensorBuilder::SmokeSensorBuilder() {
    service_ = std::make_shared<Service>(kType_SmokeSensor, "Smoke Sensor", true);
    
    smoke_detected_ = chr::SmokeDetected();
    add_characteristic(smoke_detected_);
}

SmokeSensorBuilder& SmokeSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

SmokeSensorBuilder& SmokeSensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

SmokeSensorBuilder& SmokeSensorBuilder::with_battery_status() {
    add_characteristic(chr::StatusLowBatteryChar());
    return *this;
}

std::shared_ptr<Service> SmokeSensorBuilder::build() {
    return service_;
}

//==============================================================================
// Leak Sensor Service Builder (9.17)
//==============================================================================

LeakSensorBuilder::LeakSensorBuilder() {
    service_ = std::make_shared<Service>(kType_LeakSensor, "Leak Sensor", true);
    
    leak_detected_ = chr::LeakDetected();
    add_characteristic(leak_detected_);
}

LeakSensorBuilder& LeakSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

LeakSensorBuilder& LeakSensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

LeakSensorBuilder& LeakSensorBuilder::with_battery_status() {
    add_characteristic(chr::StatusLowBatteryChar());
    return *this;
}

std::shared_ptr<Service> LeakSensorBuilder::build() {
    return service_;
}

//==============================================================================
// Light Sensor Service Builder (9.18)
//==============================================================================

LightSensorBuilder::LightSensorBuilder() {
    service_ = std::make_shared<Service>(kType_LightSensor, "Light Sensor", true);
    
    current_light_level_ = chr::CurrentAmbientLightLevel();
    add_characteristic(current_light_level_);
}

LightSensorBuilder& LightSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

LightSensorBuilder& LightSensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

LightSensorBuilder& LightSensorBuilder::with_battery_status() {
    add_characteristic(chr::StatusLowBatteryChar());
    return *this;
}

std::shared_ptr<Service> LightSensorBuilder::build() {
    return service_;
}

//==============================================================================
// Carbon Dioxide Sensor Service Builder (9.27)
//==============================================================================

CarbonDioxideSensorBuilder::CarbonDioxideSensorBuilder() {
    service_ = std::make_shared<Service>(kType_CarbonDioxideSensor, "Carbon Dioxide Sensor", true);
    
    co2_detected_ = chr::CarbonDioxideDetectedChar();
    add_characteristic(co2_detected_);
}

CarbonDioxideSensorBuilder& CarbonDioxideSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

CarbonDioxideSensorBuilder& CarbonDioxideSensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

CarbonDioxideSensorBuilder& CarbonDioxideSensorBuilder::with_level() {
    add_characteristic(chr::CarbonDioxideLevel());
    add_characteristic(chr::CarbonDioxidePeakLevel());
    return *this;
}

std::shared_ptr<Service> CarbonDioxideSensorBuilder::build() {
    return service_;
}

//==============================================================================
// Doorbell Service Builder (9.31)
//==============================================================================

DoorbellBuilder::DoorbellBuilder() {
    // Doorbell is the primary service for Video Doorbell Profile per HAP Spec
    service_ = std::make_shared<Service>(kType_Doorbell, "Doorbell", true);
    
    switch_event_ = chr::ProgrammableSwitchEventChar();
    add_characteristic(switch_event_);
}

DoorbellBuilder& DoorbellBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

DoorbellBuilder& DoorbellBuilder::with_volume() {
    add_characteristic(chr::Volume());
    return *this;
}

DoorbellBuilder& DoorbellBuilder::with_brightness() {
    add_characteristic(chr::Brightness());
    return *this;
}

std::shared_ptr<Service> DoorbellBuilder::build() {
    return service_;
}

//==============================================================================
// Air Purifier Service Builder (9.35)
//==============================================================================

AirPurifierBuilder::AirPurifierBuilder() {
    service_ = std::make_shared<Service>(kType_AirPurifier, "Air Purifier", true);
    
    active_ = chr::ActiveChar();
    current_state_ = chr::CurrentAirPurifierStateChar();
    target_state_ = chr::TargetAirPurifierStateChar();
    
    add_characteristic(active_);
    add_characteristic(current_state_);
    add_characteristic(target_state_);
}

AirPurifierBuilder& AirPurifierBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

AirPurifierBuilder& AirPurifierBuilder::with_rotation_speed() {
    add_characteristic(chr::RotationSpeed());
    return *this;
}

AirPurifierBuilder& AirPurifierBuilder::with_swing_mode() {
    add_characteristic(chr::SwingModeChar());
    return *this;
}

AirPurifierBuilder& AirPurifierBuilder::with_lock_physical_controls() {
    add_characteristic(chr::LockPhysicalControls());
    return *this;
}

std::shared_ptr<Service> AirPurifierBuilder::build() {
    return service_;
}

//==============================================================================
// Heater Cooler Service Builder (9.36)
//==============================================================================

HeaterCoolerBuilder::HeaterCoolerBuilder() {
    service_ = std::make_shared<Service>(kType_HeaterCooler, "Heater Cooler", true);
    
    active_ = chr::ActiveChar();
    current_temp_ = chr::CurrentTemperature();
    current_state_ = chr::CurrentHeaterCoolerStateChar();
    target_state_ = chr::TargetHeaterCoolerStateChar();
    
    add_characteristic(active_);
    add_characteristic(current_temp_);
    add_characteristic(current_state_);
    add_characteristic(target_state_);
}

HeaterCoolerBuilder& HeaterCoolerBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

HeaterCoolerBuilder& HeaterCoolerBuilder::with_rotation_speed() {
    add_characteristic(chr::RotationSpeed());
    return *this;
}

HeaterCoolerBuilder& HeaterCoolerBuilder::with_cooling_threshold() {
    add_characteristic(chr::CoolingThresholdTemperature());
    return *this;
}

HeaterCoolerBuilder& HeaterCoolerBuilder::with_heating_threshold() {
    add_characteristic(chr::HeatingThresholdTemperature());
    return *this;
}

HeaterCoolerBuilder& HeaterCoolerBuilder::with_swing_mode() {
    add_characteristic(chr::SwingModeChar());
    return *this;
}

std::shared_ptr<Service> HeaterCoolerBuilder::build() {
    return service_;
}

//==============================================================================
// Humidifier Dehumidifier Service Builder (9.37)
//==============================================================================

HumidifierDehumidifierBuilder::HumidifierDehumidifierBuilder() {
    service_ = std::make_shared<Service>(kType_HumidifierDehumidifier, "Humidifier Dehumidifier", true);
    
    active_ = chr::ActiveChar();
    current_humidity_ = chr::CurrentRelativeHumidity();
    current_state_ = chr::CurrentHumidifierDehumidifierStateChar();
    target_state_ = chr::TargetHumidifierDehumidifierStateChar();
    
    add_characteristic(active_);
    add_characteristic(current_humidity_);
    add_characteristic(current_state_);
    add_characteristic(target_state_);
}

HumidifierDehumidifierBuilder& HumidifierDehumidifierBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

HumidifierDehumidifierBuilder& HumidifierDehumidifierBuilder::with_dehumidifier_threshold() {
    add_characteristic(chr::RelativeHumidityDehumidifierThreshold());
    return *this;
}

HumidifierDehumidifierBuilder& HumidifierDehumidifierBuilder::with_humidifier_threshold() {
    add_characteristic(chr::RelativeHumidityHumidifierThreshold());
    return *this;
}

HumidifierDehumidifierBuilder& HumidifierDehumidifierBuilder::with_water_level() {
    add_characteristic(chr::WaterLevel());
    return *this;
}

std::shared_ptr<Service> HumidifierDehumidifierBuilder::build() {
    return service_;
}

//==============================================================================
// Filter Maintenance Service Builder (9.34)
//==============================================================================

FilterMaintenanceBuilder::FilterMaintenanceBuilder() {
    service_ = std::make_shared<Service>(kType_FilterMaintenance, "Filter Maintenance");
    
    filter_change_ = chr::FilterChangeIndication();
    add_characteristic(filter_change_);
}

FilterMaintenanceBuilder& FilterMaintenanceBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

FilterMaintenanceBuilder& FilterMaintenanceBuilder::with_filter_life_level() {
    add_characteristic(chr::FilterLifeLevel());
    return *this;
}

FilterMaintenanceBuilder& FilterMaintenanceBuilder::with_reset_filter_indication() {
    add_characteristic(chr::ResetFilterIndication());
    return *this;
}

std::shared_ptr<Service> FilterMaintenanceBuilder::build() {
    return service_;
}

//==============================================================================
// Slat Service Builder (9.33)
//==============================================================================

SlatBuilder::SlatBuilder() {
    service_ = std::make_shared<Service>(kType_Slat, "Slat");
    
    current_state_ = chr::CurrentSlatStateChar();
    slat_type_ = chr::SlatTypeChar();
    
    add_characteristic(current_state_);
    add_characteristic(slat_type_);
}

SlatBuilder& SlatBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

SlatBuilder& SlatBuilder::with_swing_mode() {
    add_characteristic(chr::SwingModeChar());
    return *this;
}

SlatBuilder& SlatBuilder::with_tilt_angle() {
    add_characteristic(chr::CurrentTiltAngle());
    add_characteristic(chr::TargetTiltAngle());
    return *this;
}

std::shared_ptr<Service> SlatBuilder::build() {
    return service_;
}

//==============================================================================
// Valve Service Builder (9.40)
//==============================================================================

ValveBuilder::ValveBuilder() {
    service_ = std::make_shared<Service>(kType_Valve, "Valve", true);
    
    active_ = chr::ActiveChar();
    in_use_ = chr::InUseChar();
    valve_type_ = chr::ValveTypeChar();
    
    add_characteristic(active_);
    add_characteristic(in_use_);
    add_characteristic(valve_type_);
}

ValveBuilder& ValveBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

ValveBuilder& ValveBuilder::with_duration() {
    add_characteristic(chr::SetDuration());
    add_characteristic(chr::RemainingDuration());
    return *this;
}

ValveBuilder& ValveBuilder::with_is_configured() {
    add_characteristic(chr::IsConfigured());
    return *this;
}

std::shared_ptr<Service> ValveBuilder::build() {
    return service_;
}

//==============================================================================
// Irrigation System Service Builder (9.39)
//==============================================================================

IrrigationSystemBuilder::IrrigationSystemBuilder() {
    service_ = std::make_shared<Service>(kType_IrrigationSystem, "Irrigation System", true);
    
    active_ = chr::ActiveChar();
    program_mode_ = chr::ProgramMode();
    in_use_ = chr::InUseChar();
    
    add_characteristic(active_);
    add_characteristic(program_mode_);
    add_characteristic(in_use_);
}

IrrigationSystemBuilder& IrrigationSystemBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

IrrigationSystemBuilder& IrrigationSystemBuilder::with_remaining_duration() {
    add_characteristic(chr::RemainingDuration());
    return *this;
}

std::shared_ptr<Service> IrrigationSystemBuilder::build() {
    return service_;
}

//==============================================================================
// Faucet Service Builder (9.41)
//==============================================================================

FaucetBuilder::FaucetBuilder() {
    service_ = std::make_shared<Service>(kType_Faucet, "Faucet", true);
    
    active_ = chr::ActiveChar();
    add_characteristic(active_);
}

FaucetBuilder& FaucetBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

FaucetBuilder& FaucetBuilder::with_status_fault() {
    add_characteristic(chr::StatusFault());
    return *this;
}

std::shared_ptr<Service> FaucetBuilder::build() {
    return service_;
}

//==============================================================================
// Service Label Service Builder (9.38)
//==============================================================================

ServiceLabelBuilder::ServiceLabelBuilder() {
    service_ = std::make_shared<Service>(kType_ServiceLabel, "Service Label");
    
    namespace_char_ = chr::ServiceLabelNamespaceChar();
    add_characteristic(namespace_char_);
}

std::shared_ptr<Service> ServiceLabelBuilder::build() {
    return service_;
}

//==============================================================================
// Carbon Monoxide Sensor Service Builder (9.13)
//==============================================================================

CarbonMonoxideSensorBuilder::CarbonMonoxideSensorBuilder() {
    service_ = std::make_shared<Service>(kType_CarbonMonoxideSensor, "Carbon Monoxide Sensor", true);
    
    co_detected_ = chr::CarbonMonoxideDetectedChar();
    add_characteristic(co_detected_);
}

CarbonMonoxideSensorBuilder& CarbonMonoxideSensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

CarbonMonoxideSensorBuilder& CarbonMonoxideSensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

CarbonMonoxideSensorBuilder& CarbonMonoxideSensorBuilder::with_level() {
    add_characteristic(chr::CarbonMonoxideLevel());
    add_characteristic(chr::CarbonMonoxidePeakLevel());
    return *this;
}

std::shared_ptr<Service> CarbonMonoxideSensorBuilder::build() {
    return service_;
}

//==============================================================================
// Air Quality Sensor Service Builder (9.11)
//==============================================================================

AirQualitySensorBuilder::AirQualitySensorBuilder() {
    service_ = std::make_shared<Service>(kType_AirQualitySensor, "Air Quality Sensor", true);
    
    air_quality_ = chr::AirQualityChar();
    add_characteristic(air_quality_);
}

AirQualitySensorBuilder& AirQualitySensorBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

AirQualitySensorBuilder& AirQualitySensorBuilder::with_pm25() {
    add_characteristic(chr::PM2_5Density());
    return *this;
}

AirQualitySensorBuilder& AirQualitySensorBuilder::with_pm10() {
    add_characteristic(chr::PM10Density());
    return *this;
}

AirQualitySensorBuilder& AirQualitySensorBuilder::with_voc() {
    add_characteristic(chr::VOCDensity());
    return *this;
}

AirQualitySensorBuilder& AirQualitySensorBuilder::with_status_active() {
    add_characteristic(chr::StatusActive());
    return *this;
}

std::shared_ptr<Service> AirQualitySensorBuilder::build() {
    return service_;
}

//==============================================================================
// Stateless Programmable Switch Service Builder (9.22)
//==============================================================================

StatelessProgrammableSwitchBuilder::StatelessProgrammableSwitchBuilder() {
    service_ = std::make_shared<Service>(kType_StatelessProgrammableSwitch, "Stateless Programmable Switch", true);
    
    switch_event_ = chr::ProgrammableSwitchEventChar();
    add_characteristic(switch_event_);
}

StatelessProgrammableSwitchBuilder& StatelessProgrammableSwitchBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

StatelessProgrammableSwitchBuilder& StatelessProgrammableSwitchBuilder::with_service_label_index(uint8_t index) {
    auto label_index = chr::ServiceLabelIndex();
    label_index->set_value(index);
    add_characteristic(label_index);
    return *this;
}

std::shared_ptr<Service> StatelessProgrammableSwitchBuilder::build() {
    return service_;
}

//==============================================================================
// Microphone Service Builder (9.29)
//==============================================================================

MicrophoneBuilder::MicrophoneBuilder() {
    service_ = std::make_shared<Service>(kType_Microphone, "Microphone", true);
    
    mute_ = chr::Mute();
    add_characteristic(mute_);
}

MicrophoneBuilder& MicrophoneBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

MicrophoneBuilder& MicrophoneBuilder::with_volume() {
    add_characteristic(chr::Volume());
    return *this;
}

std::shared_ptr<Service> MicrophoneBuilder::build() {
    return service_;
}

//==============================================================================
// Speaker Service Builder (9.30)
//==============================================================================

SpeakerBuilder::SpeakerBuilder() {
    service_ = std::make_shared<Service>(kType_Speaker, "Speaker", true);
    
    mute_ = chr::Mute();
    add_characteristic(mute_);
}

SpeakerBuilder& SpeakerBuilder::with_name(std::string name) {
    auto name_char = chr::Name();
    name_char->set_value(std::move(name));
    add_characteristic(name_char);
    return *this;
}

SpeakerBuilder& SpeakerBuilder::with_volume() {
    add_characteristic(chr::Volume());
    return *this;
}

std::shared_ptr<Service> SpeakerBuilder::build() {
    return service_;
}

} // namespace hap::service
