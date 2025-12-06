#include "hap/AccessoryServer.hpp"
#include "hap/transport/Router.hpp"
#include "hap/transport/ConnectionContext.hpp"
#include "hap/transport/PairingEndpoints.hpp"
#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/HAPStatus.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace hap {

// Pimpl idiom to hide implementation details
class AccessoryServer::Impl {
public:
    std::unique_ptr<transport::Router> router;
    std::unique_ptr<transport::PairingEndpoints> pairing_endpoints;
    std::unique_ptr<transport::AccessoryEndpoints> accessory_endpoints;
    std::map<uint32_t, std::unique_ptr<transport::ConnectionContext>> connections;
    std::map<uint32_t, transport::HTTPParser> parsers;
};

AccessoryServer::AccessoryServer(Config config) : config_(std::move(config)), impl_(std::make_unique<Impl>()) {
    // Initialize endpoints
    transport::PairingEndpoints::Config pairing_config;
    pairing_config.crypto = config_.crypto;
    pairing_config.storage = config_.storage;
    pairing_config.system = config_.system;
    pairing_config.accessory_id = config_.accessory_id;
    pairing_config.setup_code = config_.setup_code;
    pairing_config.on_pairings_changed = [this]() {
        config_.system->log(platform::System::LogLevel::Info, "[AccessoryServer] Pairings changed, updating mDNS");
        update_mdns();
    };
    impl_->pairing_endpoints = std::make_unique<transport::PairingEndpoints>(pairing_config);
    
    impl_->accessory_endpoints = std::make_unique<transport::AccessoryEndpoints>(&database_);
    
    // Initialize router
    impl_->router = std::make_unique<transport::Router>();
    setup_routes();
}

AccessoryServer::~AccessoryServer() = default;

static std::string method_to_string(transport::Method method) {
    switch (method) {
        case transport::Method::GET: return "GET";
        case transport::Method::POST: return "POST";
        case transport::Method::PUT: return "PUT";
        case transport::Method::DELETE: return "DELETE";
        case transport::Method::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

void AccessoryServer::add_accessory(std::shared_ptr<core::Accessory> accessory) {
    database_.add_accessory(accessory);
    
    // Register event callbacks
    for (const auto& service : accessory->services()) {
        for (const auto& characteristic : service->characteristics()) {
            if (core::has_permission(characteristic->permissions(), core::Permission::Notify)) {
                uint64_t aid = accessory->aid();
                uint64_t iid = characteristic->iid();
                
                characteristic->set_event_callback([this, aid, iid](const core::Value& value, const core::EventSource& source) {
                    uint32_t exclude_id = 0;
                    if (source.type == core::EventSource::Type::Connection) {
                        exclude_id = source.id;
                    }
                    broadcast_event(aid, iid, value, exclude_id);
                });
            }
        }
    }
}

void AccessoryServer::setup_routes() {
    using namespace transport;
    
    // Pairing endpoints (no pairing required)
    impl_->router->add_route(Method::POST, "/pair-setup", 
        [this](const Request& req, ConnectionContext& ctx) {            
            return impl_->pairing_endpoints->handle_pair_setup(req, ctx);
        }, false);
    
    impl_->router->add_route(Method::POST, "/pair-verify",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->pairing_endpoints->handle_pair_verify(req, ctx);
        }, false);
    
    impl_->router->add_route(Method::POST, "/pairings",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->pairing_endpoints->handle_pairings(req, ctx);
        }, true);
        
    impl_->router->add_route(Method::POST, "/prepare",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->accessory_endpoints->handle_prepare(req, ctx);
        }, true);
        
    impl_->router->add_route(Method::POST, "/identify",
        [this](const Request& req, ConnectionContext& ctx) {
            (void)req;
            (void)ctx;
            if (config_.on_identify) {
                config_.on_identify();
            }
            return transport::Response{transport::Status::NoContent};
        }, false); // Allow unencrypted access
    
    // Accessory endpoints (require verified pairing)
    impl_->router->add_route(Method::GET, "/accessories",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->accessory_endpoints->handle_get_accessories(req, ctx);
        }, true);
    
    impl_->router->add_route(Method::GET, "/characteristics",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->accessory_endpoints->handle_get_characteristics(req, ctx);
        }, true);
    
    impl_->router->add_route(Method::PUT, "/characteristics",
        [this](const Request& req, ConnectionContext& ctx) {
            return impl_->accessory_endpoints->handle_put_characteristics(req, ctx);
        }, true);
}

void AccessoryServer::start() {
    config_.system->log(platform::System::LogLevel::Info, "HAP Server starting...");
    
    // Start TCP listener
    auto receive_cb = [this](uint32_t conn_id, std::span<const uint8_t> data) {
        on_tcp_receive(conn_id, data);
    };
    
    auto disconnect_cb = [this](uint32_t conn_id) {
        on_tcp_disconnect(conn_id);
    };
    
    config_.network->tcp_listen(config_.port, receive_cb, disconnect_cb);
    
    // Register mDNS service
    update_mdns();
}

