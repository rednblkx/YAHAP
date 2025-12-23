/**
 * @file CharacteristicTypes.hpp
 * @brief Pre-defined HAP Characteristic types and factory functions
 * 
 * This file provides UUID constants and factory functions for all HAP-defined
 * characteristics per HomeKit Accessory Protocol Specification R13.
 * 
 * Usage:
 *   auto brightness = hap::characteristic::Brightness();
 *   brightness->set_value(static_cast<int32_t>(75));
 */
#pragma once

#include "hap/core/Characteristic.hpp"
#include <memory>

namespace hap::characteristic {

//==============================================================================
// Characteristic UUID Type Constants (HAP Spec R13 Section 10)
//==============================================================================

// Accessory Information
constexpr uint64_t kType_AccessoryFlags = 0xA6;
constexpr uint64_t kType_FirmwareRevision = 0x52;
constexpr uint64_t kType_HardwareRevision = 0x53;
constexpr uint64_t kType_Identify = 0x14;
constexpr uint64_t kType_Manufacturer = 0x20;
constexpr uint64_t kType_Model = 0x21;
constexpr uint64_t kType_Name = 0x23;
constexpr uint64_t kType_SerialNumber = 0x30;
constexpr uint64_t kType_HardwareFinish = 0x26C;

// Lightbulb
constexpr uint64_t kType_On = 0x25;
constexpr uint64_t kType_Brightness = 0x08;
constexpr uint64_t kType_Hue = 0x13;
constexpr uint64_t kType_Saturation = 0x2F;
constexpr uint64_t kType_ColorTemperature = 0xCE;

// Thermostat / Temperature
constexpr uint64_t kType_CurrentTemperature = 0x11;
constexpr uint64_t kType_TargetTemperature = 0x35;
constexpr uint64_t kType_TemperatureDisplayUnits = 0x36;
constexpr uint64_t kType_CurrentHeatingCoolingState = 0x0F;
constexpr uint64_t kType_TargetHeatingCoolingState = 0x33;
constexpr uint64_t kType_CoolingThresholdTemperature = 0x0D;
constexpr uint64_t kType_HeatingThresholdTemperature = 0x12;

// Humidity
constexpr uint64_t kType_CurrentRelativeHumidity = 0x10;
constexpr uint64_t kType_TargetRelativeHumidity = 0x34;
constexpr uint64_t kType_RelativeHumidityDehumidifierThreshold = 0xC9;
constexpr uint64_t kType_RelativeHumidityHumidifierThreshold = 0xCA;

// Door / Garage
constexpr uint64_t kType_CurrentDoorState = 0x0E;
constexpr uint64_t kType_TargetDoorState = 0x32;
constexpr uint64_t kType_ObstructionDetected = 0x24;

// Lock
constexpr uint64_t kType_LockCurrentState = 0x1D;
constexpr uint64_t kType_LockTargetState = 0x1E;
constexpr uint64_t kType_LockControlPoint = 0x19;
constexpr uint64_t kType_LockLastKnownAction = 0x1C;
constexpr uint64_t kType_LockManagementAutoSecurityTimeout = 0x1A;
constexpr uint64_t kType_LockPhysicalControls = 0xA7;

// NFC Access
constexpr uint64_t kType_NFCAccessControlPoint = 0x264;
constexpr uint64_t kType_NFCAccessSupportedConfiguration = 0x265;
constexpr uint64_t kType_ConfigurationState = 0x263;

// Fan
constexpr uint64_t kType_Active = 0xB0;
constexpr uint64_t kType_CurrentFanState = 0xAF;
constexpr uint64_t kType_TargetFanState = 0xBF;
constexpr uint64_t kType_RotationDirection = 0x28;
constexpr uint64_t kType_RotationSpeed = 0x29;
constexpr uint64_t kType_SwingMode = 0xB6;

// Window / Covering
constexpr uint64_t kType_CurrentPosition = 0x6D;
constexpr uint64_t kType_TargetPosition = 0x7C;
constexpr uint64_t kType_PositionState = 0x72;
constexpr uint64_t kType_HoldPosition = 0x6F;
constexpr uint64_t kType_CurrentHorizontalTiltAngle = 0x6C;
constexpr uint64_t kType_TargetHorizontalTiltAngle = 0x7B;
constexpr uint64_t kType_CurrentVerticalTiltAngle = 0x6E;
constexpr uint64_t kType_TargetVerticalTiltAngle = 0x7D;
constexpr uint64_t kType_CurrentTiltAngle = 0xC1;
constexpr uint64_t kType_TargetTiltAngle = 0xC2;

// Slat
constexpr uint64_t kType_SlatType = 0xC0;
constexpr uint64_t kType_CurrentSlatState = 0xAA;

// Sensors
constexpr uint64_t kType_MotionDetected = 0x22;
constexpr uint64_t kType_OccupancyDetected = 0x71;
constexpr uint64_t kType_ContactSensorState = 0x6A;
constexpr uint64_t kType_LeakDetected = 0x70;
constexpr uint64_t kType_SmokeDetected = 0x76;
constexpr uint64_t kType_CarbonMonoxideDetected = 0x69;
constexpr uint64_t kType_CarbonMonoxideLevel = 0x90;
constexpr uint64_t kType_CarbonMonoxidePeakLevel = 0x91;
constexpr uint64_t kType_CarbonDioxideDetected = 0x92;
constexpr uint64_t kType_CarbonDioxideLevel = 0x93;
constexpr uint64_t kType_CarbonDioxidePeakLevel = 0x94;
constexpr uint64_t kType_CurrentAmbientLightLevel = 0x6B;

// Air Quality
constexpr uint64_t kType_AirQuality = 0x95;
constexpr uint64_t kType_AirParticulateDensity = 0x64;
constexpr uint64_t kType_AirParticulateSize = 0x65;
constexpr uint64_t kType_OzoneDensity = 0xC3;
constexpr uint64_t kType_NitrogenDioxideDensity = 0xC4;
constexpr uint64_t kType_SulphurDioxideDensity = 0xC5;
constexpr uint64_t kType_PM2_5Density = 0xC6;
constexpr uint64_t kType_PM10Density = 0xC7;
constexpr uint64_t kType_VOCDensity = 0xC8;

// Air Purifier / Heater / Humidifier
constexpr uint64_t kType_CurrentAirPurifierState = 0xA9;
constexpr uint64_t kType_TargetAirPurifierState = 0xA8;
constexpr uint64_t kType_CurrentHeaterCoolerState = 0xB1;
constexpr uint64_t kType_TargetHeaterCoolerState = 0xB2;
constexpr uint64_t kType_CurrentHumidifierDehumidifierState = 0xB3;
constexpr uint64_t kType_TargetHumidifierDehumidifierState = 0xB4;
constexpr uint64_t kType_WaterLevel = 0xB5;
constexpr uint64_t kType_FilterLifeLevel = 0xAB;
constexpr uint64_t kType_FilterChangeIndication = 0xAC;
constexpr uint64_t kType_ResetFilterIndication = 0xAD;

// Security System
constexpr uint64_t kType_SecuritySystemCurrentState = 0x66;
constexpr uint64_t kType_SecuritySystemTargetState = 0x67;
constexpr uint64_t kType_SecuritySystemAlarmType = 0x8E;

// Battery
constexpr uint64_t kType_BatteryLevel = 0x68;
constexpr uint64_t kType_ChargingState = 0x8F;
constexpr uint64_t kType_StatusLowBattery = 0x79;

// Status
constexpr uint64_t kType_StatusActive = 0x75;
constexpr uint64_t kType_StatusFault = 0x77;
constexpr uint64_t kType_StatusJammed = 0x78;
constexpr uint64_t kType_StatusTampered = 0x7A;

// Outlet
constexpr uint64_t kType_OutletInUse = 0x26;

// Audio/Feedback
constexpr uint64_t kType_AudioFeedback = 0x05;
constexpr uint64_t kType_Volume = 0x119;
constexpr uint64_t kType_Mute = 0x11A;

// Switch
constexpr uint64_t kType_ProgrammableSwitchEvent = 0x73;
constexpr uint64_t kType_ServiceLabelIndex = 0xCB;
constexpr uint64_t kType_ServiceLabelNamespace = 0xCD;

// Valve / Irrigation
constexpr uint64_t kType_InUse = 0xD2;
constexpr uint64_t kType_IsConfigured = 0xD6;
constexpr uint64_t kType_ProgramMode = 0xD1;
constexpr uint64_t kType_RemainingDuration = 0xD4;
constexpr uint64_t kType_SetDuration = 0xD3;
constexpr uint64_t kType_ValveType = 0xD5;

// Camera / Video
constexpr uint64_t kType_StreamingStatus = 0x120;
constexpr uint64_t kType_SupportedVideoStreamConfiguration = 0x114;
constexpr uint64_t kType_SupportedAudioStreamConfiguration = 0x115;
constexpr uint64_t kType_SupportedRTPConfiguration = 0x116;
constexpr uint64_t kType_SelectedRTPStreamConfiguration = 0x117;
constexpr uint64_t kType_SelectedAudioStreamConfiguration = 0x128;
constexpr uint64_t kType_SetupEndpoints = 0x118;
constexpr uint64_t kType_NightVision = 0x11B;
constexpr uint64_t kType_OpticalZoom = 0x11C;
constexpr uint64_t kType_DigitalZoom = 0x11D;
constexpr uint64_t kType_ImageRotation = 0x11E;
constexpr uint64_t kType_ImageMirroring = 0x11F;

// Remote / Target Control
constexpr uint64_t kType_ActiveIdentifier = 0xE7;
constexpr uint64_t kType_TargetControlSupportedConfiguration = 0x123;
constexpr uint64_t kType_TargetControlList = 0x124;
constexpr uint64_t kType_ButtonEvent = 0x126;

// Data Stream
constexpr uint64_t kType_SupportedDataStreamTransportConfiguration = 0x130;
constexpr uint64_t kType_SetupDataStreamTransport = 0x131;
constexpr uint64_t kType_SiriInputType = 0x132;

// Misc
constexpr uint64_t kType_AdministratorOnlyAccess = 0x01;
constexpr uint64_t kType_Logs = 0x1F;
constexpr uint64_t kType_Version = 0x37;

// Pairing (internal use)
constexpr uint64_t kType_PairSetup = 0x4C;
constexpr uint64_t kType_PairVerify = 0x4E;
constexpr uint64_t kType_PairingFeatures = 0x4F;
constexpr uint64_t kType_PairingPairings = 0x50;
constexpr uint64_t kType_ServiceSignature = 0xA5;

//==============================================================================
// Characteristic Value Enums
//==============================================================================

enum class CurrentDoorState : uint8_t {
    Open = 0,
    Closed = 1,
    Opening = 2,
    Closing = 3,
    Stopped = 4
};

enum class TargetDoorState : uint8_t {
    Open = 0,
    Closed = 1
};

enum class CurrentHeatingCoolingState : uint8_t {
    Off = 0,
    Heat = 1,
    Cool = 2
};

enum class TargetHeatingCoolingState : uint8_t {
    Off = 0,
    Heat = 1,
    Cool = 2,
    Auto = 3
};

enum class TemperatureDisplayUnits : uint8_t {
    Celsius = 0,
    Fahrenheit = 1
};

enum class LockCurrentState : uint8_t {
    Unsecured = 0,
    Secured = 1,
    Jammed = 2,
    Unknown = 3
};

enum class LockTargetState : uint8_t {
    Unsecured = 0,
    Secured = 1
};

enum class RotationDirection : uint8_t {
    Clockwise = 0,
    CounterClockwise = 1
};

enum class CurrentFanState : uint8_t {
    Inactive = 0,
    Idle = 1,
    BlowingAir = 2
};

enum class TargetFanState : uint8_t {
    Manual = 0,
    Auto = 1
};

enum class PositionState : uint8_t {
    Decreasing = 0,
    Increasing = 1,
    Stopped = 2
};

enum class ContactSensorState : uint8_t {
    Detected = 0,
    NotDetected = 1
};

enum class AirQuality : uint8_t {
    Unknown = 0,
    Excellent = 1,
    Good = 2,
    Fair = 3,
    Inferior = 4,
    Poor = 5
};

enum class SecuritySystemCurrentState : uint8_t {
    StayArm = 0,
    AwayArm = 1,
    NightArm = 2,
    Disarmed = 3,
    Triggered = 4
};

enum class SecuritySystemTargetState : uint8_t {
    StayArm = 0,
    AwayArm = 1,
    NightArm = 2,
    Disarm = 3
};

enum class ChargingState : uint8_t {
    NotCharging = 0,
    Charging = 1,
    NotChargeable = 2
};

enum class StatusLowBattery : uint8_t {
    Normal = 0,
    Low = 1
};

enum class ProgrammableSwitchEvent : uint8_t {
    SinglePress = 0,
    DoublePress = 1,
    LongPress = 2
};

enum class Active : uint8_t {
    Inactive = 0,
    Active = 1
};

enum class InUse : uint8_t {
    NotInUse = 0,
    InUse = 1
};

enum class ValveType : uint8_t {
    Generic = 0,
    Irrigation = 1,
    ShowerHead = 2,
    WaterFaucet = 3
};

//==============================================================================
// Factory Functions - Returns pre-configured characteristics
// All characteristics are returned with proper type, format, permissions,
// and metadata (min/max values, units, step values) per HAP Specification R13.
//==============================================================================

// Accessory Information Characteristics
std::shared_ptr<core::Characteristic> AccessoryFlags();
std::shared_ptr<core::Characteristic> FirmwareRevision();
std::shared_ptr<core::Characteristic> HardwareRevision();
std::shared_ptr<core::Characteristic> Identify();
std::shared_ptr<core::Characteristic> Manufacturer();
std::shared_ptr<core::Characteristic> Model();
std::shared_ptr<core::Characteristic> Name();
std::shared_ptr<core::Characteristic> SerialNumber();
std::shared_ptr<core::Characteristic> HardwareFinish();

// Lightbulb Characteristics
std::shared_ptr<core::Characteristic> On();
std::shared_ptr<core::Characteristic> Brightness();
std::shared_ptr<core::Characteristic> Hue();
std::shared_ptr<core::Characteristic> Saturation();
std::shared_ptr<core::Characteristic> ColorTemperature();

// Thermostat / Temperature Characteristics
std::shared_ptr<core::Characteristic> CurrentTemperature();
std::shared_ptr<core::Characteristic> TargetTemperature();
std::shared_ptr<core::Characteristic> TemperatureDisplayUnitsChar();
std::shared_ptr<core::Characteristic> CurrentHeatingCoolingStateChar();
std::shared_ptr<core::Characteristic> TargetHeatingCoolingStateChar();
std::shared_ptr<core::Characteristic> CoolingThresholdTemperature();
std::shared_ptr<core::Characteristic> HeatingThresholdTemperature();

// Humidity Characteristics
std::shared_ptr<core::Characteristic> CurrentRelativeHumidity();
std::shared_ptr<core::Characteristic> TargetRelativeHumidity();

// Door / Garage Characteristics
std::shared_ptr<core::Characteristic> CurrentDoorStateChar();
std::shared_ptr<core::Characteristic> TargetDoorStateChar();
std::shared_ptr<core::Characteristic> ObstructionDetected();

// Lock Characteristics
std::shared_ptr<core::Characteristic> LockCurrentStateChar();
std::shared_ptr<core::Characteristic> LockTargetStateChar();

// Fan Characteristics
std::shared_ptr<core::Characteristic> ActiveChar();
std::shared_ptr<core::Characteristic> CurrentFanStateChar();
std::shared_ptr<core::Characteristic> TargetFanStateChar();
std::shared_ptr<core::Characteristic> RotationDirectionChar();
std::shared_ptr<core::Characteristic> RotationSpeed();
std::shared_ptr<core::Characteristic> SwingModeChar();

// Window / Covering Characteristics
std::shared_ptr<core::Characteristic> CurrentPosition();
std::shared_ptr<core::Characteristic> TargetPosition();
std::shared_ptr<core::Characteristic> PositionStateChar();
std::shared_ptr<core::Characteristic> HoldPositionChar();

// Sensor Characteristics
std::shared_ptr<core::Characteristic> MotionDetected();
std::shared_ptr<core::Characteristic> OccupancyDetected();
std::shared_ptr<core::Characteristic> ContactSensorStateChar();
std::shared_ptr<core::Characteristic> LeakDetected();
std::shared_ptr<core::Characteristic> SmokeDetected();
std::shared_ptr<core::Characteristic> CarbonMonoxideDetectedChar();
std::shared_ptr<core::Characteristic> CarbonMonoxideLevel();
std::shared_ptr<core::Characteristic> CarbonDioxideDetectedChar();
std::shared_ptr<core::Characteristic> CarbonDioxideLevel();
std::shared_ptr<core::Characteristic> CurrentAmbientLightLevel();

// Air Quality Characteristics
std::shared_ptr<core::Characteristic> AirQualityChar();
std::shared_ptr<core::Characteristic> PM2_5Density();
std::shared_ptr<core::Characteristic> PM10Density();
std::shared_ptr<core::Characteristic> VOCDensity();

// Security System Characteristics
std::shared_ptr<core::Characteristic> SecuritySystemCurrentStateChar();
std::shared_ptr<core::Characteristic> SecuritySystemTargetStateChar();

// Battery Characteristics
std::shared_ptr<core::Characteristic> BatteryLevel();
std::shared_ptr<core::Characteristic> ChargingStateChar();
std::shared_ptr<core::Characteristic> StatusLowBatteryChar();

// Status Characteristics
std::shared_ptr<core::Characteristic> StatusActive();
std::shared_ptr<core::Characteristic> StatusFault();
std::shared_ptr<core::Characteristic> StatusTampered();

// Outlet Characteristics
std::shared_ptr<core::Characteristic> OutletInUse();

// Audio Characteristics
std::shared_ptr<core::Characteristic> Volume();
std::shared_ptr<core::Characteristic> Mute();

// Switch Characteristics
std::shared_ptr<core::Characteristic> ProgrammableSwitchEventChar();
std::shared_ptr<core::Characteristic> ServiceLabelIndex();
std::shared_ptr<core::Characteristic> ServiceLabelNamespaceChar();

// Valve Characteristics
std::shared_ptr<core::Characteristic> InUseChar();
std::shared_ptr<core::Characteristic> IsConfigured();
std::shared_ptr<core::Characteristic> RemainingDuration();
std::shared_ptr<core::Characteristic> SetDuration();
std::shared_ptr<core::Characteristic> ValveTypeChar();
std::shared_ptr<core::Characteristic> ProgramMode();

// Air Purifier Characteristics (9.35)
std::shared_ptr<core::Characteristic> CurrentAirPurifierStateChar();
std::shared_ptr<core::Characteristic> TargetAirPurifierStateChar();

// Heater Cooler Characteristics (9.36)
std::shared_ptr<core::Characteristic> CurrentHeaterCoolerStateChar();
std::shared_ptr<core::Characteristic> TargetHeaterCoolerStateChar();

// Humidifier Dehumidifier Characteristics (9.37)
std::shared_ptr<core::Characteristic> CurrentHumidifierDehumidifierStateChar();
std::shared_ptr<core::Characteristic> TargetHumidifierDehumidifierStateChar();
std::shared_ptr<core::Characteristic> WaterLevel();
std::shared_ptr<core::Characteristic> RelativeHumidityDehumidifierThreshold();
std::shared_ptr<core::Characteristic> RelativeHumidityHumidifierThreshold();

// Filter Maintenance Characteristics (9.34)
std::shared_ptr<core::Characteristic> FilterLifeLevel();
std::shared_ptr<core::Characteristic> FilterChangeIndication();
std::shared_ptr<core::Characteristic> ResetFilterIndication();

// Slat Characteristics (9.33)
std::shared_ptr<core::Characteristic> CurrentSlatStateChar();
std::shared_ptr<core::Characteristic> SlatTypeChar();
std::shared_ptr<core::Characteristic> CurrentTiltAngle();
std::shared_ptr<core::Characteristic> TargetTiltAngle();

// Window Tilt Characteristics
std::shared_ptr<core::Characteristic> CurrentHorizontalTiltAngle();
std::shared_ptr<core::Characteristic> TargetHorizontalTiltAngle();
std::shared_ptr<core::Characteristic> CurrentVerticalTiltAngle();
std::shared_ptr<core::Characteristic> TargetVerticalTiltAngle();

// Air Quality Extended Characteristics
std::shared_ptr<core::Characteristic> OzoneDensity();
std::shared_ptr<core::Characteristic> NitrogenDioxideDensity();
std::shared_ptr<core::Characteristic> SulphurDioxideDensity();
std::shared_ptr<core::Characteristic> AirParticulateDensity();
std::shared_ptr<core::Characteristic> AirParticulateSize();
std::shared_ptr<core::Characteristic> CarbonMonoxidePeakLevel();
std::shared_ptr<core::Characteristic> CarbonDioxidePeakLevel();

// NFC Access Characteristics
std::shared_ptr<core::Characteristic> NFCAccessControlPoint();
std::shared_ptr<core::Characteristic> NFCAccessSupportedConfiguration();
std::shared_ptr<core::Characteristic> ConfigurationState();

// Lock Management Characteristics
std::shared_ptr<core::Characteristic> LockControlPoint();
std::shared_ptr<core::Characteristic> LockPhysicalControls();
std::shared_ptr<core::Characteristic> LockManagementAutoSecurityTimeout();
std::shared_ptr<core::Characteristic> LockLastKnownAction();
std::shared_ptr<core::Characteristic> Logs();

// Miscellaneous Characteristics
std::shared_ptr<core::Characteristic> Version();
std::shared_ptr<core::Characteristic> AdministratorOnlyAccess();
std::shared_ptr<core::Characteristic> AudioFeedback();
std::shared_ptr<core::Characteristic> StatusJammed();
std::shared_ptr<core::Characteristic> SecuritySystemAlarmType();

} // namespace hap::characteristic
