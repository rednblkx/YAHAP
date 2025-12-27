#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <optional>
#include <cstdint>
#include <type_traits>

namespace hap::core {

// Forward declaration - full definition in HAPStatus.hpp
enum class HAPStatus : int32_t;

/**
 * @brief Response type for characteristic read/write operations.
 * 
 * Can hold either a successful value or an HAP status code indicating failure.
 * Per HAP Spec 6.7.2.3 and 6.7.4.2, characteristic operations can fail with
 * status codes like -70402 (ServiceCommunicationFailure) when physical devices
 * are unreachable.
 */
template<typename T>
using HAPResponse = std::variant<T, HAPStatus>;

/**
 * @brief Source of a characteristic event/write
 */
struct EventSource {
    enum class Type {
        NotifyChange,
        Internal,
        Connection
    };

    Type type = Type::NotifyChange;
    uint32_t id = 0;

    static EventSource from_connection(uint32_t conn_id) {
        EventSource source;
        source.type = Type::Connection;
        source.id = conn_id;
        return source;
    }
};

/**
 * @brief HAP Characteristic Formats
 */
enum class Format {
    Bool,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Int,
    Float,
    String,
    TLV8,
    Data
};

/**
 * @brief HAP Characteristic Permissions
 */
enum class Permission {
    PairedRead,
    PairedWrite,
    Notify,
    AdditionalAuthorization,
    TimedWrite,
    Hidden,
    WriteResponse,
    Broadcast
};

/**
 * @brief HAP Characteristic Value
 */
using Value = std::variant<
    bool,
    uint8_t,
    uint16_t,
    uint32_t,
    uint64_t,
    int32_t,
    float,
    std::string,
    std::vector<uint8_t>
>;

/// Result of reading a characteristic - either a Value or an error status
using ReadResponse = HAPResponse<Value>;

/// Result of writing a characteristic - nullopt means success, HAPStatus means failure
using WriteResponse = std::optional<HAPStatus>;

/**
 * @brief HAP Characteristic
 */
class Characteristic {
public:
    /**
     * @brief Callback for reading characteristic values.
     * 
     * Can return either a Value (success) or an HAPStatus error code.
     * Example: return HAPStatus::ServiceCommunicationFailure when device is unreachable.
     */
    using ReadCallback = std::function<ReadResponse()>;
    
    /**
     * @brief Callback for writing characteristic values.
     * 
     * Returns nullopt for success, or an HAPStatus error code for failure.
     */
    using WriteCallback = std::function<WriteResponse(const Value&)>;
    
    using EventCallback = std::function<void(const Value&, const EventSource&)>;
    
    /**
     * @brief Callback for write-with-response operations.
     * 
     * Can return either a Value (success) or an HAPStatus error code.
     * Per HAP Spec 6.7.3, used for control point characteristics.
     */
    using WriteResponseCallback = std::function<HAPResponse<Value>(const Value&)>;

    Characteristic(uint64_t type, Format format, std::vector<Permission> permissions)
        : type_(type), format_(format), permissions_(std::move(permissions)) {}

    virtual ~Characteristic() = default;

    uint64_t type() const { return type_; }
    Format format() const { return format_; }
    const std::vector<Permission>& permissions() const { return permissions_; }
    uint64_t iid() const { return iid_; }
    void set_iid(uint64_t iid) { iid_ = iid; }

    /**
     * @brief Dispatcher function type for deferred callback execution.
     * 
     * When set, callbacks will be dispatched through this function rather
     * than executed immediately. This solves stack overflow issues on
     * constrained platforms like ESP32 where GATT callbacks have limited stack.
     */
    using DispatcherFunc = std::function<void(std::function<void()>)>;
    
    /**
     * @brief Set the global dispatcher for all characteristic callbacks.
     * 
     * Call this once during initialization to enable deferred execution.
     * @param dispatcher Function that queues work for later execution
     */
    static void set_dispatcher(DispatcherFunc dispatcher) {
        dispatcher_ = std::move(dispatcher);
    }
    
