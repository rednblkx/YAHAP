#include "hap/pairing/PairSetup.hpp"
#include "hap/pairing/PairingKeys.hpp"
#include "hap/common/Log.hpp"
#include "hap/common/JsonValue.hpp"
#include <algorithm>
#include <cstring>

namespace hap::pairing {

PairSetup::PairSetup(Config config) 
    : config_(std::move(config)), state_(State::M1_AwaitingSRPStartRequest) {
    ensure_long_term_keys();
}

PairSetup::~PairSetup() = default;

void PairSetup::reset() {
    state_ = State::M1_AwaitingSRPStartRequest;
    srp_session_.reset();
    session_key_.clear();
}

void PairSetup::ensure_long_term_keys() {
    if (pairing::load_accessory_ltk(config_.storage, accessory_ltsk_, accessory_ltpk_)) {
        return;
    }
    config_.crypto->ed25519_generate_keypair(accessory_ltpk_, accessory_ltsk_);
    config_.storage->set(pairing::kAccessoryLTPKKey, accessory_ltpk_);
    config_.storage->set(pairing::kAccessoryLTSKKey, accessory_ltsk_);
}

std::optional<std::vector<uint8_t>> PairSetup::handle_request(std::span<const uint8_t> request_tlv) {
    HAP_LOG(config_.system, "[PairSetup] Received request (", request_tlv.size(), " bytes)");
    
    auto tlvs = core::TLV8::parse(request_tlv);
    
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(TLVType::State));
    if (!state_val) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] No state TLV found in request");
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    PairingState requested_state = static_cast<PairingState>(*state_val);
    HAP_LOG_INFO(config_.system, "[PairSetup] Processing message state: M", static_cast<int>(*state_val));
    
    switch (requested_state) {
        case PairingState::M1:
            return handle_m1(tlvs);
        case PairingState::M3:
            return handle_m3(tlvs);
        case PairingState::M5:
            return handle_m5(tlvs);
        default:
            HAP_LOG_ERROR(config_.system, "[PairSetup] Unknown pairing state: ", *state_val);
            return build_error_response(PairingState::M2, TLVError::Unknown);
    }
}

std::optional<std::vector<uint8_t>> PairSetup::handle_m1(const std::vector<core::TLV>& request) {
    HAP_LOG_INFO(config_.system, "[PairSetup] Processing M1 (SRP Start Request)");
    
    if (state_ != State::M1_AwaitingSRPStartRequest) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Unexpected state for M1 - current state: ", static_cast<int>(state_));
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    auto method = core::TLV8::find_uint8(request, static_cast<uint8_t>(TLVType::Method));
    if (!method || *method != static_cast<uint8_t>(PairingMethod::PairSetup)) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Invalid or missing pairing method in M1");
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }

    // HAP 5.6.2 step 2: once locked out (>100 failed attempts), refuse to
    // even start a new SRP session.
    auto fail_data = config_.storage->get("pair_setup_fails");
    if (fail_data && fail_data->size() == sizeof(uint32_t)) {
        uint32_t fails = 0;
        std::memcpy(&fails, fail_data->data(), sizeof(uint32_t));
        if (fails > 100) {
            HAP_LOG_WARN(config_.system, "[PairSetup] Locked out after ", fails, " failed attempts");
            return build_error_response(PairingState::M2, TLVError::MaxTries);
        }
    }

    HAP_LOG(config_.system, "[PairSetup] Creating SRP verifier");
    srp_session_ = config_.crypto->srp_new_verifier("Pair-Setup", config_.setup_code);
    if (!srp_session_) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Failed to create SRP session");
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    auto salt = config_.crypto->srp_get_salt(srp_session_.get());
    auto public_key = config_.crypto->srp_get_public_key(srp_session_.get());
    
    HAP_LOG(config_.system, "[PairSetup] SRP salt size: ", salt.size(), ", public key size: ", public_key.size());
    
    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(PairingState::M2));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::Salt), std::span(salt));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::PublicKey), public_key);
    
    state_ = State::M3_AwaitingSRPVerifyRequest;
    HAP_LOG_INFO(config_.system, "[PairSetup] M2 response ready");
    return core::TLV8::encode(response_tlvs);
}

