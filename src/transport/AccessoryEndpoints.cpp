#include <charconv>
#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/HAPStatus.hpp"

using JsonValue = hap::common::JsonValue;

namespace hap::transport {

namespace {

std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static constexpr unsigned char decode_table[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 0-15
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 16-31
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, // 32-47  '+'=62 '/'=63
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, // 48-63  '0'-'9'
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 64-79  '@' + 'A'-'O'
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, // 80-95  'P'-'Z'
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // 96-111 '`' + 'a'-'o'
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64  // 112-127 'p'-'z'
    };

    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);

    uint32_t buffer = 0;
    int bits_collected = 0;

    for (char c : encoded) {
        if (c == '=' || c == '\0') break;
        if (static_cast<unsigned char>(c) >= 128) continue;
        unsigned char d = decode_table[static_cast<unsigned char>(c)];
        if (d == 64) continue;

        buffer = (buffer << 6) | d;
        bits_collected += 6;

        if (bits_collected >= 8) {
            bits_collected -= 8;
            result.push_back(static_cast<uint8_t>((buffer >> bits_collected) & 0xFF));
        }
    }

    return result;
}

Response json_response(hap::common::JsonValue body, Status status) {
    Response resp{status};
    resp.set_header("Content-Type", "application/hap+json");
    resp.set_body(body.dump());
    return resp;
}

// Writes `value` to the characteristic if it is type-compatible with the
// characteristic's format. Returns false (without writing) otherwise.
bool json_value_to_characteristic(core::Characteristic& characteristic,
                                  const JsonValue& value,
                                  core::EventSource source) {
    using core::Format;
    switch (characteristic.format()) {
        case Format::Bool:
            if (value.is_bool()) {
                characteristic.set_value(value.as_bool(), source);
                return true;
            }
            if (value.is_number()) {
                characteristic.set_value(value.as_int() != 0, source);
                return true;
            }
            return false;

        case Format::UInt8:
            if (value.is_number()) {
                characteristic.set_value(static_cast<uint8_t>(value.as_uint32()), source);
                return true;
            }
            if (value.is_bool()) {
                characteristic.set_value(static_cast<uint8_t>(value.as_bool() ? 1 : 0), source);
                return true;
            }
            return false;

        case Format::UInt16:
            if (value.is_number()) {
                characteristic.set_value(static_cast<uint16_t>(value.as_uint32()), source);
                return true;
            }
            if (value.is_bool()) {
                characteristic.set_value(static_cast<uint16_t>(value.as_bool() ? 1 : 0), source);
                return true;
            }
            return false;

        case Format::UInt32:
            if (value.is_number()) {
                characteristic.set_value(value.as_uint32(), source);
                return true;
            }
            if (value.is_bool()) {
                characteristic.set_value(static_cast<uint32_t>(value.as_bool() ? 1 : 0), source);
                return true;
            }
            return false;

        case Format::UInt64:
            if (value.is_number()) {
                characteristic.set_value(value.as_uint64(), source);
                return true;
            }
            if (value.is_bool()) {
                characteristic.set_value(static_cast<uint64_t>(value.as_bool() ? 1 : 0), source);
                return true;
            }
            return false;

        case Format::Int:
            if (value.is_number()) {
                characteristic.set_value(static_cast<int32_t>(value.as_int64()), source);
                return true;
            }
            if (value.is_bool()) {
                characteristic.set_value(static_cast<int32_t>(value.as_bool() ? 1 : 0), source);
                return true;
            }
            return false;

        case Format::Float:
            if (value.is_number()) {
                characteristic.set_value(static_cast<float>(value.as_double()), source);
                return true;
            }
            return false;

        case Format::String:
            if (value.is_string()) {
                characteristic.set_value(value.as_string(), source);
                return true;
            }
            return false;

        case Format::TLV8:
        case Format::Data:
            if (value.is_string()) {
                characteristic.set_value(base64_decode(value.as_string()), source);
                return true;
            }
            return false;
    }
    return false;
}

} // namespace

AccessoryEndpoints::AccessoryEndpoints(core::AttributeDatabase* database)
    : database_(database) {}

Response AccessoryEndpoints::handle_get_accessories(const Request& req, ConnectionContext& ctx) {
    (void)req;
    (void)ctx;

    std::string json_str = database_->to_json_string();

    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/hap+json");
    resp.set_body(json_str);

    return resp;
}

