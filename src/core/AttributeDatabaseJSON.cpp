#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/HAPStatus.hpp"

namespace hap::core {

namespace {

using hap::common::JsonValue;

void append_hex_u64(std::string& out, uint64_t v) {
    static const char* kHex = "0123456789ABCDEF";
    char buf[16];
    int n = 0;
    do {
        buf[n++] = kHex[v & 0xF];
        v >>= 4;
    } while (v > 0);
    while (n > 0) out.push_back(buf[--n]);
}

// Forward declarations
JsonValue to_json(const Characteristic& c);
JsonValue to_json(const Service& s);
JsonValue to_json(const Accessory& a);

JsonValue to_json(const Characteristic& c) {
    JsonValue j = JsonValue::object();

    // Type as uppercase hex string (e.g., "25"), per the HAP JSON format.
    std::string type_hex;
    append_hex_u64(type_hex, c.type());
    j.set("type", type_hex);

    j.set("iid", c.iid());

    // Permissions
    JsonValue perms = JsonValue::array();
    for (const auto& perm : c.permissions()) {
        switch (perm) {
            case Permission::PairedRead: perms.push_back("pr"); break;
            case Permission::PairedWrite: perms.push_back("pw"); break;
            case Permission::Notify: perms.push_back("ev"); break;
            case Permission::AdditionalAuthorization: perms.push_back("aa"); break;
            case Permission::TimedWrite: perms.push_back("tw"); break;
            case Permission::Hidden: perms.push_back("hd"); break;
            case Permission::WriteResponse: perms.push_back("wr"); break;
            case Permission::Broadcast: break; // BLE-only property, not in HAP JSON spec
        }
    }
    j.set("perms", std::move(perms));

    // Format
    switch (c.format()) {
        case Format::Bool: j.set("format", "bool"); break;
        case Format::UInt8: j.set("format", "uint8"); break;
        case Format::UInt16: j.set("format", "uint16"); break;
        case Format::UInt32: j.set("format", "uint32"); break;
        case Format::UInt64: j.set("format", "uint64"); break;
        case Format::Int: j.set("format", "int"); break;
        case Format::Float: j.set("format", "float"); break;
        case Format::String: j.set("format", "string"); break;
        case Format::TLV8: j.set("format", "tlv8"); break;
        case Format::Data: j.set("format", "data"); break;
    }

    // Value
    if (has_permission(c.permissions(), Permission::PairedRead)) {
        auto read_result = c.get_value();
        // Only include value if read succeeded
        if (std::holds_alternative<Value>(read_result)) {
            j.set("value", value_to_json(std::get<Value>(read_result)));
        }
    }

    // Optional metadata (only include if set)
    if (c.unit()) {
        j.set("unit", *c.unit());
    }
    if (c.min_value()) {
        j.set("minValue", *c.min_value());
    }
    if (c.max_value()) {
        j.set("maxValue", *c.max_value());
    }
    if (c.min_step()) {
        j.set("minStep", *c.min_step());
    }
    if (c.max_len()) {
        j.set("maxLen", static_cast<uint64_t>(*c.max_len()));
    }
    if (c.max_data_len()) {
        j.set("maxDataLen", static_cast<uint64_t>(*c.max_data_len()));
    }
    if (c.description()) {
        j.set("description", *c.description());
    }
    if (c.valid_values() && !c.valid_values()->empty()) {
        JsonValue values = JsonValue::array();
        for (double v : *c.valid_values()) {
            values.push_back(v);
        }
        j.set("valid-values", std::move(values));
    }
    if (c.valid_values_range()) {
        JsonValue range = JsonValue::array();
        range.push_back(c.valid_values_range()->first);
        range.push_back(c.valid_values_range()->second);
        j.set("valid-values-range", std::move(range));
    }

    return j;
}

JsonValue to_json(const Service& s) {
    JsonValue j = JsonValue::object();

    std::string type_hex;
    append_hex_u64(type_hex, s.type());
    j.set("type", type_hex);

    j.set("iid", s.iid());

    JsonValue chars = JsonValue::array();
    for (const auto& c : s.characteristics()) {
        chars.push_back(to_json(*c));
    }
    j.set("characteristics", std::move(chars));

    if (s.is_primary()) {
        j.set("primary", true);
    }

    if (s.is_hidden()) {
        j.set("hidden", true);
    }

    if (!s.linked_services().empty()) {
        JsonValue linked = JsonValue::array();
        for (uint64_t iid : s.linked_services()) {
            linked.push_back(iid);
        }
        j.set("linked", std::move(linked));
    }

    return j;
}

JsonValue to_json(const Accessory& a) {
    JsonValue j = JsonValue::object();

    j.set("aid", a.aid());

    JsonValue services = JsonValue::array();
    for (const auto& s : a.services()) {
        services.push_back(to_json(*s));
    }
    j.set("services", std::move(services));

    return j;
}

} // namespace

std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char* encoding_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t val = (data[i] << 16);
        if (i + 1 < data.size()) val |= (data[i + 1] << 8);
        if (i + 2 < data.size()) val |= data[i + 2];

        encoded.push_back(encoding_table[(val >> 18) & 0x3F]);
        encoded.push_back(encoding_table[(val >> 12) & 0x3F]);
        encoded.push_back(i + 1 < data.size() ? encoding_table[(val >> 6) & 0x3F] : '=');
        encoded.push_back(i + 2 < data.size() ? encoding_table[val & 0x3F] : '=');
    }
    return encoded;
}

JsonValue value_to_json(const Value& value) {
    return std::visit([](auto&& arg) -> JsonValue {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return base64_encode(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return JsonValue(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return JsonValue(arg);
        } else if constexpr (std::is_same_v<T, float>) {
            return JsonValue(static_cast<double>(arg));
        } else {
            return JsonValue(static_cast<uint64_t>(arg));
        }
    }, value);
}

std::string AttributeDatabase::to_json_string() const {
    JsonValue j = JsonValue::object();
    JsonValue accessories = JsonValue::array();

    for (const auto& acc : accessories_) {
        accessories.push_back(to_json(*acc));
    }

    j.set("accessories", std::move(accessories));
    return j.dump();
}

} // namespace hap::core
