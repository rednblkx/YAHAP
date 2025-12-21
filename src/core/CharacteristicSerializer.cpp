#include "hap/core/CharacteristicSerializer.hpp"
#include <cstring>

namespace hap::core {

std::vector<uint8_t> CharacteristicSerializer::to_bytes(const Value& value) {
    std::vector<uint8_t> result;
    
    std::visit([&result](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, bool>) {
            result.push_back(arg ? 1 : 0);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            result.push_back(arg);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            result.push_back(arg & 0xFF);
            result.push_back((arg >> 8) & 0xFF);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            result.push_back(arg & 0xFF);
            result.push_back((arg >> 8) & 0xFF);
            result.push_back((arg >> 16) & 0xFF);
            result.push_back((arg >> 24) & 0xFF);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            for (int i = 0; i < 8; i++) {
                result.push_back((arg >> (i * 8)) & 0xFF);
            }
        } else if constexpr (std::is_same_v<T, int32_t>) {
            uint32_t u = static_cast<uint32_t>(arg);
            result.push_back(u & 0xFF);
            result.push_back((u >> 8) & 0xFF);
            result.push_back((u >> 16) & 0xFF);
            result.push_back((u >> 24) & 0xFF);
        } else if constexpr (std::is_same_v<T, float>) {
            uint32_t bits;
            std::memcpy(&bits, &arg, sizeof(float));
            result.push_back(bits & 0xFF);
            result.push_back((bits >> 8) & 0xFF);
            result.push_back((bits >> 16) & 0xFF);
            result.push_back((bits >> 24) & 0xFF);
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.insert(result.end(), arg.begin(), arg.end());
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            result = arg;
        }
    }, value);
    
    return result;
}

common::Result<Value> CharacteristicSerializer::from_bytes(std::span<const uint8_t> data, Format format) {
    switch (format) {
        case Format::Bool:
            if (data.empty()) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            return common::Result<Value>::ok(static_cast<bool>(data[0] != 0));
            
        case Format::UInt8:
            if (data.empty()) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            return common::Result<Value>::ok(data[0]);
            
        case Format::UInt16:
            if (data.size() < 2) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            return common::Result<Value>::ok(
                static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8));
            
        case Format::UInt32:
            if (data.size() < 4) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            return common::Result<Value>::ok(
                static_cast<uint32_t>(data[0]) |
                (static_cast<uint32_t>(data[1]) << 8) |
                (static_cast<uint32_t>(data[2]) << 16) |
                (static_cast<uint32_t>(data[3]) << 24));
            
        case Format::UInt64:
            if (data.size() < 8) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            {
                uint64_t val = 0;
                for (int i = 0; i < 8; i++) {
                    val |= static_cast<uint64_t>(data[i]) << (i * 8);
                }
                return common::Result<Value>::ok(val);
            }
            
        case Format::Int:
            if (data.size() < 4) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            {
                uint32_t u = static_cast<uint32_t>(data[0]) |
                             (static_cast<uint32_t>(data[1]) << 8) |
                             (static_cast<uint32_t>(data[2]) << 16) |
                             (static_cast<uint32_t>(data[3]) << 24);
                return common::Result<Value>::ok(static_cast<int32_t>(u));
            }
            
        case Format::Float:
            if (data.size() < 4) {
                return common::Result<Value>::err(common::ErrorCode::BufferTooSmall);
            }
            {
                uint32_t bits = static_cast<uint32_t>(data[0]) |
                                (static_cast<uint32_t>(data[1]) << 8) |
                                (static_cast<uint32_t>(data[2]) << 16) |
                                (static_cast<uint32_t>(data[3]) << 24);
                float f;
                std::memcpy(&f, &bits, sizeof(float));
                return common::Result<Value>::ok(f);
            }
            
        case Format::String:
            return common::Result<Value>::ok(std::string(data.begin(), data.end()));
            
        case Format::TLV8:
        case Format::Data:
            return common::Result<Value>::ok(std::vector<uint8_t>(data.begin(), data.end()));
    }
    
    return common::Result<Value>::err(common::ErrorCode::InvalidParameter, "Unknown format");
}

uint8_t CharacteristicSerializer::gatt_format_byte(Format format) {
    // Bluetooth SIG Assigned Numbers for GATT Presentation Format
    switch (format) {
        case Format::Bool:   return 0x01;  // unsigned 1-bit; 0 = false, else true
        case Format::UInt8:  return 0x04;  // unsigned 8-bit integer
        case Format::UInt16: return 0x06;  // unsigned 16-bit integer
        case Format::UInt32: return 0x08;  // unsigned 32-bit integer
        case Format::UInt64: return 0x0A;  // unsigned 64-bit integer
        case Format::Int:    return 0x10;  // signed 32-bit integer
        case Format::Float:  return 0x14;  // IEEE-754 32-bit floating point
        case Format::String: return 0x19;  // UTF-8 string
        case Format::TLV8:   return 0x1B;  // Opaque structure
        case Format::Data:   return 0x1B;  // Opaque structure
    }
    return 0x1B;  // Default to opaque structure
}

bool CharacteristicSerializer::is_binary_format(Format format) {
    return format == Format::TLV8 || format == Format::Data;
}

} // namespace hap::core
