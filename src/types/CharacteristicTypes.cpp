#include "hap/types/CharacteristicTypes.hpp"

// The catalog table uses designated initializers with intentionally-missing
// optional fields; -Wextra flags every such row otherwise.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace hap::characteristic {

using core::Format;

namespace {
const uint8_t kHwFinishTlv8[] = {0x01, 0x04, 0xce, 0xd5, 0xda, 0x00};
const uint8_t kNfcControlTlv8[] = {0x01, 0x00};
const uint8_t kNfcSupportedTlv8[] = {0x01, 0x01, 0x10, 0x02, 0x01, 0x10};
} // namespace

// Positional: row order must match the CharId enum order.
const CharacteristicDesc kCharacteristics[static_cast<size_t>(CharId::Count)] = {
    {.type = kType_AccessoryFlags, .format = Format::UInt32, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_FirmwareRevision, .format = Format::String, .permissions = kPermPR, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = "1.0.0"},
    {.type = kType_HardwareRevision, .format = Format::String, .permissions = kPermPR, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = "1.0.0"},
    {.type = kType_Identify, .format = Format::Bool, .permissions = kPermPRPW, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_Manufacturer, .format = Format::String, .permissions = kPermPR, .max_len = 64, .has_max_len = true, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = ""},
    {.type = kType_Model, .format = Format::String, .permissions = kPermPR, .max_len = 64, .has_max_len = true, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = ""},
    {.type = kType_Name, .format = Format::String, .permissions = kPermPR, .max_len = 64, .has_max_len = true, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = ""},
    {.type = kType_SerialNumber, .format = Format::String, .permissions = kPermPR, .max_len = 64, .has_max_len = true, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = ""},
    {.type = kType_HardwareFinish, .format = Format::TLV8, .permissions = kPermPR, .default_kind = CharacteristicDesc::DefaultKind::Tlv8, .tlv8_default_data = kHwFinishTlv8, .tlv8_default_len = 6},
    {.type = kType_On, .format = Format::Bool, .permissions = kPermPRPWNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_Brightness, .format = Format::Int, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 100},
    {.type = kType_Hue, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "arcdegrees", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 360, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_Saturation, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_ColorTemperature, .format = Format::UInt32, .permissions = kPermPRPWNT, .min_value = 140, .has_min = true, .max_value = 500, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 200},
    {.type = kType_CurrentTemperature, .format = Format::Float, .permissions = kPermPRNT, .unit = "celsius", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 0.1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 20.0},
    {.type = kType_TargetTemperature, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "celsius", .has_unit = true, .min_value = 10, .has_min = true, .max_value = 38, .has_max = true, .min_step = 0.1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 20.0},
    {.type = kType_TemperatureDisplayUnits, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentHeatingCoolingState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetHeatingCoolingState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CoolingThresholdTemperature, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "celsius", .has_unit = true, .min_value = 10, .has_min = true, .max_value = 35, .has_max = true, .min_step = 0.1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 26.0},
    {.type = kType_HeatingThresholdTemperature, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "celsius", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 25, .has_max = true, .min_step = 0.1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 18.0},
    {.type = kType_CurrentRelativeHumidity, .format = Format::Float, .permissions = kPermPRNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 50.0},
    {.type = kType_TargetRelativeHumidity, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 50.0},
    {.type = kType_CurrentDoorState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 4, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_TargetDoorState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_ObstructionDetected, .format = Format::Bool, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_LockCurrentState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_LockTargetState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_Active, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentFanState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetFanState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_RotationDirection, .format = Format::Int, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_RotationSpeed, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_SwingMode, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentPosition, .format = Format::UInt8, .permissions = kPermPRNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetPosition, .format = Format::UInt8, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_PositionState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 2},
    {.type = kType_HoldPosition, .format = Format::Bool, .permissions = kPermPW, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_MotionDetected, .format = Format::Bool, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_OccupancyDetected, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_ContactSensorState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_LeakDetected, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_SmokeDetected, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CarbonMonoxideDetected, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CarbonMonoxideLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_CarbonDioxideDetected, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CarbonDioxideLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_CurrentAmbientLightLevel, .format = Format::Float, .permissions = kPermPRNT, .unit = "lux", .has_unit = true, .min_value = 0.0001, .has_min = true, .max_value = 100000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1.0},
    {.type = kType_AirQuality, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 5, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_PM2_5Density, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_PM10Density, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_VOCDensity, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_SecuritySystemCurrentState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 4, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 3},
    {.type = kType_SecuritySystemTargetState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 3},
    {.type = kType_BatteryLevel, .format = Format::UInt8, .permissions = kPermPRNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 100},
    {.type = kType_ChargingState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_StatusLowBattery, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_StatusActive, .format = Format::Bool, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_StatusFault, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_StatusTampered, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_OutletInUse, .format = Format::Bool, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_Volume, .format = Format::UInt8, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 50},
    {.type = kType_Mute, .format = Format::Bool, .permissions = kPermPRPWNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_ProgrammableSwitchEvent, .format = Format::UInt8, .permissions = core::Permissions{core::Permission::PairedRead, core::Permission::Notify}, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true},
    {.type = kType_ServiceLabelIndex, .format = Format::UInt8, .permissions = kPermPR, .min_value = 1, .has_min = true, .max_value = 255, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_ServiceLabelNamespace, .format = Format::UInt8, .permissions = kPermPR, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 1},
    {.type = kType_InUse, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_IsConfigured, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_RemainingDuration, .format = Format::UInt32, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 3600, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_SetDuration, .format = Format::UInt32, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 3600, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_ValveType, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_ProgramMode, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentAirPurifierState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetAirPurifierState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentHeaterCoolerState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetHeaterCoolerState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentHumidifierDehumidifierState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 3, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetHumidifierDehumidifierState, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_WaterLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_RelativeHumidityDehumidifierThreshold, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 50.0},
    {.type = kType_RelativeHumidityHumidifierThreshold, .format = Format::Float, .permissions = kPermPRPWNT, .unit = "percentage", .has_unit = true, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 50.0},
    {.type = kType_FilterLifeLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 100.0},
    {.type = kType_FilterChangeIndication, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_ResetFilterIndication, .format = Format::UInt8, .permissions = kPermPW, .min_value = 1, .has_min = true, .max_value = 1, .has_max = true},
    {.type = kType_CurrentSlatState, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 2, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_SlatType, .format = Format::UInt8, .permissions = kPermPR, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentTiltAngle, .format = Format::Int, .permissions = kPermPRNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetTiltAngle, .format = Format::Int, .permissions = kPermPRPWNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentHorizontalTiltAngle, .format = Format::Int, .permissions = kPermPRNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetHorizontalTiltAngle, .format = Format::Int, .permissions = kPermPRPWNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CurrentVerticalTiltAngle, .format = Format::Int, .permissions = kPermPRNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_TargetVerticalTiltAngle, .format = Format::Int, .permissions = kPermPRPWNT, .unit = "arcdegrees", .has_unit = true, .min_value = -90, .has_min = true, .max_value = 90, .has_max = true, .min_step = 1, .has_step = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_OzoneDensity, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_NitrogenDioxideDensity, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_SulphurDioxideDensity, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_AirParticulateDensity, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_AirParticulateSize, .format = Format::UInt8, .permissions = kPermPR, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_CarbonMonoxidePeakLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_CarbonDioxidePeakLevel, .format = Format::Float, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 100000, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0.0},
    {.type = kType_NFCAccessControlPoint, .format = Format::TLV8, .permissions = kPermPRPWWr, .default_kind = CharacteristicDesc::DefaultKind::Tlv8, .tlv8_default_data = kNfcControlTlv8, .tlv8_default_len = 2},
    {.type = kType_NFCAccessSupportedConfiguration, .format = Format::TLV8, .permissions = kPermPR, .default_kind = CharacteristicDesc::DefaultKind::Tlv8, .tlv8_default_data = kNfcSupportedTlv8, .tlv8_default_len = 6},
    {.type = kType_ConfigurationState, .format = Format::UInt16, .permissions = kPermPRNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_LockControlPoint, .format = Format::TLV8, .permissions = kPermPW},
    {.type = kType_LockPhysicalControls, .format = Format::UInt8, .permissions = kPermPRPWNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_LockManagementAutoSecurityTimeout, .format = Format::UInt32, .permissions = kPermPRPWNT, .unit = "seconds", .has_unit = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_LockLastKnownAction, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 10, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_Logs, .format = Format::TLV8, .permissions = kPermPRNT},
    {.type = kType_Version, .format = Format::String, .permissions = kPermPR, .default_kind = CharacteristicDesc::DefaultKind::String, .default_string = "1.0.0"},
    {.type = kType_AdministratorOnlyAccess, .format = Format::Bool, .permissions = kPermPRPWNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_AudioFeedback, .format = Format::Bool, .permissions = kPermPRPWNT, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_StatusJammed, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
    {.type = kType_SecuritySystemAlarmType, .format = Format::UInt8, .permissions = kPermPRNT, .min_value = 0, .has_min = true, .max_value = 1, .has_max = true, .default_kind = CharacteristicDesc::DefaultKind::Number, .default_number = 0},
};

const CharacteristicDesc* find_characteristic_desc(uint64_t type) {
    for (const auto& desc : kCharacteristics) {
        if (desc.type == type) return &desc;
    }
    return nullptr;
}

namespace {
// Materialize the compact default into a typed Value matching the format.
core::Value default_value_for(const CharacteristicDesc& desc) {
    switch (desc.default_kind) {
        case CharacteristicDesc::DefaultKind::Number:
            switch (desc.format) {
                case Format::Bool:   return desc.default_number != 0.0;
                case Format::UInt8:  return static_cast<uint8_t>(desc.default_number);
                case Format::UInt16: return static_cast<uint16_t>(desc.default_number);
                case Format::UInt32: return static_cast<uint32_t>(desc.default_number);
                case Format::UInt64: return static_cast<uint64_t>(desc.default_number);
                case Format::Int:    return static_cast<int32_t>(desc.default_number);
                case Format::Float:  return static_cast<float>(desc.default_number);
                default:             return static_cast<float>(desc.default_number);
            }
        case CharacteristicDesc::DefaultKind::String:
            return std::string(desc.default_string);
        case CharacteristicDesc::DefaultKind::Tlv8:
            return std::vector<uint8_t>(desc.tlv8_default_data,
                                        desc.tlv8_default_data + desc.tlv8_default_len);
        case CharacteristicDesc::DefaultKind::None:
            break;
    }
    return core::Value{};
}
} // namespace

std::unique_ptr<core::Characteristic> make_characteristic(const CharacteristicDesc& desc) {
    auto c = std::make_unique<core::Characteristic>(desc.type, desc.format, desc.permissions);
    if (desc.has_unit) c->set_unit(std::string(desc.unit));
    if (desc.has_min) c->set_min_value(desc.min_value);
    if (desc.has_max) c->set_max_value(desc.max_value);
    if (desc.has_step) c->set_min_step(desc.min_step);
    if (desc.has_max_len) c->set_max_len(desc.max_len);
    if (desc.default_kind != CharacteristicDesc::DefaultKind::None) {
        c->set_value(default_value_for(desc));
    }
    return c;
}

std::unique_ptr<core::Characteristic> make_characteristic(CharId id) {
    return make_characteristic(kCharacteristics[static_cast<size_t>(id)]);
}

} // namespace hap::characteristic

#pragma GCC diagnostic pop
