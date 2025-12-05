#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/HAPStatus.hpp"
#include <sstream>
#include <any>

using json = nlohmann::json;

namespace hap::transport {

AccessoryEndpoints::AccessoryEndpoints(core::AttributeDatabase* database) 
    : database_(database) {}

Response AccessoryEndpoints::handle_get_accessories(const Request& req, ConnectionContext& ctx) {
    (void)req; // Unused
    (void)ctx; // Unused
    
    // Serialize database to JSON
    std::string json_str = database_->to_json_string();
    
    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/hap+json");
    resp.set_body(json_str);
    
    return resp;
}

Response AccessoryEndpoints::handle_get_characteristics(const Request& req, ConnectionContext& ctx) {
    (void)ctx; // Unused
    
    // Parse query string to get characteristic IDs
    // Expected format: ?id=1.2,1.3,2.4
    std::string query;
    size_t query_start = req.path.find('?');
    if (query_start != std::string::npos) {
        query = req.path.substr(query_start + 1);
    }
    
    auto char_ids = parse_characteristic_ids(query);
    
    // Build JSON response
    json response;
    json characteristics = json::array();
    
    for (const auto& [aid, iid] : char_ids) {
        auto characteristic = database_->find_characteristic(aid, iid);
        if (characteristic) {
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            
            // Check if characteristic is readable
            if (!core::has_permission(characteristic->permissions(), core::Permission::PairedRead)) {
                char_json["status"] = core::to_int(core::HAPStatus::WriteOnlyCharacteristic);
                characteristics.push_back(char_json);
                continue;
            }
            
            // Get value
            auto value = characteristic->get_value();
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
            
            // Only include status if not success (0)
            // char_json["status"] = core::to_int(core::HAPStatus::Success); 
            characteristics.push_back(char_json);
        } else {
            json char_json;
            char_json["aid"] = aid;
            char_json["iid"] = iid;
            char_json["status"] = core::to_int(core::HAPStatus::ResourceDoesNotExist);
            characteristics.push_back(char_json);
        }
    }
    
    response["characteristics"] = characteristics;
    
    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/hap+json");
    resp.set_body(response.dump());
    
    return resp;
}

Response AccessoryEndpoints::handle_put_characteristics(const Request& req, ConnectionContext& ctx) {
    (void)ctx; // Unused
    
    try {
        // Parse JSON body
        std::string body_str(req.body.begin(), req.body.end());
        auto body_json = json::parse(body_str);
        
        if (!body_json.contains("characteristics") || !body_json["characteristics"].is_array()) {
            Response resp{Status::BadRequest};
            resp.set_body("Invalid request");
            return resp;
        }
        
        // Build response
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

            // Handle Events
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

            // Handle Value Write
            if (char_req.contains("value")) {
                // Check for Timed Write permission
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
                    // Check if characteristic is writable
                    if (!core::has_permission(characteristic->permissions(), core::Permission::PairedWrite)) {
                        status = core::to_int(core::HAPStatus::ReadOnlyCharacteristic);
                    } else {
                    // Set value
                    auto& value_json = char_req["value"];
                    
                    // Convert JSON value to characteristic value
                    bool valid_value = true;
                    auto source = std::make_any<uint32_t>(ctx.connection_id());
                    try {
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
                                // Not supported for write yet
                                valid_value = false;
                                break;
                        }
                    } catch (...) {
                        valid_value = false;
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
                
                // Include value if WriteResponse permission is present and success
                if (status == core::to_int(core::HAPStatus::Success) && 
                    core::has_permission(characteristic->permissions(), core::Permission::WriteResponse)) {
                    auto value = characteristic->get_value();
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
                }
                
                characteristics.push_back(char_json);
            }
        }
        
        if (any_error) {
            // 207 Multi-Status
            response["characteristics"] = characteristics;
            Response resp{Status::MultiStatus};
            resp.set_header("Content-Type", "application/hap+json");
            resp.set_body(response.dump());
            return resp;
        } else if (write_response_needed) {
            // 200 OK with values
            // Filter to only include those with values
            json final_chars = json::array();
                for (auto& c : characteristics) {
                    if (c.contains("value")) {
                        // Omit status if 0 for 200 OK
                        if (c.contains("status") && c["status"] == 0) {
                            c.erase("status");
                        }
                        final_chars.push_back(c);
                    }
                }
            response["characteristics"] = final_chars;
            Response resp{Status::OK};
            resp.set_header("Content-Type", "application/hap+json");
            resp.set_body(response.dump());
            return resp;
        } else {
            // 204 No Content
            return Response{Status::NoContent};
        }
    } catch (...) {
        Response resp{Status::BadRequest};
        resp.set_body("Invalid JSON");
        return resp;
    }
}

std::vector<std::pair<uint64_t, uint64_t>> AccessoryEndpoints::parse_characteristic_ids(const std::string& query) {
    std::vector<std::pair<uint64_t, uint64_t>> result;
    
    // Find "id=" parameter
    size_t id_start = query.find("id=");
    if (id_start == std::string::npos) {
        return result;
    }
    
    // Extract the value after "id="
    std::string ids_str = query.substr(id_start + 3);
    
    // Split by comma
    std::istringstream iss(ids_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Split by dot
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
    try {
        std::string body_str(req.body.begin(), req.body.end());
        auto body_json = json::parse(body_str);
        
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
            Response resp{Status::BadRequest};
            resp.set_body("Missing ttl or pid");
            return resp;
        }
    } catch (...) {
        Response resp{Status::BadRequest};
        resp.set_body("Invalid JSON");
        return resp;
    }
}

} // namespace hap::transport
