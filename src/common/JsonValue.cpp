#include "hap/common/JsonValue.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hap::common {

namespace {

constexpr int kMaxDepth = 32;

struct Parser {
    std::string_view text;
    size_t pos = 0;
    int depth = 0;
    bool failed = false;

    void skip_ws() {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos;
            } else {
                break;
            }
        }
    }

    char peek() { return pos < text.size() ? text[pos] : '\0'; }

    bool consume(char c) {
        if (peek() == c) {
            ++pos;
            return true;
        }
        return false;
    }

    void fail() { failed = true; }

    bool parse_four_hex(unsigned& out) {
        if (pos + 4 > text.size()) return false;
        auto [ptr, ec] = std::from_chars(text.data() + pos, text.data() + pos + 4, out, 16);
        if (ec != std::errc() || ptr != text.data() + pos + 4) return false;
        pos += 4;
        return true;
    }

    void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parse_string(std::string& out) {
        if (!consume('"')) return false;
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos >= text.size()) return false;
            char esc = text[pos++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = 0;
                    if (!parse_four_hex(cp)) return false;
                    // Surrogate pair
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= text.size() &&
                        text[pos] == '\\' && text[pos + 1] == 'u') {
                        pos += 2;
                        unsigned low = 0;
                        if (!parse_four_hex(low)) return false;
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            // Invalid low surrogate; encode replacement char
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
                        cp = 0xFFFD;
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false; // Unterminated
    }

    JsonValue parse_value() {
        if (failed || depth >= kMaxDepth) {
            fail();
            return {};
        }
        skip_ws();
        char c = peek();
        switch (c) {
            case '{': {
                ++depth;
                ++pos;
                JsonValue v = JsonValue::object();
                skip_ws();
                if (consume('}')) {
                    --depth;
                    return v;
                }
                while (true) {
                    skip_ws();
                    std::string key;
                    if (!parse_string(key)) {
                        fail();
                        break;
                    }
                    skip_ws();
                    if (!consume(':')) {
                        fail();
                        break;
                    }
                    JsonValue item = parse_value();
                    if (failed) break;
                    v.set(std::move(key), std::move(item));
                    skip_ws();
                    if (consume(',')) continue;
                    if (consume('}')) {
                        --depth;
                        return v;
                    }
                    fail();
                    break;
                }
                --depth;
                return {};
            }
            case '[': {
                ++depth;
                ++pos;
                JsonValue v = JsonValue::array();
                skip_ws();
                if (consume(']')) {
                    --depth;
                    return v;
                }
                while (true) {
                    JsonValue item = parse_value();
                    if (failed) break;
                    v.push_back(std::move(item));
                    skip_ws();
                    if (consume(',')) continue;
                    if (consume(']')) {
                        --depth;
                        return v;
                    }
                    fail();
                    break;
                }
                --depth;
                return {};
            }
            case '"': {
                std::string str_result;
                if (!parse_string(str_result)) {
                    fail();
                    return {};
                }
                return JsonValue(std::move(str_result));
            }
            case 't':
                if (text.substr(pos, 4) == "true") {
                    pos += 4;
                    return JsonValue(true);
                }
                fail();
                return {};
            case 'f':
                if (text.substr(pos, 5) == "false") {
                    pos += 5;
                    return JsonValue(false);
                }
                fail();
                return {};
            case 'n':
                if (text.substr(pos, 4) == "null") {
                    pos += 4;
                    return {};
                }
                fail();
                return {};
            default:
                return parse_number();
        }
    }

    JsonValue parse_number() {
        size_t start = pos;
        bool negative = consume('-');
        bool is_integer = true;
        while (pos < text.size()) {
            char c = text[pos];
            if (c >= '0' && c <= '9') {
                ++pos;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                is_integer = false;
                ++pos;
            } else {
                break;
            }
        }
        if (pos == start || (negative && pos == start + 1)) {
            fail();
            return {};
        }
        std::string_view token = text.substr(start, pos - start);
        if (is_integer) {
            unsigned long long magnitude = 0;
            auto [ptr, ec] = std::from_chars(token.data() + (negative ? 1 : 0),
                                             token.data() + token.size(), magnitude);
            if (ec == std::errc() && ptr == token.data() + token.size()) {
                return JsonValue::make_integer(magnitude, negative);
            }
        }
        // Fall back to double parsing for floats / oversized integers.
        char* end = nullptr;
        double d = std::strtod(token.data(), &end);
        if (end != token.data() + token.size()) {
            fail();
            return {};
        }
        return JsonValue(d);
    }
};

} // namespace

int JsonValue::as_int(int fallback) const {
    if (!is_number()) return fallback;
    if (neg_) {
        if (u64_ <= 2147483648ull) return static_cast<int>(-static_cast<long long>(u64_));
        return fallback;
    }
    if (u64_ <= 2147483647ull) return static_cast<int>(u64_);
    return fallback;
}

