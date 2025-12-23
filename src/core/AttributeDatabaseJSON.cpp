#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/HAPStatus.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace hap::core {

using json = nlohmann::json;

// Forward declarations
json to_json(const Characteristic& c);
json to_json(const Service& s);
json to_json(const Accessory& a);

json to_json(const Characteristic& c) {
    json j;
    
    // Type (hex string)
    // Format type as uppercase hex string (e.g., "25" not "0x25")
    std::ostringstream oss;
    oss << std::uppercase << std::hex << c.type();
    j["type"] = oss.str();
    
    // IID  
    j["iid"] = c.iid();
    
    // Permissions
    json perms = json::array();
    for (const auto& perm : c.permissions()) {
        switch (perm) {
            case Permission::PairedRead: perms.push_back("pr"); break;
            case Permission::PairedWrite: perms.push_back("pw"); break;
            case Permission::Notify: perms.push_back("ev"); break;
            case Permission::AdditionalAuthorization: perms.push_back("aa"); break;
            case Permission::TimedWrite: perms.push_back("tw"); break;
            case Permission::Hidden: perms.push_back("hd"); break;
            case Permission::WriteResponse: perms.push_back("wr"); break;
        }
    }
    j["perms"] = perms;
    
    // Format
    switch (c.format()) {
        case Format::Bool: j["format"] = "bool"; break;
        case Format::UInt8: j["format"] = "uint8"; break;
        case Format::UInt16: j["format"] = "uint16"; break;
        case Format::UInt32: j["format"] = "uint32"; break;
        case Format::UInt64: j["format"] = "uint64"; break;
        case Format::Int: j["format"] = "int"; break;
        case Format::Float: j["format"] = "float"; break;
        case Format::String: j["format"] = "string"; break;
        case Format::TLV8: j["format"] = "tlv8"; break;
        case Format::Data: j["format"] = "data"; break;
    }

    // Value
    if (has_permission(c.permissions(), Permission::PairedRead)) {
        auto value = c.get_value();
        std::visit([&j](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                j["value"] = arg;
            } else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || 
                                 std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || 
                                 std::is_same_v<T, int32_t> || std::is_same_v<T, float>) {
                j["value"] = arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                j["value"] = arg;
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                j["value"] = base64_encode(arg);
            }
        }, value);
    }
    
    // Optional metadata (only include if set)
    if (c.unit()) {
        j["unit"] = *c.unit();
    }
    if (c.min_value()) {
        j["minValue"] = *c.min_value();
    }
    if (c.max_value()) {
        j["maxValue"] = *c.max_value();
    }
    if (c.min_step()) {
        j["minStep"] = *c.min_step();
    }
    if (c.max_len()) {
        j["maxLen"] = *c.max_len();
    }
    if (c.max_data_len()) {
        j["maxDataLen"] = *c.max_data_len();
    }
    if (c.description()) {
        j["description"] = *c.description();
    }
    if (c.valid_values() && !c.valid_values()->empty()) {
        j["valid-values"] = *c.valid_values();
    }
    if (c.valid_values_range()) {
        j["valid-values-range"] = json::array({c.valid_values_range()->first, c.valid_values_range()->second});
    }
    
    return j;
}

json to_json(const Service& s) {
    json j;
    
    // Type (hex string)
    // Format type as uppercase hex string (e.g., "3E" not "0x3E")
    std::ostringstream oss;
    oss << std::uppercase << std::hex << s.type();
    j["type"] = oss.str();
    
    // IID
    j["iid"] = s.iid();
    
    // Characteristics
    json chars = json::array();
    for (const auto& c : s.characteristics()) {
        chars.push_back(to_json(*c));
    }
    j["characteristics"] = chars;
    
    // Primary (optional, only if true)
    if (s.is_primary()) {
        j["primary"] = true;
    }
    
    if (s.is_hidden()) {
        j["hidden"] = true;
    }
    
    if (!s.linked_services().empty()) {
        j["linked"] = s.linked_services();
    }
    
    return j;
}

json to_json(const Accessory& a) {
    json j;
    
    // AID
    j["aid"] = a.aid();
    
    // Services
    json services = json::array();
    for (const auto& s : a.services()) {
        services.push_back(to_json(*s));
    }
    j["services"] = services;
    
    return j;
}

std::string AttributeDatabase::to_json_string() const {
    json j;
    json accessories = json::array();
    
    for (const auto& acc : accessories_) {
        accessories.push_back(to_json(*acc));
    }
    
    j["accessories"] = accessories;
    return j.dump();
}

} // namespace hap::core
