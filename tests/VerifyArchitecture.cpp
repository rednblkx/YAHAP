#include "hap/AccessoryServer.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/Service.hpp"
#include "TestUtil.hpp"
#include "MockPal.hpp"

#include <iostream>

// Dummy network: the TCP server is never started in this test, only the
// wiring between AccessoryServer and its platform dependencies is exercised.
class DummyNetwork : public hap::platform::Network {
public:
  void mdns_register(const MdnsService&) override {}
  void mdns_update_txt_record(const MdnsService&) override {}
  void tcp_listen(uint16_t, ReceiveCallback, DisconnectCallback) override {}
  void tcp_send(ConnectionId, std::span<const uint8_t>) override {}
  void tcp_disconnect(ConnectionId) override {}
};

int main() {
  testmock::MockCrypto crypto;
  DummyNetwork network;
  testmock::MockStorage storage;
  testmock::MockSystem system{false};

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
      hap::core::Permissions{hap::core::Permission::PairedRead,
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
