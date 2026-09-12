#include <charconv>
#include "hap/AccessoryServer.hpp"
#include "hap/common/TaskScheduler.hpp"
#include "hap/common/Log.hpp"
#include "hap/transport/Router.hpp"
#include "hap/transport/BleTransport.hpp"
#include "hap/transport/ConnectionContext.hpp"
#include "hap/transport/PairingEndpoints.hpp"
#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/HAPStatus.hpp"
#include <map>

namespace hap {

namespace {

// Formats 6 random bytes as "AA:BB:CC:DD:EE:FF".
std::string format_device_id(const uint8_t* bytes) {
    static const char* kHex = "0123456789ABCDEF";
    std::string id;
    id.reserve(17);
    for (int i = 0; i < 6; ++i) {
        if (i > 0) id.push_back(':');
        id.push_back(kHex[(bytes[i] >> 4) & 0xF]);
        id.push_back(kHex[bytes[i] & 0xF]);
    }
    return id;
}

// Verbose logging helper: uppercase hex dump of a byte buffer.
[[maybe_unused]] std::string to_hex_string(const uint8_t* data, size_t len) {
    static const char* kHex = "0123456789ABCDEF";
    std::string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        s.push_back(kHex[(data[i] >> 4) & 0xF]);
        s.push_back(kHex[data[i] & 0xF]);
    }
    return s;
}

} // namespace

class AccessoryServer::Impl {
public:
    std::unique_ptr<transport::Router> router;
    std::unique_ptr<transport::PairingEndpoints> pairing_endpoints;
    std::unique_ptr<transport::BleTransport> ble_transport;
    std::unique_ptr<transport::AccessoryEndpoints> accessory_endpoints;
    std::map<uint32_t, std::unique_ptr<transport::ConnectionContext>> connections;
    std::map<uint32_t, transport::HTTPParser> parsers;
};

AccessoryServer::AccessoryServer(Config config) : config_(std::move(config)), impl_(std::make_unique<Impl>()) {
    // Auto-generate accessory ID if not provided
    if (config_.accessory_id.empty()) {
        auto stored_id = config_.storage->get("accessory_id");
        if (stored_id && !stored_id->empty()) {
            config_.accessory_id = std::string(stored_id->begin(), stored_id->end());
            HAP_LOG_INFO(config_.system, "[AccessoryServer] Loaded stored accessory ID: ", config_.accessory_id);
        } else {
            // Generate random 6 bytes and format as MAC address
            uint8_t random_id[6];
            config_.system->random_bytes(std::span<uint8_t>(random_id, 6));
            config_.accessory_id = format_device_id(random_id);

            // Store for future boots
            std::vector<uint8_t> id_bytes(config_.accessory_id.begin(), config_.accessory_id.end());
            config_.storage->set("accessory_id", id_bytes);

            HAP_LOG_INFO(config_.system, "[AccessoryServer] Generated new accessory ID: ", config_.accessory_id);
        }
    }
    
    // Initialize endpoints
    transport::PairingEndpoints::Config pairing_config;
    pairing_config.crypto = config_.crypto;
    pairing_config.storage = config_.storage;
    pairing_config.system = config_.system;
    pairing_config.accessory_id = config_.accessory_id;
    pairing_config.setup_code = config_.setup_code;
    pairing_config.on_pairings_changed = [this](const std::string& pairing_id, const std::array<uint8_t, 32>& ltpk, bool is_add) {
        HAP_LOG_INFO(config_.system,
            "[AccessoryServer] Pairing ", is_add ? "added" : "removed", ": ", pairing_id);
        
        // Check if all pairings have been removed
        auto pairing_list_data = config_.storage->get("pairing_list");
        bool is_paired = pairing_list_data && pairing_list_data->size() > 2;
        
        if (!is_paired) {
            // Clear all state and regenerate identifiers
            reset_pairing_state();
        }
        
        update_mdns();
        if (impl_->ble_transport) {
            impl_->ble_transport->update_advertising();
        }
        
        // Invoke user callback if set
        if (config_.on_pairings_changed) {
            PairingEvent event;
            event.type = is_add ? PairingEventType::Added : PairingEventType::Removed;
            event.pairing_id = pairing_id;
            event.ltpk = ltpk;
            config_.on_pairings_changed(event);
        }
    };
    impl_->pairing_endpoints = std::make_unique<transport::PairingEndpoints>(pairing_config);
    
    // Initialize IIDManager for persistent IID allocation
    iid_manager_ = std::make_unique<core::IIDManager>(config_.storage, config_.system);
    database_.set_iid_manager(iid_manager_.get());
    
    // Initialize task scheduler first (needed by BleTransport)
    scheduler_ = std::make_unique<common::TaskScheduler>(config_.system);
    
    if (config_.ble) {
        transport::BleTransport::Config ble_config;
        ble_config.ble = config_.ble;
        ble_config.crypto = config_.crypto;
        ble_config.database = &database_;
        ble_config.pairing_endpoints = impl_->pairing_endpoints.get();
        ble_config.system = config_.system;
        ble_config.storage = config_.storage;
        ble_config.accessory_id = config_.accessory_id;
        ble_config.device_name = config_.device_name;
        ble_config.category_id = static_cast<uint16_t>(config_.category_id);
        ble_config.iid_manager = iid_manager_.get();
        impl_->ble_transport = std::make_unique<transport::BleTransport>(ble_config);
        
        // Schedule periodic BLE session timeout checks (every 1 second)
        scheduler_->schedule_periodic(1000, [this]() {
            impl_->ble_transport->check_session_timeouts();
        });
    }
    
    impl_->accessory_endpoints = std::make_unique<transport::AccessoryEndpoints>(&database_);
    
    // Initialize router
    impl_->router = std::make_unique<transport::Router>();
    setup_routes();
}

