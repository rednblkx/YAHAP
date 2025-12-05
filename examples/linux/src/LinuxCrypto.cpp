#include "LinuxCrypto.hpp"
#include "simplesrp/simplesrp.h"
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

  EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr);
  EVP_DigestUpdate(ctx, data.data(), data.size());
  unsigned int len = 64;
  EVP_DigestFinal_ex(ctx, output.data(), &len);

  EVP_MD_CTX_free(ctx);
}

// HKDF-SHA512
void LinuxCrypto::hkdf_sha512(std::span<const uint8_t> input_key,
                              std::span<const uint8_t> salt,
                              std::span<const uint8_t> info,
                              std::span<uint8_t> output_key) {
  printf("\n[CRYPTO-HKDF] Deriving key with HKDF-SHA512\n");
  printf("[CRYPTO-HKDF]   Input key size: %zu bytes\n", input_key.size());
  printf("[CRYPTO-HKDF]   Input key (first 16 bytes): ");
  for (size_t i = 0; i < std::min(input_key.size(), size_t(16)); i++) {
    printf("%02x", input_key[i]);
  }
  printf("...\n");
  printf("[CRYPTO-HKDF]   Salt: '");
  for (size_t i = 0; i < salt.size(); i++) {
    if (salt[i] >= 32 && salt[i] < 127)
      printf("%c", salt[i]);
    else
      printf(".");
  }
  printf("' (%zu bytes)\n", salt.size());
  printf("[CRYPTO-HKDF]   Info: '");
  for (size_t i = 0; i < info.size(); i++) {
    if (info[i] >= 32 && info[i] < 127)
      printf("%c", info[i]);
    else
      printf(".");
  }
  printf("' (%zu bytes)\n", info.size());
  printf("[CRYPTO-HKDF]   Output key size: %zu bytes\n", output_key.size());

  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
  EVP_PKEY_derive_init(pctx);
  EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha512());
  EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), salt.size());
  EVP_PKEY_CTX_set1_hkdf_key(pctx, input_key.data(), input_key.size());
  EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(), info.size());

  size_t outlen = output_key.size();
  EVP_PKEY_derive(pctx, output_key.data(), &outlen);
  EVP_PKEY_CTX_free(pctx);

  printf("[CRYPTO-HKDF]   Derived key (first 16 bytes): ");
  for (size_t i = 0; i < std::min(output_key.size(), size_t(16)); i++) {
    printf("%02x", output_key[i]);
  }
  printf("...\n\n");
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
  printf("\n[CRYPTO-CHACHA20] Decrypting with ChaCha20-Poly1305\n");
  printf("[CRYPTO-CHACHA20]   Key (first 8 bytes): ");
  for (size_t i = 0; i < 8; i++)
    printf("%02x", key[i]);
  printf("...\n");
  printf("[CRYPTO-CHACHA20]   Nonce (12 bytes): ");
  for (size_t i = 0; i < 12; i++)
    printf("%02x", nonce[i]);
  printf("\n");
  printf("[CRYPTO-CHACHA20]   AAD size: %zu bytes\n", aad.size());
  printf("[CRYPTO-CHACHA20]   Ciphertext size: %zu bytes\n", ciphertext.size());
  printf("[CRYPTO-CHACHA20]   Tag (16 bytes): ");
  for (size_t i = 0; i < 16; i++)
    printf("%02x", tag[i]);
  printf("\n");

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    printf("[CRYPTO-CHACHA20]   EVP_CIPHER_CTX_new() failed\n");
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

  if (ret > 0) {
    printf("[CRYPTO-CHACHA20]   Decrypted %d bytes\n", plaintext_len + len);
    printf("[CRYPTO-CHACHA20]   Plaintext (first 16 bytes): ");
    for (size_t i = 0; i < std::min(size_t(plaintext_len + len), size_t(16));
         i++) {
      printf("%02x", plaintext[i]);
    }
    printf("...\n\n");
    return true;
  } else {
    printf("[CRYPTO-CHACHA20]   Tag verification failed or decryption error\n");
    printf("[CRYPTO-CHACHA20]   This usually means:\n");
    printf("[CRYPTO-CHACHA20]     - Wrong encryption key\n");
    printf("[CRYPTO-CHACHA20]     - Wrong nonce\n");
    printf("[CRYPTO-CHACHA20]     - Corrupted ciphertext\n");
    printf("[CRYPTO-CHACHA20]     - Wrong AAD\n\n");
    return false;
  }
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
  printf("\n[CRYPTO-SRP] Creating new SRP verifier\n");
  printf("[CRYPTO-SRP]   Username: '%.*s'\n", (int)username.size(),
         username.data());
  printf("[CRYPTO-SRP]   Password: '%.*s'\n", (int)password.size(),
         password.data());
  printf("[CRYPTO-SRP]   Using SHA-512, 3072-bit group\n");

  simplesrp::SRPVerifierGenerator generator(digestType, srpBits);

  // CRITICAL: Set SRPFlagSkipZeroes_M1_M2 for HomeKit/Apple SRP compatibility
  // HAP spec requires M1/M2 calculation without padding
  generator.params.flags = simplesrp::SRPFlagSkipZeroes_M1_M2;

  std::vector<uint8_t> salt;
  std::vector<uint8_t> verifier;
  generator.generate(std::string{username}, std::string{password}, 16, salt,
                     verifier);

  printf("[CRYPTO-SRP]   Generated salt size: %zu bytes\n", salt.size());
  printf("[CRYPTO-SRP]   Generated verifier size: %zu bytes\n",
         verifier.size());
  printf("[CRYPTO-SRP]   Salt (first 8 bytes): ");
  for (size_t i = 0; i < std::min(salt.size(), size_t(8)); i++) {
    printf("%02x", salt[i]);
  }
  printf("...\n");

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

  printf("\n[CRYPTO-SRP] Getting server public key (B)\n");
  printf("[CRYPTO-SRP]   Session username: '%s'\n", session->username.c_str());
  printf("[CRYPTO-SRP]   Salt size: %zu bytes\n", session->salt.size());
  printf("[CRYPTO-SRP]   Verifier size: %zu bytes\n", session->verifier.size());

  std::vector<uint8_t> B;
  std::vector<uint8_t> salt;
  salt.reserve(16);
  std::copy(session->salt.begin(), session->salt.end(),
            std::back_inserter(salt));

  // This generates b and B, and stores them in the server object
  linux_session->server.startAuthentication(session->username, salt,
                                            session->verifier, B);

  printf("[CRYPTO-SRP]   Generated B size: %zu bytes\n", B.size());
  printf("[CRYPTO-SRP]   B (first 8 bytes): ");
  for (size_t i = 0; i < std::min(B.size(), size_t(8)); i++) {
    printf("%02x", B[i]);
  }
  printf("...\n");

  session->server_public_key = B;
  return B;
}

