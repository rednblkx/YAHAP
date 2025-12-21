#include "hap/transport/ble/HapPdu.hpp"

namespace hap::transport::ble {

common::Result<HapPduHeader> HapPdu::parse_header(std::span<const uint8_t> data) {
    if (data.empty()) {
        return common::Result<HapPduHeader>::err(
            common::ErrorCode::BufferTooSmall, "Empty PDU data");
    }
    
    HapPduHeader header;
    header.control_field = data[0];
    
    // Check if this is a continuation fragment
    if (header.is_continuation()) {
        return common::Result<HapPduHeader>::err(
            common::ErrorCode::InvalidParameter, 
            "Continuation fragment - use parse_continuation instead");
    }
    
    // Need at least 5 bytes for a request header
    if (data.size() < HapPduHeader::kMinRequestHeaderSize) {
        return common::Result<HapPduHeader>::err(
            common::ErrorCode::BufferTooSmall, 
            "PDU too short for header");
    }
    
    header.opcode = static_cast<PDUOpcode>(data[1]);
    header.transaction_id = data[2];
    header.instance_id = static_cast<uint16_t>(data[3]) | 
                         (static_cast<uint16_t>(data[4]) << 8);
    
    // Parse body length for write operations
    if (opcode_has_body(header.opcode)) {
        if (data.size() < HapPduHeader::kWriteHeaderSize) {
            return common::Result<HapPduHeader>::err(
                common::ErrorCode::BufferTooSmall, 
                "PDU too short for write header");
        }
        header.body_length = static_cast<uint16_t>(data[5]) | 
                             (static_cast<uint16_t>(data[6]) << 8);
    }
    
    return common::Result<HapPduHeader>::ok(header);
}

common::Result<std::pair<uint16_t, size_t>> HapPdu::parse_continuation(std::span<const uint8_t> data) {
    if (data.size() < HapPduHeader::kContinuationHeaderSize) {
        return common::Result<std::pair<uint16_t, size_t>>::err(
            common::ErrorCode::BufferTooSmall, 
            "Continuation fragment too short");
    }
    
    uint8_t control_field = data[0];
    if ((control_field & 0x80) == 0) {
        return common::Result<std::pair<uint16_t, size_t>>::err(
            common::ErrorCode::InvalidParameter, 
            "Not a continuation fragment");
    }
    
    uint16_t tid = data[1];
    return common::Result<std::pair<uint16_t, size_t>>::ok({tid, 2});
}

std::vector<uint8_t> HapPdu::build_response(uint16_t tid, uint8_t status, std::span<const uint8_t> body) {
    // Response header: CF(1) + TID(1) + Status(1) + Length(2) + Body
    std::vector<uint8_t> pdu;
    pdu.reserve(5 + body.size());
    
    // Control field: 0x02 indicates response PDU
    pdu.push_back(0x02);
    
    // Transaction ID (1 byte for BLE, low byte only)
    pdu.push_back(static_cast<uint8_t>(tid & 0xFF));
    
    // Status
    pdu.push_back(status);
    
    // Body length (little-endian)
    uint16_t len = static_cast<uint16_t>(body.size());
    pdu.push_back(len & 0xFF);
    pdu.push_back((len >> 8) & 0xFF);
    
    // Body
    pdu.insert(pdu.end(), body.begin(), body.end());
    
    return pdu;
}

bool HapPdu::opcode_has_body(PDUOpcode opcode) {
    switch (opcode) {
        case PDUOpcode::CharacteristicWrite:
        case PDUOpcode::CharacteristicTimedWrite:
        case PDUOpcode::CharacteristicConfiguration:
        case PDUOpcode::ProtocolConfiguration:
            return true;
        default:
            return false;
    }
}

size_t HapPdu::body_offset(PDUOpcode opcode) {
    return opcode_has_body(opcode) ? HapPduHeader::kWriteHeaderSize : HapPduHeader::kMinRequestHeaderSize;
}

} // namespace hap::transport::ble
