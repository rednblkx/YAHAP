#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <optional>
#include <cstdint>
#include <type_traits>

namespace hap::core {

/**
 * @brief Source of a characteristic event/write
 */
struct EventSource {
    enum class Type {
        None,
        Connection      // Triggered by a HAP connection
    };

    Type type = Type::None;
    uint32_t id = 0;    // Connection ID if type == Connection

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
    WriteResponse
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

/**
 * @brief HAP Characteristic
 */
class Characteristic {
public:
    using ReadCallback = std::function<Value()>;
    using WriteCallback = std::function<void(const Value&)>;
    using EventCallback = std::function<void(const Value&, const EventSource&)>;

    Characteristic(uint64_t type, Format format, std::vector<Permission> permissions)
        : type_(type), format_(format), permissions_(std::move(permissions)) {}

    virtual ~Characteristic() = default;

    // Getters
    uint64_t type() const { return type_; }
    Format format() const { return format_; }
    const std::vector<Permission>& permissions() const { return permissions_; }
    uint64_t iid() const { return iid_; }
    void set_iid(uint64_t iid) { iid_ = iid; }

    void set_value(Value value, EventSource source = {}) {
        value_ = coerce_value(std::move(value));
        if (write_callback_) write_callback_(value_);
        if (event_callback_) event_callback_(value_, source);
    }

    Value get_value() const {
        if (read_cb_) return coerce_value(read_cb_());
        return value_;
    }

    // Callbacks
    void on_read(ReadCallback cb) { read_cb_ = std::move(cb); }
    void set_write_callback(WriteCallback callback) { write_callback_ = std::move(callback); }
    void set_event_callback(EventCallback callback) { event_callback_ = std::move(callback); }
    
    // Metadata setters (optional fields per HAP spec 6.3.3)
    void set_unit(std::string unit) { unit_ = std::move(unit); }
    void set_min_value(double min) { min_value_ = min; }
    void set_max_value(double max) { max_value_ = max; }
    void set_min_step(double step) { min_step_ = step; }
    void set_max_len(uint32_t len) { max_len_ = len; }
    void set_max_data_len(uint32_t len) { max_data_len_ = len; }
    void set_description(std::string desc) { description_ = std::move(desc); }
    void set_valid_values(std::vector<double> values) { valid_values_ = std::move(values); }
    void set_valid_values_range(double start, double end) { valid_values_range_ = std::make_pair(start, end); }
    
    // Metadata getters
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
    
    // Optional metadata (HAP spec 6.3.3)
    std::optional<std::string> unit_;              // e.g., "celsius", "percentage"
    std::optional<double> min_value_;              // Minimum value
    std::optional<double> max_value_;              // Maximum value
    std::optional<double> min_step_;               // Minimum step value
    std::optional<uint32_t> max_len_;              // Max length for strings
    std::optional<uint32_t> max_data_len_;         // Max length for TLV8/data
    std::optional<std::string> description_;       // Human-readable description
    std::optional<std::vector<double>> valid_values_;  // Valid values (enum)
    std::optional<std::pair<double, double>> valid_values_range_; // Valid values range
    
    /**
     * @brief Coerces a value to the correct variant type based on format_
     * 
     * This allows application code to use natural literals (e.g., set_value(0))
     * without needing explicit casts like static_cast<uint8_t>(0).
     */
    Value coerce_value(Value input) const {
        return std::visit([this, &input](auto&& arg) -> Value {
            using T = std::decay_t<decltype(arg)>;
            
            // Only coerce arithmetic types - strings and byte vectors pass through
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
                        return input; // String, TLV8, Data - pass through
                }
            } else {
                return input; // Non-arithmetic types pass through unchanged
            }
        }, input);
    }
};

} // namespace hap::core

