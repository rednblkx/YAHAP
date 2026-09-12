#pragma once

// Internal helpers shared between PairSetup and PairVerify.

#include "hap/platform/Crypto.hpp"
#include "hap/platform/Storage.hpp"
#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace hap::pairing {

/// Storage keys for the accessory Ed25519 long-term key pair.
inline constexpr const char* kAccessoryLTSKKey = "accessory_ltsk";
inline constexpr const char* kAccessoryLTPKKey = "accessory_ltpk";

/// Load the accessory long-term key pair from storage.
/// Returns true and fills both arrays when a valid stored pair exists.
bool load_accessory_ltk(platform::Storage* storage,
                        std::array<uint8_t, 64>& ltsk,
                        std::array<uint8_t, 32>& ltpk);

} // namespace hap::pairing
