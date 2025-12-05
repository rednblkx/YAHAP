#include "hap/AccessoryServer.hpp"
#include "hap/HAP.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/Service.hpp"
#include "hap/platform/CryptoSRP.hpp"

#include <iostream>
#include <vector>

// Dummy implementations for verification
class DummyCrypto : public hap::platform::CryptoSRP {
public:
  void sha512(std::span<const uint8_t>, std::span<uint8_t, 64>) override {}
  void hkdf_sha512(std::span<const uint8_t>, std::span<const uint8_t>,
                   std::span<const uint8_t>, std::span<uint8_t>) override {}
  void ed25519_generate_keypair(std::span<uint8_t, 32>,
                                std::span<uint8_t, 64>) override {}
  void ed25519_sign(std::span<const uint8_t, 64>, std::span<const uint8_t>,
                    std::span<uint8_t, 64>) override {}
  bool ed25519_verify(std::span<const uint8_t, 32>, std::span<const uint8_t>,
                      std::span<const uint8_t, 64>) override {
    return true;
  }
  void x25519_generate_keypair(std::span<uint8_t, 32>,
                               std::span<uint8_t, 32>) override {}
  void x25519_shared_secret(std::span<const uint8_t, 32>,
                            std::span<const uint8_t, 32>,
                            std::span<uint8_t, 32>) override {}
  bool chacha20_poly1305_encrypt_and_tag(std::span<const uint8_t, 32>,
                                         std::span<const uint8_t, 12>,
                                         std::span<const uint8_t>,
                                         std::span<const uint8_t>,
                                         std::span<uint8_t>,
                                         std::span<uint8_t, 16>) override {
    return true;
  }
  bool chacha20_poly1305_decrypt_and_verify(std::span<const uint8_t, 32>,
                                            std::span<const uint8_t, 12>,
                                            std::span<const uint8_t>,
                                            std::span<const uint8_t>,
                                            std::span<const uint8_t, 16>,
                                            std::span<uint8_t>) override {
    return true;
  }

  // SRP methods
  std::unique_ptr<hap::platform::SRPSession>
  srp_new_verifier(std::string_view, std::string_view) override {
    return nullptr;
  }
  std::array<uint8_t, 16> srp_get_salt(hap::platform::SRPSession *) override {
    return {};
  }
  std::vector<uint8_t>
  srp_get_public_key(hap::platform::SRPSession *) override {
    return {};
  }
  bool srp_set_client_public_key(hap::platform::SRPSession *,
                                 std::span<const uint8_t>) override {
    return true;
  }
  bool srp_verify_client_proof(hap::platform::SRPSession *,
                               std::span<const uint8_t>) override {
    return true;
  }
  std::vector<uint8_t>
  srp_get_server_proof(hap::platform::SRPSession *) override {
    return {};
  }
  std::vector<uint8_t>
  srp_get_session_key(hap::platform::SRPSession *) override {
    return {};
  }
};

class DummyNetwork : public hap::platform::Network {
public:
  void mdns_register(const MdnsService &) override {}
  void mdns_update_txt_record(const MdnsService &) override {}
  void tcp_listen(uint16_t, ReceiveCallback, DisconnectCallback) override {}
  void tcp_send(ConnectionId, std::span<const uint8_t>) override {}
  void tcp_disconnect(ConnectionId) override {}
};

class DummyStorage : public hap::platform::Storage {
public:
  void set(std::string_view, std::span<const uint8_t>) override {}
  std::optional<std::vector<uint8_t>> get(std::string_view) override {
    return std::nullopt;
  }
  void remove(std::string_view) override {}
  bool has(std::string_view) override { return false; }
};

class DummySystem : public hap::platform::System {
public:
  uint64_t millis() override { return 0; }
  void random_bytes(std::span<uint8_t>) override {}
  void log(LogLevel, std::string_view msg) override {
    std::cout << "[LOG] " << msg << std::endl;
  }
};

int main() {
  DummyCrypto crypto;
  DummyNetwork network;
  DummyStorage storage;
  DummySystem system;

  hap::AccessoryServer::Config config;
  config.crypto = &crypto;
  config.network = &network;
  config.storage = &storage;
  config.system = &system;
  config.accessory_id = "12:34:56:78:9A:BC";
  config.setup_code = "123-45-678";
  config.device_name = "Test Lightbulb";
  config.port = 8080;

  hap::AccessoryServer server(config);

  auto acc = std::make_shared<hap::core::Accessory>(1);
  auto svc = std::make_shared<hap::core::Service>(
      0x3E, "Lightbulb"); // 0x3E is Lightbulb service type
  auto char_on = std::make_shared<hap::core::Characteristic>(
      0x25, // On characteristic type
      hap::core::Format::Bool,
      std::vector<hap::core::Permission>{hap::core::Permission::PairedRead,
                                         hap::core::Permission::PairedWrite,
                                         hap::core::Permission::Notify});

  svc->add_characteristic(char_on);
  acc->add_service(svc);
  server.add_accessory(acc);

  server.start();
  server.stop();

  std::cout << "Architecture verification successful!" << std::endl;
  return 0;
}
