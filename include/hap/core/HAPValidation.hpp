#pragma once

#include <cstddef>
#include <cstdint>

namespace hap::core {

/**
 * @brief HAP validation constants.
 * 
 * Universal limits (Section 6.11):
 * - Max 100 services per accessory
 * - Max 100 characteristics per service
 * 
 * Bridge limits (Section 2.5.3.2):
 * - Max 150 accessory objects per bridge
 */
struct HAPValidation {
    /// Maximum services per accessory - UNIVERSAL (HAP Spec 6.11 test 16)
    static constexpr size_t kMaxServicesPerAccessory = 100;
    
    /// Maximum characteristics per service - UNIVERSAL (HAP Spec 6.11 test 15)
    static constexpr size_t kMaxCharacteristicsPerService = 100;
    
    /// Maximum accessories per bridge - BRIDGE (HAP Spec 2.5.3.2)
    static constexpr size_t kMaxAccessoriesPerBridge = 150;
    
    /// Primary accessory must have AID=1 (HAP Spec 2.5.3.3)
    static constexpr uint64_t kPrimaryAccessoryId = 1;
    
    /// Accessory Information service type (HAP Spec 9.1)
    static constexpr uint16_t kAccessoryInfoServiceType = 0x3E;
};

/**
 * @brief Result of accessory validation.
 */
enum class ValidationResult {
    /// Accessory validated successfully
    Success,
    
    /// Bridge would exceed 150 accessory limit
    TooManyAccessories,
    
    /// Another accessory already has this AID
    DuplicateAccessoryId,
    
    /// Accessory has more than 100 services
    TooManyServices,
    
    /// A service has more than 100 characteristics
    TooManyCharacteristics,
};

/**
 * @brief Get human-readable description of validation result.
 */
inline const char* validation_result_str(ValidationResult result) {
    switch (result) {
        case ValidationResult::Success: return "Success";
        case ValidationResult::TooManyAccessories: return "Bridge exceeds 150 accessory limit";
        case ValidationResult::DuplicateAccessoryId: return "Duplicate accessory ID";
        case ValidationResult::TooManyServices: return "Accessory exceeds 100 service limit";
        case ValidationResult::TooManyCharacteristics: return "Service exceeds 100 characteristic limit";
        default: return "Unknown validation error";
    }
}

} // namespace hap::core
