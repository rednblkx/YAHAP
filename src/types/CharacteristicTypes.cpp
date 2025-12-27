/**
 * @file CharacteristicTypes.cpp
 * @brief Implementation of HAP Characteristic factory functions
 * 
 * Factory functions return pre-configured characteristics with proper
 * type, format, permissions, and metadata per HAP Specification R13.
 */

#include "hap/types/CharacteristicTypes.hpp"
#include "hap/core/TLV8.hpp"

namespace hap::characteristic {

using namespace hap::core;

//==============================================================================
// Helper macros for common permission patterns
//==============================================================================
#define PERM_PR     std::vector{Permission::PairedRead}
#define PERM_PW     std::vector{Permission::PairedWrite}
#define PERM_PR_NT  std::vector{Permission::PairedRead, Permission::Notify, Permission::Broadcast}
#define PERM_PR_PW  std::vector{Permission::PairedRead, Permission::PairedWrite}
#define PERM_PR_PW_NT std::vector{Permission::PairedRead, Permission::PairedWrite, Permission::Notify, Permission::Broadcast}
#define PERM_PR_PW_WR std::vector{Permission::PairedRead, Permission::PairedWrite, Permission::WriteResponse}

//==============================================================================
// Accessory Information Characteristics
//==============================================================================

std::shared_ptr<Characteristic> AccessoryFlags() {
    auto c = std::make_shared<Characteristic>(kType_AccessoryFlags, Format::UInt32, PERM_PR_NT);
    c->set_value(static_cast<uint32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> FirmwareRevision() {
    auto c = std::make_shared<Characteristic>(kType_FirmwareRevision, Format::String, PERM_PR);
    c->set_value(std::string("1.0.0"));
    return c;
}

std::shared_ptr<Characteristic> HardwareRevision() {
    auto c = std::make_shared<Characteristic>(kType_HardwareRevision, Format::String, PERM_PR);
    c->set_value(std::string("1.0.0"));
    return c;
}

std::shared_ptr<Characteristic> Identify() {
    auto c = std::make_shared<Characteristic>(kType_Identify, Format::Bool, PERM_PR_PW);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> Manufacturer() {
    auto c = std::make_shared<Characteristic>(kType_Manufacturer, Format::String, PERM_PR);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> Model() {
    auto c = std::make_shared<Characteristic>(kType_Model, Format::String, PERM_PR);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> Name() {
    auto c = std::make_shared<Characteristic>(kType_Name, Format::String, PERM_PR);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> SerialNumber() {
    auto c = std::make_shared<Characteristic>(kType_SerialNumber, Format::String, PERM_PR);
    c->set_max_len(64);
    c->set_value(std::string(""));
    return c;
}

std::shared_ptr<Characteristic> HardwareFinish() {
    auto c = std::make_shared<Characteristic>(kType_HardwareFinish, Format::TLV8, PERM_PR);
    c->set_value(TLV8::encode({TLV(0x01,{0xce,0xd5,0xda,0x00})}));
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

std::shared_ptr<Characteristic> ProgramMode() {
    auto c = std::make_shared<Characteristic>(kType_ProgramMode, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // No program scheduled
    return c;
}

//==============================================================================
// Air Purifier Characteristics (9.35)
//==============================================================================

std::shared_ptr<Characteristic> CurrentAirPurifierStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentAirPurifierState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Inactive
    return c;
}

std::shared_ptr<Characteristic> TargetAirPurifierStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetAirPurifierState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Manual
    return c;
}

//==============================================================================
// Heater Cooler Characteristics (9.36)
//==============================================================================

std::shared_ptr<Characteristic> CurrentHeaterCoolerStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentHeaterCoolerState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(0)); // Inactive
    return c;
}

std::shared_ptr<Characteristic> TargetHeaterCoolerStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetHeaterCoolerState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Auto
    return c;
}

//==============================================================================
// Humidifier Dehumidifier Characteristics (9.37)
//==============================================================================

std::shared_ptr<Characteristic> CurrentHumidifierDehumidifierStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentHumidifierDehumidifierState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(3);
    c->set_value(static_cast<uint8_t>(0)); // Inactive
    return c;
}

std::shared_ptr<Characteristic> TargetHumidifierDehumidifierStateChar() {
    auto c = std::make_shared<Characteristic>(kType_TargetHumidifierDehumidifierState, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Humidifier or Dehumidifier
    return c;
}

std::shared_ptr<Characteristic> WaterLevel() {
    auto c = std::make_shared<Characteristic>(kType_WaterLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> RelativeHumidityDehumidifierThreshold() {
    auto c = std::make_shared<Characteristic>(kType_RelativeHumidityDehumidifierThreshold, Format::Float, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(50.0f);
    return c;
}

std::shared_ptr<Characteristic> RelativeHumidityHumidifierThreshold() {
    auto c = std::make_shared<Characteristic>(kType_RelativeHumidityHumidifierThreshold, Format::Float, PERM_PR_PW_NT);
    c->set_unit("percentage");
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_min_step(1);
    c->set_value(50.0f);
    return c;
}

//==============================================================================
// Filter Maintenance Characteristics (9.34)
//==============================================================================

std::shared_ptr<Characteristic> FilterLifeLevel() {
    auto c = std::make_shared<Characteristic>(kType_FilterLifeLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_value(100.0f);
    return c;
}

std::shared_ptr<Characteristic> FilterChangeIndication() {
    auto c = std::make_shared<Characteristic>(kType_FilterChangeIndication, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Filter OK
    return c;
}

std::shared_ptr<Characteristic> ResetFilterIndication() {
    auto c = std::make_shared<Characteristic>(kType_ResetFilterIndication, Format::UInt8, PERM_PW);
    c->set_min_value(1);
    c->set_max_value(1);
    return c;
}

//==============================================================================
// Slat Characteristics (9.33)
//==============================================================================

std::shared_ptr<Characteristic> CurrentSlatStateChar() {
    auto c = std::make_shared<Characteristic>(kType_CurrentSlatState, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(2);
    c->set_value(static_cast<uint8_t>(0)); // Fixed
    return c;
}

std::shared_ptr<Characteristic> SlatTypeChar() {
    auto c = std::make_shared<Characteristic>(kType_SlatType, Format::UInt8, PERM_PR);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Horizontal
    return c;
}

std::shared_ptr<Characteristic> CurrentTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_CurrentTiltAngle, Format::Int, PERM_PR_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> TargetTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_TargetTiltAngle, Format::Int, PERM_PR_PW_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

//==============================================================================
// Window Tilt Characteristics
//==============================================================================

std::shared_ptr<Characteristic> CurrentHorizontalTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_CurrentHorizontalTiltAngle, Format::Int, PERM_PR_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> TargetHorizontalTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_TargetHorizontalTiltAngle, Format::Int, PERM_PR_PW_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> CurrentVerticalTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_CurrentVerticalTiltAngle, Format::Int, PERM_PR_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> TargetVerticalTiltAngle() {
    auto c = std::make_shared<Characteristic>(kType_TargetVerticalTiltAngle, Format::Int, PERM_PR_PW_NT);
    c->set_unit("arcdegrees");
    c->set_min_value(-90);
    c->set_max_value(90);
    c->set_min_step(1);
    c->set_value(static_cast<int32_t>(0));
    return c;
}

//==============================================================================
// Air Quality Extended Characteristics
//==============================================================================

std::shared_ptr<Characteristic> OzoneDensity() {
    auto c = std::make_shared<Characteristic>(kType_OzoneDensity, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> NitrogenDioxideDensity() {
    auto c = std::make_shared<Characteristic>(kType_NitrogenDioxideDensity, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> SulphurDioxideDensity() {
    auto c = std::make_shared<Characteristic>(kType_SulphurDioxideDensity, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> AirParticulateDensity() {
    auto c = std::make_shared<Characteristic>(kType_AirParticulateDensity, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1000);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> AirParticulateSize() {
    auto c = std::make_shared<Characteristic>(kType_AirParticulateSize, Format::UInt8, PERM_PR);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // 2.5 um
    return c;
}

std::shared_ptr<Characteristic> CarbonMonoxidePeakLevel() {
    auto c = std::make_shared<Characteristic>(kType_CarbonMonoxidePeakLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100);
    c->set_value(0.0f);
    return c;
}

std::shared_ptr<Characteristic> CarbonDioxidePeakLevel() {
    auto c = std::make_shared<Characteristic>(kType_CarbonDioxidePeakLevel, Format::Float, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(100000);
    c->set_value(0.0f);
    return c;
}

//============================================================================== 
// NFC Access Characteristics
//==============================================================================

std::shared_ptr<Characteristic> NFCAccessControlPoint() {
    auto c = std::make_shared<Characteristic>(kType_NFCAccessControlPoint, Format::TLV8, PERM_PR_PW_WR);
    c->set_value(TLV8::encode({}));
    return c;
}

std::shared_ptr<Characteristic> NFCAccessSupportedConfiguration() {
    auto c = std::make_shared<Characteristic>(kType_NFCAccessSupportedConfiguration, Format::TLV8, PERM_PR);
    c->set_value(TLV8::encode({TLV(0x01,0x10), TLV(0x02,0x10)}));
    return c;
}

std::shared_ptr<Characteristic> ConfigurationState() {
    auto c = std::make_shared<Characteristic>(kType_ConfigurationState, Format::UInt16, PERM_PR_NT);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

//==============================================================================
// Lock Management Characteristics
//==============================================================================

std::shared_ptr<Characteristic> LockControlPoint() {
    auto c = std::make_shared<Characteristic>(kType_LockControlPoint, Format::TLV8, PERM_PW);
    return c;
}

std::shared_ptr<Characteristic> LockPhysicalControls() {
    auto c = std::make_shared<Characteristic>(kType_LockPhysicalControls, Format::UInt8, PERM_PR_PW_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Control Lock Disabled
    return c;
}

std::shared_ptr<Characteristic> LockManagementAutoSecurityTimeout() {
    auto c = std::make_shared<Characteristic>(kType_LockManagementAutoSecurityTimeout, Format::UInt32, PERM_PR_PW_NT);
    c->set_unit("seconds");
    c->set_value(static_cast<uint32_t>(0));
    return c;
}

std::shared_ptr<Characteristic> LockLastKnownAction() {
    auto c = std::make_shared<Characteristic>(kType_LockLastKnownAction, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(10);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

std::shared_ptr<Characteristic> Logs() {
    auto c = std::make_shared<Characteristic>(kType_Logs, Format::TLV8, PERM_PR_NT);
    return c;
}

//==============================================================================
// Miscellaneous Characteristics
//==============================================================================

std::shared_ptr<Characteristic> Version() {
    auto c = std::make_shared<Characteristic>(kType_Version, Format::String, PERM_PR);
    c->set_value(std::string("1.0.0"));
    return c;
}

std::shared_ptr<Characteristic> AdministratorOnlyAccess() {
    auto c = std::make_shared<Characteristic>(kType_AdministratorOnlyAccess, Format::Bool, PERM_PR_PW_NT);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> AudioFeedback() {
    auto c = std::make_shared<Characteristic>(kType_AudioFeedback, Format::Bool, PERM_PR_PW_NT);
    c->set_value(false);
    return c;
}

std::shared_ptr<Characteristic> StatusJammed() {
    auto c = std::make_shared<Characteristic>(kType_StatusJammed, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0)); // Not Jammed
    return c;
}

std::shared_ptr<Characteristic> SecuritySystemAlarmType() {
    auto c = std::make_shared<Characteristic>(kType_SecuritySystemAlarmType, Format::UInt8, PERM_PR_NT);
    c->set_min_value(0);
    c->set_max_value(1);
    c->set_value(static_cast<uint8_t>(0));
    return c;
}

} // namespace hap::characteristic
