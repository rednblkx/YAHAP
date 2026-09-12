#pragma once

// Minimal JSON value type for the HAP protocol surface.
//
// This class covers exactly what HAP needs: objects, arrays,
// numbers (double + uint64 for exact large integers), strings, booleans and
// null, plus a serializer and a bounded, exception-free parser.
//
// Number policy: every number is stored as double; if the text is an integer
// that round-trips exactly through double, it is also kept in u64_/i64_ so
// that 64-bit AIDs/IIDs serialize without precision loss or ".0" suffixes.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hap::common {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<JsonValue>;
    // Object members in insertion order (tiny objects; linear lookup beats a
    // tree on cache footprint).
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue() = default;
    JsonValue(std::nullptr_t) {}
    JsonValue(bool b) : type_(Type::Bool), bool_(b) {}
    JsonValue(int v) { set_number(static_cast<double>(v), static_cast<long long>(v), v < 0); }
    JsonValue(unsigned v) { set_number(static_cast<double>(v), v, false); }
    JsonValue(long v) { set_number(static_cast<double>(v), static_cast<long long>(v), v < 0); }
    JsonValue(unsigned long v) { set_number(static_cast<double>(v), v, false); }
    JsonValue(long long v) { set_number(static_cast<double>(v), v, v < 0); }
    JsonValue(unsigned long long v) { set_number(static_cast<double>(v), v, false); }
    JsonValue(double v) : type_(Type::Number), num_(v) {}
    JsonValue(const char* s) : type_(Type::String), str_(s) {}
    JsonValue(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static JsonValue array() { JsonValue v; v.type_ = Type::Array; return v; }
    static JsonValue object() { JsonValue v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool(bool fallback = false) const { return is_bool() ? bool_ : fallback; }

    // Numeric reads: exact when the source text was an integer that fits,
    // otherwise the double value converted.
    int as_int(int fallback = 0) const;
    uint32_t as_uint32(uint32_t fallback = 0) const;
    uint64_t as_uint64(uint64_t fallback = 0) const;
    int64_t as_int64(int64_t fallback = 0) const;
    double as_double(double fallback = 0.0) const { return is_number() ? num_ : fallback; }

    const std::string& as_string() const { return str_; }

    // ---- array access ----
    Array& items() { return arr_; }
    const Array& items() const { return arr_; }
    void push_back(JsonValue v) { arr_.push_back(std::move(v)); }
    size_t size() const { return type_ == Type::Array ? arr_.size() : (type_ == Type::Object ? obj_.size() : 0); }

    // ---- object access ----
    void set(std::string key, JsonValue v);
    const JsonValue* find(std::string_view key) const;
    bool contains(std::string_view key) const { return find(key) != nullptr; }
    void erase(std::string_view key);

    // ---- serialization ----
    // Serializes to minimal JSON (no whitespace). Numbers that are integral
    // and exactly representable print without a decimal point.
    std::string dump() const;
    void dump_to(std::string& out) const;

    // ---- parsing ----
    // Parses `text`. On failure returns Null and sets *error (if given).
    // Depth is bounded (kMaxDepth) and the input must be a single JSON value
    // with only trailing whitespace.
    static JsonValue parse(std::string_view text, bool* error = nullptr);

    // Parser-internal factory for integer text: keeps the exact magnitude
    // alongside the double value.
    static JsonValue make_integer(unsigned long long magnitude, bool negative);

private:
    struct NumberTag {};
    JsonValue(NumberTag, double d, unsigned long long u, bool negative)
        : type_(Type::Number), num_(d), u64_(u), neg_(negative) {}

    void set_number(double d, unsigned long long u, bool neg) {
        type_ = Type::Number; num_ = d; u64_ = u; neg_ = neg;
    }

    static void escape_string_to(std::string& out, std::string_view s);
    static void append_number(std::string& out, double d, unsigned long long u, bool neg);

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    unsigned long long u64_ = 0;
    bool neg_ = false;
    std::string str_;
    Array arr_;
    Object obj_;
};

} // namespace hap::common
