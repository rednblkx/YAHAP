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
    uint8_t control_field = 0;
    PDUOpcode opcode = PDUOpcode::CharacteristicRead;
    uint16_t transaction_id = 0;
    uint16_t instance_id = 0;
    uint16_t body_length = 0;  // Only valid for write operations
    
    [[nodiscard]] bool is_continuation() const { return (control_field & 0x80) != 0; }
    [[nodiscard]] bool is_response() const { return (control_field & 0x02) != 0; }
    
    // Minimum header sizes
    static constexpr size_t kMinRequestHeaderSize = 5;  // CF + Op + TID + IID(2)
    static constexpr size_t kWriteHeaderSize = 7;       // CF + Op + TID + IID(2) + Len(2)
    static constexpr size_t kContinuationHeaderSize = 2; // CF + TID
};

/**
 * @brief HAP-BLE PDU parser and builder.
 * 
 * Handles parsing of incoming PDUs and building response PDUs.
 * 
 */
class HapPdu {
public:
    /**
     * @brief Parse a PDU header from raw bytes.
     * @param data Raw PDU data (at least 5 bytes for request, 2 for continuation)
     * @return Parsed header or error if malformed
     */
    static common::Result<HapPduHeader> parse_header(std::span<const uint8_t> data);
    
    /**
     * @brief Parse a continuation fragment header.
     * @param data Raw fragment data (at least 2 bytes)
     * @return Transaction ID and start offset of body data
     */
    static common::Result<std::pair<uint16_t, size_t>> parse_continuation(std::span<const uint8_t> data);
    
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
