/**
 * @file CharacteristicTypes.hpp
 * @brief HAP characteristic type constants and descriptor table
 *
 * Every HAP-defined characteristic is described by a CharacteristicDesc row:
 * type, format, permissions, optional metadata (unit/min/max/step/maxLen) and
 * an optional default value. `make_characteristic()` materializes a row into
 * a live Characteristic object. This replaces ~112 per-characteristic factory
 * functions with one data table (~40 KB smaller on MinSizeRel).
 *
 * Usage:
 *   auto brightness = hap::characteristic::make_characteristic(
 *       hap::characteristic::kBrightness);
 */
#pragma once

#include "hap/core/Characteristic.hpp"
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace hap::characteristic {

//==============================================================================
// Characteristic UUID Type Constants
//==============================================================================

constexpr uint64_t kType_AccessoryFlags = 0xA6;
constexpr uint64_t kType_FirmwareRevision = 0x52;
constexpr uint64_t kType_HardwareRevision = 0x53;
constexpr uint64_t kType_Identify = 0x14;
constexpr uint64_t kType_Manufacturer = 0x20;
constexpr uint64_t kType_Model = 0x21;
constexpr uint64_t kType_Name = 0x23;
constexpr uint64_t kType_SerialNumber = 0x30;
constexpr uint64_t kType_HardwareFinish = 0x26C;
constexpr uint64_t kType_On = 0x25;
constexpr uint64_t kType_Brightness = 0x08;
constexpr uint64_t kType_Hue = 0x13;
constexpr uint64_t kType_Saturation = 0x2F;
constexpr uint64_t kType_ColorTemperature = 0xCE;
constexpr uint64_t kType_CurrentTemperature = 0x11;
constexpr uint64_t kType_TargetTemperature = 0x35;
constexpr uint64_t kType_TemperatureDisplayUnits = 0x36;
constexpr uint64_t kType_CurrentHeatingCoolingState = 0x0F;
constexpr uint64_t kType_TargetHeatingCoolingState = 0x33;
constexpr uint64_t kType_CoolingThresholdTemperature = 0x0D;
constexpr uint64_t kType_HeatingThresholdTemperature = 0x12;
constexpr uint64_t kType_CurrentRelativeHumidity = 0x10;
constexpr uint64_t kType_TargetRelativeHumidity = 0x34;
constexpr uint64_t kType_RelativeHumidityDehumidifierThreshold = 0xC9;
constexpr uint64_t kType_RelativeHumidityHumidifierThreshold = 0xCA;
constexpr uint64_t kType_CurrentDoorState = 0x0E;
constexpr uint64_t kType_TargetDoorState = 0x32;
constexpr uint64_t kType_ObstructionDetected = 0x24;
constexpr uint64_t kType_LockCurrentState = 0x1D;
constexpr uint64_t kType_LockTargetState = 0x1E;
constexpr uint64_t kType_LockControlPoint = 0x19;
constexpr uint64_t kType_LockLastKnownAction = 0x1C;
constexpr uint64_t kType_LockManagementAutoSecurityTimeout = 0x1A;
constexpr uint64_t kType_LockPhysicalControls = 0xA7;
constexpr uint64_t kType_NFCAccessControlPoint = 0x264;
constexpr uint64_t kType_NFCAccessSupportedConfiguration = 0x265;
constexpr uint64_t kType_ConfigurationState = 0x263;
constexpr uint64_t kType_Active = 0xB0;
constexpr uint64_t kType_CurrentFanState = 0xAF;
constexpr uint64_t kType_TargetFanState = 0xBF;
constexpr uint64_t kType_RotationDirection = 0x28;
constexpr uint64_t kType_RotationSpeed = 0x29;
constexpr uint64_t kType_SwingMode = 0xB6;
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
constexpr uint64_t kType_SlatType = 0xC0;
constexpr uint64_t kType_CurrentSlatState = 0xAA;
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
constexpr uint64_t kType_AirQuality = 0x95;
constexpr uint64_t kType_AirParticulateDensity = 0x64;
constexpr uint64_t kType_AirParticulateSize = 0x65;
constexpr uint64_t kType_OzoneDensity = 0xC3;
constexpr uint64_t kType_NitrogenDioxideDensity = 0xC4;
constexpr uint64_t kType_SulphurDioxideDensity = 0xC5;
constexpr uint64_t kType_PM2_5Density = 0xC6;
constexpr uint64_t kType_PM10Density = 0xC7;
constexpr uint64_t kType_VOCDensity = 0xC8;
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
constexpr uint64_t kType_SecuritySystemCurrentState = 0x66;
constexpr uint64_t kType_SecuritySystemTargetState = 0x67;
constexpr uint64_t kType_SecuritySystemAlarmType = 0x8E;
constexpr uint64_t kType_BatteryLevel = 0x68;
constexpr uint64_t kType_ChargingState = 0x8F;
constexpr uint64_t kType_StatusLowBattery = 0x79;
constexpr uint64_t kType_StatusActive = 0x75;
constexpr uint64_t kType_StatusFault = 0x77;
constexpr uint64_t kType_StatusJammed = 0x78;
constexpr uint64_t kType_StatusTampered = 0x7A;
constexpr uint64_t kType_OutletInUse = 0x26;
constexpr uint64_t kType_AudioFeedback = 0x05;
constexpr uint64_t kType_Volume = 0x119;
constexpr uint64_t kType_Mute = 0x11A;
constexpr uint64_t kType_ProgrammableSwitchEvent = 0x73;
constexpr uint64_t kType_ServiceLabelIndex = 0xCB;
constexpr uint64_t kType_ServiceLabelNamespace = 0xCD;
constexpr uint64_t kType_InUse = 0xD2;
constexpr uint64_t kType_IsConfigured = 0xD6;
constexpr uint64_t kType_ProgramMode = 0xD1;
constexpr uint64_t kType_RemainingDuration = 0xD4;
constexpr uint64_t kType_SetDuration = 0xD3;
constexpr uint64_t kType_ValveType = 0xD5;
constexpr uint64_t kType_AdministratorOnlyAccess = 0x01;
constexpr uint64_t kType_Logs = 0x1F;
constexpr uint64_t kType_Version = 0x37;
constexpr uint64_t kType_PairSetup = 0x4C;
constexpr uint64_t kType_PairVerify = 0x4E;
constexpr uint64_t kType_PairingFeatures = 0x4F;
constexpr uint64_t kType_PairingPairings = 0x50;
constexpr uint64_t kType_ServiceSignature = 0xA5;

//==============================================================================
// Permission set shorthands
//==============================================================================

constexpr core::Permissions kPermPR{core::Permission::PairedRead};
constexpr core::Permissions kPermPW{core::Permission::PairedWrite};
constexpr core::Permissions kPermPRNT{core::Permission::PairedRead, core::Permission::Notify};
constexpr core::Permissions kPermPRPW{core::Permission::PairedRead, core::Permission::PairedWrite};
constexpr core::Permissions kPermPRPWNT{core::Permission::PairedRead, core::Permission::PairedWrite, core::Permission::Notify};
constexpr core::Permissions kPermPRPWWr{core::Permission::PairedRead, core::Permission::PairedWrite, core::Permission::WriteResponse};

//==============================================================================
// Characteristic descriptor
//==============================================================================

struct CharacteristicDesc {
    uint64_t type;
    core::Format format;
    core::Permissions permissions;
    // Optional metadata (field order matters: designated initializers in
    // the table must follow this declaration order).
    std::string_view unit;
    bool has_unit = false;
    double min_value = 0.0;
    bool has_min = false;
    double max_value = 0.0;
    bool has_max = false;
    double min_step = 0.0;
    bool has_step = false;
    uint32_t max_len = 0;
    bool has_max_len = false;
    // Optional default value, stored compactly: the catalog is ROM-resident
    // data, so a full core::Value variant (with std::string + vector members)
    // would cost ~120 bytes/row plus static-init code. Defaults are one of:
    // a double (numeric/bool formats), a string literal, or a TLV8 byte stream.
    enum class DefaultKind : uint8_t { None, Number, String, Tlv8 };
    DefaultKind default_kind = DefaultKind::None;
    double default_number = 0.0;
    const char* default_string = nullptr;
    const uint8_t* tlv8_default_data = nullptr;
    uint8_t tlv8_default_len = 0;
};

// Named handles into kCharacteristics for the entries the builders reference.
enum class CharId : uint8_t {
    AccessoryFlags,
    FirmwareRevision,
    HardwareRevision,
    Identify,
    Manufacturer,
    Model,
    Name,
    SerialNumber,
    HardwareFinish,
    On,
    Brightness,
    Hue,
    Saturation,
    ColorTemperature,
    CurrentTemperature,
    TargetTemperature,
    TemperatureDisplayUnitsChar,
    CurrentHeatingCoolingStateChar,
    TargetHeatingCoolingStateChar,
    CoolingThresholdTemperature,
    HeatingThresholdTemperature,
    CurrentRelativeHumidity,
    TargetRelativeHumidity,
    CurrentDoorStateChar,
    TargetDoorStateChar,
    ObstructionDetected,
    LockCurrentStateChar,
    LockTargetStateChar,
    ActiveChar,
    CurrentFanStateChar,
    TargetFanStateChar,
    RotationDirectionChar,
    RotationSpeed,
    SwingModeChar,
    CurrentPosition,
    TargetPosition,
    PositionStateChar,
    HoldPositionChar,
    MotionDetected,
    OccupancyDetected,
    ContactSensorStateChar,
    LeakDetected,
    SmokeDetected,
    CarbonMonoxideDetectedChar,
    CarbonMonoxideLevel,
    CarbonDioxideDetectedChar,
    CarbonDioxideLevel,
    CurrentAmbientLightLevel,
    AirQualityChar,
    PM2_5Density,
    PM10Density,
    VOCDensity,
    SecuritySystemCurrentStateChar,
    SecuritySystemTargetStateChar,
    BatteryLevel,
    ChargingStateChar,
    StatusLowBatteryChar,
    StatusActive,
    StatusFault,
    StatusTampered,
    OutletInUse,
    Volume,
    Mute,
    ProgrammableSwitchEventChar,
    ServiceLabelIndex,
    ServiceLabelNamespaceChar,
    InUseChar,
    IsConfigured,
    RemainingDuration,
    SetDuration,
    ValveTypeChar,
    ProgramMode,
    CurrentAirPurifierStateChar,
    TargetAirPurifierStateChar,
    CurrentHeaterCoolerStateChar,
    TargetHeaterCoolerStateChar,
    CurrentHumidifierDehumidifierStateChar,
    TargetHumidifierDehumidifierStateChar,
    WaterLevel,
    RelativeHumidityDehumidifierThreshold,
    RelativeHumidityHumidifierThreshold,
    FilterLifeLevel,
    FilterChangeIndication,
    ResetFilterIndication,
    CurrentSlatStateChar,
    SlatTypeChar,
    CurrentTiltAngle,
    TargetTiltAngle,
    CurrentHorizontalTiltAngle,
    TargetHorizontalTiltAngle,
    CurrentVerticalTiltAngle,
    TargetVerticalTiltAngle,
    OzoneDensity,
    NitrogenDioxideDensity,
    SulphurDioxideDensity,
    AirParticulateDensity,
    AirParticulateSize,
    CarbonMonoxidePeakLevel,
    CarbonDioxidePeakLevel,
    NFCAccessControlPoint,
    NFCAccessSupportedConfiguration,
    ConfigurationState,
    LockControlPoint,
    LockPhysicalControls,
    LockManagementAutoSecurityTimeout,
    LockLastKnownAction,
    Logs,
    Version,
    AdministratorOnlyAccess,
    AudioFeedback,
    StatusJammed,
    SecuritySystemAlarmType,
    Count
};

/**
 * @brief The full HAP characteristic catalog, indexed by CharId.
 */
extern const CharacteristicDesc kCharacteristics[static_cast<size_t>(CharId::Count)];

/**
 * @brief Look up a descriptor by characteristic type value.
 * @return nullptr if the type is not in the catalog.
 */
const CharacteristicDesc* find_characteristic_desc(uint64_t type);

/**
 * @brief Materialize a descriptor into a live Characteristic.
 */
std::unique_ptr<core::Characteristic> make_characteristic(CharId id);
std::unique_ptr<core::Characteristic> make_characteristic(const CharacteristicDesc& desc);

} // namespace hap::characteristic
