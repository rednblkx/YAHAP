#include "hap/transport/PairingEndpoints.hpp"
#include <algorithm>
#include "hap/common/Log.hpp"
#include "hap/common/JsonValue.hpp"

namespace hap::transport {

PairingEndpoints::PairingEndpoints(Config config) : config_(std::move(config)) {
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] Initialized");
}

Response PairingEndpoints::handle_pair_setup(const Request& req, ConnectionContext& ctx) {
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] /pair-setup request from connection #", ctx.connection_id(), ", body size: ", req.body.size());

    // HAP 5.6.2 step 1: an already-paired accessory must reject Pair Setup
    // with kTLVError_Unavailable.
    auto pairing_list = config_.storage->get("pairing_list");
    if (pairing_list && pairing_list->size() > 2) {
        HAP_LOG_WARN(config_.system, "[PairingEndpoints] Pair Setup rejected - accessory already paired");
        std::vector<core::TLV> tlvs = {
            {static_cast<uint8_t>(pairing::TLVType::State), std::vector<uint8_t>{static_cast<uint8_t>(pairing::PairingState::M2)}},
            {static_cast<uint8_t>(pairing::TLVType::Error), std::vector<uint8_t>{static_cast<uint8_t>(pairing::TLVError::Unavailable)}},
        };
        Response resp{Status::OK};
        resp.set_header("Content-Type", "application/pairing+tlv8");
        resp.set_body(core::TLV8::encode(tlvs));
        return resp;
    }

    // Parse request to check what state we're handling
    auto tlvs = core::TLV8::parse(req.body);
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::State));
    bool is_m1 = state_val && *state_val == static_cast<uint8_t>(pairing::PairingState::M1);
    
    // Get or create session
    // For BLE: always create a new session on M1 to handle reconnection properly
    // (BLE reconnects with same connection_id=0, so old completed session would be reused)
    auto* session_slot = find_session(pair_setup_sessions_, ctx.connection_id());
    if (is_m1 && session_slot) {
        // BLE reconnects with the same connection_id=0: a fresh M1 must start
        // a clean session instead of reusing the completed one.
        erase_session(pair_setup_sessions_, ctx.connection_id());
        session_slot = nullptr;
    }
    if (!session_slot) {
        pair_setup_sessions_.emplace_back(ctx.connection_id(), nullptr);
        session_slot = &pair_setup_sessions_.back().second;
    }
    auto& session = *session_slot;
    if (!session || is_m1) {
        HAP_LOG_INFO(config_.system, "[PairingEndpoints] Creating new pair-setup session");
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
        HAP_LOG_INFO(config_.system, "[PairingEndpoints] Pair-setup response ready (", static_cast<uint64_t>(response_tlv->size()), " bytes)");
        resp.set_body(*response_tlv);
    } else {
        HAP_LOG_ERROR(config_.system, "[PairingEndpoints] Pair-setup failed - no response from session");
        resp = Response{Status::InternalServerError};
        resp.set_body("Pairing error");
    }
    
    return resp;
}

Response PairingEndpoints::handle_pair_verify(const Request& req, ConnectionContext& ctx) {
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] /pair-verify request from connection #", ctx.connection_id(), ", body size: ", req.body.size());
    
    // Parse request to check what state we're handling
    auto tlvs = core::TLV8::parse(req.body);
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::State));
    bool is_m1 = state_val && *state_val == static_cast<uint8_t>(pairing::PairingState::M1);
    
    // Get or create session
    // For BLE: always create a new session on M1 to handle reconnection properly
    // (BLE reconnects with same connection_id=0, so old verified session would be reused)
    auto* session_slot = find_session(pair_verify_sessions_, ctx.connection_id());
    if (is_m1 && session_slot) {
        erase_session(pair_verify_sessions_, ctx.connection_id());
        session_slot = nullptr;
    }
    if (!session_slot) {
        pair_verify_sessions_.emplace_back(ctx.connection_id(), nullptr);
        session_slot = &pair_verify_sessions_.back().second;
    }
    auto& session = *session_slot;
    if (!session || is_m1) {
        HAP_LOG_INFO(config_.system, "[PairingEndpoints] Creating new pair-verify session");
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
        
        if (session->is_verified()) {
            // Defer the upgrade until the M4 response has been sent: the M4
            // response itself must travel in cleartext (HAP session security
            // starts only after Pair Verify completes).
            pending_verify_upgrades_.emplace_back(ctx.connection_id(), std::move(session));
            erase_session(pair_verify_sessions_, ctx.connection_id());
            HAP_LOG_INFO(config_.system, "[PairingEndpoints] Pair-verify succeeded - upgrade pending until response is sent");
        } else {
            HAP_LOG(config_.system, "[PairingEndpoints] Pair-verify response sent (", static_cast<uint64_t>(response_tlv->size()), " bytes)");
        }
    } else {
        HAP_LOG_ERROR(config_.system, "[PairingEndpoints] Pair-verify failed - no response from session");
        resp = Response{Status::InternalServerError};
        resp.set_body("Verification error");
    }
    
    return resp;
}