Response AccessoryEndpoints::handle_get_characteristics(const Request& req, ConnectionContext& ctx) {
    (void)ctx;

    // ?id=1.2,1.3,2.4
    std::string query;
    size_t query_start = req.path.find('?');
    if (query_start != std::string::npos) {
        query = req.path.substr(query_start + 1);
    }

    auto char_ids = parse_characteristic_ids(query);

    JsonValue response = JsonValue::object();
    JsonValue characteristics = JsonValue::array();
    bool any_error = false;

    for (const auto& [aid, iid] : char_ids) {
        auto characteristic = database_->find_characteristic(aid, iid);
        if (characteristic) {
            JsonValue char_json = JsonValue::object();
            char_json.set("aid", aid);
            char_json.set("iid", iid);

            if (!core::has_permission(characteristic->permissions(), core::Permission::PairedRead)) {
                char_json.set("status", core::to_int(core::HAPStatus::WriteOnlyCharacteristic));
                any_error = true;
                characteristics.push_back(std::move(char_json));
                continue;
            }

            // get_value() returns ReadResponse (variant of Value or HAPStatus)
            auto read_result = characteristic->get_value();

            if (std::holds_alternative<core::HAPStatus>(read_result)) {
                // Read callback returned an error status
                char_json.set("status", core::to_int(std::get<core::HAPStatus>(read_result)));
                any_error = true;
                characteristics.push_back(std::move(char_json));
                continue;
            }

            // Success - extract the Value
            char_json.set("status", core::to_int(core::HAPStatus::Success));
            char_json.set("value", core::value_to_json(std::get<core::Value>(read_result)));

            characteristics.push_back(std::move(char_json));
        } else {
            JsonValue char_json = JsonValue::object();
            char_json.set("aid", aid);
            char_json.set("iid", iid);
            char_json.set("status", core::to_int(core::HAPStatus::ResourceDoesNotExist));
            any_error = true;
            characteristics.push_back(std::move(char_json));
        }
    }

    // HAP Spec 6.7.4.2: Return 207 Multi-Status if any read fails
    if (any_error) {
        response.set("characteristics", std::move(characteristics));
        return json_response(std::move(response), Status::MultiStatus);
    }

    // For 200 OK, strip status:0 as it's optional for successful reads
    for (auto& c : characteristics.items()) {
        const JsonValue* status = c.find("status");
        if (status && status->as_int() == 0) {
            c.erase("status");
        }
    }
    response.set("characteristics", std::move(characteristics));

    return json_response(std::move(response), Status::OK);
}

Response AccessoryEndpoints::handle_put_characteristics(const Request& req, ConnectionContext& ctx) {
    (void)ctx;
    std::string body_str(req.body.begin(), req.body.end());
    bool parse_error = false;
    JsonValue body_json = JsonValue::parse(body_str, &parse_error);
    if (parse_error) {
        JsonValue error_response = JsonValue::object();
        error_response.set("status", core::to_int(core::HAPStatus::InvalidValueInRequest));
        return json_response(std::move(error_response), Status::BadRequest);
    }

    const JsonValue* characteristics_req = body_json.find("characteristics");
    if (!characteristics_req || !characteristics_req->is_array()) {
        JsonValue error_response = JsonValue::object();
        error_response.set("status", core::to_int(core::HAPStatus::InvalidValueInRequest));
        return json_response(std::move(error_response), Status::BadRequest);
    }

    JsonValue response = JsonValue::object();
    JsonValue characteristics = JsonValue::array();
    bool any_error = false;
    bool write_response_needed = false;

    for (const auto& char_req : characteristics_req->items()) {
        uint64_t aid = char_req.find("aid") ? char_req.find("aid")->as_uint64() : 0;
        uint64_t iid = char_req.find("iid") ? char_req.find("iid")->as_uint64() : 0;

        auto characteristic = database_->find_characteristic(aid, iid);

        if (!characteristic) {
            JsonValue char_json = JsonValue::object();
            char_json.set("aid", aid);
            char_json.set("iid", iid);
            char_json.set("status", core::to_int(core::HAPStatus::ResourceDoesNotExist));
            characteristics.push_back(std::move(char_json));
            any_error = true;
            continue;
        }

        bool processed = false;
        int status = core::to_int(core::HAPStatus::Success);

        const JsonValue* ev = char_req.find("ev");
        if (ev) {
            bool enable = ev->is_bool() ? ev->as_bool() : (ev->is_number() && ev->as_int() != 0);
            if (core::has_permission(characteristic->permissions(), core::Permission::Notify)) {
                if (enable) ctx.add_subscription(aid, iid);
                else ctx.remove_subscription(aid, iid);
                processed = true;
            } else {
                status = core::to_int(core::HAPStatus::NotificationNotSupported);
                processed = true;
            }
        }

        const JsonValue* value_json = char_req.find("value");
        if (value_json) {
            if (core::has_permission(characteristic->permissions(), core::Permission::TimedWrite)) {
                const JsonValue* pid = char_req.find("pid");
                if (!pid) {
                    status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                } else if (!ctx.validate_timed_write(pid->as_uint64())) {
                    status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                }
            }

            if (status == core::to_int(core::HAPStatus::Success)) {
                if (!core::has_permission(characteristic->permissions(), core::Permission::PairedWrite)) {
                    status = core::to_int(core::HAPStatus::ReadOnlyCharacteristic);
                } else if (!json_value_to_characteristic(*characteristic, *value_json,
                                                          core::EventSource::from_connection(ctx.connection_id()))) {
                    status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                }
            }
            processed = true;
        }

        if (status != core::to_int(core::HAPStatus::Success)) {
            any_error = true;
        }

        if (core::has_permission(characteristic->permissions(), core::Permission::WriteResponse)) {
            write_response_needed = true;
        }

        if (processed) {
            JsonValue char_json = JsonValue::object();
            char_json.set("aid", aid);
            char_json.set("iid", iid);
            char_json.set("status", status);

            if (status == core::to_int(core::HAPStatus::Success) &&
                core::has_permission(characteristic->permissions(), core::Permission::WriteResponse)) {
                // Get input value for write-response callback
                auto read_result = characteristic->get_value();
                if (std::holds_alternative<core::Value>(read_result)) {
                    auto input_value = std::get<core::Value>(read_result);
                    auto response_opt = characteristic->handle_write_response(input_value);

                    core::Value value_to_send;
                    if (response_opt.has_value()) {
                        auto& response_cb = *response_opt;
                        if (std::holds_alternative<core::HAPStatus>(response_cb)) {
                            // WriteResponse callback returned an error
                            status = core::to_int(std::get<core::HAPStatus>(response_cb));
                            any_error = true;
                        } else {
                            value_to_send = std::get<core::Value>(response_cb);
                        }
                    } else {
                        // No callback, use the current value
                        value_to_send = input_value;
                    }

                    if (status == core::to_int(core::HAPStatus::Success)) {
                        char_json.set("value", core::value_to_json(value_to_send));
                    }
                    // Update the status in char_json in case it changed
                    char_json.set("status", status);
                }
            }

            characteristics.push_back(std::move(char_json));
        }
    }

    if (any_error) {
        response.set("characteristics", std::move(characteristics));
        return json_response(std::move(response), Status::MultiStatus);
    } else if (write_response_needed) {
        JsonValue final_chars = JsonValue::array();
        for (auto& c : characteristics.items()) {
            if (c.contains("value")) {
                // HAP Spec 6.7.3: 207 Multi-Status MUST include status for each characteristic
                // Ensure status is present (default to 0 if not set)
                if (!c.contains("status")) {
                    c.set("status", core::to_int(core::HAPStatus::Success));
                }
                final_chars.push_back(std::move(c));
            }
        }
        response.set("characteristics", std::move(final_chars));
        return json_response(std::move(response), Status::MultiStatus); // HAP Spec 6.7.3: WriteResponse uses 207 Multi-Status
    } else {
        return Response{Status::NoContent};
    }
}

