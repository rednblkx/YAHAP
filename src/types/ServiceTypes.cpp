/**
 * @file ServiceTypes.cpp
 * @brief Out-of-line ServiceBuilder/CharacteristicRef method bodies
 *
 * The variant-extraction wrappers are kept out-of-line so they are emitted
 * once in the library instead of being instantiated in every consumer
 * translation unit (the builder is header-only for API convenience only).
 */
#include "hap/types/ServiceTypes.hpp"
#include "hap/core/HAPStatus.hpp"

#include <cassert>

namespace hap::service {

CharacteristicRef ServiceBuilder::add(characteristic::CharId id) {
    assert(service_ && "ServiceBuilder::add() after build()");
    auto c = characteristic::make_characteristic(id);
    c->set_iid(next_iid_++);
    service_->add_characteristic(std::move(c));
    return CharacteristicRef(service_->characteristics().back().get(), this);
}

CharacteristicRef& CharacteristicRef::set(core::Value value) {
    assert(characteristic_ && "CharacteristicRef used after ServiceBuilder::build()");
    if (characteristic_) {
        characteristic_->set_value(std::move(value));
    }
    return *this;
}

CharacteristicRef& CharacteristicRef::on_write(core::Characteristic::WriteCallback cb) {
    assert(characteristic_ && "CharacteristicRef used after ServiceBuilder::build()");
    if (characteristic_) {
        // Compose with any callback already installed on this characteristic
        // (e.g. state propagation + logging): run in install order, first
        // error short-circuits. set_write_callback overwrites, so chaining
        // here is what keeps multiple on_write* calls from clobbering.
        auto previous = characteristic_->write_callback();
        if (previous) {
            characteristic_->set_write_callback(
                [previous = std::move(previous),
                 cb = std::move(cb)](const core::Value& v) -> core::WriteResponse {
                    if (auto result = previous(v)) {
                        return result;
                    }
                    return cb(v);
                });
        } else {
            characteristic_->set_write_callback(std::move(cb));
        }
    }
    return *this;
}

CharacteristicRef& CharacteristicRef::on_write_bool(std::function<void(bool)> cb) {
    // Bool-format characteristics store bool, but 0/1 UInt8 characteristics
    // (e.g. LockTargetState) store uint8_t after coercion — accept both.
    assert(characteristic_ &&
           (characteristic_->format() == core::Format::Bool ||
            characteristic_->format() == core::Format::UInt8) &&
           "on_write_bool on a non-bool/0-1 characteristic");
    return on_write([cb = std::move(cb)](const core::Value& v) -> core::WriteResponse {
        bool b = false;
        if (auto* p = std::get_if<bool>(&v)) {
            b = *p;
        } else if (auto* p = std::get_if<uint8_t>(&v)) {
            b = *p != 0;
        } else {
            return std::nullopt;
        }
        if (cb) cb(b);
        return std::nullopt;
    });
}

CharacteristicRef& CharacteristicRef::on_write_bool(std::function<void()> cb) {
    return on_write_bool([cb = std::move(cb)](bool) {
        if (cb) cb();
    });
}

CharacteristicRef& CharacteristicRef::on_write_int(std::function<void(int)> cb) {
    // Values are stored coerced to the characteristic's format, so accept
    // every integer-width alternative the asserted formats can produce.
    assert(characteristic_ &&
           (characteristic_->format() == core::Format::Int ||
            characteristic_->format() == core::Format::UInt8 ||
            characteristic_->format() == core::Format::UInt16 ||
            characteristic_->format() == core::Format::UInt32) &&
           "on_write_int on a non-integer characteristic");
    return on_write([cb = std::move(cb)](const core::Value& v) -> core::WriteResponse {
        int i = 0;
        if (auto* p = std::get_if<int32_t>(&v)) {
            i = *p;
        } else if (auto* p = std::get_if<uint8_t>(&v)) {
            i = *p;
        } else if (auto* p = std::get_if<uint16_t>(&v)) {
            i = *p;
        } else if (auto* p = std::get_if<uint32_t>(&v)) {
            i = static_cast<int>(*p);
        } else {
            return std::nullopt;
        }
        if (cb) cb(i);
        return std::nullopt;
    });
}

CharacteristicRef& CharacteristicRef::on_write_tlv(
        std::function<void(const std::vector<uint8_t>&)> cb) {
    assert(characteristic_ &&
           (characteristic_->format() == core::Format::TLV8 ||
            characteristic_->format() == core::Format::Data) &&
           "on_write_tlv on a non-TLV8/Data characteristic");
    return on_write([cb = std::move(cb)](const core::Value& v) -> core::WriteResponse {
        if (auto* d = std::get_if<std::vector<uint8_t>>(&v)) {
            if (cb) cb(*d);
        }
        return std::nullopt;
    });
}

CharacteristicRef& CharacteristicRef::on_write_response_tlv(
        std::function<std::optional<core::HAPResponse<core::Value>>(const std::vector<uint8_t>&)> cb) {
    assert(characteristic_ &&
           (characteristic_->format() == core::Format::TLV8 ||
            characteristic_->format() == core::Format::Data) &&
           "on_write_response_tlv on a non-TLV8/Data characteristic");
    if (characteristic_ && cb) {
        // Must go through set_write_response_callback: the PUT /characteristics
        // handler only sends the composed value when it comes from the
        // write-response callback, not from the plain write callback.
        characteristic_->set_write_response_callback(
            [cb = std::move(cb)](const core::Value& v) -> core::HAPResponse<core::Value> {
                const auto* d = std::get_if<std::vector<uint8_t>>(&v);
                if (!d) {
                    return core::HAPStatus::InvalidValueInRequest;
                }
                if (auto response = cb(*d)) {
                    return *response;
                }
                // No response composed: echo the written value, matching the
                // no-callback fallback in the PUT /characteristics handler.
                return v;
            });
    }
    return *this;
}

} // namespace hap::service