void PairingEndpoints::complete_pair_verify(ConnectionContext& ctx) {
    auto* slot = find_session(pending_verify_upgrades_, ctx.connection_id());
    if (!slot || !*slot) {
        return;
    }
    auto& session = *slot;
    if (session->is_verified()) {
        HAP_LOG_INFO(config_.system, "[PairingEndpoints] Upgrading connection to encrypted after M4 response");
        // Honor the controller's stored permissions bit (0x01 = admin);
        // legacy pairings without a stored permission default to admin.
        bool admin = true;
        if (auto perm_data = config_.storage->get("pairing_" + session->get_controller_id() + "_perm");
            perm_data && !perm_data->empty()) {
            admin = ((*perm_data)[0] & 0x01) != 0;
        }
        ctx.upgrade_to_secure(
            session->get_session_keys(),
            session->get_shared_secret(),
            session->get_controller_id(),
            admin);
    }
    erase_session(pending_verify_upgrades_, ctx.connection_id());
}

Response PairingEndpoints::handle_pairings(const Request& req, ConnectionContext& ctx) {
    if (!ctx.is_encrypted()) {
        HAP_LOG_ERROR(config_.system, "[PairingEndpoints] /pairings request on unencrypted connection");
        return Response{Status::Unauthorized};
    }

    auto tlvs = core::TLV8::parse(req.body);
    auto method_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::Method));
    if (!method_val) {
        return Response{Status::BadRequest};
    }
    
    pairing::PairingMethod method = static_cast<pairing::PairingMethod>(*method_val);
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] /pairings method: ", static_cast<int>(method));

    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::State), static_cast<uint8_t>(pairing::PairingState::M2));

    if (method == pairing::PairingMethod::AddPairing) {
        if (!ctx.is_admin()) {
            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Authentication));
        } else {
            auto identifier = core::TLV8::find(tlvs, static_cast<uint8_t>(pairing::TLVType::Identifier));
            auto public_key = core::TLV8::find(tlvs, static_cast<uint8_t>(pairing::TLVType::PublicKey));
            auto permissions = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(pairing::TLVType::Permissions));

            if (!identifier || !public_key || public_key->size() != 32 || !permissions) {
                 response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Unknown));
            } else {
                std::string pairing_id(identifier->begin(), identifier->end());
                std::string pairing_key = "pairing_" + pairing_id;

                auto existing_key = config_.storage->get(pairing_key);
                if (existing_key && !existing_key->empty()) {
                    // HAP 5.10.2 step 3a: an existing pairing must only be
                    // updated if the presented LTPK matches the stored one;
                    // a mismatched key for the same identifier is Unknown.
                    if (*existing_key != *public_key) {
                        response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::Unknown));
                    }
                    // else: step 3b - permissions update (persisted below).
                } else {
                    // HAP 5.10.2 step 4a: at least 16 pairings must be supported.
                    auto list_data = config_.storage->get("pairing_list");
                    size_t count = 0;
                    if (list_data) {
                        bool parse_error = false;
                        auto list_json = hap::common::JsonValue::parse(
                            std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error);
                        if (!parse_error && list_json.is_array()) count = list_json.items().size();
                    }
                    if (count >= 16) {
                        response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Error), static_cast<uint8_t>(pairing::TLVError::MaxPeers));
                    } else {
                        config_.storage->set(pairing_key, *public_key);

                        // Maintain the pairing list used for counting and
                        // factory-reset detection.
                        bool parse_error = false;
                        hap::common::JsonValue list_json = list_data
                            ? hap::common::JsonValue::parse(
                                  std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error)
                            : hap::common::JsonValue();
                        if (parse_error || !list_json.is_array()) list_json = hap::common::JsonValue::array();
                        list_json.push_back(pairing_id);
                        std::string list_str = list_json.dump();
                        config_.storage->set("pairing_list",
                                             std::vector<uint8_t>(list_str.begin(), list_str.end()));

                        if (config_.on_pairings_changed) {
                            std::array<uint8_t, 32> ltpk_arr;
                            std::copy_n(public_key->begin(), 32, ltpk_arr.begin());
                            config_.on_pairings_changed(pairing_id, ltpk_arr, true);
                        }
                    }
                }

                // Persist the requested permissions (bit 0x01 = admin) when the
                // request was accepted. Regular users (0x00) are stored so
                // is_admin() can honor them on future sessions.
                if (response_tlvs.size() == 1) { // only State TLV so far = success
                    std::vector<uint8_t> perm_blob{*permissions};
                    config_.storage->set(pairing_key + "_perm", perm_blob);
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

                // Get LTPK before removing (for callback)
                std::array<uint8_t, 32> ltpk_arr{};
                auto ltpk_data = config_.storage->get(pairing_key);
                if (ltpk_data && ltpk_data->size() == 32) {
                    std::copy_n(ltpk_data->begin(), 32, ltpk_arr.begin());
                }

                bool existed = ltpk_data.has_value();
                config_.storage->remove(pairing_key);
                config_.storage->remove(pairing_key + "_perm");

                // HAP 5.11: removing the last admin pairing must remove ALL
                // pairings (the accessory would otherwise be locked out).
                if (existed && pairing_id == ctx.controller_id()) {
                    auto list_data = config_.storage->get("pairing_list");
                    if (list_data) {
                        bool parse_error = false;
                        auto list_json = hap::common::JsonValue::parse(
                            std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error);
                        if (!parse_error && list_json.is_array()) {
                            size_t remaining = 0;
                            for (const auto& id : list_json.items()) {
                                if (id.is_string() && id.as_string() != pairing_id) ++remaining;
                            }
                            if (remaining == 0) {
                                // Last pairing removed: wipe everything.
                                for (const auto& id : list_json.items()) {
                                    if (id.is_string()) {
                                        config_.storage->remove("pairing_" + id.as_string());
                                        config_.storage->remove("pairing_" + id.as_string() + "_perm");
                                    }
                                }
                                config_.storage->remove("pairing_list");
                            }
                        }
                    }
                }

                // Update list. The list may already be gone when the last
                // pairing was just wiped above, so fire the callback in that
                // case too — it drives the mDNS sf=1 (unpaired) update.
                auto list_data = config_.storage->get("pairing_list");
                if (list_data) {
                    bool parse_error = false;
                    auto list_json = hap::common::JsonValue::parse(std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error);
                    if (!parse_error && list_json.is_array()) {
                        hap::common::JsonValue::Array kept;
                        for (auto& id : list_json.items()) {
                            if (!(id.is_string() && id.as_string() == pairing_id)) {
                                kept.push_back(std::move(id));
                            }
                        }
                        list_json.items() = std::move(kept);
                        std::string list_str = list_json.dump();
                        config_.storage->set("pairing_list", std::vector<uint8_t>(list_str.begin(), list_str.end()));
                        if (config_.on_pairings_changed) {
                            config_.on_pairings_changed(pairing_id, ltpk_arr, false);
                        }
                    }
                } else if (existed) {
                    if (config_.on_pairings_changed) {
                        config_.on_pairings_changed(pairing_id, ltpk_arr, false);
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
                bool parse_error = false;
                auto list_json = hap::common::JsonValue::parse(std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error);
                if (!parse_error && list_json.is_array()) {
                    bool first = true;
                    for (const auto& id_json : list_json.items()) {
                        if (!id_json.is_string()) continue;
                        std::string id = id_json.as_string();
                        auto ltpk_data = config_.storage->get("pairing_" + id);
                        if (ltpk_data && ltpk_data->size() == 32) {
                            if (!first) {
                                response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Separator), std::vector<uint8_t>{});
                            }
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Identifier), id);
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::PublicKey), *ltpk_data);
                            // Report the stored permissions (0x01 = admin).
                            // Legacy pairings created before permissions were
                            // tracked default to admin.
                            uint8_t perms = 0x01;
                            if (auto perm_data = config_.storage->get("pairing_" + id + "_perm");
                                perm_data && !perm_data->empty()) {
                                perms = (*perm_data)[0];
                            }
                            response_tlvs.emplace_back(static_cast<uint8_t>(pairing::TLVType::Permissions), std::vector<uint8_t>{perms});
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
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] Accessory ID updated to: ", new_id);
}

void PairingEndpoints::reset() {
    // Clear all session state
    pair_setup_sessions_.clear();
    pair_verify_sessions_.clear();
    pending_verify_upgrades_.clear();
    HAP_LOG_INFO(config_.system, "[PairingEndpoints] All sessions cleared");
}

} // namespace hap::transport