std::vector<std::pair<uint64_t, uint64_t>> AccessoryEndpoints::parse_characteristic_ids(const std::string& query) {
    std::vector<std::pair<uint64_t, uint64_t>> result;

    size_t id_start = query.find("id=");
    if (id_start == std::string::npos) {
        return result;
    }

    std::string_view ids(query.data() + id_start + 3, query.size() - id_start - 3);
    size_t pos = 0;
    while (pos < ids.size()) {
        size_t comma = ids.find(',', pos);
        if (comma == std::string_view::npos) comma = ids.size();

        std::string_view token = ids.substr(pos, comma - pos);
        pos = comma + 1;

        size_t dot_pos = token.find('.');
        if (dot_pos == std::string_view::npos) continue;
        std::string_view aid_sv = token.substr(0, dot_pos);
        std::string_view iid_sv = token.substr(dot_pos + 1);

        uint64_t aid = 0, iid = 0;
        auto [p1, ec1] = std::from_chars(aid_sv.data(), aid_sv.data() + aid_sv.size(), aid);
        auto [p2, ec2] = std::from_chars(iid_sv.data(), iid_sv.data() + iid_sv.size(), iid);
        if (ec1 == std::errc() && p1 == aid_sv.data() + aid_sv.size() &&
            ec2 == std::errc() && p2 == iid_sv.data() + iid_sv.size()) {
            result.emplace_back(aid, iid);
        }
    }

    return result;
}

Response AccessoryEndpoints::handle_prepare(const Request& req, ConnectionContext& ctx) {
    std::string body_str(req.body.begin(), req.body.end());
    bool parse_error = false;
    JsonValue body_json = JsonValue::parse(body_str, &parse_error);

    const JsonValue* ttl = body_json.find("ttl");
    const JsonValue* pid = body_json.find("pid");
    if (parse_error || !ttl || !pid) {
        JsonValue error_response = JsonValue::object();
        error_response.set("status", core::to_int(core::HAPStatus::InvalidValueInRequest));
        return json_response(std::move(error_response), Status::BadRequest);
    }

    ctx.prepare_timed_write(pid->as_uint64(), ttl->as_uint64());

    JsonValue response = JsonValue::object();
    response.set("status", core::to_int(core::HAPStatus::Success));
    return json_response(std::move(response), Status::OK);
}

} // namespace hap::transport
