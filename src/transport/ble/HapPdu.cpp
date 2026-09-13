#include "hap/transport/ble/HapPdu.hpp"

namespace hap::transport::ble {

std::vector<uint8_t> HapPdu::build_response(uint16_t tid, uint8_t status, std::span<const uint8_t> body) {
    // Response header per Spec 7.3.3.3: CF(1) | TID(1) | Status(1) | Length(2) | Body
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