    /**
     * @brief Set the characteristic value.
     * @param value The new value to set.
     * @param source The source of the write (e.g., connection ID).
     * @return nullopt on success, or HAPStatus error code on failure.
     */
    WriteResponse set_value(Value value, EventSource source = {}) {
        value_ = coerce_value(std::move(value));
        
        WriteResponse result = std::nullopt; // Success by default
        
        if (write_callback_ && source.type == EventSource::Type::Connection) {
            // Synchronous callback - captures result
            result = write_callback_(value_);
            if (result.has_value()) {
                return result; // Return error immediately
            }
        }
        
        if (dispatcher_) {
            Value captured_value = value_;
            if (event_callback_ && source.type == EventSource::Type::NotifyChange) {
                auto cb = event_callback_;
                dispatcher_([cb, captured_value, source]() { cb(captured_value, source); });
            }
        } else {
            if (event_callback_ && source.type == EventSource::Type::NotifyChange) event_callback_(value_, source);
        }
        
        return result;
    }

    /**
     * @brief Get the characteristic value.
     * @return Value on success, or HAPStatus error code if read callback fails.
     */
    ReadResponse get_value() const {
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

    void on_read(ReadCallback cb) { read_cb_ = std::move(cb); }
    void set_write_callback(WriteCallback callback) { write_callback_ = std::move(callback); }
    void set_event_callback(EventCallback callback) { event_callback_ = std::move(callback); }
    void set_write_response_callback(WriteResponseCallback callback) { write_response_callback_ = std::move(callback); }
    
    /**
     * @brief Handle write-with-response by invoking callback.
     * @return Response from callback (Value or HAPStatus), or nullopt if no callback set.
     */
    std::optional<HAPResponse<Value>> handle_write_response(const Value& input) {
        if (write_response_callback_) {
            return write_response_callback_(input);
        }
        return std::nullopt;
    }
    
    void set_unit(std::string unit) { unit_ = std::move(unit); }
    void set_min_value(double min) { min_value_ = min; }
    void set_max_value(double max) { max_value_ = max; }
    void set_min_step(double step) { min_step_ = step; }
    void set_max_len(uint32_t len) { max_len_ = len; }
    void set_max_data_len(uint32_t len) { max_data_len_ = len; }
    void set_description(std::string desc) { description_ = std::move(desc); }
    void set_valid_values(std::vector<double> values) { valid_values_ = std::move(values); }
    void set_valid_values_range(double start, double end) { valid_values_range_ = std::make_pair(start, end); }
    
    const std::optional<std::string>& unit() const { return unit_; }
    const std::optional<double>& min_value() const { return min_value_; }
    const std::optional<double>& max_value() const { return max_value_; }
    const std::optional<double>& min_step() const { return min_step_; }
    const std::optional<uint32_t>& max_len() const { return max_len_; }
    const std::optional<uint32_t>& max_data_len() const { return max_data_len_; }
    const std::optional<std::string>& description() const { return description_; }
    const std::optional<std::vector<double>>& valid_values() const { return valid_values_; }
    const std::optional<std::pair<double, double>>& valid_values_range() const { return valid_values_range_; }

private:
    uint64_t type_;
    Format format_;
    std::vector<Permission> permissions_;
    uint64_t iid_ = 0;
    
    Value value_;
    
    ReadCallback read_cb_;
    WriteCallback write_callback_;
    EventCallback event_callback_;
    WriteResponseCallback write_response_callback_;
    
    std::optional<std::string> unit_;              // e.g., "celsius", "percentage"
    std::optional<double> min_value_;              // Minimum value
    std::optional<double> max_value_;              // Maximum value
    std::optional<double> min_step_;               // Minimum step value
    std::optional<uint32_t> max_len_;              // Max length for strings
    std::optional<uint32_t> max_data_len_;         // Max length for TLV8/data
    std::optional<std::string> description_;       // Human-readable description
    std::optional<std::vector<double>> valid_values_;  // Valid values (enum)
    std::optional<std::pair<double, double>> valid_values_range_; // Valid values range
    
    static inline DispatcherFunc dispatcher_;
    
    /**
     * @brief Coerces a value to the correct variant type based on format_
     * 
     * This allows application code to use natural literals (e.g., set_value(0))
     * without needing explicit casts like static_cast<uint8_t>(0).
     */
    Value coerce_value(Value input) const {
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
};

} // namespace hap::core

