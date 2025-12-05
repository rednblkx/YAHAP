#pragma once

#include "hap/core/Characteristic.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

namespace hap::core {

/**
 * @brief HAP Status Codes (Section 6.7.1.4)
 * These codes are returned in JSON responses to indicate operation results.
 */
enum class HAPStatus : int32_t {
    // Success
    Success = 0,
    
    // Error codes
    InsufficientPrivileges = -70401,        // Request requires admin privileges
    ServiceCommunicationFailure = -70402,   // Unable to communicate with requested service
    ResourceBusy = -70403,                  // Resource is busy, try again
    ReadOnlyCharacteristic = -70404,        // Cannot write to read-only characteristic
    WriteOnlyCharacteristic = -70405,       // Cannot read from write-only characteristic
    NotificationNotSupported = -70406,      // Notifications are not supported for characteristic
    OutOfResources = -70407,                // Out of resources to process request
    OperationTimedOut = -70408,             // Operation timed out
    ResourceDoesNotExist = -70409,          // Resource does not exist
    InvalidValueInRequest = -70410,         // Invalid value in request
    InsufficientAuthorization = -70411      // Insufficient authorization
};

/**
 * @brief Convert HAP status to integer for JSON serialization
 */
inline int32_t to_int(HAPStatus status) {
    return static_cast<int32_t>(status);
}

/**
 * @brief Check if a characteristic has a specific permission
 */
inline bool has_permission(const std::vector<Permission>& perms, Permission perm) {
    return std::find(perms.begin(), perms.end(), perm) != perms.end();
}

} // namespace hap::core
