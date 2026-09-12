// Single definition of the variant-heavy Characteristic operations.
//
// set_value/coerce_value use std::visit over the 9-alternative Value variant;
// keeping them out-of-line here instead of inline in the header avoids every
// translation unit that touches a characteristic emitting its own copy of the
// dispatch code (measured at ~1.4 KB per copy on x86-64 MinSizeRel).

#include "hap/core/Characteristic.hpp"

namespace hap::core {

Value Characteristic::coerce_value(Value input) const {
    return std::visit([this, &input](auto&& arg) -> Value {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_arithmetic_v<T>) {
            switch (format_) {
                case Format::Bool:
                    return static_cast<bool>(arg);
                case Format::UInt8:
                    return static_cast<uint8_t>(arg);
                case Format::UInt16:
                    return static_cast<uint16_t>(arg);
                case Format::UInt32:
                    return static_cast<uint32_t>(arg);
                case Format::UInt64:
                    return static_cast<uint64_t>(arg);
                case Format::Int:
                    return static_cast<int32_t>(arg);
                case Format::Float:
                    return static_cast<float>(arg);
                default:
                    return input;
            }
        } else {
            return input;
        }
    }, input);
}

WriteResponse Characteristic::set_value(Value value, EventSource source) {
    value_ = coerce_value(std::move(value));

    WriteResponse result = std::nullopt; // Success by default

    if (write_callback_ && source.type == EventSource::Type::Connection) {
        // Synchronous callback - captures result
        result = write_callback_(value_);
        if (result.has_value()) {
            return result; // Return error immediately
        }
    }

    if (event_callback_ && source.type == EventSource::Type::NotifyChange) {
        if (dispatcher_) {
            Value captured_value = value_;
            auto cb = event_callback_;
            dispatcher_([cb, captured_value, source]() { cb(captured_value, source); });
        } else {
            event_callback_(value_, source);
        }
    }

    return result;
}

ReadResponse Characteristic::get_value() const {
    if (read_cb_) {
        auto result = read_cb_();
        // If callback returned a Value, coerce it
        if (std::holds_alternative<Value>(result)) {
            return coerce_value(std::get<Value>(result));
        }
        // Otherwise return the error status as-is
        return result;
    }
    return value_;
}

} // namespace hap::core
