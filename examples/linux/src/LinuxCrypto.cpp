#include "LinuxCrypto.hpp"
#include "simplesrp/simplesrp.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <vector>

namespace linux_pal {

LinuxCrypto::LinuxCrypto() {}
LinuxCrypto::~LinuxCrypto() {}

// SHA-512
void LinuxCrypto::sha512(std::span<const uint8_t> data,
                         std::span<uint8_t, 64> output) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx)
    return;

  unsigned int len = 64;
  if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) == 1 &&
      EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 &&
      EVP_DigestFinal_ex(ctx, output.data(), &len) == 1) {
    // success
  } else {
    std::fill(output.begin(), output.end(), 0);
  }

  EVP_MD_CTX_free(ctx);
}

// HKDF-SHA512
void LinuxCrypto::hkdf_sha512(std::span<const uint8_t> input_key,
                              std::span<const uint8_t> salt,
                              std::span<const uint8_t> info,
                              std::span<uint8_t> output_key) {
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
  if (!pctx)
    return;

  if (EVP_PKEY_derive_init(pctx) <= 0 ||
      EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha512()) <= 0 ||
      EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), salt.size()) <= 0 ||
      EVP_PKEY_CTX_set1_hkdf_key(pctx, input_key.data(), input_key.size()) <= 0 ||
      EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(), info.size()) <= 0) {
    EVP_PKEY_CTX_free(pctx);
    std::fill(output_key.begin(), output_key.end(), 0);
    return;
  }

  size_t outlen = output_key.size();
  if (EVP_PKEY_derive(pctx, output_key.data(), &outlen) <= 0) {
    // Failure must not silently leave stale buffer contents.
    std::fill(output_key.begin(), output_key.end(), 0);
  }
  EVP_PKEY_CTX_free(pctx);
}

// Ed25519 keypair generation
void LinuxCrypto::ed25519_generate_keypair(std::span<uint8_t, 32> public_key,
                                           std::span<uint8_t, 64> private_key) {
  EVP_PKEY *pkey = nullptr;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);

  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &pkey);

  // Extract public key
  size_t pub_len = 32;
  EVP_PKEY_get_raw_public_key(pkey, public_key.data(), &pub_len);

  // Extract private key
  size_t priv_len = 32;
  uint8_t priv_seed[32];
  EVP_PKEY_get_raw_private_key(pkey, priv_seed, &priv_len);

  // HAP uses 64-byte private key (32-byte seed + 32-byte public)
  memcpy(private_key.data(), priv_seed, 32);
  memcpy(private_key.data() + 32, public_key.data(), 32);

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);
}

// Ed25519 sign
void LinuxCrypto::ed25519_sign(std::span<const uint8_t, 64> private_key,
                               std::span<const uint8_t> message,
                               std::span<uint8_t, 64> signature) {
  // Create key from seed (first 32 bytes of private_key)
  EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                                                private_key.data(), 32);

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey);

  size_t sig_len = 64;
  EVP_DigestSign(mdctx, signature.data(), &sig_len, message.data(),
                 message.size());

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pkey);
}

// Ed25519 verify
bool LinuxCrypto::ed25519_verify(std::span<const uint8_t, 32> public_key,
                                 std::span<const uint8_t> message,
                                 std::span<const uint8_t, 64> signature) {
  EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                               public_key.data(), 32);

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey);

  int result = EVP_DigestVerify(mdctx, signature.data(), 64, message.data(),
                                message.size());

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pkey);

  return result == 1;
}

// X25519 keypair generation
void LinuxCrypto::x25519_generate_keypair(std::span<uint8_t, 32> public_key,
                                          std::span<uint8_t, 32> private_key) {
  EVP_PKEY *pkey = nullptr;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);

  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &pkey);

  size_t pub_len = 32;
  EVP_PKEY_get_raw_public_key(pkey, public_key.data(), &pub_len);

  size_t priv_len = 32;
  EVP_PKEY_get_raw_private_key(pkey, private_key.data(), &priv_len);

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);
}

// X25519 shared secret
void LinuxCrypto::x25519_shared_secret(
    std::span<const uint8_t, 32> private_key,
    std::span<const uint8_t, 32> peer_public_key,
    std::span<uint8_t, 32> shared_secret) {
  EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
                                                private_key.data(), 32);

  EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
                                                   peer_public_key.data(), 32);

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
  EVP_PKEY_derive_init(ctx);
  EVP_PKEY_derive_set_peer(ctx, peer_key);

  size_t secret_len = 32;
  EVP_PKEY_derive(ctx, shared_secret.data(), &secret_len);

  EVP_PKEY_CTX_free(ctx);
  EVP_PKEY_free(peer_key);
  EVP_PKEY_free(pkey);
}

// ChaCha20-Poly1305 encrypt
bool LinuxCrypto::chacha20_poly1305_encrypt_and_tag(
    std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce,
    std::span<const uint8_t> aad, std::span<const uint8_t> plaintext,
    std::span<uint8_t> ciphertext, std::span<uint8_t, 16> tag) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
    return false;

  EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key.data(),
                     nonce.data());

  // Set AAD if present
  int len;
  if (!aad.empty()) {
    EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), aad.size());
  }

  // Encrypt
  int ciphertext_len;
  EVP_EncryptUpdate(ctx, ciphertext.data(), &ciphertext_len, plaintext.data(),
                    plaintext.size());

  // Finalize
  int final_len;
  EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertext_len, &final_len);

  // Get tag
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag.data());

  EVP_CIPHER_CTX_free(ctx);
  return true;
}

