#include "hap/core/TLV8.hpp"

namespace hap::core {

std::vector<TLV> TLV8::parse(std::span<const uint8_t> data) {
    std::vector<TLV> result;
    size_t offset = 0;

    while (offset < data.size()) {
        if (offset + 2 > data.size()) {
            break;
        }

        uint8_t type = data[offset++];
        uint8_t length = data[offset++];

        if (offset + length > data.size()) {
            break;
        }

        std::span<const uint8_t> value_span = data.subspan(offset, length);
        offset += length;

        if (!result.empty() && result.back().type == type && result.back().value.size() % 255 == 0 && !result.back().value.empty()) {
            result.back().value.insert(result.back().value.end(), value_span.begin(), value_span.end());
        } else {
            result.emplace_back(type, std::vector<uint8_t>(value_span.begin(), value_span.end()));
        }
    }

    return result;
}

std::vector<uint8_t> TLV8::encode(const std::vector<TLV>& tlvs) {
    std::vector<uint8_t> buffer;

    for (const auto& item : tlvs) {
        const auto& value = item.value;
        size_t value_offset = 0;
        size_t remaining = value.size();

        if (remaining == 0) {
            buffer.push_back(item.type);
            buffer.push_back(0);
        } else {
            while (remaining > 0) {
                uint8_t chunk_len = static_cast<uint8_t>(std::min<size_t>(remaining, 255));
                
                buffer.push_back(item.type);
                buffer.push_back(chunk_len);
                buffer.insert(buffer.end(), value.begin() + value_offset, value.begin() + value_offset + chunk_len);

                value_offset += chunk_len;
                remaining -= chunk_len;
            }
        }
    }

    return buffer;
}

std::optional<std::vector<uint8_t>> TLV8::find(const std::vector<TLV>& tlvs, uint8_t type) {
    for (const auto& item : tlvs) {
        if (item.type == type) {
            return item.value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> TLV8::find_string(const std::vector<TLV>& tlvs, uint8_t type) {
    auto val = find(tlvs, type);
    if (val) {
        return std::string(val->begin(), val->end());
    }
    return std::nullopt;
}

std::optional<uint8_t> TLV8::find_uint8(const std::vector<TLV>& tlvs, uint8_t type) {
    auto val = find(tlvs, type);
    if (val && val->size() == 1) {
        return (*val)[0];
    }
    return std::nullopt;
}

} // namespace hap::core
