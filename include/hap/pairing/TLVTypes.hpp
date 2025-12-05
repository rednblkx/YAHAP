#pragma once

#include <cstdint>

namespace hap::pairing {

// TLV Types from HAP Spec 5.16
enum class TLVType : uint8_t {
    Method = 0,
    Identifier = 1,
    Salt = 2,
    PublicKey = 3,
    Proof = 4,
    EncryptedData = 5,
    State = 6,
    Error = 7,
    RetryDelay = 8,
    Certificate = 9,
    Signature = 10,
    Permissions = 11,
    FragmentData = 12,
    FragmentLast = 13,
    Separator = 255
};

// Error Codes from HAP Spec 5.16
enum class TLVError : uint8_t {
    Unknown = 1,
    Authentication = 2,
    Backoff = 3,
    MaxPeers = 4,
    MaxTries = 5,
    Unavailable = 6,
    Busy = 7
};

// Pairing Methods from HAP Spec 5.16
enum class PairingMethod : uint8_t {
    PairSetup = 0,
    PairSetupWithAuth = 1,
    PairVerify = 2,
    AddPairing = 3,
    RemovePairing = 4,
    ListPairings = 5
};

// Pairing States (M1-M6 for Pair Setup, M1-M4 for Pair Verify)
enum class PairingState : uint8_t {
    M1 = 1,
    M2 = 2,
    M3 = 3,
    M4 = 4,
    M5 = 5,
    M6 = 6
};

} // namespace hap::pairing
