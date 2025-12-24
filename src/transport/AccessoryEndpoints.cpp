#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/HAPStatus.hpp"
#include <sstream>

using json = nlohmann::json;

namespace hap::transport {

static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static constexpr unsigned char decode_table[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
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
    
    json response;
    json characteristics = json::array();
    bool any_error = false;
    
    for (const auto& [aid, iid] : char_ids) {
        auto characteristic = database_->find_characteristic(aid, iid);
        if (characteristic) {
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            
            if (!core::has_permission(characteristic->permissions(), core::Permission::PairedRead)) {
                char_json["status"] = core::to_int(core::HAPStatus::WriteOnlyCharacteristic);
                any_error = true;
                characteristics.push_back(char_json);
                continue;
            }
            
            // get_value() now returns ReadResponse (variant of Value or HAPStatus)
            auto read_result = characteristic->get_value();
            
            if (std::holds_alternative<core::HAPStatus>(read_result)) {
                // Read callback returned an error status
                char_json["status"] = core::to_int(std::get<core::HAPStatus>(read_result));
                any_error = true;
                characteristics.push_back(char_json);
                continue;
            }
            
            // Success - extract the Value
            auto value = std::get<core::Value>(read_result);
            char_json["status"] = core::to_int(core::HAPStatus::Success);
            
            std::visit([&char_json](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, uint8_t> || 
                             std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || 
                             std::is_same_v<T, uint64_t> || std::is_same_v<T, int32_t> || 
                             std::is_same_v<T, float> || std::is_same_v<T, std::string>) {
                    char_json["value"] = arg;
                } else {
                    char_json["value"] = nullptr;
                }
            }, value);
            
            characteristics.push_back(char_json);
        } else {
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            char_json["status"] = core::to_int(core::HAPStatus::ResourceDoesNotExist);
            any_error = true;
            characteristics.push_back(char_json);
        }
    }
    
    response["characteristics"] = characteristics;
    
    // HAP Spec 6.7.4.2: Return 207 Multi-Status if any read fails
    if (any_error) {
        Response resp{Status::MultiStatus};
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(response.dump());
        return resp;
    }
    
    // For 200 OK, strip status:0 as it's optional for successful reads
    for (auto& c : characteristics) {
        if (c.contains("status") && c["status"] == 0) {
            c.erase("status");
        }
    }
    response["characteristics"] = characteristics;
    
    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/hap+json");
    resp.set_body(response.dump());
    
    return resp;
}

