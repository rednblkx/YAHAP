/**
 * @file CharacteristicTypes.cpp
 * @brief Implementation of HAP Characteristic factory functions
 * 
 * Factory functions return pre-configured characteristics with proper
 * type, format, permissions, and metadata per HAP Specification R13.
 */

#include "hap/types/CharacteristicTypes.hpp"

namespace hap::characteristic {

using namespace hap::core;

//==============================================================================
// Helper macros for common permission patterns
//==============================================================================
#define PERM_PR     std::vector{Permission::PairedRead}
#define PERM_PW     std::vector{Permission::PairedWrite}
#define PERM_PR_NT  std::vector{Permission::PairedRead, Permission::Notify}
#define PERM_PR_PW  std::vector{Permission::PairedRead, Permission::PairedWrite}
#define PERM_PR_PW_NT std::vector{Permission::PairedRead, Permission::PairedWrite, Permission::Notify}

//==============================================================================
// Accessory Information Characteristics
//==============================================================================

std::shared_ptr<Characteristic> AccessoryFlags() {
    auto c = std::make_shared<Characteristic>(kType_AccessoryFlags, Format::UInt32, PERM_PR_NT);
    c->set_value(static_cast<uint32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> FirmwareRevision() {
    auto c = std::make_shared<Characteristic>(kType_FirmwareRevision, Format::String, PERM_PR_PW);
    c->set_value(std::string("1.0.0"));
    return c;
}

std::shared_ptr<Characteristic> HardwareRevision() {
    auto c = std::make_shared<Characteristic>(kType_HardwareRevision, Format::String, PERM_PR_PW);
    c->set_value(std::string("1.0.0"));
    return c;
}

std::shared_ptr<Characteristic> Identify() {
    auto c = std::make_shared<Characteristic>(kType_Identify, Format::Bool, PERM_PR_PW);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> Manufacturer() {
    auto c = std::make_shared<Characteristic>(kType_Manufacturer, Format::String, PERM_PR_PW);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> Model() {
    auto c = std::make_shared<Characteristic>(kType_Model, Format::String, PERM_PR_PW);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> Name() {
    auto c = std::make_shared<Characteristic>(kType_Name, Format::String, PERM_PR_PW);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> SerialNumber() {
    auto c = std::make_shared<Characteristic>(kType_SerialNumber, Format::String, PERM_PR_PW);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

//==============================================================================
// Lightbulb Characteristics
//==============================================================================

std::shared_ptr<Characteristic> On() {
    auto c = std::make_shared<Characteristic>(kType_On, Format::Bool, PERM_PR_PW_NT);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> Brightness() {
    auto c = std::make_shared<Characteristic>(kType_Brightness, Format::Int, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(100));
    return c;
}

std::shared_ptr<Characteristic> Hue() {
    auto c = std::make_shared<Characteristic>(kType_Hue, Format::Float, PERM_PR_PW_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(0);
    c->set_max_value(360);
    c->set_min_step(1);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> Saturation() {
    auto c = std::make_shared<Characteristic>(kType_Saturation, Format::Float, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> ColorTemperature() {
    auto c = std::make_shared<Characteristic>(kType_ColorTemperature, Format::UInt32, PERM_PR_PW_NT);
    c->set_min_value(140);    // ~7142K (cool white)
    c->set_max_value(500);    // 2000K (warm white)
    c->set_min_step(1);
    c->set_value(static_cast<uint32_t>(200));
    return c;
}

//==============================================================================
// Thermostat / Temperature Characteristics
//==============================================================================

std::shared_ptr<Characteristic> CurrentTemperature() {
    auto c = std::make_shared<Characteristic>(kType_CurrentTemperature, Format::Float, PERM_PR_NT);
    c->set_unit("celsius");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(0.1);
    c->set_value(20.0f);
    return c;
}

std::shared_ptr<Characteristic> TargetTemperature() {
    auto c = std::make_shared<Characteristic>(kType_TargetTemperature, Format::Float, PERM_PR_PW_NT);
    c->set_unit("celsius");
    c->set_min_value(10);
    c->set_max_value(38);
    c->set_min_step(0.1);
    c->set_value(20.0f);
    return c;
}

std::shared_ptr<Characteristic> TemperatureDisplayUnitsChar() {
    auto c = std::make_shared<Characteristic>(kType_TemperatureDisplayUnits, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Celsius
    return c;
}

std::shared_ptr<Characteristic> CurrentHeatingCoolingStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentHeatingCoolingState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Off
    return c;
}

std::shared_ptr<Characteristic> TargetHeatingCoolingStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetHeatingCoolingState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(0)); // Off
    return c;
}

std::shared_ptr<Characteristic> CoolingThresholdTemperature() {
    auto c = std::make_shared<Characteristic>(kType_CoolingThresholdTemperature, Format::Float, PERM_PR_PW_NT);
    c->set_unit("celsius");
    c->set_min_value(10);
    c->set_max_value(35);
    c->set_min_step(0.1);
    c->set_value(26.0f);
    return c;
}

std::shared_ptr<Characteristic> HeatingThresholdTemperature() {
    auto c = std::make_shared<Characteristic>(kType_HeatingThresholdTemperature, Format::Float, PERM_PR_PW_NT);
    c->set_unit("celsius");
    c->set_min_value(0);
    c->set_max_value(25);
    c->set_min_step(0.1);
    c->set_value(18.0f);
    return c;
}

//==============================================================================
// Humidity Characteristics
//==============================================================================

std::shared_ptr<Characteristic> CurrentRelativeHumidity() {
    auto c = std::make_shared<Characteristic>(kType_CurrentRelativeHumidity, Format::Float, PERM_PR_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(50.0f);
    return c;
}

std::shared_ptr<Characteristic> TargetRelativeHumidity() {
    auto c = std::make_shared<Characteristic>(kType_TargetRelativeHumidity, Format::Float, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(50.0f);
    return c;
}

//==============================================================================
// Door / Garage Characteristics
//==============================================================================

std::shared_ptr<Characteristic> CurrentDoorStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentDoorState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(4);
    c->set_value(static_cast<uint8_t>(1)); // Closed
    return c;
}

std::shared_ptr<Characteristic> TargetDoorStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetDoorState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(1)); // Closed
    return c;
}

std::shared_ptr<Characteristic> ObstructionDetected() {
    auto c = std::make_shared<Characteristic>(kType_ObstructionDetected, Format::Bool, PERM_PR_NT);
    c->set_value(false);
    return c;
}

//==============================================================================
// Lock Characteristics
//==============================================================================

std::shared_ptr<Characteristic> LockCurrentStateChar() {
    auto c = std::make_shared<Characteristic>(kType_LockCurrentState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(1)); // Secured
    return c;
}

std::shared_ptr<Characteristic> LockTargetStateChar() {
    auto c = std::make_shared<Characteristic>(kType_LockTargetState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(1)); // Secured
    return c;
}

//==============================================================================
// Fan Characteristics
//==============================================================================

std::shared_ptr<Characteristic> ActiveChar() {
    auto c = std::make_shared<Characteristic>(kType_Active, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Inactive
    return c;
}

std::shared_ptr<Characteristic> CurrentFanStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentFanState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Inactive
    return c;
}

std::shared_ptr<Characteristic> TargetFanStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetFanState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Manual
    return c;
}

std::shared_ptr<Characteristic> RotationDirectionChar() {
    auto c = std::make_shared<Characteristic>(kType_RotationDirection, Format::Int, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<int32_t>(0)); // Clockwise
    return c;
}

std::shared_ptr<Characteristic> RotationSpeed() {
    auto c = std::make_shared<Characteristic>(kType_RotationSpeed, Format::Float, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> SwingModeChar() {
    auto c = std::make_shared<Characteristic>(kType_SwingMode, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Disabled
    return c;
}

//==============================================================================
// Window / Covering Characteristics
//==============================================================================

std::shared_ptr<Characteristic> CurrentPosition() {
    auto c = std::make_shared<Characteristic>(kType_CurrentPosition, Format::UInt8, PERM_PR_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> TargetPosition() {
    auto c = std::make_shared<Characteristic>(kType_TargetPosition, Format::UInt8, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> PositionStateChar() {
    auto c = std::make_shared<Characteristic>(kType_PositionState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(2)); // Stopped
    return c;
}

std::shared_ptr<Characteristic> HoldPositionChar() {
    auto c = std::make_shared<Characteristic>(kType_HoldPosition, Format::Bool, PERM_PW);
    c->set_value(false);
    return c;
}

//==============================================================================
// Sensor Characteristics
//==============================================================================

std::shared_ptr<Characteristic> MotionDetected() {
    auto c = std::make_shared<Characteristic>(kType_MotionDetected, Format::Bool, PERM_PR_NT);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> OccupancyDetected() {
    auto c = std::make_shared<Characteristic>(kType_OccupancyDetected, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> ContactSensorStateChar() {
    auto c = std::make_shared<Characteristic>(kType_ContactSensorState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> LeakDetected() {
    auto c = std::make_shared<Characteristic>(kType_LeakDetected, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> SmokeDetected() {
    auto c = std::make_shared<Characteristic>(kType_SmokeDetected, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> CarbonMonoxideDetectedChar() {
    auto c = std::make_shared<Characteristic>(kType_CarbonMonoxideDetected, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> CarbonMonoxideLevel() {
    auto c = std::make_shared<Characteristic>(kType_CarbonMonoxideLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> CarbonDioxideDetectedChar() {
    auto c = std::make_shared<Characteristic>(kType_CarbonDioxideDetected, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> CarbonDioxideLevel() {
    auto c = std::make_shared<Characteristic>(kType_CarbonDioxideLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> CurrentAmbientLightLevel() {
    auto c = std::make_shared<Characteristic>(kType_CurrentAmbientLightLevel, Format::Float, PERM_PR_NT);
    c->set_unit("lux");
    c->set_min_value(0.0001);
    c->set_max_value(100000);
    c->set_value(1.0f);
    return c;
}

//==============================================================================
// Air Quality Characteristics
//==============================================================================

std::shared_ptr<Characteristic> AirQualityChar() {
    auto c = std::make_shared<Characteristic>(kType_AirQuality, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(5);
    c->set_value(static_cast<uint8_t>(0)); // Unknown
    return c;
}

std::shared_ptr<Characteristic> PM2_5Density() {
    auto c = std::make_shared<Characteristic>(kType_PM2_5Density, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> PM10Density() {
    auto c = std::make_shared<Characteristic>(kType_PM10Density, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> VOCDensity() {
    auto c = std::make_shared<Characteristic>(kType_VOCDensity, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

//==============================================================================
// Security System Characteristics
//==============================================================================

std::shared_ptr<Characteristic> SecuritySystemCurrentStateChar() {
    auto c = std::make_shared<Characteristic>(kType_SecuritySystemCurrentState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(4);
    c->set_value(static_cast<uint8_t>(3)); // Disarmed
    return c;
}

std::shared_ptr<Characteristic> SecuritySystemTargetStateChar() {
    auto c = std::make_shared<Characteristic>(kType_SecuritySystemTargetState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(3)); // Disarm
    return c;
}

//==============================================================================
// Battery Characteristics
//==============================================================================

std::shared_ptr<Characteristic> BatteryLevel() {
    auto c = std::make_shared<Characteristic>(kType_BatteryLevel, Format::UInt8, PERM_PR_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_value(static_cast<uint8_t>(100));
    return c;
}

std::shared_ptr<Characteristic> ChargingStateChar() {
    auto c = std::make_shared<Characteristic>(kType_ChargingState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Not Charging
    return c;
}

std::shared_ptr<Characteristic> StatusLowBatteryChar() {
    auto c = std::make_shared<Characteristic>(kType_StatusLowBattery, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Normal
    return c;
}

//==============================================================================
// Status Characteristics
//==============================================================================

std::shared_ptr<Characteristic> StatusActive() {
    auto c = std::make_shared<Characteristic>(kType_StatusActive, Format::Bool, PERM_PR_NT);
    c->set_value(true);
    return c;
}

std::shared_ptr<Characteristic> StatusFault() {
    auto c = std::make_shared<Characteristic>(kType_StatusFault, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // No Fault
    return c;
}

std::shared_ptr<Characteristic> StatusTampered() {
    auto c = std::make_shared<Characteristic>(kType_StatusTampered, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Not Tampered
    return c;
}

//==============================================================================
// Outlet Characteristics
//==============================================================================

std::shared_ptr<Characteristic> OutletInUse() {
    auto c = std::make_shared<Characteristic>(kType_OutletInUse, Format::Bool, PERM_PR_NT);
    c->set_value(false);
    return c;
}

//==============================================================================
// Audio Characteristics
//==============================================================================

std::shared_ptr<Characteristic> Volume() {
    auto c = std::make_shared<Characteristic>(kType_Volume, Format::UInt8, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(static_cast<uint8_t>(50));
    return c;
}

std::shared_ptr<Characteristic> Mute() {
    auto c = std::make_shared<Characteristic>(kType_Mute, Format::Bool, PERM_PR_PW_NT);
    c->set_value(false);
    return c;
}

//==============================================================================
// Switch Characteristics
//==============================================================================

std::shared_ptr<Characteristic> ProgrammableSwitchEventChar() {
    // Note: This characteristic is event-only (null value allowed)
    auto c = std::make_shared<Characteristic>(kType_ProgrammableSwitchEvent, Format::UInt8, 
        std::vector{Permission::PairedRead, Permission::Notify});
    c->set_min_value(0);
    c->set_max_value(2);
    return c;
}

std::shared_ptr<Characteristic> ServiceLabelIndex() {
    auto c = std::make_shared<Characteristic>(kType_ServiceLabelIndex, Format::UInt8, PERM_PR);
    c->set_min_value(1);
    c->set_max_value(255);
    c->set_value(static_cast<uint8_t>(1));
    return c;
}

std::shared_ptr<Characteristic> ServiceLabelNamespaceChar() {
    auto c = std::make_shared<Characteristic>(kType_ServiceLabelNamespace, Format::UInt8, PERM_PR);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(1)); // Arabic numerals
    return c;
}

//==============================================================================
// Valve Characteristics
//==============================================================================

std::shared_ptr<Characteristic> InUseChar() {
    auto c = std::make_shared<Characteristic>(kType_InUse, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Not in use
    return c;
}

std::shared_ptr<Characteristic> IsConfigured() {
    auto c = std::make_shared<Characteristic>(kType_IsConfigured, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Not configured
    return c;
}

std::shared_ptr<Characteristic> RemainingDuration() {
    auto c = std::make_shared<Characteristic>(kType_RemainingDuration, Format::UInt32, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(3600);
    c->set_value(static_cast<uint32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> SetDuration() {
    auto c = std::make_shared<Characteristic>(kType_SetDuration, Format::UInt32, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(3600);
    c->set_value(static_cast<uint32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> ValveTypeChar() {
    auto c = std::make_shared<Characteristic>(kType_ValveType, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(0)); // Generic
    return c;
}

} // namespace hap::characteristic
