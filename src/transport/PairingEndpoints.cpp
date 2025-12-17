#include "hap/transport/PairingEndpoints.hpp"
#include <nlohmann/json.hpp>

namespace hap::transport {

PairingEndpoints::PairingEndpoints(Config config) : config_(std::move(config)) {
    config_.system->log(platform::System::LogLevel::Info, "[PairingEndpoints] Initialized");
}

Response PairingEndpoints::handle_pair_setup(const Request& req, ConnectionContext& ctx) {
    config_.system->log(platform::System::LogLevel::Info, 
        "[PairingEndpoints] /pair-setup request from connection #" + std::to_string(ctx.connection_id()) + 
        ", body size: " + std::to_string(req.body.size()));
    
    // Parse request to check what state we're handling
    auto tlvs = core::TLV8::parse(req.body);
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::State));
    bool is_m1 = state_val && *state_val == static_cast<uint8_t>(pairing::PairingState::M1);
    
    // Get or create session
    // For BLE: always create a new session on M1 to handle reconnection properly
    // (BLE reconnects with same connection_id=0, so old completed session would be reused)
    auto& session = pair_setup_sessions_[ctx.connection_id()];
    if (!session || is_m1) {
        config_.system->log(platform::System::LogLevel::Info, 
            "[PairingEndpoints] Creating new pair-setup session");
        pairing::PairSetup::Config setup_config;
        setup_config.crypto = config_.crypto;
        setup_config.storage = config_.storage;
        setup_config.system = config_.system;
        setup_config.accessory_id = config_.accessory_id;
        setup_config.setup_code = config_.setup_code;
        setup_config.on_pairings_changed = config_.on_pairings_changed;
        session = std::make_unique<pairing::PairSetup>(setup_config);
    }

    // Handle request
    auto response_tlv = session->handle_request(req.body);
    
    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/pairing+tlv8");
    
    if (response_tlv) {
        config_.system->log(platform::System::LogLevel::Info, 
            "[PairingEndpoints] Pair-setup response ready (" + std::to_string(response_tlv->size()) + " bytes)");
        resp.set_body(*response_tlv);
    } else {
        config_.system->log(platform::System::LogLevel::Error, 
            "[PairingEndpoints] Pair-setup failed - no response from session");
        resp = Response{Status::InternalServerError};
        resp.set_body("Pairing error");
    }
    
    return resp;
}

Response PairingEndpoints::handle_pair_verify(const Request& req, ConnectionContext& ctx) {
    config_.system->log(platform::System::LogLevel::Info, 
        "[PairingEndpoints] /pair-verify request from connection #" + std::to_string(ctx.connection_id()) + 
        ", body size: " + std::to_string(req.body.size()));
    
    // Parse request to check what state we're handling
    auto tlvs = core::TLV8::parse(req.body);
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::State));
    bool is_m1 = state_val && *state_val == static_cast<uint8_t>(pairing::PairingState::M1);
    
    // Get or create session
    // For BLE: always create a new session on M1 to handle reconnection properly
    // (BLE reconnects with same connection_id=0, so old verified session would be reused)
    auto& session = pair_verify_sessions_[ctx.connection_id()];
    if (!session || is_m1) {
        config_.system->log(platform::System::LogLevel::Info, 
            "[PairingEndpoints] Creating new pair-verify session");
        pairing::PairVerify::Config verify_config;
        verify_config.crypto = config_.crypto;
        verify_config.storage = config_.storage;
        verify_config.accessory_id = config_.accessory_id;
        session = std::make_unique<pairing::PairVerify>(verify_config);
    }

    // Handle request
    auto response_tlv = session->handle_request(req.body);
    
    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/pairing+tlv8");
    
    if (response_tlv) {
        resp.set_body(*response_tlv);
        
        // If verification succeeded, upgrade connection to encrypted
        if (session->is_verified()) {
            config_.system->log(platform::System::LogLevel::Info, 
                "[PairingEndpoints] Pair-verify succeeded - upgrading connection to encrypted");
            ctx.upgrade_to_secure(
                session->get_session_keys(),
                session->get_shared_secret(),
                session->get_controller_id());
        } else {
            config_.system->log(platform::System::LogLevel::Debug, 
                "[PairingEndpoints] Pair-verify response sent (" + std::to_string(response_tlv->size()) + " bytes)");
        }
    } else {
        config_.system->log(platform::System::LogLevel::Error, 
            "[PairingEndpoints] Pair-verify failed - no response from session");
        resp = Response{Status::InternalServerError};
        resp.set_body("Verification error");
    }
    
    return resp;
}