void AccessoryServer::update_mdns() {
    platform::Network::MdnsService mdns_service;
    mdns_service.name = config_.device_name;
    mdns_service.type = "_hap._tcp";
    mdns_service.port = config_.port;
    
    // Check pairing status
    auto pairing_list_data = config_.storage->get("pairing_list");
    bool is_paired = false;
    if (pairing_list_data) {
        if (pairing_list_data->size() > 2) {
            is_paired = true;
        }
    }
    
    std::string sf_val = is_paired ? "0" : "1";
    
    // Check config number
    auto config_num_data = config_.storage->get("config_number");
    std::string config_num = "1";
    if (config_num_data && !config_num_data->empty()) {
        config_num = std::string(config_num_data->begin(), config_num_data->end());
    } else {
        // Initialize to 1
        std::vector<uint8_t> initial_cn = {'1'};
        config_.storage->set("config_number", initial_cn);
    }
    
    // Add required TXT records
    mdns_service.txt_records.push_back({"c#", config_num}); // Configuration number
    mdns_service.txt_records.push_back({"id", config_.accessory_id}); // Device ID
    mdns_service.txt_records.push_back({"md", config_.device_name}); // Model name
    mdns_service.txt_records.push_back({"pv", "1.1"}); // Protocol version
    mdns_service.txt_records.push_back({"s#", "1"}); // State number
    mdns_service.txt_records.push_back({"sf", sf_val}); // Status flags (1 = discoverable/unpaired, 0 = paired)
    mdns_service.txt_records.push_back({"ci", "5"}); // Category identifier (5 = Lightbulb)
    mdns_service.txt_records.push_back({"ff", "0"}); // Feature flags
    
    // Use update instead of re-register if already registered
    if (mdns_registered_) {
        config_.network->mdns_update_txt_record(mdns_service);
    } else {
        config_.network->mdns_register(mdns_service);
        mdns_registered_ = true;
    }
}

void AccessoryServer::stop() {
    config_.system->log(platform::System::LogLevel::Info, "HAP Server stopping...");
    impl_->connections.clear();
    impl_->parsers.clear();
}