AccessoryServer::~AccessoryServer() {
    // Detach characteristics from this server's scheduler before the
    // scheduler dies; without this, callbacks would dispatch into freed state.
    for (const auto& acc : database_.accessories()) {
        for (const auto& svc : acc->services()) {
            for (const auto& ch : svc->characteristics()) {
                ch->clear_dispatcher();
                ch->set_event_callback(nullptr);
            }
        }
    }
}

[[maybe_unused]] static std::string method_to_string(transport::Method method) {
    switch (method) {
        case transport::Method::GET: return "GET";
        case transport::Method::POST: return "POST";
        case transport::Method::PUT: return "PUT";
        case transport::Method::DELETE: return "DELETE";
        case transport::Method::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

bool AccessoryServer::add_accessory(std::shared_ptr<core::Accessory> accessory) {
    auto result = database_.add_accessory(accessory);
    if (result != core::ValidationResult::Success) {
        HAP_LOG_ERROR(config_.system,
            "[AccessoryServer] add_accessory rejected: ", core::validation_result_str(result));
        return false;
    }
    
    // Register event callbacks
    uint64_t aid = accessory->aid();
    for (const auto& service : accessory->services()) {
        for (const auto& characteristic : service->characteristics()) {
            if (core::has_permission(characteristic->permissions(), core::Permission::Notify)) {
                auto ch_ptr = characteristic.get();
                characteristic->set_dispatcher([this](std::function<void()> work) {
                    scheduler_->schedule_once(0, std::move(work));
                });
                characteristic->set_event_callback([this, aid, ch_ptr](const core::Value& value, const core::EventSource& source) {
                    uint64_t iid = ch_ptr->iid();
                    // NotifyChange events carry no connection (id = 0), so
                    // every subscribed controller is notified — including
                    // the one whose write triggered a propagated state
                    // change. Connection-source writes never reach this
                    // callback (the write response confirms them).
                    broadcast_event(aid, iid, value, source.id);
                });
            }
        }
    }
    return true;
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

            // /identify is only valid if accessory is unpaired
            auto pairing_list_data = config_.storage->get("pairing_list");
            bool is_paired = pairing_list_data && pairing_list_data->size() > 2;

            if (is_paired) {
                // Return 400 Bad Request with HAP status -70401 (InsufficientPrivileges)
                hap::common::JsonValue error_response = hap::common::JsonValue::object();
                error_response.set("status", core::to_int(core::HAPStatus::InsufficientPrivileges));
                transport::Response resp{transport::Status::BadRequest};
                resp.set_header("Content-Type", "application/hap+json");
                resp.set_body(error_response.dump());
                return resp;
            }

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
    HAP_LOG_INFO(config_.system, "HAP Server starting...");
    
    // Check if database structure changed and increment CN if needed
    check_and_update_config_number();
    
    // Start TCP listener
    auto receive_cb = [this](uint32_t conn_id, std::span<const uint8_t> data) {
        on_tcp_receive(conn_id, data);
    };
    
    auto disconnect_cb = [this](uint32_t conn_id) {
        on_tcp_disconnect(conn_id);
    };
    
    if (config_.network) {
        config_.network->tcp_listen(config_.port, receive_cb, disconnect_cb);
        
        // Register mDNS service
        update_mdns();
    }
    
    if (impl_->ble_transport) {
        impl_->ble_transport->start();
    }
}

void AccessoryServer::update_mdns() {
    if (!config_.network) return;
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
    mdns_service.txt_records.push_back({"ci", std::to_string(static_cast<uint16_t>(config_.category_id))}); // Category identifier
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
    HAP_LOG_INFO(config_.system, "HAP Server stopping...");
    
    if (impl_->ble_transport) {
        impl_->ble_transport->stop();
    }
    
    impl_->connections.clear();
    impl_->parsers.clear();
}

void AccessoryServer::reset_pairing_state() {
    const char* keys_to_clear[] = {
        "accessory_id",      // Device ID
        "setup_id",          // BLE Setup ID
        "accessory_ltsk",    // Long-term secret key
        "accessory_ltpk",    // Long-term public key
        "pairing_list",      // List of paired controllers
        "gsn",               // Global State Number
        "config_number",     // Configuration number
        "ble_addr",
        "ble_addr_cn"
    };
    
    for (const char* key : keys_to_clear) {
        config_.storage->remove(key);
    }

    HAP_LOG_INFO(config_.system, "[AccessoryServer] All pairing state cleared");

    // Generate new accessory ID
    uint8_t random_id[6];
    config_.system->random_bytes(std::span<uint8_t>(random_id, 6));
    config_.accessory_id = format_device_id(random_id);

    // Store new ID
    std::vector<uint8_t> id_bytes(config_.accessory_id.begin(), config_.accessory_id.end());
    config_.storage->set("accessory_id", id_bytes);

    HAP_LOG_INFO(config_.system, "[AccessoryServer] Generated new accessory ID: ", config_.accessory_id);
    
    // Reset all in-memory session state
    if (impl_->pairing_endpoints) {
        impl_->pairing_endpoints->set_accessory_id(config_.accessory_id);
        impl_->pairing_endpoints->reset();
    }
    
    // Update BleTransport with new accessory ID
    if (impl_->ble_transport) {
        impl_->ble_transport->set_accessory_id(config_.accessory_id);
    }
    
    // Reset IIDManager - allows IID reuse after factory reset
    if (iid_manager_) {
        iid_manager_->reset();
    }
    
    pending_connection_cleanup_ = true;
}

void AccessoryServer::factory_reset() {
    HAP_LOG_WARN(config_.system, "[AccessoryServer] Factory reset initiated");
    
    reset_pairing_state();
    
    // Update advertising/mDNS
    update_mdns();
    if (impl_->ble_transport) {
        impl_->ble_transport->update_advertising();
    }
    
    // Invoke user callback if set (device is now unpaired)
    if (config_.on_pairings_changed) {
        PairingEvent event;
        event.type = PairingEventType::AllRemoved;
        event.pairing_id = "";  // All pairings removed
        event.ltpk = {};        // No specific LTPK
        config_.on_pairings_changed(event);
    }
    
    HAP_LOG_INFO(config_.system,
        "[AccessoryServer] Factory reset complete - accessory is now unpaired");
}

void AccessoryServer::tick() {
    if (scheduler_) {
        scheduler_->tick();
    }
}

void AccessoryServer::on_tcp_receive(uint32_t connection_id, std::span<const uint8_t> data) {
    HAP_LOG(config_.system,
        "[AccessoryServer] Received ", static_cast<uint64_t>(data.size()), " bytes from connection ", connection_id);

    // Get or create connection context
    auto& ctx = impl_->connections[connection_id];
    if (!ctx) {
        HAP_LOG_INFO(config_.system, "[AccessoryServer] New connection #", connection_id);
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
        auto decrypted = ctx->get_secure_session()->decrypt_frame(data);
        if (!decrypted) {
            HAP_LOG_WARN(config_.system,
                "[AccessoryServer] Decryption failed or incomplete frame for connection #", connection_id);
            return;
        }
        plaintext_data = *decrypted;
    } else {
        plaintext_data.assign(data.begin(), data.end());
    }

    // Feed to HTTP parser
    if (parser.feed(plaintext_data)) {
        auto request = parser.take_request();
        parser.reset();

        HAP_LOG(config_.system,
            "[AccessoryServer] HTTP Request: ", method_to_string(request.method), " ", request.path);

        // Verbose payload logging (compiled out below HAP_LOG_LEVEL 0)
        for (const auto& header : request.headers) {
            (void)header;
            HAP_LOG(config_.system, "[AccessoryServer] Header: ", header.first, ": ", header.second);
        }
        if (!request.body.empty()) {
            std::string content_type = request.get_header("Content-Type");
            if (content_type == "application/pairing+tlv8") {
                HAP_LOG(config_.system, "[AccessoryServer] Body (TLV8): ",
                    to_hex_string(request.body.data(), request.body.size()));
            } else {
                HAP_LOG(config_.system, "[AccessoryServer] Body: ",
                    std::string_view(reinterpret_cast<const char*>(request.body.data()), request.body.size()));
            }
        }

        // Dispatch to router
        auto response = impl_->router->dispatch(request, *ctx);

        transport::Response final_response;
        if (response) {
            final_response = *response;
            HAP_LOG(config_.system,
                "[AccessoryServer] HTTP Response: ", static_cast<int>(final_response.status));

            // Verbose payload logging (compiled out below HAP_LOG_LEVEL 0)
            for (const auto& header : final_response.headers) {
                (void)header;
                HAP_LOG(config_.system, "[AccessoryServer] Response Header: ", header.first, ": ", header.second);
            }
            if (!final_response.body.empty()) {
                auto it = final_response.headers.find("Content-Type");
                std::string content_type = (it != final_response.headers.end()) ? it->second : "";

                if (content_type == "application/pairing+tlv8") {
                    HAP_LOG(config_.system, "[AccessoryServer] Response Body (TLV8): ",
                        to_hex_string(final_response.body.data(), final_response.body.size()));
                } else {
                    HAP_LOG(config_.system, "[AccessoryServer] Response Body: ",
                        std::string_view(reinterpret_cast<const char*>(final_response.body.data()), final_response.body.size()));
                }
            }
        } else {
            HAP_LOG_WARN(config_.system, "[AccessoryServer] No route found for: ", request.path);
            // 4xx responses must include HAP status code
            hap::common::JsonValue error_response = hap::common::JsonValue::object();
            error_response.set("status", core::to_int(core::HAPStatus::ResourceDoesNotExist));
            final_response = transport::Response{transport::Status::NotFound};
            final_response.set_header("Content-Type", "application/hap+json");
            final_response.set_body(error_response.dump());
        }

        // Build HTTP response
        auto response_bytes = transport::HTTPBuilder::build(final_response);
        
        // Encrypt if connection is encrypted
        if (ctx->is_encrypted() && ctx->rx_encrypted()) {
            size_t offset = 0;
            while (offset < response_bytes.size()) {
                size_t chunk_size = std::min(response_bytes.size() - offset, (size_t)1024);
                std::span<const uint8_t> chunk(response_bytes.data() + offset, chunk_size);
                auto encrypted = ctx->get_secure_session()->encrypt_frame(chunk);
                config_.network->tcp_send(connection_id, encrypted);
                offset += chunk_size;
            }
        } else {
            config_.network->tcp_send(connection_id, response_bytes);
        }

        // If this exchange was a Pair Verify completion, upgrade the
        // connection to encrypted only AFTER the M4 response has gone out in
        // cleartext (HAP: session security starts after Pair Verify ends).
        impl_->pairing_endpoints->complete_pair_verify(*ctx);

        if (ctx->should_close()) {
            HAP_LOG_INFO(config_.system,
                "[AccessoryServer] Closing connection #", connection_id, " as requested");
            config_.network->tcp_disconnect(connection_id);
        }
    }

    if (pending_connection_cleanup_) {
        pending_connection_cleanup_ = false;
        impl_->connections.clear();
        impl_->parsers.clear();
        HAP_LOG(config_.system, "[AccessoryServer] Deferred connection cleanup completed");
    }
}

void AccessoryServer::on_tcp_disconnect(uint32_t connection_id) {
    HAP_LOG_INFO(config_.system, "[AccessoryServer] Connection #", connection_id, " disconnected");
    impl_->connections.erase(connection_id);
    impl_->parsers.erase(connection_id);
}

void AccessoryServer::broadcast_event(uint64_t aid, uint64_t iid, const core::Value& value, uint32_t exclude_conn_id) {
    if (impl_->ble_transport) {
        impl_->ble_transport->notify_value_changed(aid, iid, value, exclude_conn_id);
    }
    if(!config_.network) {
        return;
    }

    hap::common::JsonValue body_json = hap::common::JsonValue::object();
    hap::common::JsonValue characteristics = hap::common::JsonValue::array();
    hap::common::JsonValue char_json = hap::common::JsonValue::object();
    char_json.set("aid", aid);
    char_json.set("iid", iid);
    char_json.set("value", core::value_to_json(value));
    characteristics.push_back(std::move(char_json));
    body_json.set("characteristics", std::move(characteristics));
    std::string body = body_json.dump();

    // EVENT/1.0 has no status text, so the header block is a fixed-size
    // template with only the length substituted.
    char header[96];
    int header_len = snprintf(header, sizeof(header),
        "EVENT/1.0 200 OK\r\nContent-Type: application/hap+json\r\nContent-Length: %zu\r\n\r\n",
        body.size());
    if (header_len < 0) return;

    std::vector<uint8_t> response_bytes;
    response_bytes.reserve(static_cast<size_t>(header_len) + body.size());
    response_bytes.insert(response_bytes.end(), header, header + header_len);
    response_bytes.insert(response_bytes.end(), body.begin(), body.end());

    for (auto& [conn_id, ctx] : impl_->connections) {
        if (conn_id == exclude_conn_id) continue;

        if (ctx->is_encrypted() && ctx->has_subscription(aid, iid)) {
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

void AccessoryServer::check_and_update_config_number() {
    // Build database structure hash
    std::string hash_input;
    hash_input.reserve(64);

    auto append_hex = [&hash_input](uint64_t v) {
        static const char* kHex = "0123456789abcdef";
        char buf[16];
        int n = 0;
        do { buf[n++] = kHex[v & 0xF]; v >>= 4; } while (v > 0);
        while (n > 0) hash_input.push_back(buf[--n]);
    };

    for (const auto& acc : database_.accessories()) {
        hash_input += "A";
        append_hex(acc->aid());
        hash_input += ":";
        for (const auto& svc : acc->services()) {
            hash_input += "S";
            append_hex(svc->type());
            hash_input += ":";
            for (const auto& ch : svc->characteristics()) {
                hash_input += "C";
                append_hex(ch->type());
                hash_input += ",";
            }
        }
    }

    // Simple DJB2 hash
    uint32_t hash = 5381;
    for (char c : hash_input) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
    }

    std::string current_hash = std::to_string(hash);
    
    // Check for structure change using IIDManager if available, else direct storage
    bool structure_changed = false;
    if (iid_manager_) {
        structure_changed = iid_manager_->has_structure_changed(current_hash);
    } else {
        auto stored_hash_data = config_.storage->get("db_hash");
        std::string stored_hash;
        if (stored_hash_data && !stored_hash_data->empty()) {
            stored_hash = std::string(stored_hash_data->begin(), stored_hash_data->end());
        }
        structure_changed = (stored_hash != current_hash);
    }
    
    if (structure_changed) {
        HAP_LOG_INFO(config_.system,
            "[AccessoryServer] Database structure changed, incrementing Configuration Number");
        
        auto cn_data = config_.storage->get("config_number");
        uint16_t cn = 0;
        if (cn_data && !cn_data->empty()) {
            std::string_view cn_sv(reinterpret_cast<const char*>(cn_data->data()), cn_data->size());
            uint32_t parsed = 0;
            auto [ptr, ec] = std::from_chars(cn_sv.begin(), cn_sv.end(), parsed);
            if (ec == std::errc() && ptr == cn_sv.end() && parsed <= 65535) {
                cn = static_cast<uint16_t>(parsed);
            }
        }
        
        cn = (cn >= 65535) ? 1 : cn + 1;
        
        std::string cn_str = std::to_string(cn);
        config_.storage->set("config_number", std::vector<uint8_t>(cn_str.begin(), cn_str.end()));
        
        // Update stored hash
        if (iid_manager_) {
            iid_manager_->update_stored_hash(current_hash);
        } else {
            config_.storage->set("db_hash", std::vector<uint8_t>(current_hash.begin(), current_hash.end()));
        }

        HAP_LOG_INFO(config_.system,
            "[AccessoryServer] Configuration Number updated to: ", cn_str);
    }
}

} // namespace hap