Response AccessoryEndpoints::handle_put_characteristics(const Request& req, ConnectionContext& ctx) {
    (void)ctx;
    std::string body_str(req.body.begin(), req.body.end());
    auto body_json = json::parse(body_str, nullptr, false);
    if (body_json.is_discarded()) {
            json error_response;
            error_response["status"] = core::to_int(core::HAPStatus::InvalidValueInRequest);
            Response resp{Status::BadRequest};
            resp.set_header("Content-Type", "application/hap+json");
            resp.set_body(error_response.dump());
            return resp;
    }
    
    if (!body_json.contains("characteristics") || !body_json["characteristics"].is_array()) {
        json error_response;
        error_response["status"] = core::to_int(core::HAPStatus::InvalidValueInRequest);
        Response resp{Status::BadRequest};
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(error_response.dump());
        return resp;
    }
    
    json response;
    json characteristics = json::array();
    bool any_error = false;
    bool write_response_needed = false;
    
    for (const auto& char_req : body_json["characteristics"]) {
        uint64_t aid = char_req["aid"];
        uint64_t iid = char_req["iid"];
        
        auto characteristic = database_->find_characteristic(aid, iid);
        
        if (!characteristic) {
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            char_json["status"] = core::to_int(core::HAPStatus::ResourceDoesNotExist);
            characteristics.push_back(char_json);
            any_error = true;
            continue;
        }

        bool processed = false;
        int status = core::to_int(core::HAPStatus::Success);

        if (char_req.contains("ev")) {
            bool enable = char_req["ev"];
            if (core::has_permission(characteristic->permissions(), core::Permission::Notify)) {
                if (enable) ctx.add_subscription(aid, iid);
                else ctx.remove_subscription(aid, iid);
                processed = true;
            } else {
                status = core::to_int(core::HAPStatus::NotificationNotSupported);
                processed = true;
            }
        }

        if (char_req.contains("value")) {
            if (core::has_permission(characteristic->permissions(), core::Permission::TimedWrite)) {
                if (!char_req.contains("pid")) {
                    status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                } else {
                    uint64_t pid = char_req["pid"];
                    if (!ctx.validate_timed_write(pid)) {
                        status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                    }
                }
            }

            if (status == core::to_int(core::HAPStatus::Success)) {
                if (!core::has_permission(characteristic->permissions(), core::Permission::PairedWrite)) {
                    status = core::to_int(core::HAPStatus::ReadOnlyCharacteristic);
                } else {
                auto& value_json = char_req["value"];
                
                bool valid_value = true;
                auto source = core::EventSource::from_connection(ctx.connection_id());
                    switch (characteristic->format()) {
                        case core::Format::Bool:
                            if (value_json.is_boolean()) {
                                characteristic->set_value(value_json.get<bool>(), source);
                            } else if (value_json.is_number()) {
                                characteristic->set_value(value_json.get<int>() != 0, source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::UInt8:
                            if (value_json.is_number()) {
                                characteristic->set_value(static_cast<uint8_t>(value_json.get<uint32_t>()), source);
                            } else if (value_json.is_boolean()) {
                                characteristic->set_value(static_cast<uint8_t>(value_json.get<bool>() ? 1 : 0), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::UInt16:
                            if (value_json.is_number()) {
                                characteristic->set_value(static_cast<uint16_t>(value_json.get<uint32_t>()), source);
                            } else if (value_json.is_boolean()) {
                                characteristic->set_value(static_cast<uint16_t>(value_json.get<bool>() ? 1 : 0), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::UInt32:
                            if (value_json.is_number()) {
                                characteristic->set_value(value_json.get<uint32_t>(), source);
                            } else if (value_json.is_boolean()) {
                                characteristic->set_value(static_cast<uint32_t>(value_json.get<bool>() ? 1 : 0), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::UInt64:
                            if (value_json.is_number()) {
                                characteristic->set_value(value_json.get<uint64_t>(), source);
                            } else if (value_json.is_boolean()) {
                                characteristic->set_value(static_cast<uint64_t>(value_json.get<bool>() ? 1 : 0), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::Int:
                            if (value_json.is_number()) {
                                characteristic->set_value(value_json.get<int32_t>(), source);
                            } else if (value_json.is_boolean()) {
                                characteristic->set_value(static_cast<int32_t>(value_json.get<bool>() ? 1 : 0), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::Float:
                            if (value_json.is_number()) {
                                characteristic->set_value(value_json.get<float>(), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::String:
                            if (value_json.is_string()) {
                                characteristic->set_value(value_json.get<std::string>(), source);
                            } else {
                                valid_value = false;
                            }
                            break;
                            
                        case core::Format::TLV8:
                        case core::Format::Data:
                            if (value_json.is_string()) {
                                std::string b64_str = value_json.get<std::string>();
                                std::vector<uint8_t> data = base64_decode(b64_str);
                                characteristic->set_value(data, source);
                            } else {
                                valid_value = false;
                            }
                            break;
                    }
                
                if (!valid_value) {
                    status = core::to_int(core::HAPStatus::InvalidValueInRequest);
                }
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
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            char_json["status"] = status;
            
            if (status == core::to_int(core::HAPStatus::Success) && 
                core::has_permission(characteristic->permissions(), core::Permission::WriteResponse)) {
                // Get input value for write-response callback
                auto read_result = characteristic->get_value();
                if (std::holds_alternative<core::Value>(read_result)) {
                    auto input_value = std::get<core::Value>(read_result);
                    auto response_opt = characteristic->handle_write_response(input_value);
                    
                    core::Value value_to_send;
                    if (response_opt.has_value()) {
                        auto& response = *response_opt;
                        if (std::holds_alternative<core::HAPStatus>(response)) {
                            // WriteResponse callback returned an error
                            status = core::to_int(std::get<core::HAPStatus>(response));
                            any_error = true;
                        } else {
                            value_to_send = std::get<core::Value>(response);
                        }
                    } else {
                        // No callback, use the current value
                        value_to_send = input_value;
                    }
                    
                    if (status == core::to_int(core::HAPStatus::Success)) {
                        std::visit([&char_json](auto&& arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, uint8_t> || 
                                            std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || 
                                            std::is_same_v<T, uint64_t> || std::is_same_v<T, int32_t> || 
                                            std::is_same_v<T, float> || std::is_same_v<T, std::string>) {
                                char_json["value"] = arg;
                            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                                char_json["value"] = core::base64_encode(arg);
                            } else {
                                char_json["value"] = nullptr;
                            }
                        }, value_to_send);
                    }
                    // Update the status in char_json in case it changed
                    char_json["status"] = status;
                }
            }
            
            characteristics.push_back(char_json);
        }
    }
    
    if (any_error) {
        response["characteristics"] = characteristics;
        Response resp{Status::MultiStatus};
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(response.dump());
        return resp;
    } else if (write_response_needed) {
        json final_chars = json::array();
            for (auto& c : characteristics) {
                if (c.contains("value")) {
                    // HAP Spec 6.7.3: 207 Multi-Status MUST include status for each characteristic
                    // Ensure status is present (default to 0 if not set)
                    if (!c.contains("status")) {
                        c["status"] = core::to_int(core::HAPStatus::Success);
                    }
                    final_chars.push_back(c);
                }
            }
        response["characteristics"] = final_chars;
        Response resp{Status::MultiStatus};  // HAP Spec 6.7.3: WriteResponse uses 207 Multi-Status
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(response.dump());
        return resp;
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
    
    std::string ids_str = query.substr(id_start + 3);
    
    std::istringstream iss(ids_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        size_t dot_pos = token.find('.');
        if (dot_pos != std::string::npos) {
            uint64_t aid = std::stoull(token.substr(0, dot_pos));
            uint64_t iid = std::stoull(token.substr(dot_pos + 1));
            result.emplace_back(aid, iid);
        }
    }
    
    return result;
}

Response AccessoryEndpoints::handle_prepare(const Request& req, ConnectionContext& ctx) {
    std::string body_str(req.body.begin(), req.body.end());
    auto body_json = json::parse(body_str, nullptr, false);

    if (body_json.is_discarded()) {
            json error_response;
            error_response["status"] = core::to_int(core::HAPStatus::InvalidValueInRequest);
            Response resp{Status::BadRequest};
            resp.set_header("Content-Type", "application/hap+json");
            resp.set_body(error_response.dump());
            return resp;
    }

    if (body_json.contains("ttl") && body_json.contains("pid")) {
        uint64_t ttl = body_json["ttl"];
        uint64_t pid = body_json["pid"];
        
        ctx.prepare_timed_write(pid, ttl);
        
        json response;
        response["status"] = core::to_int(core::HAPStatus::Success);
        
        Response resp{Status::OK};
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(response.dump());
        return resp;
    } else {
        json error_response;
        error_response["status"] = core::to_int(core::HAPStatus::InvalidValueInRequest);
        Response resp{Status::BadRequest};
        resp.set_header("Content-Type", "application/hap+json");
        resp.set_body(error_response.dump());
        return resp;
    }
}

} // namespace hap::transport