void AccessoryServer::on_tcp_receive(uint32_t connection_id, std::span<const uint8_t> data) {
    config_.system->log(platform::System::LogLevel::Debug, 
        "[AccessoryServer] Received " + std::to_string(data.size()) + " bytes from connection " + std::to_string(connection_id));
    
    // Get or create connection context
    auto& ctx = impl_->connections[connection_id];
    if (!ctx) {
        config_.system->log(platform::System::LogLevel::Info, 
            "[AccessoryServer] New connection #" + std::to_string(connection_id));
        ctx = std::make_unique<transport::ConnectionContext>(config_.crypto, config_.system, connection_id);
    }
    
    // Get or create HTTP parser
    auto& parser = impl_->parsers[connection_id];
    
    // Decrypt if connection is encrypted
    std::vector<uint8_t> plaintext_data;
    if (ctx->is_encrypted() && ctx->get_secure_session()) {
        if(!ctx->rx_encrypted()) {
            ctx->set_rx_encrypted(true);
        }
        config_.system->log(platform::System::LogLevel::Debug, 
            "[AccessoryServer] Decrypting frame for connection #" + std::to_string(connection_id));
        auto decrypted = ctx->get_secure_session()->decrypt_frame(data);
        if (!decrypted) {
            config_.system->log(platform::System::LogLevel::Warning, 
                "[AccessoryServer] Decryption failed or incomplete frame for connection #" + std::to_string(connection_id));
            return;
        }
        plaintext_data = *decrypted;
        config_.system->log(platform::System::LogLevel::Debug, 
            "[AccessoryServer] Decrypted " + std::to_string(plaintext_data.size()) + " bytes");
    } else {
        plaintext_data.assign(data.begin(), data.end());
    }
    
    // Feed to HTTP parser
    if (parser.feed(plaintext_data)) {
        auto request = parser.take_request();
        parser.reset();
        
        config_.system->log(platform::System::LogLevel::Info, 
            "[AccessoryServer] HTTP Request: " + method_to_string(request.method) + " " + request.path);
        
        // Log headers
        for (const auto& [key, value] : request.headers) {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[AccessoryServer] Header: " + key + ": " + value);
        }
        
        // Log body
        if (!request.body.empty()) {
            std::string content_type = request.get_header("Content-Type");
            if (content_type == "application/pairing+tlv8") {
                std::ostringstream oss;
                for (uint8_t b : request.body) {
                    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                }
                config_.system->log(platform::System::LogLevel::Debug, 
                    "[AccessoryServer] Body (TLV8): " + oss.str());
            } else {
                std::string body_str(request.body.begin(), request.body.end());
                config_.system->log(platform::System::LogLevel::Debug, 
                    "[AccessoryServer] Body: " + body_str);
            }
        }
        
        // Dispatch to router
        auto response = impl_->router->dispatch(request, *ctx);
        
        transport::Response final_response;
        if (response) {
            final_response = *response;
            config_.system->log(platform::System::LogLevel::Info, 
                "[AccessoryServer] HTTP Response: " + std::to_string(static_cast<int>(final_response.status)));
            
            // Log headers
            for (const auto& [key, value] : final_response.headers) {
                config_.system->log(platform::System::LogLevel::Debug, 
                    "[AccessoryServer] Response Header: " + key + ": " + value);
            }
            
            // Log body
            if (!final_response.body.empty()) {
                auto it = final_response.headers.find("Content-Type");
                std::string content_type = (it != final_response.headers.end()) ? it->second : "";
                
                if (content_type == "application/pairing+tlv8") {
                    std::ostringstream oss;
                    for (uint8_t b : final_response.body) {
                        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                    }
                    config_.system->log(platform::System::LogLevel::Debug, 
                        "[AccessoryServer] Response Body (TLV8): " + oss.str());
                } else {
                    std::string body_str(final_response.body.begin(), final_response.body.end());
                    config_.system->log(platform::System::LogLevel::Debug, 
                        "[AccessoryServer] Response Body: " + body_str);
                }
            }
        } else {
            config_.system->log(platform::System::LogLevel::Warning, 
                "[AccessoryServer] No route found for: " + request.path);
            final_response = transport::Response{transport::Status::NotFound};
            final_response.set_body("Not Found");
        }
        
        // Build HTTP response
        auto response_bytes = transport::HTTPBuilder::build(final_response);
        
        // Encrypt if connection is encrypted
        if (ctx->is_encrypted() && ctx->rx_encrypted()) {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[AccessoryServer] Encrypting response (" + std::to_string(response_bytes.size()) + " bytes)");
            
            size_t offset = 0;
            while (offset < response_bytes.size()) {
                size_t chunk_size = std::min(response_bytes.size() - offset, (size_t)1024);
                std::span<const uint8_t> chunk(response_bytes.data() + offset, chunk_size);
                auto encrypted = ctx->get_secure_session()->encrypt_frame(chunk);
                config_.network->tcp_send(connection_id, encrypted);
                offset += chunk_size;
            }
        } else {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[AccessoryServer] Sending plaintext response (" + std::to_string(response_bytes.size()) + " bytes)");
            config_.network->tcp_send(connection_id, response_bytes);
        }
        
        if (ctx->should_close()) {
            config_.system->log(platform::System::LogLevel::Info, 
                "[AccessoryServer] Closing connection #" + std::to_string(connection_id) + " as requested");
            config_.network->tcp_disconnect(connection_id);
        }
    }
}

void AccessoryServer::on_tcp_disconnect(uint32_t connection_id) {
    config_.system->log(platform::System::LogLevel::Info, 
        "[AccessoryServer] Connection #" + std::to_string(connection_id) + " disconnected");
    impl_->connections.erase(connection_id);
    impl_->parsers.erase(connection_id);
}

void AccessoryServer::broadcast_event(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id) {
    // Build JSON body
    nlohmann::json body_json;
    nlohmann::json characteristics = nlohmann::json::array();
    nlohmann::json char_json;
    char_json["aid"] = aid;
    char_json["iid"] = iid;

    std::visit([&char_json](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            char_json["value"] = arg;
        } else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || 
                             std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || 
                             std::is_same_v<T, int32_t> || std::is_same_v<T, float>) {
            char_json["value"] = arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            char_json["value"] = arg;
        } else {
            char_json["value"] = nullptr; // Handle other types if needed
        }
    }, value);

    characteristics.push_back(char_json);
    body_json["characteristics"] = characteristics;
    std::string body = body_json.dump();

    // Build HTTP EVENT response
    std::ostringstream ss;
    ss << "EVENT/1.0 200 OK\r\n";
    ss << "Content-Type: application/hap+json\r\n";
    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "\r\n";
    ss << body;
    
    std::string response_str = ss.str();
    std::vector<uint8_t> response_bytes(response_str.begin(), response_str.end());

    // Send to all subscribed connections
    for (auto& [conn_id, ctx] : impl_->connections) {
        if (conn_id == exclude_conn_id) continue;
        
        if (ctx->is_encrypted() && ctx->has_subscription(aid, iid)) {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[AccessoryServer] Sending event to connection #" + std::to_string(conn_id));
            
            size_t offset = 0;
            while (offset < response_bytes.size()) {
                size_t chunk_size = std::min(response_bytes.size() - offset, (size_t)1024);
                std::span<const uint8_t> chunk(response_bytes.data() + offset, chunk_size);
                auto encrypted = ctx->get_secure_session()->encrypt_frame(chunk);
                config_.network->tcp_send(conn_id, encrypted);
                offset += chunk_size;
            }
        }
    }
}

} // namespace hap
