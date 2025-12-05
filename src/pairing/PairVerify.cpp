#include "hap/pairing/PairVerify.hpp"
#include <algorithm>

namespace hap::pairing {

PairVerify::PairVerify(Config config) 
    : config_(std::move(config)), state_(State::M1_AwaitingVerifyStartRequest) {
    load_long_term_keys();
}

PairVerify::~PairVerify() = default;

void PairVerify::reset() {
    state_ = State::M1_AwaitingVerifyStartRequest;
    p_session_key_ = {};
    shared_secret_ = {};
    controller_id_.clear();
}

void PairVerify::load_long_term_keys() {
    auto ltsk_data = config_.storage->get("accessory_ltsk");
    auto ltpk_data = config_.storage->get("accessory_ltpk");
    
    if (ltsk_data && ltpk_data && ltsk_data->size() == 64 && ltpk_data->size() == 32) {
        std::copy_n(ltsk_data->begin(), 64, accessory_ltsk_.begin());
        std::copy_n(ltpk_data->begin(), 32, accessory_ltpk_.begin());
    } else {
        // Keys should have been generated during Pair Setup
        // For now, generate new ones (this is not correct behavior)
        config_.crypto->ed25519_generate_keypair(accessory_ltpk_, accessory_ltsk_);
    }
}

std::optional<std::vector<uint8_t>> PairVerify::handle_request(std::span<const uint8_t> request_tlv) {
    auto tlvs = core::TLV8::parse(request_tlv);
    
    auto state_val = core::TLV8::find_uint8(tlvs, static_cast<uint8_t>(TLVType::State));
    if (!state_val) {
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    PairingState requested_state = static_cast<PairingState>(*state_val);
    
    switch (requested_state) {
        case PairingState::M1:
            return handle_m1(tlvs);
        case PairingState::M3:
            return handle_m3(tlvs);
        default:
            return build_error_response(PairingState::M2, TLVError::Unknown);
    }
}

std::optional<std::vector<uint8_t>> PairVerify::handle_m1(const std::vector<core::TLV>& request) {
    if (state_ != State::M1_AwaitingVerifyStartRequest) {
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    // Extract client's Curve25519 public key
    auto client_public_key = core::TLV8::find(request, static_cast<uint8_t>(TLVType::PublicKey));
    if (!client_public_key || client_public_key->size() != 32) {
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    std::copy_n(client_public_key->begin(), 32, client_curve_public_.begin());
    
    // Generate accessory's ephemeral Curve25519 key pair
    config_.crypto->x25519_generate_keypair(accessory_curve_public_, accessory_curve_secret_);
    
    // Compute shared secret
    config_.crypto->x25519_shared_secret(accessory_curve_secret_, client_curve_public_, shared_secret_);
    
    // Derive SessionKey using HKDF-SHA-512
    config_.crypto->hkdf_sha512(
        shared_secret_,
        std::span(reinterpret_cast<const uint8_t*>("Pair-Verify-Encrypt-Salt"), 24),
        std::span(reinterpret_cast<const uint8_t*>("Pair-Verify-Encrypt-Info"), 24),
        p_session_key_
    );
    
    // Construct AccessoryInfo = AccessoryCurvePublicKey || AccessoryPairingID || ClientCurvePublicKey
    std::vector<uint8_t> accessory_info;
    accessory_info.insert(accessory_info.end(), accessory_curve_public_.begin(), accessory_curve_public_.end());
    accessory_info.insert(accessory_info.end(), config_.accessory_id.begin(), config_.accessory_id.end());
    accessory_info.insert(accessory_info.end(), client_curve_public_.begin(), client_curve_public_.end());
    
    // Sign AccessoryInfo with long-term secret key
    std::array<uint8_t, 64> accessory_signature;
    config_.crypto->ed25519_sign(accessory_ltsk_, accessory_info, accessory_signature);
    
    // Build sub-TLV
    std::vector<core::TLV> sub_tlvs;
    sub_tlvs.emplace_back(static_cast<uint8_t>(TLVType::Identifier), config_.accessory_id);
    sub_tlvs.emplace_back(static_cast<uint8_t>(TLVType::Signature), std::span(accessory_signature));
    auto sub_tlv_data = core::TLV8::encode(sub_tlvs);
    
    // Encrypt sub-TLV with ChaCha20-Poly1305
    const char nonce_str[] = "\x00\x00\x00\x00PV-Msg02";
    std::array<uint8_t, 12> nonce = {};
    std::copy_n(nonce_str, sizeof(nonce_str) - 1, nonce.begin());
    
    std::vector<uint8_t> ciphertext(sub_tlv_data.size());
    std::array<uint8_t, 16> auth_tag;
    
    if (!config_.crypto->chacha20_poly1305_encrypt_and_tag(
            p_session_key_, nonce, {}, sub_tlv_data, ciphertext, auth_tag)) {
        return build_error_response(PairingState::M2, TLVError::Unknown);
    }
    
    // Append auth tag
    ciphertext.insert(ciphertext.end(), auth_tag.begin(), auth_tag.end());
    
    // Build M2 response
    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(PairingState::M2));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::PublicKey), std::span(accessory_curve_public_));
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::EncryptedData), ciphertext);
    
    state_ = State::M3_AwaitingVerifyFinishRequest;
    return core::TLV8::encode(response_tlvs);
}