std::optional<std::vector<uint8_t>> PairSetup::handle_m3(const std::vector<core::TLV>& request) {
    HAP_LOG_INFO(config_.system, "[PairSetup] Processing M3 (SRP Verify Request)");
    
    if (state_ != State::M3_AwaitingSRPVerifyRequest || !srp_session_) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Unexpected state for M3 or missing SRP session");
        return build_error_response(PairingState::M4, TLVError::Unknown);
    }
    
    auto client_public_key = core::TLV8::find(request, static_cast<uint8_t>(TLVType::PublicKey));
    auto client_proof = core::TLV8::find(request, static_cast<uint8_t>(TLVType::Proof));
    
    if (!client_public_key || !client_proof) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Missing client public key or proof in M3");
        return build_error_response(PairingState::M4, TLVError::Unknown);
    }
    
    HAP_LOG(config_.system, "[PairSetup] Client public key size: ", client_public_key->size(), ", proof size: ", client_proof->size());
    
    if (!config_.crypto->srp_set_client_public_key(srp_session_.get(), *client_public_key)) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Failed to set client public key");
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    HAP_LOG(config_.system, "[PairSetup] Verifying client proof");
    if (!config_.crypto->srp_verify_client_proof(srp_session_.get(), *client_proof)) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Client proof verification FAILED");
        // HAP 5.6.2 step 2: after more than 100 unsuccessful authentication
        // attempts the accessory must refuse further Pair Setup with MaxTries.
        // The counter persists so a reboot cannot reset the lockout.
        uint32_t fails = 0;
        auto fail_data = config_.storage->get("pair_setup_fails");
        if (fail_data && fail_data->size() == sizeof(uint32_t)) {
            std::memcpy(&fails, fail_data->data(), sizeof(uint32_t));
        }
        ++fails;
        std::vector<uint8_t> fail_bytes(sizeof(uint32_t));
        std::memcpy(fail_bytes.data(), &fails, sizeof(uint32_t));
        config_.storage->set("pair_setup_fails", fail_bytes);
        if (fails > 100) {
            return build_error_response(PairingState::M4, TLVError::MaxTries);
        }
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }

    // Successful authentication clears the failed-attempt counter.
    config_.storage->remove("pair_setup_fails");
    
    HAP_LOG_INFO(config_.system, "[PairSetup] Client proof verified successfully");
    
    auto server_proof = config_.crypto->srp_get_server_proof(srp_session_.get());
    session_key_ = config_.crypto->srp_get_session_key(srp_session_.get());
    
    HAP_LOG(config_.system, "[PairSetup] Server proof size: ", server_proof.size(), ", session key size: ", session_key_.size());
    
    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(PairingState::M4));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::Proof), server_proof);
    
    state_ = State::M5_AwaitingExchangeRequest;
    HAP_LOG_INFO(config_.system, "[PairSetup] M4 response ready");
    return core::TLV8::encode(response_tlvs);
}