uint32_t JsonValue::as_uint32(uint32_t fallback) const {
    if (!is_number()) return fallback;
    if (neg_ || u64_ > 4294967295ull) return fallback;
    return static_cast<uint32_t>(u64_);
}

uint64_t JsonValue::as_uint64(uint64_t fallback) const {
    if (!is_number()) return fallback;
    if (neg_) return fallback;
    return u64_;
}

int64_t JsonValue::as_int64(int64_t fallback) const {
    if (!is_number()) return fallback;
    if (neg_) {
        if (u64_ <= 9223372036854775808ull) return -static_cast<int64_t>(u64_ - 1) - 1;
        return fallback;
    }
    if (u64_ <= 9223372036854775807ull) return static_cast<int64_t>(u64_);
    return fallback;
}

void JsonValue::set(std::string key, JsonValue v) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        obj_.clear();
    }
    for (auto& [k, val] : obj_) {
        if (k == key) {
            val = std::move(v);
            return;
        }
    }
    obj_.emplace_back(std::move(key), std::move(v));
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& [k, val] : obj_) {
        if (k == key) return &val;
    }
    return nullptr;
}

void JsonValue::erase(std::string_view key) {
    if (type_ != Type::Object) return;
    for (auto it = obj_.begin(); it != obj_.end(); ++it) {
        if (it->first == key) {
            obj_.erase(it);
            return;
        }
    }
}

JsonValue JsonValue::make_integer(unsigned long long magnitude, bool negative) {
    return JsonValue(NumberTag{}, static_cast<double>(negative ? -magnitude : magnitude),
                     magnitude, negative);
}

void JsonValue::escape_string_to(std::string& out, std::string_view s) {
    out.push_back('"');
    for (char raw : s) {
        unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0xF]);
                    out.push_back(kHex[c & 0xF]);
                } else {
                    out.push_back(raw);
                }
        }
    }
    out.push_back('"');
}

void JsonValue::append_number(std::string& out, double d, unsigned long long u, bool neg) {
    (void)u;
    (void)neg;
    // Integral values print as integers; everything else uses a compact
    // fixed-point form (HAP values are temperatures/percentages, so a few
    // decimals is plenty). Avoids std::to_chars(double), which pulls
    // libstdc++'s ryu tables (~100 KB of flash data on ESP32) into the link.
    // NOTE: always format from d — u64_ is only populated when the value came
    // from parsed integer text (or an integer constructor); a JsonValue built
    // as a double (e.g. maxValue = 3.0) has u64_ == 0.
    if (d == std::floor(d) && std::fabs(d) < 1e18) {
        char buf[24];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), static_cast<long long>(d));
        if (ec == std::errc()) {
            out.append(buf, ptr);
            return;
        }
    }
    // Fixed-point with up to 6 decimals, trailing zeros trimmed.
    if (!std::isfinite(d)) {
        out += "0"; // JSON has no NaN/Inf; degrade to 0
        return;
    }
    char buf[40];
    char* p = buf;
    if (std::signbit(d)) {
        *p++ = '-';
        d = -d;
    }
    double scaled = std::round(d * 1e6);
    long long scaled_ll = static_cast<long long>(scaled);
    long long whole = scaled_ll / 1000000;
    long long digits = scaled_ll % 1000000;
    if (digits < 0) digits = -digits;
    p += std::to_chars(p, buf + sizeof(buf), whole).ptr - p;
    if (digits > 0) {
        char frac_buf[8];
        std::snprintf(frac_buf, sizeof(frac_buf), "%06lld", digits);
        int len = 6;
        while (len > 0 && frac_buf[len - 1] == '0') --len; // trim zeros
        *p++ = '.';
        for (int i = 0; i < len; ++i) *p++ = frac_buf[i];
    }
    out.append(buf, p);
}

void JsonValue::dump_to(std::string& out) const {
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: append_number(out, num_, u64_, neg_); break;
        case Type::String: escape_string_to(out, str_); break;
        case Type::Array: {
            out.push_back('[');
            bool first = true;
            for (const auto& item : arr_) {
                if (!first) out.push_back(',');
                first = false;
                item.dump_to(out);
            }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            bool first = true;
            for (const auto& [key, val] : obj_) {
                if (!first) out.push_back(',');
                first = false;
                escape_string_to(out, key);
                out.push_back(':');
                val.dump_to(out);
            }
            out.push_back('}');
            break;
        }
    }
}

std::string JsonValue::dump() const {
    std::string out;
    // Objects/arrays in HAP are small; this bound avoids repeated reallocation
    // without reserving exact sizes.
    out.reserve(256);
    dump_to(out);
    return out;
}

JsonValue JsonValue::parse(std::string_view text, bool* error) {
    Parser p{text};
    JsonValue v = p.parse_value();
    if (!p.failed) {
        p.skip_ws();
        if (p.pos != text.size()) p.failed = true; // Trailing garbage
    }
    if (error) *error = p.failed;
    return v;
}

} // namespace hap::common