// ChaCha20-Poly1305 decrypt
bool LinuxCrypto::chacha20_poly1305_decrypt_and_verify(
    std::span<const uint8_t, 32> key, std::span<const uint8_t, 12> nonce,
    std::span<const uint8_t> aad, std::span<const uint8_t> ciphertext,
    std::span<const uint8_t, 16> tag, std::span<uint8_t> plaintext) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return false;
  }

  EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key.data(),
                     nonce.data());

  // Set AAD if present
  int len;
  if (!aad.empty()) {
    EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), aad.size());
  }

  // Decrypt
  int plaintext_len;
  EVP_DecryptUpdate(ctx, plaintext.data(), &plaintext_len, ciphertext.data(),
                    ciphertext.size());

  // Set expected tag
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,
                      const_cast<uint8_t *>(tag.data()));

  // Verify tag
  int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintext_len, &len);
  EVP_CIPHER_CTX_free(ctx);

  return ret > 0;
}

// SRP Implementation using simplesrp
// We derive from SRPSession to hold the stateful simplesrp::SRPServer object

static simplesrp::SRPBits srpBits = simplesrp::SRPBits::Key3072;
static simplesrp::DigestType digestType = simplesrp::DigestType::SHA512;

struct LinuxSRPSession : public hap::platform::SRPSession {
  LinuxSRPSession(std::array<uint8_t, 16> s, std::vector<uint8_t> v,
                  std::string u, std::string p)
      : hap::platform::SRPSession(s, std::move(v), std::move(u), std::move(p)),
        server(digestType, srpBits) {}

  simplesrp::SRPServer server;
  std::vector<uint8_t> raw_shared_secret; // Store unhashed S for HomeKit HKDF
};

std::unique_ptr<hap::platform::SRPSession>
LinuxCrypto::srp_new_verifier(std::string_view username,
                              std::string_view password) {
  simplesrp::SRPVerifierGenerator generator(digestType, srpBits);

  // CRITICAL: Set SRPFlagSkipZeroes_M1_M2 for HomeKit/Apple SRP compatibility
  // HAP spec requires M1/M2 calculation without padding
  generator.params.flags = simplesrp::SRPFlagSkipZeroes_M1_M2;

  std::vector<uint8_t> salt;
  std::vector<uint8_t> verifier;
  generator.generate(std::string{username}, std::string{password}, 16, salt,
                     verifier);

  if (salt.size() < 16 || verifier.empty()) {
    std::fill(salt.begin(), salt.end(), 0);
    return nullptr; // don't construct a session from garbage
  }

  std::array<uint8_t, 16> salt_array;
  std::copy(salt.begin(), salt.begin() + 16, salt_array.begin());

  // Create session with the same flag
  auto session = std::make_unique<LinuxSRPSession>(
      salt_array, std::move(verifier), std::string{username},
      std::string{password});
  // Set the flag on the server's params too
  static_cast<LinuxSRPSession *>(session.get())->server.params.flags =
      simplesrp::SRPFlagSkipZeroes_M1_M2;

  return session;
}

std::array<uint8_t, 16>
LinuxCrypto::srp_get_salt(hap::platform::SRPSession *session) {
  return session->salt;
}

std::vector<uint8_t>
LinuxCrypto::srp_get_public_key(hap::platform::SRPSession *session) {
  auto *linux_session = static_cast<LinuxSRPSession *>(session);

  std::vector<uint8_t> B;
  std::vector<uint8_t> salt;
  salt.reserve(16);
  std::copy(session->salt.begin(), session->salt.end(),
            std::back_inserter(salt));

  // This generates b and B, and stores them in the server object
  linux_session->server.startAuthentication(session->username, salt,
                                            session->verifier, B);

  session->server_public_key = B;
  return B;
}

bool LinuxCrypto::srp_set_client_public_key(
    hap::platform::SRPSession *session,
    std::span<const uint8_t> client_public) {
  // Just store A for now, we'll use it in verify_client_proof
  session->client_public_key.assign(client_public.begin(), client_public.end());
  return true;
}

bool LinuxCrypto::srp_verify_client_proof(hap::platform::SRPSession *session,
                                          std::span<const uint8_t> proof) {
  auto *linux_session = static_cast<LinuxSRPSession *>(session);

  std::vector<uint8_t> M1(proof.begin(), proof.end());
  std::vector<uint8_t> M2;

  // Verify A and M1, generate M2 and K
  bool result =
      linux_session->server.verifySession(session->client_public_key, M1, M2);

  if (result) {
    session->M1 = std::move(M1);
    session->M2 = std::move(M2);
    linux_session->raw_shared_secret = linux_session->server.sessionKey();
  }

  return result;
}

std::vector<uint8_t>
LinuxCrypto::srp_get_server_proof(hap::platform::SRPSession *session) {
  return session->M2;
}

std::vector<uint8_t>
LinuxCrypto::srp_get_session_key(hap::platform::SRPSession *session) {
  auto *linux_session = static_cast<LinuxSRPSession *>(session);

  if (!linux_session->raw_shared_secret.empty()) {
    return linux_session->raw_shared_secret;
  }
  return linux_session->server.sessionKey();
}

} // namespace linux_pal