std::optional<std::vector<uint8_t>> PairVerify::handle_m3(const std::vector<core::TLV>& request) {
    if (state_ != State::M3_AwaitingVerifyFinishRequest) {
        return build_error_response(PairingState::M4, TLVError::Unknown);
    }
    
    // Extract encrypted data
    auto encrypted_data_tlv = core::TLV8::find(request, static_cast<uint8_t>(TLVType::EncryptedData));
    if (!encrypted_data_tlv || encrypted_data_tlv->size() < 16) {
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    // Split ciphertext and auth tag
    std::vector<uint8_t> ciphertext(encrypted_data_tlv->begin(), encrypted_data_tlv->end() - 16);
    std::array<uint8_t, 16> auth_tag;
    std::copy_n(encrypted_data_tlv->end() - 16, 16, auth_tag.begin());
    
    // Decrypt with ChaCha20-Poly1305
    const char nonce_str[] = "\x00\x00\x00\x00PV-Msg03";
    std::array<uint8_t, 12> nonce = {};
    std::copy_n(nonce_str, sizeof(nonce_str) - 1, nonce.begin());
    
    std::vector<uint8_t> plaintext(ciphertext.size());
    if (!config_.crypto->chacha20_poly1305_decrypt_and_verify(
            p_session_key_, nonce, {}, ciphertext, auth_tag, plaintext)) {
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    // Parse decrypted sub-TLV
    auto sub_tlvs = core::TLV8::parse(plaintext);
    auto ios_identifier = core::TLV8::find(sub_tlvs, static_cast<uint8_t>(TLVType::Identifier));
    auto ios_signature = core::TLV8::find(sub_tlvs, static_cast<uint8_t>(TLVType::Signature));
    
    if (!ios_identifier || !ios_signature || ios_signature->size() != 64) {
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    controller_id_ = std::string(ios_identifier->begin(), ios_identifier->end());

    // Look up iOS device's long-term public key from storage
    std::string pairing_key = "pairing_" + std::string(ios_identifier->begin(), ios_identifier->end());
    auto ios_ltpk_data = config_.storage->get(pairing_key);
    if (!ios_ltpk_data || ios_ltpk_data->size() != 32) {
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    std::array<uint8_t, 32> ios_ltpk;
    std::copy_n(ios_ltpk_data->begin(), 32, ios_ltpk.begin());
    
    // Construct iOSDeviceInfo = ClientCurvePublicKey || iOSDevicePairingID || AccessoryCurvePublicKey
    std::vector<uint8_t> ios_device_info;
    ios_device_info.insert(ios_device_info.end(), client_curve_public_.begin(), client_curve_public_.end());
    ios_device_info.insert(ios_device_info.end(), ios_identifier->begin(), ios_identifier->end());
    ios_device_info.insert(ios_device_info.end(), accessory_curve_public_.begin(), accessory_curve_public_.end());
    
    // Verify signature
    std::array<uint8_t, 64> ios_sig_arr;
    std::copy_n(ios_signature->begin(), 64, ios_sig_arr.begin());
    
    if (!config_.crypto->ed25519_verify(ios_ltpk, ios_device_info, ios_sig_arr)) {
        return build_error_response(PairingState::M4, TLVError::Authentication);
    }
    
    // Build M4 response (success - empty TLV with just state)
    std::vector<core::TLV> response_tlvs;
    response_tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(PairingState::M4));
    
    state_ = State::Verified;

    config_.crypto->hkdf_sha512(
        shared_secret_,
        std::span(reinterpret_cast<const uint8_t*>("Control-Salt"), 12),
        std::span(reinterpret_cast<const uint8_t*>("Control-Read-Encryption-Key"), 27),
        a_session_key_
    );

    config_.crypto->hkdf_sha512(
        shared_secret_,
        std::span(reinterpret_cast<const uint8_t*>("Control-Salt"), 12),
        std::span(reinterpret_cast<const uint8_t*>("Control-Write-Encryption-Key"), 28),
        c_session_key_
    );

    return core::TLV8::encode(response_tlvs);
}

std::vector<uint8_t> PairVerify::build_error_response(PairingState state, TLVError error) {
    std::vector<core::TLV> tlvs;
    tlvs.emplace_back(static_cast<uint8_t>(TLVType::State), static_cast<uint8_t>(state));
    tlvs.emplace_back(static_cast<uint8_t>(TLVType::Error), static_cast<uint8_t>(error));
    return core::TLV8::encode(tlvs);
}

} // namespace hap::pairing
