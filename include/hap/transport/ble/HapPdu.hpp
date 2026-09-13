#pragma once

#include "hap/common/Result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace hap::transport::ble {

/**
 * @brief HAP-BLE PDU Opcodes (Table 7-8)
 */
enum class PDUOpcode : uint8_t {
    CharacteristicSignatureRead = 0x01,
    CharacteristicWrite = 0x02,
    CharacteristicRead = 0x03,
    CharacteristicTimedWrite = 0x04,
    CharacteristicExecuteWrite = 0x05,
    ServiceSignatureRead = 0x06,
    CharacteristicConfiguration = 0x07,
    ProtocolConfiguration = 0x08
};

/**
 * @brief Parsed HAP-BLE PDU header.
 */
struct HapPduHeader {
    // Minimum header sizes
    static constexpr size_t kMinRequestHeaderSize = 5;  // CF + Op + TID + IID(2)
    static constexpr size_t kWriteHeaderSize = 7;       // CF + Op + TID + IID(2) + Len(2)
    static constexpr size_t kContinuationHeaderSize = 2; // CF + TID
};

/**
 * @brief HAP-BLE PDU builder and layout helpers (Spec 7.3.3).
 */
class HapPdu {
public:
    /**
     * @brief Build a response PDU.
     * @param tid Transaction ID (must match request)
     * @param status HAP status code (0 = success)
     * @param body Response body (may be empty)
     * @return Complete response PDU
     */
    static std::vector<uint8_t> build_response(uint16_t tid, uint8_t status, std::span<const uint8_t> body);

    /**
     * @brief Check if an opcode requires a body length field.
     */
    static bool opcode_has_body(PDUOpcode opcode);

    /**
     * @brief Get the body offset for a given opcode.
     */
    static size_t body_offset(PDUOpcode opcode);
};

} // namespace hap::transport::ble