bool LinuxCrypto::srp_set_client_public_key(
    hap::platform::SRPSession *session,
    std::span<const uint8_t> client_public) {
  printf("\n[CRYPTO-SRP] Setting client public key (A)\n");
  printf("[CRYPTO-SRP]   Client A size: %zu bytes\n", client_public.size());
  printf("[CRYPTO-SRP]   A (first 8 bytes): ");
  for (size_t i = 0; i < std::min(client_public.size(), size_t(8)); i++) {
    printf("%02x", client_public[i]);
  }
  printf("...\n");

  // Just store A for now, we'll use it in verify_client_proof
  session->client_public_key.assign(client_public.begin(), client_public.end());
  return true;
}

bool LinuxCrypto::srp_verify_client_proof(hap::platform::SRPSession *session,
                                          std::span<const uint8_t> proof) {
  auto *linux_session = static_cast<LinuxSRPSession *>(session);

  printf("\n[CRYPTO-SRP] ========== VERIFYING CLIENT PROOF ==========\n");
  printf("[CRYPTO-SRP]   Session username: '%s'\n", session->username.c_str());
  printf("[CRYPTO-SRP]   Session password: '%s'\n", session->password.c_str());
  printf("[CRYPTO-SRP]   Client proof (M1) size: %zu bytes\n", proof.size());
  printf("[CRYPTO-SRP]   M1 (first 16 bytes): ");
  for (size_t i = 0; i < std::min(proof.size(), size_t(16)); i++) {
    printf("%02x", proof[i]);
  }
  printf("...\n");
  printf("[CRYPTO-SRP]   Client public key (A) size: %zu bytes\n",
         session->client_public_key.size());
  printf("[CRYPTO-SRP]   Salt (first 8 bytes): ");
  for (size_t i = 0; i < std::min(session->salt.size(), size_t(8)); i++) {
    printf("%02x", session->salt[i]);
  }
  printf("...\n");
  printf("[CRYPTO-SRP]   Verifier size: %zu bytes\n", session->verifier.size());

  std::vector<uint8_t> M1(proof.begin(), proof.end());
  std::vector<uint8_t> M2;

  printf("[CRYPTO-SRP]   Calling simplesrp::verifySession()...\n");
  // Verify A and M1, generate M2 and K
  bool result =
      linux_session->server.verifySession(session->client_public_key, M1, M2);

  printf("[CRYPTO-SRP]   verifySession() returned: %s\n",
         result ? "✓ SUCCESS" : "✗ FAILED");

  if (result) {
    printf("[CRYPTO-SRP]   Generated M2 size: %zu bytes\n", M2.size());
    printf("[CRYPTO-SRP]   M2 (first 16 bytes): ");
    for (size_t i = 0; i < std::min(M2.size(), size_t(16)); i++) {
      printf("%02x", M2[i]);
    }
    printf("...\n");
    session->M1 = std::move(M1);
    session->M2 = std::move(M2);

    linux_session->raw_shared_secret = linux_session->server.sessionKey();

    printf("[CRYPTO-SRP]   Shared secret size: %zu bytes\n",
           linux_session->raw_shared_secret.size());
  } else {
    printf(
        "[CRYPTO-SRP]   VERIFICATION FAILED - Client proof does not match!\n");
  }
  printf("[CRYPTO-SRP] ==========================================\n\n");

  return result;
}

std::vector<uint8_t>
LinuxCrypto::srp_get_server_proof(hap::platform::SRPSession *session) {
  printf("[CRYPTO-SRP] Getting server proof (M2) - size: %zu bytes\n",
         session->M2.size());
  return session->M2;
}

std::vector<uint8_t>
LinuxCrypto::srp_get_session_key(hap::platform::SRPSession *session) {
  auto *linux_session = static_cast<LinuxSRPSession *>(session);

  if (!linux_session->raw_shared_secret.empty()) {
    printf("[CRYPTO-SRP] Returning RAW shared secret (S) - size: %zu bytes\n",
           linux_session->raw_shared_secret.size());
    return linux_session->raw_shared_secret;
  }

  auto key = linux_session->server.sessionKey();
  printf("[CRYPTO-SRP] WARNING: Returning HASHED session key (K) - size: %zu "
         "bytes\n",
         key.size());
  printf("[CRYPTO-SRP] This may cause M5 decryption to fail!\n");
  return key;
}

} // namespace linux_pal
