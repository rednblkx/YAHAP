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
    service_ = std::make_shared<Service>(kType_LightBulb, "Lightbulb");
    
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
    service_ = std::make_shared<Service>(kType_Switch, "Switch");
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
    service_ = std::make_shared<Service>(kType_Outlet, "Outlet");
    
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
    service_ = std::make_shared<Service>(kType_Thermostat, "Thermostat");
    
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
    service_ = std::make_shared<Service>(kType_TemperatureSensor, "Temperature Sensor");
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
    service_ = std::make_shared<Service>(kType_HumiditySensor, "Humidity Sensor");
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
    service_ = std::make_shared<Service>(kType_MotionSensor, "Motion Sensor");
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
    service_ = std::make_shared<Service>(kType_ContactSensor, "Contact Sensor");
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
    service_ = std::make_shared<Service>(kType_GarageDoorOpener, "Garage Door Opener");
    
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
    service_ = std::make_shared<Service>(kType_LockMechanism, "Lock Mechanism");
    
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
// FanBuilder
//==============================================================================

FanBuilder::FanBuilder() {
    service_ = std::make_shared<Service>(kType_Fan, "Fan");
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
    service_ = std::make_shared<Service>(kType_WindowCovering, "Window Covering");
    
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
    service_ = std::make_shared<Service>(kType_SecuritySystem, "Security System");
    
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

} // namespace hap::service