Response PairingEndpoints::handle_pairings(const Request& req, ConnectionContext& ctx) {
    if (!ctx.is_encrypted()) {
        config_.system->log(platform::System::LogLevel::Error, "[PairingEndpoints] /pairings request on unencrypted connection");
        return Response{Status::Unauthorized};
    }

    auto tlvs = core::TLV8::parse(req.body);
    auto method_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::Method));
    if (!method_val) {
        return Response{Status::BadRequest};
    }
    
    pairing::PairingMethod method = static_cast<pairing::PairingMethod>(*method_val);
    config_.system->log(platform::System::LogLevel::Info, 
        "[PairingEndpoints] /pairings method: " + std::to_string(static_cast<int>(method)));

    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::State), static_cast<uint8_t>(pairing::PairingState::M2));

    if (method == pairing::PairingMethod::AddPairing) {
        if (!ctx.is_admin()) {
            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Authentication));
        } else {
            auto identifier = core::TLV8::find(tlvs, static_cast<uint8_t>(pairing::TLVType::Identifier));
            auto public_key = core::TLV8::find(tlvs, static_cast<uint8_t>(pairing::TLVType::PublicKey));
            
            if (!identifier || !public_key || public_key->size() != 32) {
                 response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Unknown));
            } else {
                std::string pairing_id(identifier->begin(), identifier->end());
                std::string pairing_key = "pairing_" + pairing_id;
                config_.storage->set(pairing_key, *public_key);
                
                // Update list
                auto list_data = config_.storage->get("pairing_list");
                nlohmann::json list_json = list_data ? nlohmann::json::parse(list_data->begin(), list_data->end(), nullptr, false) : nlohmann::json::array();
                if (list_json.is_discarded()) list_json = nlohmann::json::array();

                bool exists = false;
                for (const auto& id : list_json) {
                    if (id == pairing_id) { exists = true; break; }
                }
                if (!exists) {
                    list_json.push_back(pairing_id);
                    std::string list_str = list_json.dump();
                    config_.storage->set("pairing_list", std::vector<uint8_t>(list_str.begin(), list_str.end()));
                    if (config_.on_pairings_changed) config_.on_pairings_changed();
                }
            }
        }
    } else if (method == pairing::PairingMethod::RemovePairing) {
        if (!ctx.is_admin()) {
            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Authentication));
        } else {
            auto identifier = core::TLV8::find(tlvs, static_cast<uint8_t>(pairing::TLVType::Identifier));
            if (!identifier) {
                response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Unknown));
            } else {
                std::string pairing_id(identifier->begin(), identifier->end());
                std::string pairing_key = "pairing_" + pairing_id;
                config_.storage->remove(pairing_key);
                
                // Update list
                auto list_data = config_.storage->get("pairing_list");
                if (list_data) {
                    auto list_json = nlohmann::json::parse(list_data->begin(), list_data->end(), nullptr, false);
                    if (!list_json.is_discarded()) {
                        for (auto it = list_json.begin(); it != list_json.end(); ) {
                            if (*it == pairing_id) {
                                it = list_json.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        std::string list_str = list_json.dump();
                        config_.storage->set("pairing_list", std::vector<uint8_t>(list_str.begin(), list_str.end()));
                        if (config_.on_pairings_changed) config_.on_pairings_changed();
                    }
                }
                
                if (pairing_id == ctx.controller_id()) {
                    ctx.request_close();
                }
            }
        }
    } else if (method == pairing::PairingMethod::ListPairings) {
        if (!ctx.is_admin()) {
            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Authentication));
        } else {
            auto list_data = config_.storage->get("pairing_list");
            if (list_data) {
                auto list_json = nlohmann::json::parse(list_data->begin(), list_data->end(), nullptr, false);
                if (!list_json.is_discarded()) {
                    bool first = true;
                    for (const auto& id_json : list_json) {
                        std::string id = id_json.get<std::string>();
                        auto ltpk_data = config_.storage->get("pairing_" + id);
                        if (ltpk_data && ltpk_data->size() == 32) {
                            if (!first) {
                                response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Separator), std::vector<uint8_t>{});
                            }
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Identifier), id);
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::PublicKey), *ltpk_data);
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Permissions), std::vector<uint8_t>{0x01}); // Admin
                            first = false;
                        }
                    }
                }
            }
        }
    }

    Response resp{Status::OK};
    resp.set_header("Content-Type", "application/pairing+tlv8");
    resp.set_body(core::TLV8::encode(response_tlvs));
    return resp;
}

void PairingEndpoints::set_accessory_id(const std::string& new_id) {
    config_.accessory_id = new_id;
    config_.system->log(platform::System::LogLevel::Info, 
        "[PairingEndpoints] Accessory ID updated to: " + new_id);
}

void PairingEndpoints::reset() {
    // Clear all session state
    pair_setup_sessions_.clear();
    pair_verify_sessions_.clear();
    config_.system->log(platform::System::LogLevel::Info, 
        "[PairingEndpoints] All sessions cleared");
}

} // namespace hap::transport