std::optional<std::vector<uint8_t>> PairSetup::handle_m5(const std::vector<core::TLV>& request) {
    HAP_LOG_INFO(config_.system, "[PairSetup] Processing M5 (Exchange Request)");
    
    if (state_ != State::M5_AwaitingExchangeRequest || session_key_.empty()) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Unexpected state for M5 or missing session key");
        return build_error_response(PairingState::M6, TLVError::Unknown);
    }
    
    auto encrypted_data_tlv = core::TLV8::find(request, static_cast<uint8_t>(TLVType::EncryptedData));
    if (!encrypted_data_tlv || encrypted_data_tlv->size() < 16) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Missing or invalid encrypted data in M5");
        return build_error_response(PairingState::M6, TLVError::Authentication);
    }
    
    HAP_LOG(config_.system, "[PairSetup] Encrypted data size: ", encrypted_data_tlv->size());

    std::vector<uint8_t> ciphertext(encrypted_data_tlv->begin(), encrypted_data_tlv->end() - 16);
    std::array<uint8_t, 16> auth_tag;
    std::copy_n(encrypted_data_tlv->end() - 16, 16, auth_tag.begin());
    
    std::array<uint8_t, 32> session_key;
    HAP_LOG(config_.system, "[PairSetup] Deriving session key using HKDF");
    HAP_LOG(config_.system, "[PairSetup] Session key size: ", session_key_.size());
    std::string hkdf_salt = "Pair-Setup-Encrypt-Salt";
    std::string hkdf_info = "Pair-Setup-Encrypt-Info";
    HAP_LOG(config_.system, "[PairSetup] HKDF salt: ", hkdf_salt);
    HAP_LOG(config_.system, "[PairSetup] HKDF info: ", hkdf_info);
    config_.crypto->hkdf_sha512(
        session_key_,
        std::span(reinterpret_cast<const uint8_t*>(hkdf_salt.data()), hkdf_salt.size()),
        std::span(reinterpret_cast<const uint8_t*>(hkdf_info.data()), hkdf_info.size()),
        session_key
    );
    
    const char nonce_str[] = "\x00\x00\x00\x00PS-Msg05";
    std::array<uint8_t, 12> nonce = {};
    std::copy_n(nonce_str, sizeof(nonce_str) - 1, nonce.begin());
    
    HAP_LOG(config_.system, "[PairSetup] Decrypting M5 payload");
    std::vector<uint8_t> plaintext(ciphertext.size());
    if (!config_.crypto->chacha20_poly1305_decrypt_and_verify(
            session_key, nonce, {}, ciphertext, auth_tag, plaintext)) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] Failed to decrypt M5 payload");
        return build_error_response(PairingState::M6, TLVError::Authentication);
    }
    
    HAP_LOG(config_.system, "[PairSetup] Decrypted ", plaintext.size(), " bytes");
    
    auto sub_tlvs = core::TLV8::parse(plaintext);
    auto ios_identifier = core::TLV8::find(sub_tlvs, static_cast<uint8_t>(TLVType::Identifier));
    auto ios_ltpk = core::TLV8::find(sub_tlvs, static_cast<uint8_t>(TLVType::PublicKey));
    auto ios_signature = core::TLV8::find(sub_tlvs, static_cast<uint8_t>(TLVType::Signature));
    
    if (!ios_identifier || !ios_ltpk || !ios_signature || ios_ltpk->size() != 32 || ios_signature->size() != 64) {
        return build_error_response(PairingState::M6, TLVError::Authentication);
    }
    
    // Derive iOSDeviceX from session key
    std::array<uint8_t, 32> ios_device_x;
    config_.crypto->hkdf_sha512(
        session_key_,
        std::span(reinterpret_cast<const uint8_t*>("Pair-Setup-Controller-Sign-Salt"), 31),
        std::span(reinterpret_cast<const uint8_t*>("Pair-Setup-Controller-Sign-Info"), 31),
        ios_device_x
    );
    
    // Construct iOSDeviceInfo = iOSDeviceX || iOSDevicePairingID || iOSDeviceLTPK
    std::vector<uint8_t> ios_device_info;
    ios_device_info.insert(ios_device_info.end(), ios_device_x.begin(), ios_device_x.end());
    ios_device_info.insert(ios_device_info.end(), ios_identifier->begin(), ios_identifier->end());
    ios_device_info.insert(ios_device_info.end(), ios_ltpk->begin(), ios_ltpk->end());
    
    std::array<uint8_t, 32> ios_ltpk_arr;
    std::copy_n(ios_ltpk->begin(), 32, ios_ltpk_arr.begin());
    std::array<uint8_t, 64> ios_sig_arr;
    std::copy_n(ios_signature->begin(), 64, ios_sig_arr.begin());
    
    HAP_LOG(config_.system, "[PairSetup] Verifying iOS device signature");
    if (!config_.crypto->ed25519_verify(ios_ltpk_arr, ios_device_info, ios_sig_arr)) {
        HAP_LOG_ERROR(config_.system, "[PairSetup] iOS device signature verification FAILED");
        return build_error_response(PairingState::M6, TLVError::Authentication);
    }
    
    HAP_LOG_INFO(config_.system, "[PairSetup] iOS device signature verified successfully");
    
    std::string pairing_id(ios_identifier->begin(), ios_identifier->end());
    std::string pairing_key = "pairing_" + pairing_id;
    HAP_LOG_INFO(config_.system, "[PairSetup] Saving pairing for controller: ", pairing_id);
    config_.storage->set(pairing_key, *ios_ltpk);

    auto list_data = config_.storage->get("pairing_list");
    bool parse_error = false;
    hap::common::JsonValue list_json = list_data
        ? hap::common::JsonValue::parse(std::string_view(reinterpret_cast<const char*>(list_data->data()), list_data->size()), &parse_error)
        : hap::common::JsonValue();
    if (parse_error || !list_json.is_array()) {
        list_json = hap::common::JsonValue::array();
    }

    bool exists = false;
    for (const auto& id : list_json.items()) {
        if (id.is_string() && id.as_string() == pairing_id) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        list_json.push_back(pairing_id);
        std::string list_str = list_json.dump();
        std::vector<uint8_t> list_bytes(list_str.begin(), list_str.end());
        config_.storage->set("pairing_list", list_bytes);

        if (config_.on_pairings_changed) {
            config_.on_pairings_changed(pairing_id, ios_ltpk_arr, true);
        }
    }
    
    // --- Generate M6 Response ---
    
    // Derive AccessoryX
    std::array<uint8_t, 32> accessory_x;
    config_.crypto->hkdf_sha512(
        session_key_,
        std::span(reinterpret_cast<const uint8_t*>("Pair-Setup-Accessory-Sign-Salt"), 30),
        std::span(reinterpret_cast<const uint8_t*>("Pair-Setup-Accessory-Sign-Info"), 30),
        accessory_x
    );
    
    // Construct AccessoryInfo = AccessoryX || AccessoryPairingID || AccessoryLTPK
    std::vector<uint8_t> accessory_info;
    accessory_info.insert(accessory_info.end(), accessory_x.begin(), accessory_x.end());
    accessory_info.insert(accessory_info.end(), config_.accessory_id.begin(), config_.accessory_id.end());
    accessory_info.insert(accessory_info.end(), accessory_ltpk_.begin(), accessory_ltpk_.end());
    
    std::array<uint8_t, 64> accessory_signature;
    config_.crypto->ed25519_sign(accessory_ltsk_, accessory_info, accessory_signature);
    
    std::vector<core::TLV> sub_tlvs_resp;
    sub_tlvs_resp.emplace_back(static_cast<uint8_t>(TLVType::Identifier), config_.accessory_id);
    sub_tlvs_resp.emplace_back(static_cast<uint8_t>(TLVType::PublicKey), std::span(accessory_ltpk_));
    sub_tlvs_resp.emplace_back(static_cast<uint8_t>(TLVType::Signature), std::span(accessory_signature));
    
    auto sub_tlv_data = core::TLV8::encode(sub_tlvs_resp);
    
    const char nonce_m6_str[] = "\x00\x00\x00\x00PS-Msg06";
    std::array<uint8_t, 12> nonce_m6 = {};
    std::copy_n(nonce_m6_str, sizeof(nonce_m6_str) - 1, nonce_m6.begin());
    
    std::vector<uint8_t> ciphertext_m6(sub_tlv_data.size());
    std::array<uint8_t, 16> auth_tag_m6;
    
    if (!config_.crypto->chacha20_poly1305_encrypt_and_tag(
            session_key, nonce_m6, {}, sub_tlv_data, ciphertext_m6, auth_tag_m6)) {
        return build_error_response(PairingState::M6, TLVError::Unknown);
    }
    
    ciphertext_m6.insert(ciphertext_m6.end(), auth_tag_m6.begin(), auth_tag_m6.end());
    
    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(PairingState::M6));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::EncryptedData), ciphertext_m6);
    
    state_ = State::Completed;
    HAP_LOG_INFO(config_.system, "[PairSetup] Pair-Setup completed successfully! M6 response ready.");
    return core::TLV8::encode(response_tlvs);
}

std::vector<uint8_t> PairSetup::build_error_response(PairingState state, TLVError error) {
    std::vector<core::TLV> tlvs;
    tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(state));
    tlvs.emplace_back(static_cast<uint8_t>(TLVType::Error), static_cast<uint8_t>(error));
    return core::TLV8::encode(tlvs);
}

} // namespace hap::pairing
