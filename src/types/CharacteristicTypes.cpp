#include "hap/types/CharacteristicTypes.hpp"

// The catalog table uses designated initializers with intentionally-missing
// optional fields; -Wextra flags every such row otherwise.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace hap::characteristic {

using core::Format;

const CharacteristicDesc* find_characteristic_desc(uint64_t type) {
    for (const auto& desc : kCharacteristics) {
        if (desc.type == type) return &desc;
    }
    return nullptr;
}

// Materialize the compact default into a typed Value matching the format.
core::Value default_value_for(const CharacteristicDesc& desc) {
    switch (desc.default_kind) {
        case CharacteristicDesc::DefaultKind::Number:
            switch (desc.format) {
                case Format::Bool:   return desc.default_number != 0.0;
                case Format::UInt8:  return static_cast<uint8_t>(desc.default_number);
                case Format::UInt16: return static_cast<uint16_t>(desc.default_number);
                case Format::UInt32: return static_cast<uint32_t>(desc.default_number);
                case Format::UInt64: return static_cast<uint64_t>(desc.default_number);
                case Format::Int:    return static_cast<int32_t>(desc.default_number);
                case Format::Float:  return static_cast<float>(desc.default_number);
                default:             return static_cast<float>(desc.default_number);
            }
        case CharacteristicDesc::DefaultKind::String:
            return std::string(desc.default_string);
        case CharacteristicDesc::DefaultKind::Tlv8:
            return std::vector<uint8_t>(desc.tlv8_default_data,
                                        desc.tlv8_default_data + desc.tlv8_default_len);
        case CharacteristicDesc::DefaultKind::None:
            break;
    }
    return core::Value{};
}
std::unique_ptr<core::Characteristic> make_characteristic(const CharacteristicDesc& desc) {
    auto c = std::make_unique<core::Characteristic>(desc.type, desc.format, desc.permissions);
    if (desc.has_unit) c->set_unit(std::string(desc.unit));
    if (desc.has_min) c->set_min_value(desc.min_value);
    if (desc.has_max) c->set_max_value(desc.max_value);
    if (desc.has_step) c->set_min_step(desc.min_step);
    if (desc.has_max_len) c->set_max_len(desc.max_len);
    if (desc.default_kind != CharacteristicDesc::DefaultKind::None) {
        c->set_value(default_value_for(desc));
    }
    return c;
}

std::unique_ptr<core::Characteristic> make_characteristic(CharId id) {
    return make_characteristic(kCharacteristics[static_cast<size_t>(id)]);
}

} // namespace hap::characteristic

#pragma GCC diagnostic pop
